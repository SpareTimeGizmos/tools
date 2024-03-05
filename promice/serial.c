//++
//serial.c - host dependent serial port routines for PromICE
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
//   This module implements the host dependent serial support necessary for
// downloading the Grammar Engine PromICE emulators.  Right now the only
// operating systems supported are Linux and Windows, and the serial port
// functions supported are -
// 
//      serOpen()         - open the serial port
//      serClose()        - close the serial port
//      serFlush()        - flush the serial port buffers
//      serSend()         - blocking transmit one or more bytes 
//      serReceive()      - blocking (with timout) receive multiple bytes
//      serSendByte()     - same as above, but for just a single byte
//      serReceiveByte()  - ditto ...
//      serSetDTR()       - assert or deassert serial DTR
//      serSleep()        - generic delay/sleep function
// 
// NOTES
//   Currently any data associated with the serial connection is maintained in
// static variables local to this module.  Because of that only one active
// serial connection is supported.  That's plenty for PromICE.
// 
//   If neither _WIN32 nor the __linux__ is defined, then this module generates
// code that always returns a failure status!  That means you can build PromICE
// for other platforms, but it won't do anything...
//
// REVISION HISTORY:
// 24-MAR-23  RLA  New file.
//  6-APR-23  RLA  Add Linux support.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdio.h>              // printf(), FILE, etc ...
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>             // uint8_t, uint32_t, etc ...
#include <stdbool.h>            // bool, true, false, etc ...
#include <assert.h>             // assert() (what else??)
#include <memory.h>             // memset(), et al ...
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN     // only use the most minimal set of
#include <windows.h>            //  ... windows API declarations!
#elif defined(__linux__)
#include <limits.h>             // PATH_MAX ...
#include <unistd.h>             // open(), close(), read(), write(), etc ...
#include <fcntl.h>              // _O_TRUNC, _O_WRONLY, etc...
#include <termios.h>            // POSIX terminal control functions
#include <errno.h>              // errno, ENOENT, etc ...
#include <time.h>		// struct timeval for select()...
#include <sys/ioctl.h>          // ioctl(), et al ...
#include <sys/select.h>		// select() (what else?)
#endif
#include "PromICE.h"            // global declarations for this project
#include "serial.h"             // declarations for this module

// Local data for this module ...
#if defined(_WIN32)
PRIVATE HANDLE m_hSerialPort = NULL;  // Windows device handle for the port
#elif defined(__linux__)
PRIVATE int    m_hSerialPort = 0;     // Linux device handle for the port
#endif


#ifdef __linux__
PRIVATE speed_t serBaudToSpeed (uint32_t nBaud)
{
  //++
  //   Convert a simple baud rate (e.g. 57600) to one of the Un*x Bxxx
  // constants (e.g. B57600).  This seems really stupid, and there must be
  // a better way to do this!
  //--
  switch (nBaud) {
    case    300: return B300;
    case   1200: return B1200;
    case   2400: return B2400;
    case   4800: return B4800;
    case   9600: return B9600;
    case  19200: return B19200;
    case  38400: return B38400;
    case  57600: return B57600;
    case 115200: return B115200;
    default: break;
  }
  FatalError("unsupported baud rate %d", nBaud);  return 0;
}
#endif

PUBLIC void serOpen (const char *pszName, uint32_t nBaud)
{
  //++
  //   This routine opens the serial port specified by "name" and sets the port
  // parameters to 8N1 and the baud rate specified.  The port name depends on
  // the OS and is normally something like "COM4" (for Windows) or "ttyUSB1"
  // (for Linux).
  // 
  //    Note that this routine never fails - if any error occurs then it prints
  // a message and exits this program by calling FatalError()...
  //--
  assert(pszName != NULL);
  if ((pszName == NULL) || (strlen(pszName) == 0))
    FatalError("no COM port specified");
  if (nBaud == 0) nBaud = DEFAULTBAUD;

#if defined(_WIN32)
  // WINDOWS VERSION ...

  //   Rewrite the device name to make Windows happy (e.g. change a plain
  // "COM4" into "\\.\COM4" and then open the device.  
  char szWin32Name[_MAX_PATH];  DCB dcb;
  snprintf (szWin32Name, sizeof (szWin32Name), "\\\\.\\%s", pszName);
  m_hSerialPort = CreateFileA(szWin32Name, GENERIC_READ | GENERIC_WRITE,
                              0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (m_hSerialPort == INVALID_HANDLE_VALUE)
    FatalError("error (%d) opening COM port %s", GetLastError(), pszName);
  //   Set the character format to 8N1 (always!) and set the baud rate to
  // whatever the caller selected.  Note that if the device name passed by
  // the caller DOESN'T refer to an actual serial port then the CreateFile()
  // (above) will still work, but this step will fail...
  memset(&dcb, 0, sizeof(dcb));  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(m_hSerialPort, &dcb))
    FatalError("error (%d) getting COM port mode", GetLastError());
  dcb.BaudRate = nBaud;  dcb.fBinary = true;
  dcb.Parity = NOPARITY;  dcb.ByteSize = 8;  dcb.StopBits = ONESTOPBIT;
  dcb.fOutxCtsFlow = dcb.fOutxDsrFlow = dcb.fNull = false;
  dcb.fDtrControl = DTR_CONTROL_ENABLE;  dcb.fRtsControl = RTS_CONTROL_ENABLE;
  dcb.fDsrSensitivity = dcb.fOutX = dcb.fInX = dcb.fTXContinueOnXoff = false;
  if (!SetCommState (m_hSerialPort, &dcb))
    FatalError("error (%d) setting COM port mode", GetLastError());

#elif defined(__linux__)
  // LINUX VERSION ...

  // Open the device (and be sure it's a terminal!) ...
  m_hSerialPort = open(pszName, O_RDWR | O_NOCTTY);
  if (m_hSerialPort == -1)
    FatalError("error (%s) opening %s", strerror(errno), pszName);
  if (!isatty(m_hSerialPort))
    FatalError("%s is not a serial port", pszName);

  // Configure the terminal settings ...
  struct termios ts;
  bzero(&ts, sizeof(ts));
  if (tcgetattr(m_hSerialPort, &ts) < 0)
    FatalError("error (%s) getting port settings", strerror(errno));
  cfsetispeed(&ts, serBaudToSpeed(nBaud));
  cfsetospeed(&ts, serBaudToSpeed(nBaud));
  cfmakeraw(&ts);
  ts.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
  ts.c_cflag |=  (CLOCAL | CREAD  | CS8);
  if (tcsetattr(m_hSerialPort, TCSANOW, &ts) < 0)
    FatalError("error (%s) setting port mode", strerror(errno));
#endif
}

PUBLIC void serClose()
{
  //++
  // Close the serial port opened by a call to serOpen()...
  //--
  assert(m_hSerialPort);
#if defined(_WIN32)
  CloseHandle(m_hSerialPort);  m_hSerialPort = NULL;
#elif defined(__linux__)
  close(m_hSerialPort);  m_hSerialPort = 0;
#endif
}

PUBLIC void serFlush()
{
  //++
  //   Flush the serial port buffers (both transmit and receive!).  This is
  // used during startup to flush any garbage characters received during
  // the initial negotiation.
  //--
  assert(m_hSerialPort);
#if defined(_WIN32)
  FlushFileBuffers(m_hSerialPort);
#elif defined(__linux__)
  tcflush(m_hSerialPort, TCIOFLUSH);
#endif
}

PUBLIC void serSend (const uint8_t *pabBuffer, size_t cbBuffer)
{
  //++
  //   Write cbBuffer bytes from pabBuffer to the serial port.  This is a
  // blocking write, and we'll hang here until all the bytes have been
  // transferred to the serial port buffer. Note that this doesn't guarantee
  // that they've actually been transmitted out the serial port TXD line yet,
  // only that they've been buffered to be sent eventually.
  // 
  //   As with serOpen(), this routine never fails.  If there are any errors
  // then it will call FatalError() and exit this program.
  //--
  assert(m_hSerialPort);
#if defined(_WIN32)
  // WINDOWS VERSION ...
  DWORD dwWritten;
  if (!WriteFile(m_hSerialPort, pabBuffer, cbBuffer, &dwWritten, NULL))
    FatalError("error (%d) writing to COM port", GetLastError());
  if (dwWritten != cbBuffer)
    FatalError("unable to write to COM port");
#elif defined(__linux__)
  // LINUX VERSION ...
  if ((write(m_hSerialPort, pabBuffer, cbBuffer)) != cbBuffer)
    FatalError("error (%s) writing to serial port", strerror(errno));
#endif
}

PUBLIC void serSendByte (uint8_t bData)
{
  //++
  // Like serSend(), but send exactly one byte ...
  //--
  serSend(&bData, 1);
}

PUBLIC size_t serReceive (uint8_t *pabBuffer, size_t cbBuffer, uint32_t lTimeout)
{
  //++
  //   Receive characters from the serial port and store them in the caller's
  // buffer.  This is a blocking receive (i.e. this program will wait for data
  // if necessary) BUT with a timeout.  This routine will return whenever either
  // a) cbBuffer characters have been read, or b) lTimeout milliseconds elapses
  // without any new data received.  The return value is the number of bytes
  // actually read, which may be zero if the timeout expires with no input.
  //--
  assert(m_hSerialPort);
#if defined(_WIN32)
  // WINDOWS VERSION ...

  //   Set the serial port timeouts.  Note that the Windows timeouts are a
  // little more elaborate, with both an inter-character timeout AND a
  // total receive time timeout.  We're only using the inter-character
  // timeout here, BUT apparently if you set the ReadIntervalTimeout but
  // don't set a ReadTotalTimeout, then the first character will never
  // time out!  We have to set a ReadTotalTimeoutMultiplier, which sets
  // a timeout for the entire message based on the number of bytes.
  COMMTIMEOUTS cto;  memset(&cto, 0, sizeof(cto));
  cto.ReadTotalTimeoutConstant = lTimeout;
  if (!SetCommTimeouts(m_hSerialPort, &cto))
    FatalError("error (%d) setting COM port timeouts", GetLastError());

  // And try to read some bytes ... 
  DWORD dwReceived;
  if (!ReadFile(m_hSerialPort, pabBuffer, cbBuffer, &dwReceived, NULL))
    FatalError("error (%d) reading COM port", GetLastError());
  return dwReceived;

#elif defined(__linux__)
  // LINUX VERSION ...

  //   Linux is also messy for setting a serial port timeout (why is this hard?!).
  // You can set VMIN and VTIME in the termios structure, BUT if both of those
  // are non-zero then the timeout only applies to the second and subsequent
  // characters.  THERE IS NO TIMEOUT FOR THE FIRST CHARACTER!  The only way to
  // get a read with an overall timeout is to use select() in addition to VMIN
  // and VTIME.

  // Set the intercharacter timeout to 100ms ...
  struct termios ts;  bzero(&ts, sizeof(ts));
  if (tcgetattr(m_hSerialPort, &ts) < 0)
    FatalError("error (%s) getting port settings", strerror(errno));
  ts.c_cc[VMIN] = 0/*cbBuffer*/;  ts.c_cc[VTIME] = 1UL;
  if (tcsetattr(m_hSerialPort, TCSANOW, &ts) < 0)
    FatalError("error (%s) setting port mode", strerror(errno));

  // Set the timeout for the first character to lTimeout ...
  int maxfd = m_hSerialPort+1;  struct timeval tv;
  tv.tv_sec = lTimeout/1000UL;  tv.tv_usec = (lTimeout%1000UL)*1000UL;
  fd_set fs;  FD_ZERO(&fs);  FD_SET(m_hSerialPort, &fs);

  //   Loop unti we either fulfill the entire read request, or the
  // timeout occurs  Note that select() will update the timeout
  // structure after each call to reflect the time remaining, so
  // the total timeout specified by the caller remains in effect.
  size_t cbReceived = 0;
  while ((cbReceived < cbBuffer) && (select(maxfd, &fs, NULL, NULL, &tv) > 0)) {
    assert(FD_ISSET(m_hSerialPort, &fs));
    ssize_t cb = read(m_hSerialPort, pabBuffer+cbReceived, cbBuffer-cbReceived);
    if (cb < 0)
      FatalError("error (%) reading serial port", strerror(errno));
    cbReceived += cb;
  }

  //   select() timed out so there's nothing more OR we've reached
  // the requested byte count.
  return cbReceived;

#else
  // Anything else is not implemented!
  return 0;
#endif
}

PUBLIC bool serReceiveByte (uint8_t *pbData, uint32_t lTimeout)
{
  //++
  // Like serReceive(), but receive exactly one byte ...
  //--
  return serReceive(pbData, 1, lTimeout) > 0;
}

PUBLIC void serSetDTR (bool fDTR)
{
  //++
  // Assert or deassert the serial port DTR signal.
  //--
  assert(m_hSerialPort);
#if defined(_WIN32)
  // WINDOWS VERSION ...
  if (!EscapeCommFunction(m_hSerialPort, fDTR ? SETDTR : CLRDTR))
    FatalError("error controlling COM port DTR");
#elif defined(__linux__)
  // LINUX VERSION ...
  int bits = TIOCM_DTR;
  unsigned long func = fDTR ? TIOCMBIS : TIOCMBIC;
  if (ioctl(m_hSerialPort, func, &bits) != 0)
    FatalError("error (%s) controlling DTR", strerror(errno));
#endif
}

PUBLIC void serSleep (uint32_t lDelay)
{
  //++
  //   This function will delay for the specified number of milliseconds. 
  // The protocol module calls this when we need to give the PromICE some
  // extra time to respond to a command.  Yes, it really has nothing to do
  // with the serial port, but it's a OS dependent function and currently
  // those are all isolated to this module.
  //--
#if defined(_WIN32)
  // WINDOWS VERSION ...
  Sleep(lDelay);
#elif defined(__linux__)
  // LINUX VERSION ...
  usleep(lDelay*1000UL);
#endif
}

