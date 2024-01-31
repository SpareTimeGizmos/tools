//++
// abs2asm.c - convert PDP11 Absolute Loader files to Assembly
//
// Copyright (C) 2021 by Robert Armstrong.  All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, visit the website of the Free
// Software Foundation, Inc., www.gnu.org.
//
// DESCRIPTION:
//   This program converts a PDP11 paper tape image file in absolute loader
// format into a sequence of assembly language ".WORD ..." statements.  No,
// it doesn't actually disassemble the original PDP11 program.  Sorry...  This
// is used to embed images of paper tape programs into the SBCT11 EPROM.
//
//   For review, absolute loader paper tapes consist of several data blocks.
// Each data block is formatted like this -
//
//   any number of leader bytes (all zeros)
//   start of data marker (1 byte of 0x01)
//   unused byte (zero)
//   low byte of count word
//   high byte of count word
//   low byte of load address
//   high byte of load address
//   ... data bytes ...
//   one byte checksum
//
// The count word is a count of BYTES (not words!) and includes all data bytes
// plus the header bytes (six in all), but NOt including the checksum.  The
// last block of the tape should be one with a length of six bytes (i.e. just
// the header and zero data bytes).  The load address contained in this last
// block is the starting address of the program.
//
//   The output of this program consists of blocks formatted like this -
//
//      .WORD     <count>
//      .WORD     <address>
//      .BYTE     <data1>
//      ...
//      .BYTE     <dataN>
//      .EVEN
//
// The count and address fields are exactly as they appear in the paper tape
// image except converted to words, as are the data bytes.  The leader bytes
// and all checksum bytes are verified but then discarded.  A .EVEN directive
// ensures that if there are an odd number of data bytes then the next record
// always starts on a word address.
//
// USAGE:
//      abs2asm [-v] <paper time image file> <assembly output file>
//
// REVISION HISTORY
// 11-Apr-21    RLA             New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdio.h>		// printf(), scanf(), et al.
#include <stdlib.h>		// exit(), ...
#include <stdint.h>             // uint8_t, uint32_t, etc ...
#include <stdbool.h>            // bool, TRUE, FALSE, etc ...
#include <assert.h>             // assert() (what else?)
#include <string.h>		// strlen(), etc ...
#include <ctype.h>              // isprint(), et al ...
#ifdef __linux__
#include <unistd.h>		// read(), open(), etc ...
#include <libgen.h>		// basename(), ...
#include <sys/io.h>             // _read(), _open(), et al...
#include <linux/limits.h>	// PATH_MAX, ...
#define O_BINARY 0
#else
#include <io.h>                 // _read(), _open(), et al...
#endif
#include <fcntl.h>              // _O_RDONLY, _O_BINARY, etc...

// CONSTANTS ...
#define ABSBUFSIZ	  512	// size of the ABS file chunks we read
#define MAXABSBLK       32768   // largest possible single ABS file block

// VARIABLES ...
// Command line values ...
char g_szInputFile [PATH_MAX];  // name of the input (PDP-11 BIN) file
char g_szOutputFile[PATH_MAX];  // name of the high byte output file
bool      g_fVerbose;           // be extra verbose when proessing
// Absolute file data ...
int       g_hABSfile;           // handle, from _open() of the OBJ file
FILE     *g_fASMfile;           // assembly language output file
uint8_t   g_abABSbuf[ABSBUFSIZ];// buffer for data read from the OBJ file
int       g_cbABSbuf;		// number of bytes in the buffer now
int       g_nABSbufPos;		// next byte in the buffer to be used

// ERROR MACROS ...
#define FAIL(msg)		\
  {fprintf(stderr, "abs2asm: " msg "\n");  exit(EXIT_FAILURE);}
#define FAIL1(msg,arg)	        \
  {fprintf(stderr, "abs2asm: " msg "\n", arg);  exit(EXIT_FAILURE);}
#define WARN(msg)		\
  fprintf(stderr, "abs2asm: " msg "\n");

// BYTE/WORD MACROS ...
#define ISODD(x)      (((x) & 1) != 0)
#define HIBYTE(x)     ((uint8_t) (((x) >> 8) & 0xFF))
#define LOBYTE(x)     ((uint8_t) ((x) & 0xFF))
#define MKWORD(h,l)   ((uint16_t) (((h) << 8) | (l)))
#define SETHIGH(b,w)  w = MKWORD((b), LOBYTE(w))
#define SETLOW(b,w)   w = MKWORD(HIBYTE(w), (b))
#define GETWORD(a)    MKWORD(g_pabMemory[a|1], g_pabMemory[a])
#define SETWORD(a,w)  {g_pabMemory[a|1] = HIBYTE((w));  g_pabMemory[a] = LOBYTE((w));}


///////////////////////////////////////////////////////////////////////////////
///////////////   C O M M A N D   L I N E   U T I L I T I E S   ///////////////
///////////////////////////////////////////////////////////////////////////////

void GetExtension(char *lpszName, char *lpszType)
{
  //++
  //   This routine will return the current extension (e.g. ".hex", ".bin", etc)
  // of the file name specified ...
  //--
  lpszType[0] = '\0';
#ifdef __linux__
  // Linux version ...
  //   Note that basename() just returns a pointer to our own lpszName buffer,
  // so we have to make a copy to lpszType!
  char *pszFileName = basename(lpszName);
  char *psz = strrchr(pszFileName, '.');
  if (psz != NULL) strcpy(lpszType, psz);
#else
  // The Windows version ...
  _splitpath_s(lpszName, NULL, 0, NULL, 0, NULL, 0, lpszType, PATH_MAX);
#endif
}


void SetFileType(char *pszName, const char *pszType)
{
  //++
  // Apply a default extension to a file that doesn't already have one...
  //--
  char szExt[PATH_MAX];
  GetExtension(pszName, szExt);
  if (strlen(szExt) == 0) strcat(pszName, pszType);
}

void ParseCommand (int argc, char *argv[])
{
  //++
  //   This function parses the command line and initializes all the global
  // variables accordingly.  It's tedious, but fairly brainless work.  If there 
  // are any errors in the command line, then it simply prints an error message
  // and exits - there is no error return!
  //--
  int nArg;

  // First, set all the defaults...
  g_szInputFile[0] = g_szOutputFile[0] = '\0';
  g_fVerbose = false;
  
  // If there are no arguments, then just print the help and exit...
  if (argc == 1) {
    fprintf(stderr, "Usage:\n");
        fprintf(stderr,"\tabs2asm [-v] input-file output-file\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Options:\n");
    fprintf(stderr, "\t-v\t\t- be extra verbose when processing\n");
    exit(EXIT_SUCCESS);
  }

  for (nArg = 1;  nArg < argc;  ++nArg) {
    // If it doesn't start with a "-" character, then it must be a file name.
    if (argv[nArg][0] != '-') {
           if (strlen(g_szInputFile)  == 0) strcpy(g_szInputFile,  argv[nArg]);
      else if (strlen(g_szOutputFile) == 0) strcpy(g_szOutputFile, argv[nArg]);
      else FAIL1("too many files specified: \"%s\"", argv[nArg]);
      continue;
    }
    
    // Handle the -v (verbose) option...
    if (strcmp(argv[nArg], "-v") == 0) {
      g_fVerbose = true;  continue;
    }

    // Otherwise it's an illegal option...
    FAIL1("unknown option - \"%s\"\n", argv[nArg]);
  }

  // Make sure all the file names were specified...
  if (strlen(g_szOutputFile) == 0)
    FAIL("required file names missing");
  SetFileType(g_szInputFile, ".ptp");
  SetFileType(g_szOutputFile, ".asm");
}


///////////////////////////////////////////////////////////////////////////////
//////   P A P E R   T A P E   I M A G E   F I L E   U T I L I T I E S   //////
///////////////////////////////////////////////////////////////////////////////

bool ReadABSbyte (uint8_t *pb)
{
  //++ 
  //   This function reads and returns the next byte from the ABS file.
  // Bytes are buffered, one block at a time, and if we've finished this
  // block then we'll attempt to read the next one.  When we reach the end
  // of the image file, we return FALSE.
  //--
  int Count;
  if (g_nABSbufPos >= g_cbABSbuf) {
    Count = read(g_hABSfile, g_abABSbuf, sizeof(g_abABSbuf));
    if (Count <= 0) return false;
    g_cbABSbuf = Count;  g_nABSbufPos = 0;
  }
  *pb = g_abABSbuf[g_nABSbufPos++];
  return true;
}

bool ReadABSword (uint16_t *pw)
{
  //++
  //   Read two bytes from the PDP11 absolute loader file and return them as
  // a sixteen bit word.  REMEMBER that PDP11s are little endian machines!
  //--
  uint8_t b1, b2;
  if (!ReadABSbyte(&b1)) return false;
  if (!ReadABSbyte(&b2)) return false;
  *pw = MKWORD(b2, b1);
  return true;
}

bool ReadABSrecord (uint16_t *pwAddress, uint16_t *pcbData, uint8_t *pbData)
{
  //++
  //   This routine reads one absolute loader block and verifies the checksum.
  // If all is well it returns TRUE and the length of the block (including the
  // six header bytes).  If a bad record is found it aborts, and when we reach
  // the end of the image file it returns FALSE.
  //--
  uint8_t b, bChecksum;  uint16_t cbData, wAddress;

  // Skip over zero bytes, and quit if we find EOF ...
  do
    if (!ReadABSbyte(&b)) return 0;
  while (b == 0);

  // Valid records start with the bytes 0x01 and 0x00 ...
  if ((b != 0x01) || !ReadABSbyte(&b) || (b != 0x00))
    FAIL("failed to find 0x0001 record header in image file");
  bChecksum = 0x01;

  // Read the record length ...
  if (!ReadABSword(&cbData)) FAIL("failed to find record length in image file");
  if ((cbData < 6) || (cbData > MAXABSBLK)) FAIL1("image file record length (%d) too long", cbData)
  bChecksum += LOBYTE(cbData) + HIBYTE(cbData);  *pcbData = cbData-6;

  // Read the load address ...
  if (!ReadABSword(&wAddress)) FAIL("failed to find record address in image file");
  bChecksum += LOBYTE(wAddress) + HIBYTE(wAddress);  *pwAddress = wAddress;

  // Now read the actual data, one byte at a time ...
  for (uint16_t i = 0;  i < cbData-6;  ++i) {
    if (!ReadABSbyte(&pbData[i])) FAIL("premature EOF while reading image file");
    bChecksum += pbData[i];
  }

  //   Lastly, read and verify the checksum ...   Note that the checksum of the
  // image file record is just the complement of the sum of all the bytes...
  if (!ReadABSbyte(&b)) FAIL("failed to find checksum in image file");
  if (LOBYTE(bChecksum+b) != 0) FAIL("bad checksum found in image file");
  if (g_fVerbose) fprintf(stderr, "abs2asm: read block, length=%d, address=%06o\n", cbData, wAddress);

  // All's well - return success...
  return true;
}


///////////////////////////////////////////////////////////////////////////////
/////////////   A S S E M B L Y   L A N G U A G E    O U T P U T   ////////////
///////////////////////////////////////////////////////////////////////////////

void WriteData (uint16_t wAddress, uint16_t cbData, uint8_t *pbData)
{
  //++
  //   This routine dumps the image file data record to the output in PDP11
  // assembly language (well, it's just a bunch of .WORD and .BYTE statements,
  // but they can be assembled!).
  //--
  fprintf(g_fASMfile, "\n");
  if (g_fVerbose) fprintf(g_fASMfile, "; Record length=%d, address=%06o\n", cbData, wAddress);
  fprintf(g_fASMfile, "\t.WORD\t^D%d\n", cbData);
  fprintf(g_fASMfile, "\t.WORD\t%06o\n", wAddress);
  for (uint16_t cb = 0; cb < cbData;) {
    fprintf(g_fASMfile, "\t.BYTE\t");
    for (uint16_t i = 0;  i < 8;  ++i) {
      if (i > 0) fprintf(g_fASMfile, ", ");
      fprintf(g_fASMfile, "%03o", pbData[cb]);
      if (++cb >= cbData) break;
    }
    fprintf(g_fASMfile, "\n");
  }
  if (ISODD(cbData)) fprintf(g_fASMfile, "\t.EVEN\n");
}

///////////////////////////////////////////////////////////////////////////////
/////////////////////////   M A I N   P R O G R A M   /////////////////////////
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
  //++
  //--
  uint8_t abData[MAXABSBLK];  uint16_t wAddress, cbData;

  // Parse the command line...
  ParseCommand(argc, argv);

  // Open the PDP-11 absolute loader file ... 
  g_hABSfile = open(g_szInputFile, O_RDONLY | O_BINARY);
  if (g_hABSfile == -1) FAIL1("unable to read %s\n", g_szInputFile);
  g_cbABSbuf = g_nABSbufPos = 0;

  // Open the output file for writing...
  if ((g_fASMfile=fopen(g_szOutputFile, "wt")) == NULL)
    FAIL1("unable to write %s", g_szOutputFile);

  // Read and process the image file ...
  while (ReadABSrecord(&wAddress, &cbData, abData)) {
    WriteData(wAddress, cbData, abData);
    if (cbData == 0) break;
  }

  // And we're done!
  return EXIT_SUCCESS;
}

