//++
//protocol.c - GEI serial protocol routines for PromICE
//
//   COPYRIGHT (C) 2023 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the PromICE project.  PromICE is free software; you
// may redistribute it and/or modify it under the terms of the GNU Affero
// General Public License as published by the Free Software Foundation, either
// version 3 of the License, or (at your option) any later version.
//
//    PromICE is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more
// details.  You should have received a copy of the GNU Affero General Public
// License along with PromICE.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This module implements Grammar Engine PromICE serial protocol.  It provides
// several functions for connecting, uploading or downloading files, and so on.
// The general rule is that you should call geiConnect() first, before anything
// else, to establish the baud rate and link to the PromICE.  After that you can
// do as you please, and then call geiDisconnect() when you're finished.  The
// latter will reset the PromICE and put it back into RUN (emulation) mode.
// 
//      geiConnect()      - connect and return the number of units
//      geiDisconnect()   - disconnect and put the PromICE in RUN mode
//      geiGetVersion()   - return PromICE firmware version
//      geiGetSerial()    - return PromICE serial number
//      geiGetSize()      - return PromICE memory size
//      geiAddressMask()  - return suitable address mask
//      geiTestRAM()      - execute TEST RAM command
//      geiFillRAM()      - execute FILL RAM command
//      geiResetTarget()  - toggle PromICE RESET output (reset target)
//      geiLoadMode()     - put PromICE in LOAD mode
//      geiDownload()     - download data to the PromICE
//      geiUpload()       - upload data from the PromICE
// 
// NOTES:
//   One subtle "gotcha" of the PromICE is that it expects all unused address
// bits to be ones, not zeros.  This is important if the EPROM you want to
// emulate is smaller than the actual memory size of the PromICE.  For example,
// if you're using a 1Mb PromICE to emulate a 32K 27C256 EPROM, then the upper
// nine bits of the addresses (0xFF8000) must be ones.  This affects both the
// DOWNLOAD and the UPLOAD commands.  The geiAddressMask() method can help
// the caller figure out a suitable mask for the unused bits.
//
// REVISION HISTORY:
// 24-MAR-23  RLA  New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdio.h>              // printf(), FILE, etc ...
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>             // uint8_t, uint32_t, etc ...
#include <stdbool.h>            // bool, true, false, etc ...
#include <stdarg.h>             // va_arg, va_list, et al ...
#include <assert.h>             // assert() (what else??)
#include <memory.h>             // memset(), et al ...
#include "PromICE.h"            // global declarations for this project
#include "serial.h"             // host specific serial port routines
#include "protocol.h"           // declarations for this module

// Local data for this module ...
PRIVATE GEI_MESSAGE m_geiCommand;   // command to send to the PromICE
PRIVATE GEI_MESSAGE m_geiResponse;  // response received from the PromICE
PRIVATE uint8_t     m_geiUnits;     // number of units attached


PUBLIC uint8_t geiConnect (const char *pszName, uint32_t nBaud)
{
  //++
  //   This routine will "connect" to the PromICE.  It opens the serial port,
  // sets the baud rate, and toggles DTR, which acts as a "hard reset" for the
  // PromICE.  It then executes the autobaud sequence and negotiates a common
  // baud rate with the PromICE.  After that we send the "IDENTIFY" command,
  // which causes the PromICE to return the number of units in the daisy chain,
  // and that count is returned by this function.
  // 
  //   This function never fails - if we can't talk with the PromICE then
  // we print an error message and abort.
  //--
  uint32_t lTimeout = 0;  uint8_t bData;
  serOpen(pszName, nBaud);
  fprintf(stderr, "Connecting to %s at %d baud ...", pszName, nBaud);

  //   Set DTR and the clear it - this causes the PromICE to reset.  Yes, the
  // PromICE wants to see DTR deasserted for normal operation.  That seems kind
  // of backwards to me, but I didn't design it.  The minimum pulse with for
  // reset isn't specified, but empirically 100ms is plenty.
  serSetDTR(true);  serSleep(SHORT_TIMEOUT);
  serSetDTR(false);  serSleep(LONG_TIMEOUT);

  //   We the AUTOBAUD command (it's just a ^C, 0x03!) and when the PromICE
  // finds the right baud rate and recognizes it, the PromICE will respond
  // with the same.  We just keep repeating this until we both agree.
  for (;;) {
    serFlush();  serSendByte(GEI_AUTOBAUD);
    if (serReceiveByte(&bData, SHORT_TIMEOUT)) {
      //   If the PromICE replied with AUTOBAUD too, then we're in sync.  Just
      // ignore any other garbage we might see..
      if (bData == GEI_AUTOBAUD) break;
    } else {
      fprintf(stderr, ".");  lTimeout += SHORT_TIMEOUT;
    }
    // Give up after 20 seconds ...
    if (lTimeout >= CONNECT_TIMEOUT)
      {fprintf(stderr, " TIMEOUT\n");  FatalError("no response to AUTOBAUD from PromICE");}
  }

  //   Ok, we agree on the baud rate.  Send one IDENTIFY command (it's a null
  // byte, 0x00) and wait until the PromICE should echos that back.
  lTimeout = 0;  serSendByte(GEI_IDENTIFY);
  for (;;) {
    if (serReceiveByte(&bData, SHORT_TIMEOUT)) {
      //   Sometimes the PromICE will send additional AUTOBAUD characters,
      // which we can ignore.  When we get the IDENTIFY back, we're in sync
      // and at this point anything else is an error.
      if (bData == GEI_AUTOBAUD) continue;
      if (bData == GEI_IDENTIFY) break;
      fprintf(stderr, " FAILED\n");
      FatalError("expecting 0x00 but received 0x%02X from PromICE", bData);
    } else {
      fprintf(stderr, ".");  lTimeout += SHORT_TIMEOUT;
    }
    if (lTimeout >= CONNECT_TIMEOUT)
      {fprintf(stderr, " TIMEOUT\n");  FatalError("no response to IDENTIFY from PromICE");}
  }

  //   And lastly send one more IDENTIFY command.  The PromICE will actually
  // execute this one, and will return the number of units in the daisy chain.
  lTimeout = 0;  serSendByte(GEI_IDENTIFY);
  for (;;) {
    if (serReceiveByte(&bData, SHORT_TIMEOUT)) {
      //  In theory any number of units can be chained, but we only support one
      // or two.  Any other response we'll consider to be an error ...
      if ((bData > 0) && (bData <= MAXUNIT)) break;
      fprintf(stderr, " FAILED\n");  FatalError("invalid unit count %d", bData);
    } else {
      fprintf(stderr, ".");  lTimeout += SHORT_TIMEOUT;
    }
    if (lTimeout >= CONNECT_TIMEOUT)
      {fprintf(stderr, " TIMEOUT\n");  FatalError("no response to IDENTIFY from PromICE");}
  }

  // Success!
  fprintf(stderr, " %d unit(s)\n", bData);
  return (m_geiUnits = bData);
}

PRIVATE void geiDoCommand (uint32_t lTimeout)
{
  //++
  //   This routine will send a command to the PromICE, and then wait for and
  // receive the response from it.  The command to be sent should already be
  // set up in m_geiCommand and the response is returned in m_getResponse.
  // Note that there is no error return - if anything goes wrong then a fatal
  // error occurs and this program exits.
  //--

  // First send the command ...
  //   Remember that a count of 0 means 256 bytes of data!
  size_t cbCommand = GEI_HEADERLEN + m_geiCommand.bCount;
  if (m_geiCommand.bCount == 0) cbCommand += GEI_MAXDATALEN;
  serSend((uint8_t *) &m_geiCommand, cbCommand);

  // If this command doesn't need/want a response, then quit now!
  memset(&m_geiResponse, 0, sizeof(m_geiResponse));
  if (ISSET(m_geiCommand.bCommand, GEI_CM_NORESPONSE)) return;

  // Now read the first three bytes of the response ...
  if (serReceive((uint8_t *) &m_geiResponse, GEI_HEADERLEN, lTimeout) != GEI_HEADERLEN)
    FatalError("timeout waiting for response to command 0x%02X", m_geiCommand.bCommand);

  // Verify that this is indeed a response to the command we just sent ...
  if (m_geiCommand.bUnitID != m_geiResponse.bUnitID)
    FatalError("received response from unit %d not %d", m_geiResponse.bUnitID, m_geiCommand.bUnitID);
  if (!ISSET(m_geiResponse.bCommand, GEI_CM_RESPONSE))
    FatalError("message received is not a response 0x%02X", m_geiResponse.bCommand);
  //if ((m_geiResponse.bCommand & GEI_MF_MASK) != (m_geiCommand.bCommand & GEI_MF_MASK))
  //  FatalError("response 0x%02X does not match command 0x%02X", m_geiResponse.bCommand, m_geiCommand.bCommand);

  // All looks good - read the rest of the payload ...
  size_t cbResponse = m_geiResponse.bCount;
  if (cbResponse == 0) cbResponse = GEI_MAXDATALEN;
  uint32_t lResponseTimeout = SHORT_TIMEOUT*cbResponse;
  size_t cbReceived = serReceive(&m_geiResponse.abData[0], cbResponse, lResponseTimeout);
  if (cbReceived != cbResponse)
    FatalError("timeout waiting for data for response 0x%02X (received %d expected %d)", m_geiCommand.bCommand, cbReceived, cbResponse);
}

PRIVATE void geiSendCommand (uint32_t lTimeout, uint8_t bUnit, uint8_t bCommand, uint8_t bCount, uint8_t bData, ...)
{
  //++
  //   This routine will build a GEI_COMMAND packet from the arguments passed
  // and then call geiDoCommand() to send it and wait for the response.  The
  // lTimeout parameter specifies the maximum time to wait, and bUnit, bCommand
  // specify the PromICE unit and the specific command to be executed.  The
  // bCount parameter gives the number of data bytes associated with this
  // command, and the remaining arguments are the actual data bytes.
  // 
  //    Remember that in the GEI protocol, a byte count of zero really means
  // 256 bytes, so it's actually impossible to send a command with zero
  // data bytes.  Some commands which don't actually need any additional data,
  // like READ VERSION, have to send a dummy data byte (which the PromICE
  // ignores).
  //--
  assert((bCount != 0) && (bUnit < MAXUNIT));
  va_list ap;

  // Build the PromICE command packet ...
  memset(&m_geiCommand, 0, sizeof(m_geiCommand));
  m_geiCommand.bUnitID = bUnit;
  m_geiCommand.bCommand = bCommand;
  m_geiCommand.bCount = bCount;

  // Copy in the data bytes ...
  m_geiCommand.abData[0] = bData;
  va_start(ap, bData);
  for (uint32_t i = 1; i < bCount; ++i)
    m_geiCommand.abData[i] = va_arg(ap, int);
  va_end(ap);

  // Send it and we're done ...
  geiDoCommand(lTimeout);
}

PUBLIC void geiDisconnect()
{
  //++
  //   This issues a RESTART command to the PromICE.  It's the last thing that
  // should be called after a download, and it will put the PromICE in run
  // (emulation) mode and release the target RESET signal.
  // 
  //   Note that with multiple units, we need to disconnect unit 1 first, and
  // then unit 0.  That's because the latter is the one which actually drives
  // the RESET output to the target, and we need to be sure unit 1 is online
  // before releasing RESET.  
  //--
  assert(m_geiUnits > 0);
  fprintf(stderr, "Disconnecting ... ");
  for (uint8_t bUnit = m_geiUnits-1; bUnit > 0; --bUnit) {
    geiSendCommand(SHORT_TIMEOUT, bUnit, GEI_SETMODE|GEI_CM_NORESPONSE, 1, 0);
    geiSendCommand(SHORT_TIMEOUT, bUnit, GEI_RESTART|GEI_CM_NORESPONSE, 1, 0);
  }
  fprintf(stderr, "%d unit(s)\n", m_geiUnits);
  serClose();
}

PUBLIC char *geiGetVersion (uint8_t bUnit)
{
  //++
  //   This routine will return the firmware version number of the specified
  // PromICE unit as an ASCII string.  Note that the string pointer returned
  // is actuall a pointer into our m_getResponse structure, so if the caller
  // needs to use that value later he'd better copy it elsewhere!
  // 
  //   The PromICE READ VERSION command always returns exactly 4 characters,
  // in ASCII.  Later versions of the firmware actually retrurn five bytes,
  // but the fifth byte is unimportant to us (I don't exactly know what it
  // contains!) and we just overwrite it with a zero.
  //--
  geiSendCommand(SHORT_TIMEOUT, bUnit, GEI_READVERSION, 1, 0);
  if ((m_geiResponse.bCount < 4) || (m_geiResponse.bCount > 5))
    FatalError("unexpected respomse length %d to READ VERSION command", m_geiResponse.bCount);
  m_geiResponse.abData[4] = 0;
  return (char *) &m_geiResponse.abData;
}

PUBLIC uint32_t geiGetSerial (uint8_t bUnit)
{
  //++
  //   This is similar to READ VERSION, except that this time we return the
  // PromICE serial number.  This is a 32 bit value which the Grammar Engine
  // software prints in hex.  I don't know exactly where this serial number
  // comes from, since it doesn't seem to match up with anything printed on
  // the PromICE enclosure, but here it is ...
  //--
  geiSendCommand(SHORT_TIMEOUT, bUnit, GEI_READSERIAL, 1, 0);
  if ((m_geiResponse.bCount < 4) || (m_geiResponse.bCount > 5))
    FatalError("unexpected respomse length %d to READ SERIAL command", m_geiResponse.bCount);
  uint32_t lSerial =  (m_geiResponse.abData[0] << 24)
                    | (m_geiResponse.abData[1] << 16)
                    | (m_geiResponse.abData[2] <<  8)
                    |  m_geiResponse.abData[3];
    return lSerial;
}

PUBLIC uint32_t geiGetSize (uint8_t bUnit)
{
  //++
  //   Return the RAM size of the selected PromICE unit.  Returning the RAM
  // size is a side effect of the SET MODE command, which means that calling
  // this routine will put the unit in RUN mode.  Unfortunately there's no
  // way out of that...
  //--
  geiSendCommand(SHORT_TIMEOUT, bUnit, GEI_SETMODE, 1, 0);
  if   ((m_geiResponse.bCount != 1) 
    || ((m_geiResponse.bCommand & GEI_CM_MASK) != GEI_SETMODE))
    FatalError("unexpected response 0x%02x to SET MODE", m_geiResponse.bCommand);
  uint32_t lSize = 1024 << (m_geiResponse.abData[0] & 0x0F);
  return lSize;
}

PRIVATE int32_t geiTestFillRAM (uint8_t bUnit, uint8_t bCommand, uint8_t bArgument)
{
  //++
  //   This routine is the common code for the TEST and FILL RAM functions.
  // They're essentially the same except for the command code and the meaning 
  // of the argument.  For TEST RAM the argument is a pass count, and for FILL
  // RAM the argument is the filler value.  It returns -1 if the operation
  // succeeds, or the 32 bit address of the first failing RAM location if there
  // is an error. 
  // 
  //   Note this function is a VERY slow command - testing or filling 1Mb of
  // RAM takes about 30 seconds.  ANd in the case of RAM TEST, if there are
  // multiple passes then we need to multiply by the pass count!
  //--
  assert((bCommand == GEI_TESTRAM) || (bCommand == GEI_FILLRAM));
  uint32_t lTimeout = RAMTEST_TIMEOUT;
  if (bCommand == GEI_TESTRAM) lTimeout *= (uint32_t) bArgument;
  geiSendCommand (lTimeout, bUnit, bCommand, 1, bArgument);
  if (m_geiResponse.bCount == 3) {
    uint32_t lAddress = (m_geiResponse.abData[2]<<16)
                      | (m_geiResponse.abData[1]<<8)
                      | (m_geiResponse.abData[0]);
    return lAddress;
  }
  if (m_geiResponse.bCount != 1)
    FatalError("unexpected response length %d to the TEST/FILL RAM command", m_geiResponse.bCount);
  return -1;
}

PUBLIC int32_t geiTestRAM (uint8_t bUnit, uint8_t nPasses)
{
  //++
  // Test the PromICE RAM ...
  //--
  return geiTestFillRAM(bUnit, GEI_TESTRAM, nPasses);
}

PUBLIC int32_t geiFillRAM (uint8_t bUnit, uint8_t bValue)
{
  //++
  // Fill the PromICE RAM with a constant ...
  //--
  return geiTestFillRAM(bUnit, GEI_FILLRAM, bValue);
}

PUBLIC void geiResetTarget()
{
  //++
  //    This routine resets the target gizmo attached to the PromICE (assuming
  // that you've connected the reset jumper wire, of course!).  Note that the
  // PromICE actually allows you to specify the length of the RESET pulse with
  // the parameter; we default to something around 100ms here...
  // 
  //   Note that the unit number is irrelevant here, since there's only one
  // reset output on the PromICE and that's connected to the master unit.
  // 
  //   One more subtle point - the PromICE doesnt return a response until after
  // the RESET pulse is generated.  So if you expect a 100ms pulse, you'd better
  // have at least a 100ms timeout!
  //--
  geiSendCommand(LONG_TIMEOUT, 0, GEI_RESETTARGET, 1, RESET_LENGTH);
  if   ((m_geiResponse.bCount != 1) 
    || ((m_geiResponse.bCommand & GEI_CM_MASK) != GEI_RESETTARGET))
    FatalError("unexpected response 0x%02x to RESET TARGET", m_geiResponse.bCommand);
}

PUBLIC void geiLoadMode()
{
  //++
  //   This routine puts the PromICE in "load" mode and also enables the auto
  // reset function.  This asserts the RESET output and (if you connected the
  // jumper) holds the target in reset while we download new code.
  //
  //   In the case of multiple units we need to put them ALL into load mode,
  // and in this case we start with unit 0 and work up to unit 1.  That's
  // because unit 0 drives the target RESET output, and we want that asserted
  // before unit 1 goes offline.
  //--
  assert(m_geiUnits > 0);
  for (uint8_t bUnit = 0;  bUnit < m_geiUnits;  ++bUnit) {
    geiSendCommand(SHORT_TIMEOUT, bUnit, GEI_SETMODE, 1, GEI_MD_FASTXMIT|GEI_MD_AUTORST|GEI_MD_LOADMODE);
    if   ((m_geiResponse.bCount != 1) 
      || ((m_geiResponse.bCommand & GEI_CM_MASK) != GEI_SETMODE))
      FatalError("unexpected response 0x%02x to SET MODE", m_geiResponse.bCommand);
  }
}

PUBLIC uint32_t geiAddressMask (uint32_t lSize)
{
  //++
  //   The PromICE wants all unused address bits to be ones (not zeros!), so
  // when you call geiLoadPointer() then the first address in the EPROM image
  // would be 0xFF8000 and NOT 0x000000 as you might expect.  This routine
  // takes the desired emulation size and computes the address mask to set
  // all these extra bits to ones.
  // 
  //   Note that the smallest EPROM size is 2K bytes, so we can save a little
  // time by skipping the bottom 11 bits.  And yes, I know that I could just
  // do something like ~(lSize-1) but that assumes lSize is a power of 2.  This
  // kludgier algorithm will round up sizes that aren't powers of 2.
  //--
  uint32_t lMask = 0xFFFFF800;  --lSize;  lSize >>= 11;
  while (lSize != 0) {lMask <<= 1;  lSize >>= 1;}
  return lMask & 0x00FFFFFF;
}

PRIVATE void geiLoadPointer (uint8_t bUnit, uint32_t lAddress)
{
  //++
  //   Execute the LOAD POINTER command.  This sets the initial address for
  // the WRITE DATA and READ DATA commands.
  //--
  uint8_t bEX = (lAddress >> 16) & 0xFF;
  uint8_t bHI = (lAddress >>  8) & 0xFF;
  uint8_t bLO =  lAddress        & 0xFF;
  geiSendCommand(SHORT_TIMEOUT, bUnit, GEI_LOADPOINTER|GEI_CM_NORESPONSE, 3, bEX, bHI, bLO);
  //if   ((m_geiResponse.bCount != 1) 
  //  || ((m_geiResponse.bCommand & GEI_CM_MASK) != GEI_LOADPOINTER))
  //  FatalError("unexpected response 0x%02x to LOAD POINTER", m_geiResponse.bCommand);
}

PUBLIC void geiDownload (uint8_t bUnit, uint8_t *pabData, size_t cbData, uint32_t lAddress)
{
  //++
  //   This routine downloads cbData bytes from the buffer at pabData to the
  // PromICE target address lAddress.  Note that the maximum amount of data
  // that can be downloaded in one message is 256 bytes - to download an entire
  // EPROM image you'll need to call this routine many times.  lAddress can be
  // up to 24 bits, since many of the EPROMs emulated are larger than 64k bytes.
  //--
  assert((pabData != NULL) && (cbData <= GEI_MAXDATALEN));

  // First set the PromICE address pointer ...
  geiLoadPointer(bUnit, lAddress);

  // Build the command packet header ...
  memset(&m_geiCommand, 0, sizeof(m_geiCommand));
  m_geiCommand.bUnitID = bUnit;
  m_geiCommand.bCommand = GEI_WRITEDATA;
  m_geiCommand.bCount = cbData & 0xFF;

  // Copy in the data bytes ...
  uint8_t bCK = 0;
  for (uint32_t i = 0; i < cbData; ++i) {
    m_geiCommand.abData[i] = pabData[i];
    bCK ^= pabData[i];
  }

  // Send it and get the response ...
  geiDoCommand(LONG_TIMEOUT);
  if   ((m_geiResponse.bCount != 1) 
    || ((m_geiResponse.bCommand & GEI_CM_MASK) != GEI_WRITEDATA))
    FatalError("unexpected response 0x%02x to WRITE DATA", m_geiResponse.bCommand);
  if (m_geiResponse.abData[0] != bCK)
    FatalError("XOR mismatch for WRITE DATA 0x%02X != 0x%02X", bCK, m_geiResponse.abData[0]);
}

PUBLIC void geiUpload (uint8_t bUnit, uint8_t *pabData, size_t cbData, uint32_t lAddress)
{
  //++
  //   This routine is the reverise of geiDownload() and uploads data from the
  // PromICE to our buffer.  You could actually use this to read back the
  // entire PromICE and save it to a file if you wanted, but we use it to 
  // verify downloads and the PromICE RAM.  Except for the direction of data
  // flow, the parameters are the same as for geiDownload().
  //--
  assert((pabData != NULL) && (cbData <= GEI_MAXDATALEN));

  // Set the address pointer and read the reqested data ...
  geiLoadPointer(bUnit, lAddress);
  geiSendCommand(LONG_TIMEOUT, bUnit, GEI_READDATA, 1, (cbData & 0xFF));
  if ((m_geiResponse.bCommand & GEI_CM_MASK) != GEI_READDATA)
    FatalError("unexpected response 0x%02x to READ DATA", m_geiResponse.bCommand);
  if (m_geiResponse.bCount != (cbData & 0xFF))
    FatalError("short response for READ DATA 0x%03X != 0x%03X", cbData, m_geiResponse.bCount);

  // Store the result and we're done ...
  memcpy(pabData, &m_geiResponse.abData, cbData);
}
