//++
//PromICE.c - download and test Grammar Engine PromICE EPROM Emulator
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
//    This program communicates with a Grammar Engine PromICE EPROM emulator.
// It can download files to the emulator, test the PromICE memory, reset the
// attached target device, and more.  
//    
// USAGE:
//        promice [global options] [command] [local options] [file0] [file1]
//      
//        Global options applicable to all commands
//          -p port     -> set COM port
//          -b baud     -> set serial baud rate
//      
//        Commands (only one may appear!)
//          v[erify]    -> verify communication with PromICE
//          r[eset]     -> reset target without downloading
//          t[est]      -> execute self test
//          d[ownload]  -> download file(s)
//      
//        Local options for downloading only
//          -f dd       -> fill unused locations with dd
//          -v          -> verify after downloading
//          -s size     -> set size of emulated device
//
// ENVIRONMENT VARIABLES:
//        PROMICE_PORT  -> set the default serial port to use
//        PROMICE_BAUD  -> set the default serial baud rate
//
// NOTES:
//   Yes, Grammer Engine does have a LOADICE program which already does all
// this and more.  Mine is more portable and besides - I like it better!
// 
//   In the case of a dual, master/slave, PromICE then two file names can be
// specified for download.  The first file is downloaded to unit 0 (that's the
// master unit, or the bottom connector on the back panel) and the second file
// is downloaded to unit 1 (the slave, or upper connector on the back).  This
// program assumes that both emulated EPROMs are the same size, although I don't
// think the actual PromICE hardware requires that.
//
// REVISION HISTORY:
// 27-MAR-23  RLA  New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdio.h>              // printf(), FILE, etc ...
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>             // uint8_t, uint32_t, etc ...
#include <stdbool.h>            // bool, true, false, etc ...
#include <assert.h>             // assert() (what else??)
#include <stdarg.h>             // va_list, va_start(), va_end(), etc...
#include <memory.h>             // memset(), et al ...
#include <string.h>             // strlen(), strcpy(), strcat(), etc...
#include <ctype.h>              // islower(), toupper(), etc...
#ifdef __linux__
#include <strings.h>		// strcasecmp(), strncasecmp(), ...
#include <limits.h>		// PATH_MAX, etc ...
#include <libgen.h>		// basename(), dirname(), ...
#include <errno.h>		// errno_t, etc ...
#endif
#include "PromICE.h"            // global declarations for this project
#include "serial.h"             // host dependent serial port interface
#include "protocol.h"           // GEI protocol definitions
#include "hexfile.h"            // Intel .HEX file routines

// Locals to this module ...
PRIVATE PROMICE_COMMAND m_Command;    // command to execute 
PRIVATE char    *m_pszSerialPort;     // PromICE serial port
PRIVATE uint32_t m_lBaudRate;         // PromICE baud rate
PRIVATE uint32_t m_lEmulationSize;    // size of EPROM to emulate
PRIVATE uint8_t  m_bFillerByte;       // fill unused locations
PRIVATE bool     m_fVerifyDownload;   // verify after downloading
PRIVATE char m_szFileName1[_MAX_PATH];// first file name to download
PRIVATE char m_szFileName2[_MAX_PATH];// second file name to download


////////////////////////////////////////////////////////////////////////////////
////////////////////////////   MESSAGE ROUTINES  ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

PRIVATE void MessageList (const char *pszMsg, va_list ap)
{
  //++
  //   Handle the actual printing of error and iformational messages.  This
  // routine is shared between Message() and FatalError() ...
  //--
  fprintf(stderr, "%s: ", PROMICE);
  vfprintf(stderr, pszMsg, ap);
  fprintf(stderr, "\n");
}

PUBLIC void Message (const char *pszMsg, ...)
{
  //++
  //   Print an informational or warning message to stderr and return.  The
  // advantage to this over fprintf() is that we format the message nicely!
  //--
  va_list ap;  va_start(ap, pszMsg);
  MessageList(pszMsg, ap);  va_end(ap);
}

PUBLIC void FatalError (const char *pszMsg, ...)
{
  //++
  // Print a fatal error message and then abort this program!
  //--
  va_list ap;  va_start(ap, pszMsg);
  fprintf(stderr, "\n");
  MessageList(pszMsg, ap);  va_end(ap);
  exit(EXIT_FAILURE);
}

#ifdef __linux__
errno_t _splitpath_s (const char *pszPath,
		      char *pszDrive,     size_t cbDrive,
		      char *pszDirectory, size_t cbDirectory,
		      char *pszName,      size_t cbName,
		      char *pszType,      size_t cbType)
{
  //++
  //   And now the Linux version.  Note that the dirname() and basename()
  // library functions are a bit, well, funky.  dirname() modifies the caller's
  // buffer by replacing the last "/" in the path with a null and then it
  // returns a pointer to start of the string.  basename() doesn't modify the
  // buffer it's passed and just returns a pointer to the first character after
  // the last "/" in the name.  Calling dirname() before basename() doesn't
  // work because the latter will be unable to find the actual file name after
  // dirname terminates the string just in front of it.  Calling them in the
  // reverse order, however, works fine.
  //--
  *pszDrive = *pszDirectory = *pszName = *pszType = EOS;
  if (strlen(pszPath) == 0) return 0;
  char szPath[PATH_MAX];
  strcpy(szPath, pszPath);
  strcpy(pszName, basename(szPath));
  strcpy(pszDirectory, dirname(szPath));
  // dirname() DOES NOT include the trailing "/", so we need to add one!
  strcat(pszDirectory, "/");
  //   Now we play the same game ourselves to separate the extension (if any)
  // from the file name.  Note that we extract the extension string BEFORE
  // replacing the "." with an EOS because the dot is part of the extension!
  char *psz = strrchr(pszName, '.');
  if (psz != NULL) {
    strcpy(pszType, psz);  *psz = EOS;
  }
  return 0;
}
#endif

PRIVATE void ApplyDefaultExtension (char *pszFileName, const char *pszType)
{
  //++
  //   This method will apply a default extension (e.g. ".hex") to the file name
  // specified and return the result.  It's not super smart, but it's enough...
  //--

  // First use the WIN32 _splitpath() function to parse the original file name ...
  char szDrive[_MAX_PATH], szDirectory[_MAX_PATH];
  char szFileName[_MAX_PATH], szExtension[_MAX_PATH];
  errno_t err = _splitpath_s(pszFileName, szDrive, sizeof(szDrive),
                             szDirectory, sizeof(szDirectory), szFileName, sizeof(szFileName),
                             szExtension, sizeof(szExtension));
  if (err != 0) return;

  // if the extension is null, then apply the default ...
  if (strlen(szExtension) == 0) strcat_s(pszFileName, _MAX_PATH, pszType);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////   SIMPLE COMMANDS   ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

PRIVATE void VerifyConnection()
{
  //++
  //   The VERIFY command just tests the connection to the PromICE.  It
  // tries to connect, and the prints the firmware version number and actual
  // RAM size of each emulation unit that it finds...
  // 
  //   Note that this function works even while the PromICE is in RUN mode,
  // so we don't want to end by calling geiDisconnect() because that would
  // unnecessarily reset the target.  It's enough to call serClose() instead.
  //--
  uint8_t nUnits = geiConnect(m_pszSerialPort, m_lBaudRate);
  for (uint8_t i = 0; i < nUnits; ++i) {
    fprintf(stderr,"Unit %d: firmware version %s, serial %08X, %dK bytes\n",
      i, geiGetVersion(i), geiGetSerial(i), (geiGetSize(i) >> 10));
  }
  serClose();
}

PRIVATE void TestPromICE()
{
  //++
  //   The TEST command executes (what else?) the PromICE TEST RAM command
  // to test the built in RAM of all connected units.  Note that this is a
  // VERY slow command - a 1Mb unit takes about 30 seconds for one pass.
  //--
  uint8_t nUnits = geiConnect(m_pszSerialPort, m_lBaudRate);
  for (uint8_t i = 0; i < nUnits; ++i) {
    uint32_t lSize = geiGetSize(i);  geiLoadMode();
    fprintf(stderr, "Unit %d: testing %dK bytes ... ", i, (lSize >> 10));
    int32_t lFail = geiTestRAM(i, 1);
    if (lFail > 0)
      fprintf(stderr, "FAILED at %06x\n", lFail);
    else
      fprintf(stderr, "PASSED!\n");
  }
  geiDisconnect();
}

PRIVATE void ResetTarget()
{
  //++
  //   This function activates the RESET output on the PromICE and (what else?)
  // resets the target.  It doesn't download anything or do anything else, just
  // toggles that output.  
  //--
  geiConnect(m_pszSerialPort, m_lBaudRate);
  geiResetTarget();
  fprintf(stderr, "Resetting target ...\n");
  serClose();
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////   DOWNLOAD COMMAND   //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

PRIVATE uint8_t *LoadFile (char *pszFileName, uint32_t lSize)
{
  //++
  //   This routine will load a file (currently only Intel .HEX files are
  // supported!) into memory.  It allocated a chunk of memory of the specified
  // size, loads the HEX file, and returns a pointer to the memory block.
  //--
  if (strlen(pszFileName) == 0)
    FatalError("not enough file names");
  if (lSize > 65536)
    FatalError("unable to load .HEX file larger than 64K");
  ApplyDefaultExtension(pszFileName, ".hex");
  uint8_t *pabData = malloc(lSize);
  if (pabData == NULL)
    FatalError("out of memory memory");
  memset(pabData, m_bFillerByte, lSize);
  uint32_t cbTotal = hexLoad(pszFileName, pabData, 0, lSize);
  fprintf(stderr, "%d bytes loaded from %s\n", cbTotal, pszFileName);
  return pabData;
}

PRIVATE void DownloadUnit (uint8_t bUnit, uint8_t *pabData, uint32_t lSize, bool fVerify)
{
  //++
  //   This routine handles the actual downloading, with optional verification,
  // of an EPROM image to a single PromICE unit.  The maximum chunk that we can
  // send to the PromICE in one message is only 256 bytes, so most images will
  // require a number of chunks to be sent.
  //
  //   If the fVerify parameter is true, then after downloading this routine
  // will try to upload the data from the PromICE and compare it to what we
  // originally sent.  If they're not the same then a fatal error occurs and
  // we abort.
  //--
  uint32_t lMask = geiAddressMask(lSize);  uint8_t abVerify[GEI_MAXDATALEN];
  fprintf(stderr, "Unit %d: %dK bytes ...", bUnit, (lSize>>10));

  //printf(" Filling 0x%02X ...", m_bFillerByte);
  //geiFillRAM(bUnit, m_bFillerByte);

  uint32_t lAddress, lCount;
  for (lAddress = lCount = 0;  lCount < lSize;) {
    uint32_t cb = lSize-lCount;
    if (cb > GEI_MAXDATALEN)  cb = GEI_MAXDATALEN;
    if ((lCount & 0x3FF) == 0)
      fprintf(stderr, "\rUnit %d: %dK bytes ... Downloading %dK ...", bUnit, (lSize >> 10), (lCount >> 10));
    geiDownload(bUnit, pabData+lCount, cb, lAddress | lMask);
    lCount += cb;  lAddress += cb;
  }
  fprintf(stderr, "\rUnit %d: %dK bytes ... Downloading %dK ...", bUnit, (lSize >> 10), (lCount >> 10));

  if (fVerify) {
    for (lAddress = lCount = 0; lCount < lSize;) {
      uint32_t cb = lSize - lCount;
      if (cb > GEI_MAXDATALEN)  cb = GEI_MAXDATALEN;
      if ((lCount & 0x3FF) == 0)
        fprintf(stderr, "\rUnit %d: %dK bytes ... Downloading %dK ... Verifying %dK ...", bUnit, (lSize >> 10), (lSize >> 10), (lCount >> 10));
      geiUpload(bUnit, abVerify, cb, lAddress | lMask);
      if (memcmp(&abVerify, (pabData+lAddress), cb) != 0)
        FatalError("verification error at 0x%06x", lAddress);
      lCount += cb;  lAddress += cb;
    }
    fprintf(stderr, "\rUnit %d: %dK bytes ... Downloading %dK ... Verifying %dK ...", bUnit, (lSize >> 10), (lSize >> 10), (lCount >> 10));
  }

  fprintf(stderr, " DONE\n");
}

PRIVATE void DownloadFiles()
{
  //++
  //   This routine handles the DOWNLOAD command.  It loads one or two (in
  // the case of a master/slave PromICE) Intel HEX files and downloads them to
  // the emulator.  If the "-v" (verify) option was specified, then after
  // downloading we immediately upload the data from the PromICE and compare
  // it with the original file contents.
  //--
  uint8_t *pabData1 = NULL, *pabData2 = NULL;

  // Connect, get the PromICE size, and put it in LOAD mode ...
  uint8_t nUnits = geiConnect(m_pszSerialPort, m_lBaudRate);
  uint32_t lSize = geiGetSize(0);
  if (m_lEmulationSize == 0) m_lEmulationSize = lSize;
  geiLoadMode();

  // Load the first file ...
  if (strlen(m_szFileName1) == 0)
    FatalError("specify at least one file name");
  pabData1 = LoadFile(m_szFileName1, m_lEmulationSize);

  // And load the second file, IFF we have two units!
  if (nUnits > 1) {
    if (strlen(m_szFileName2) == 0)
      Message("unit 1 will not be loaded");
    else
      pabData2 = LoadFile(m_szFileName2, m_lEmulationSize);
  } else if (strlen(m_szFileName2) > 0)
    Message("file name %s ignored", m_szFileName2);

  // Download the data ...
  DownloadUnit(0, pabData1, m_lEmulationSize, m_fVerifyDownload);
  if (pabData2 != NULL)
    DownloadUnit(1, pabData2, m_lEmulationSize, m_fVerifyDownload);

  // And we all done ...
  geiDisconnect();
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////   COMMAND LINE PARSING   ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

PRIVATE void ShowUsage()
{
  //++
  // Print the help (aka usage) text  ...
  //--
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  promice [global options] [command] [local options] [file-0] [file-1]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  Global options applicable to all commands\n");
  fprintf(stderr, "    -p port\t-> set COM port\n");
  fprintf(stderr, "    -b baud\t-> set serial baud rate\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  Commands (only one may appear!)\n");
  fprintf(stderr, "    v[erify]\t-> verify communication with PromICE\n");
  fprintf(stderr, "    r[eset]\t-> reset target without downloading\n");
  fprintf(stderr, "    t[est]\t-> execute self test\n");
  fprintf(stderr, "    d[ownload]\t->download file(s)\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  Local options for downloading only\n");
  fprintf(stderr, "    -f dd\t-> fill unused locations with dd\n");
  fprintf(stderr, "    -v\t\t-> verify after downloading\n");
  fprintf(stderr, "    -s size\t-> set size of emulated device\n");
}

PRIVATE void SetDefaults()
{
  //++
  //   This routine will initialize all command line parameters to their
  // default values.  Most things are initialized to NULL or zero, but the
  // serial port name and baud rate can be initialized from environment
  // variables ...
  //--
  m_Command = CMD_NONE;  m_fVerifyDownload = false;
  m_lBaudRate = m_lEmulationSize = 0;  m_bFillerByte = 0;
  memset(&m_szFileName1, 0, sizeof(m_szFileName1));
  memset(&m_szFileName2, 0, sizeof(m_szFileName2));

  // Set the default serial port from the environment, or NULL if none...
  m_pszSerialPort = getenv(SERIALPORTENV);

  // Set the default baud rate from the environment too...
  m_lBaudRate = DEFAULTBAUD;
  char *pszBaud = getenv(SERIALBAUDENV);
  if (pszBaud != NULL) {
    char *psz = NULL;
    m_lBaudRate = (unsigned) strtoul(pszBaud, &psz, 10);
    if (*psz != '\0')
      FatalError("bad baud rate in environment \"%s\"", pszBaud);
  }
}

PRIVATE void ParseName (char *pszName)
{
  //++
  //   Parse a "name" (that is, anything that doesn't start with a '-'!) from
  // the command line.  The first name is always the command to execute -
  // download, reset, verify, test or help.  Any remaining names are files for
  // the download command.
  //--
  if (m_Command == CMD_NONE) {
    if (STRIEQL(pszName, "v") || STRIEQL(pszName, "verify")) {
      m_Command = CMD_VERIFY;  return;
    } else if (STRIEQL(pszName, "r") || STRIEQL(pszName, "reset")) {
      m_Command = CMD_RESET;  return;
    } else if (STRIEQL(pszName, "t") || STRIEQL(pszName, "test")) {
      m_Command = CMD_TEST;  return;
    } else if (STRIEQL(pszName, "d") || STRIEQL(pszName, "download")) {
      m_Command = CMD_DOWNLOAD;  return;
    } else if (STRIEQL(pszName, "h") || STRIEQL(pszName, "help")) {
      m_Command = CMD_HELP;  return;
   } else
      FatalError("unknown command \"%s\"", pszName);
  } else {
    if (strlen(m_szFileName1) == 0)
      strcpy_s(m_szFileName1, sizeof(m_szFileName1), pszName);
    else if (strlen(m_szFileName2) == 0)
      strcpy_s(m_szFileName2, sizeof(m_szFileName1), pszName);
    else
      FatalError("too many file names \"%s\"", pszName);
  }
}

PRIVATE int ParseOption (char *pszName, char *pszValue)
{
  //++
  //   This routine will parse an option from the command line, an "option"
  // being anything that starts with a "-".  pszName is the name of the option
  // and pszValue is the argument for that option.  Some options require values
  // (e.g. "-p", "-s", etc) and some do not (e.g. "-v"), and in the latter case
  // the pszValue parameter is ignored.  This routine returns 2 if the argument
  // was used, and 1 if it was not.
  // 
  //   Note that the argument may be combined with the option (e.g. "-b57600")
  // or as a separate argument ("-b 57600") but the caller takes care of
  // figuring that out for us.  That's the reason we return 1 or 2 - so the
  // caller knows whether to skip a separate argument or not.  
  // 
  //   Also note that pszValue MAY BE NULL if no argument was specied on the
  // command line.  This can happen if an option is the last thing on the line
  // (e.g. "promice -p" has no argument for the "-p") or if another option
  // follows this one (e.g. "promice -p -b 57600" has no argument for the "-p").
  //--
  char *psz;

  if (STREQL(pszName, "-v")) {
    // "-v" - verify after downloading ...
    m_fVerifyDownload = true;  return 1;

  } else if (STRNEQL(pszName, "-p", 2)) {
    // "-p port" - specify the serial COM port ...
    if (pszValue == NULL)
      FatalError("specify port name for -p option");
    m_pszSerialPort = pszValue;  return 2;

  } else if (STRNEQL(pszName, "-b", 2)) {
    // "-b baud" - specify the serial port baud rate ...
    if (pszValue == NULL)
      FatalError("specify baud rate for -b option");
    m_lBaudRate = (unsigned) strtoul(pszValue, &psz, 10);
    if (*psz != '\0')
      FatalError("bad baud rate \"%s\"", pszValue);
    return 2;

  } else if (STRNEQL(pszName, "-f", 2)) {
    // "-f xx" - specify the filler byte (in HEX!) for unused locations ...
    if (pszValue == NULL)
      FatalError("specify filler byte for -f option");
    m_bFillerByte = strtoul(pszValue, &psz, 16) & 0xFF;
    if (*psz != '\0')
      FatalError("bad filler byte \"%s\"", pszValue);
    return 2;

  } else if (STRNEQL(pszName, "-s", 2)) {
    // "-s size" - specify the emulated EPROM size, in bytes ...
    //   (or, if followed by "k", in kilobytes!)
    if (pszValue == NULL)
      FatalError("specify emulation size for -s option");
    m_lEmulationSize = (unsigned) strtoul(pszValue, &psz, 10);
    if (*psz == 'k' || *psz == 'K')   m_lEmulationSize <<= 10, ++psz;
    if (*psz != '\0')
      FatalError("bad emulation size \"%s\"", pszValue);
    return 2;

  } else
    // Any other option is unknown ...
    FatalError("unknown option \"&s\"", pszName);
  return 0;
}

PRIVATE void ParseArguments (int argc, char *argv[])
{
  //++
  //   This routine parses the argument list, argc and argv, for this program.
  // It basically just iterates thru all the arguments and calls either
  // ParseOption() for items that start with "-", or ParseName() for things
  // that don't.
  //--

  // Iterate thru the argument list ...
  for (int i = 1; i < argc;) {
    // Is this an option?
    if (argv[i][0] == '-') {
      // Option - figure out the options' argument, if any ...
      if (strlen(argv[i]) > 2) {
        // Argument and option combined (e.g. "-b57600") ...
        ParseOption(argv[i], &(argv[i][2]));  ++i;
      } else if (((i+1) < argc) && (argv[i+1][0] != '-')) {
        // Argument and option are separate (e.g. "-b 57600") ...
        i += ParseOption(argv[i], argv[i+1]);
      } else {
        // No argument present ...
        ParseOption(argv[i], NULL);  ++i;
      }
    } else {
      // Not an option - must be a command or file name ...
      ParseName(argv[i]);  ++i;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////   MAIN   ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char *argv[])
{
  // Parse the command arguments ...
  if (argc == 1) {ShowUsage();  exit(EXIT_FAILURE);}
  SetDefaults();  ParseArguments(argc, argv);

  // And execute the requested command ...
  switch (m_Command) {
    case CMD_HELP:     ShowUsage();         break;
    case CMD_VERIFY:   VerifyConnection();  break;
    case CMD_RESET:    ResetTarget();       break;
    case CMD_TEST:     TestPromICE();       break;
    case CMD_DOWNLOAD: DownloadFiles();     break;
    default:
      FatalError("specify download, reset, verify, test or help");
  }

  // All done!
  exit(EXIT_SUCCESS);
}
