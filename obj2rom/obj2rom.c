//++
// obj2rom.c - convert MACRO11 OBJ files to EPROM images for SBCT11
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
//   This program converts a PDP11 object file produced by the MACRO11 
// assembler and converts it into one or two Intel HEX format files for
// programming EPROMs for the Spare Time Gizmos SBCT11 systems.  One EPROM
// is used for T11 eight bit bus systems, and two are needed for sixteen
// bit DCT11 implementations.
//
// Usage:
//      obj2rom [-8] [-d] [-v] [-onnnnnn] [-sdddd] [-cnnnnnn] input-file low-file [high-file]
//
//      -8 specifies an eight bit bus system.  Only one output file should be
//      specified in this instance.
//
//      -onnnnnn specifies the base address, in octal, where the EPROMs are
//       mapped in PDP-11 address space.  This offset is subtracted from the
//       addresses all data loaded.
//
//      -sddddd specifies the size of the EPROMs, in decimal bytes.
//       This the size of the physical EPROM(s), NOT the size of the PDP11
//       image.  For example, "-8 -s32768" specifies ONE 32K byte EPROM, or
//       16K PDP11 words.  But "-s32768" (NO -8!) specifies TWO 32K EPROMs,
//       or 32K PDP11 words.
//
//      -cnnnnnn specifies that obj2rom should calculate the 16 bit checksum
//       of the memory image and then store the two's complement of that at
//       the address specified by nnnnnn (octal).  As a result the sun of all
//       words in the EPROM, including the checksum word, should be zero.
//      
//      -d dump the resulting PDP11 memory image to stdout
//
//      -v be extra verbose while processing
//
// REVISION HISTORY
// 10-Feb-00    RLA             New file.
// 31-Jan-09    RLA		Adapted for the PDP-11
// 16-Mar-21    RLA             Update for the SBCT11 v2
// 30-NOV-24	RLA		Fixes to compile on Linux
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
#include <fcntl.h>              // _O_RDONLY, _O_BINARY, etc...
#ifdef __linux__
#include <limits.h>		// PATH_MAX ...
#include <unistd.h>		// open(), write(), close(), etc ...
#include <sys/io.h>		// _open(), _write(), _close(), etc...
#include <libgen.h>		// basename() and dirname() ...
#include <errno.h>		// errno, ENOENT, etc ...
#else
#include <io.h>			// _open(), _write(), _close(), etc...
#endif


// Linux has slightly different names for these symbols...
#ifdef __linux__
//#define _O_CREAT        O_CREAT
//#define _O_WRONLY       O_WRONLY
#define _O_RDONLY	O_RDONLY
//#define _O_TRUNC        O_TRUNC
#define _O_BINARY       0
//#define _write          write
#define _read           read
#define _open           open
#define _close          close
#define _MAX_PATH	PATH_MAX
#endif

// CONSTANTS ...
#define PROGRAM     "obj2rom"   // name used for messages
#define PDPMEMSIZE	65536	// PDP11 memory size, in BYTES!
#define MAXOBJREC	  512	// longest OBJ file record allowed

// VARIABLES ...
// Command line values ...
char g_szInputFile[_MAX_PATH];  // name of the input (PDP-11 BIN) file
char g_szHighFile[_MAX_PATH];   // name of the high byte output file
char g_szLowFile[_MAX_PATH];    //   "   "   " low   "     "     "
uint16_t  g_wROMoffset;         // EPROM address offset, if any
uint32_t  g_lROMsize;           // EPROM size, in bytes
bool      g_fEightBit;          // TRUE for an 8 bit bus system
bool      g_fDumpMemory;        // TRUE to dump memory image ....
bool      g_fVerbose;           // be extra verbose when proessing
bool      g_fChecksum;          // TRUE to insert a checksum word
bool      g_fAssembler;         // TRUE to output assembler instead of hex
uint16_t  g_wChecksumLocation;  // EPROM location to receive the checksum
// OBJ file data ...
int       g_hOBJfile;           // handle, from _open() of the OBJ file
uint8_t   g_abOBJbuf[MAXOBJREC];// buffer for data read from the OBJ file
int       g_cbOBJbuf;		// number of bytes in the buffer now
int       g_nOBJbufPos;		// next byte in the buffer to be used
uint16_t  g_wLastTextAddress;	// address of last data byte loaded
uint16_t  g_wStartAddress;      // program start (transfer) address
// PDP11 memory image ...
uint8_t  *g_pabMemory;		// PDP-11 memory image

// ERROR MACROS ...
#define FAIL(msg)		\
  {fprintf(stderr, PROGRAM ": " msg "\n");  exit(EXIT_FAILURE);}
#define FAIL1(msg,arg)	\
  {fprintf(stderr, PROGRAM ": " msg "\n", arg);  exit(EXIT_FAILURE);}
#define WARN(msg)		\
  fprintf(stderr, PROGRAM ": " msg "\n");

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

char *GetFileType (char *pszName)
{
  //++
  //   This routine will return a pointer to the extension (file type) part
  // of the filename, or NULL if no extension was given.
  //
  //   Note that the pointer returned points to the "." at the beginning of
  // the extension, and it's a pointer to the caller's original string!
  // No string copy is made...
  //--
//  if (lpszName == NULL) return NULL;
  char *psz = strrchr(pszName, '.');
  if (psz == NULL) return NULL;
  //   We found a ".", but we can't guarantee that an extension follows
  // because it could be part of the path.  For example, "../foo", or
  // "..\foo", or on VMS even "[A.B.C]FOO"...  We need to do more checks
  // before we can be sure.
#ifdef VMS
  if (strchr(psz, ']') != NULL) return NULL;
#else
  if (strchr(psz, '/' ) != NULL) return NULL;
  if (strchr(psz, '\\') != NULL) return NULL;
#endif
  return psz;
}

//++
//   This routine will apply a default extension to a file that doesn't
// already have one...
//--
void SetFileType (char *pszName, char *pszType)
{
  if (GetFileType(pszName) == NULL) strcat(pszName, pszType);
}


void ParseCommand (int argc, char *argv[])
{
  //++
  //   This function parses the command line and initializes all the global
  // variables accordingly.  It's tedious, but fairly brainless work.  If there 
  // are any errors in the command line, then it simply prints an error message
  // and exits - there is no error return!
  //--
  int nArg;  char *psz;

  // First, set all the defaults...
  g_szInputFile[0] = g_szHighFile[0] = g_szLowFile[0] = '\0';
  g_wROMoffset = g_wChecksumLocation = g_wStartAddress = 0;  g_lROMsize = 0;
  g_fEightBit = g_fDumpMemory = g_fVerbose = g_fChecksum = g_fAssembler = false;
  
  // If there are no arguments, then just print the help and exit...
  if (argc == 1) {
    fprintf(stderr, "Usage:\n");
        fprintf(stderr,"\t%s [-8] [-d] [-v] [-onnnnnn] [-sddddd] [-cnnnnnn] input-file low-file [high-file]\n", PROGRAM);
    fprintf(stderr,"\n");
    fprintf(stderr,"Options:\n");
    fprintf(stderr, "\t-8\t\t- eight bit bus system\n");
    fprintf(stderr, "\t-d\t\t- dump memory to stdout\n");
    fprintf(stderr, "\t-v\t\t- be extra verbose when processing\n");
    fprintf(stderr, "\t-onnnnnn\t- set EPROM offset, in octal\n");
    fprintf(stderr, "\t-sddddd\t\t- set EPROM size, in decimal\n");
    fprintf(stderr, "\t-cnnnnnn\t- compute checksum and store in nnnnnn (octal)\n");
    fprintf(stderr, "\t-a\t\t- output assembler file instead of hex\n");
    exit(EXIT_SUCCESS);
  }

  for (nArg = 1;  nArg < argc;  ++nArg) {
    // If it doesn't start with a "-" character, then it must be a file name.
    if (argv[nArg][0] != '-') {
           if (strlen(g_szInputFile) == 0) strcpy(g_szInputFile, argv[nArg]);
      else if (strlen(g_szLowFile)   == 0) strcpy(g_szLowFile,   argv[nArg]);
      else if (strlen(g_szHighFile)  == 0) strcpy(g_szHighFile,  argv[nArg]);
      else FAIL1("too many files specified: \"%s\"", argv[nArg]);
      continue;
    }
    
    // Handle the -s (size) option...
    if (strncmp(argv[nArg], "-s", 2) == 0) {
      g_lROMsize = (uint32_t) strtoul(argv[nArg]+2, &psz, 10);
      if ((*psz != '\0')  ||  (g_lROMsize == 0))
        FAIL1("illegal EPROM size: \"%s\"", argv[nArg]);
      continue;
    }
    
    // Handle the -o (offset) option...
    if (strncmp(argv[nArg], "-o", 2) == 0) {
      g_wROMoffset = (uint16_t) strtoul(argv[nArg]+2, &psz, 8);
      if (*psz != '\0')
        FAIL1("illegal EPROM offset: \"%s\"", argv[nArg]);
      continue;
    }
    
    // Handle the -c (checksum) option...
    if (strncmp(argv[nArg], "-c", 2) == 0) {
      g_fChecksum = true;
      g_wChecksumLocation = (uint16_t) strtoul(argv[nArg]+2, &psz, 8);
      if (*psz != '\0')
        FAIL1("illegal checksum location: \"%s\"", argv[nArg]);
      continue;
    }

    // Handle the -8 (eight bit) option...
    if (strcmp(argv[nArg], "-8") == 0) {
      g_fEightBit = true;  continue;
    }
    
    // Handle the -a (assembler) option...
    if (strcmp(argv[nArg], "-a") == 0) {
      g_fAssembler = true;  continue;
    }

    // Handle the -d (dump memory) option...
    if (strcmp(argv[nArg], "-d") == 0) {
      g_fDumpMemory = true;  continue;
    }

    // Handle the -v (verbose) option...
    if (strcmp(argv[nArg], "-v") == 0) {
      g_fVerbose = true;  continue;
    }

    // Otherwise it's an illegal option...
    FAIL1("unknown option - \"%s\"\n", argv[nArg]);
  }

  // Make sure all the file names were specified...
  if (g_fEightBit || g_fAssembler) {
    if (strlen(g_szLowFile) == 0)
      FAIL("required file names missing");
    if (strlen(g_szHighFile) != 0)
      FAIL("specify only one output file"); 
    SetFileType(g_szLowFile, g_fAssembler ? ".asm" : ".hex");
  } else {
    if (strlen(g_szHighFile) == 0)
      FAIL("required file names missing");
    SetFileType(g_szLowFile, ".hex");
    SetFileType(g_szHighFile, ".hex");
  }
  SetFileType(g_szInputFile, ".obj");
  if (g_lROMsize == 0) FAIL("specify EPROM size with -s option");
}


///////////////////////////////////////////////////////////////////////////////
////////////////   O B J E C T   F I L E   U T I L I T I E S   ////////////////
///////////////////////////////////////////////////////////////////////////////

void RAD2ASC (uint16_t w1, uint16_t w2, char *pszName)
{
  //++
  //   This routine converts a RADIX50 symbol name to ASCII.  The result is
  // always six characters long, including trailing spaces, and pszName had
  // better be at least 7 bytes long!
  //--
  static const char RAD50[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ$.%0123456789";
  for (unsigned i = 0; i < 3; i++) {
    uint16_t ch = w1 % 050;  w1 /= 050;
    pszName[2-i] = RAD50[ch];
  }
  for (unsigned i = 0; i < 3; i++) {
    uint16_t ch = w2 % 050;  w2 /= 050;
    pszName[5-i] = RAD50[ch];
  }
  pszName[6] = 0;
}

bool ReadOBJbyte (uint8_t *pb)
{
  //++ 
  //   This function reads and returns the next byte from the OBJ file.
  // Bytes are buffered, one block at a time, and if we've finished this
  // block then we'll attempt to read the next one.  When we reach the end
  // of the object file, we return FALSE.
  //--
  int Count;
  if (g_nOBJbufPos >= g_cbOBJbuf) {
    Count = _read(g_hOBJfile, g_abOBJbuf, sizeof(g_abOBJbuf));
    if (Count <= 0) return false;
    g_cbOBJbuf = Count;  g_nOBJbufPos = 0;
  }
  *pb = g_abOBJbuf[g_nOBJbufPos++];
  return true;
}

bool ReadOBJword (uint16_t *pw)
{
  //++
  //   Read two bytes from the PDP11 object file and return them as a sixteen
  // bit word.  REMEMBER that PDP11s are little endian machines!
  //--
  uint8_t b1, b2;
  if (!ReadOBJbyte(&b1)) return false;
  if (!ReadOBJbyte(&b2)) return false;
  *pw = MKWORD(b2, b1);
  return true;
}

bool ReadOBJrecord (uint16_t *pcwRecord, uint8_t *pbRecord)
{
  //++
  //   This routine reads one object file record and verifies the checksum.  If
  // all is well it returns TRUE and the length of the record (plus the actual
  // data, of course).  If a bad record is found it aborts, and when we reach
  // the end of the object file it returns FALSE.
  //--
  uint8_t b, bChecksum1, bChecksum2;  uint16_t wLength, i;

  // Skip over zero bytes, and quit if we find EOF ...
  do
    if (!ReadOBJbyte(&b)) return 0;
  while (b == 0);

  // Valid records start with the bytes 0x01 and 0x00 ...
  if ((b != 0x01) || !ReadOBJbyte(&b) || (b != 0x00))
    FAIL("failed to find 0x0001 record header in object file");
  bChecksum1 = 0x01;

  // Read the record length ...
  if (!ReadOBJword(&wLength)) FAIL("failed to find record length in object file");
  if ((wLength < 4) || (wLength > MAXOBJREC)) FAIL1("object file record length (%d) too long", wLength)
  bChecksum1 += LOBYTE(wLength);  bChecksum1 += HIBYTE(wLength);

  // Now read the actual data, one byte at a time ...
  for (i = 0;  i < wLength-4;  ++i) {
    if (!ReadOBJbyte(&pbRecord[i])) FAIL("premature EOF while reading object file");
    bChecksum1 += pbRecord[i];
  }

  //   Lastly, read and verify the checksum ...   Note that the checksum of the
  // object file record is just the complement of the sum of all the bytes...
  if (!ReadOBJbyte(&bChecksum2)) FAIL("failed to find checksum in object file");
  if (LOBYTE(bChecksum1+bChecksum2) != 0) FAIL("bad checksum found in object file");

  // All's well - return success...
  *pcwRecord = wLength-4;  return true;
}

void ProcessGSD (uint16_t wLength, uint8_t *pbRecord)
{
  //++
  //   Process global symbol directory records.  Currently we don't do anything
  // meaningful with these, but we decode them and print 'em out just for fun.
  //--
  static const char *apszTypes[] = {"MODULE", "CSECT", "INTSYM", "XFRADR",
                                    "GBLSYM", "PSECT", "IDENT", "VSECT"};
  char szSymbol[7];  uint8_t bFlags, bType;  uint16_t wValue;
  for (uint16_t i = 0;  i < wLength;  i += 8) {
    RAD2ASC(MKWORD(pbRecord[i+1], pbRecord[i]), MKWORD(pbRecord[i+3], pbRecord[i+2]), szSymbol);
    bFlags = pbRecord[i+4];
    bType = pbRecord[i+5];
    wValue = MKWORD(pbRecord[i+7], pbRecord[i+6]);
    if (g_fVerbose)
      fprintf(stderr, PROGRAM ": GSD record, SYM=\"%-6s\", type=%-6s, flags=%03o, value=%06o\n", szSymbol, apszTypes[bType], bFlags, wValue);
    if (bType == 3) g_wStartAddress = wValue;
  }
}

void ProcessRLD (uint16_t wLength, uint8_t *pbRecord)
{
  //++
  //   Process relocation directory records - we have to process these at least at
  // a minimal level.  There are two that update the location counter (don't really
  // know why we need two different ones for that!) and more importantly RLD type 3
  // is an "internal displacement".  These are generated whenever PC relative
  // addressing is used (PC mode 6), and that happens a lot!
  //--
  uint16_t i, adr, off, loc;
  for (i = 0;  i < wLength;  ) {
    switch (pbRecord[i]) {

      case 0x03:
	// internal displacement
	adr = g_wLastTextAddress + pbRecord[i+1] - 4;
	loc = MKWORD(pbRecord[i+3], pbRecord[i+2]);
	off = loc - adr - 2;
	if (g_fVerbose) fprintf(stderr, PROGRAM ": RLD record type 3, adr=%o off=%o loc=%o\n", adr, off, loc);
        SETWORD(adr, off);
	i += 4;  break;

      case 0x07:
	// location counter definition
	loc = MKWORD(pbRecord[i+7], pbRecord[i+6]);
        if (g_fVerbose) fprintf(stderr, PROGRAM ": RLD record type 7, loc=%o\n", loc);
	g_wLastTextAddress = loc;
	i += 8;  break;

      case 0x08:
	// location counter modification
	loc = MKWORD(pbRecord[i+3], pbRecord[i+2]);
        if (g_fVerbose) fprintf(stderr, PROGRAM ": RLD record type 8, loc=%o\n", loc);
	g_wLastTextAddress = loc;
	i += 4;  break;

      default:
	FAIL1("unknown RLD record type 0x%02x", pbRecord[i]);
    }
  }
}

void LoadText (uint16_t wAddress, uint16_t cbText, uint8_t *pbText)
{
  //++
  // Load a text record into the memory image.  This one, at least, is easy...
  //--
  if (g_fVerbose) fprintf(stderr, PROGRAM ": TEXT record, loading %d bytes at %0o\n", cbText, wAddress);
  g_wLastTextAddress = wAddress;
  memcpy(&g_pabMemory[wAddress], pbText, cbText);
}

void ReadObjectFile (void)
{
  //++
  //   Open the object file; read it and process all the records we find there,
  // and then close it.  If any errors occur then just abort this program.
  //--
  uint8_t bRecord[MAXOBJREC];  uint16_t wLength;

  // Open the PDP-11 object file ... 
  g_hOBJfile = _open(g_szInputFile, _O_RDONLY | _O_BINARY);
  if (g_hOBJfile == -1) FAIL1("unable to read %s\n", g_szInputFile);
  g_cbOBJbuf = g_nOBJbufPos = 0;  g_wLastTextAddress = 0;

  // Read records from the object file and process each one ...
  while (ReadOBJrecord(&wLength, &bRecord[0])) {
    switch (bRecord[0]) {

      // Process global symbol definitions ...
      case 0x01:
        ProcessGSD(wLength-2, &bRecord[2]);  break;

      // Load text records into the memory image ...
      case 0x03:
        if (wLength < 5) FAIL1("object file text record length (%d) too short", wLength);
        LoadText(MKWORD(bRecord[3], bRecord[2]), wLength-4, &bRecord[4]);  break;

      // Process relocation records ...
      case 0x04:
        ProcessRLD(wLength-2, &bRecord[2]);  break;

      // Ignored record types ...
      case 0x02:
        if (g_fVerbose) fprintf(stderr, PROGRAM ": ENDGSD record ignored, length=%d\n", wLength);
        break;
      case 0x05:
        if (g_fVerbose) fprintf(stderr, PROGRAM ": ISD record ignored, length=%d\n", wLength);
        break;
      case 0x06:
        if (g_fVerbose) fprintf(stderr, PROGRAM ": ENDMOD record ignored, length=%d\n", wLength);
        break;
      case 0x07:
        if (g_fVerbose) fprintf(stderr, PROGRAM ": LIBHDR record ignored, length=%d\n", wLength);
        break;
      case 0x08:
        if (g_fVerbose) fprintf(stderr, PROGRAM ": LIBEND record ignored, length=%d\n", wLength);
        break;

      // And anything else is an error...
      default:
        FAIL1("unknown object record type 0x%02x", bRecord[0]);
      }
  }

  // Close the object file and we're done...
  _close(g_hOBJfile);
}

void CalculateChecksum()
{
  //++
  //   Calculate the checksum of the memory image and print it out.  If the
  // "-c" command line option was specified then also store the checksum in
  // memory at the requested location.
  //
  //   The checksum is calculated by summing all the words in memory and then
  // taking the two's complement of that.  If the checksum is stored in memory
  // then this means that the total of all words in memory, including the
  // checksum itself, will always be zero.  
  //
  //   Notice that we always compute the checksum for ALL of memory, even it
  // the EPROM only encompasses a part of that.  The PDP11 memory image is
  // initialized to zeros so in theory any unused words shouldn't affect the
  // result.
  //--
  int16_t iChecksum1 = 0;
  for (uint32_t i = 0; i < PDPMEMSIZE-2; i += 2) iChecksum1 += GETWORD(i);
  int16_t iChecksum2 = (-iChecksum1) & 0177777;
  if (g_fChecksum) {
    SETWORD(g_wChecksumLocation, iChecksum2);
    fprintf(stderr, PROGRAM ": checksum %06ho %06ho stored at %06o\n", iChecksum1, iChecksum2, g_wChecksumLocation);
  }
}


///////////////////////////////////////////////////////////////////////////////
/////////////   I N T E L   H E X   F I L E   U T I L I T I E S   /////////////
///////////////////////////////////////////////////////////////////////////////

bool WriteHex (char *lpszFile, uint8_t *pbMemory, uint32_t cbMemory, uint16_t wIncrement)
{
  //++
  //DumpHex
  //
  //   This procedure writes a memory dump in standard Intel HEX file format.
  // This particular implementation has the ability to write every byte, only
  // the even bytes, or only the odd bytes.
  //--
  FILE     *fpHex;        // handle of the file we're writing
  uint16_t  cbRecord;     // length of the current .HEX record
  uint16_t  wChecksum;    // checksum     "   "     "      "     "
  uint32_t  lMemAddr;     // current memory address
  uint16_t  i;            // temporary...

  // Open the output file for writing...
  if ((fpHex=fopen(lpszFile, "wt")) == NULL)
    FAIL1("unable to write file %s\n", lpszFile);

  // Dump all of memory...
  for (lMemAddr = 0;  lMemAddr < cbMemory;  lMemAddr += cbRecord*wIncrement) {

    // Figure out the record size and address for that record.  
    cbRecord = 16;
    if ((uint32_t) (cbRecord*wIncrement) > (cbMemory-lMemAddr))
      cbRecord = (uint16_t) (cbMemory-lMemAddr) / wIncrement;

    // Write the record header (for a type 0 record)...
    uint16_t wRecAddr = lMemAddr / wIncrement;
    fprintf(fpHex, ":%02X%04X00", cbRecord, wRecAddr);
    
    // Start the checksum calculation...
    wChecksum = cbRecord + HIBYTE(wRecAddr) + LOBYTE(wRecAddr) + 00 /*nType*/;

    // And dump all the data bytes in this record.
    for (i = 0;  i < cbRecord;  ++i) {
      uint8_t b = pbMemory[lMemAddr + (i*wIncrement)];
      fprintf(fpHex, "%02X", b);  wChecksum += b;
    }

    // Write out the checksum byte and finish off this record.
    fprintf(fpHex,"%02X\n", (- (short int) wChecksum) & 0xFF);
  }

  //   Don't forget to write an EOF record, which is easy in our case, since it
  // doesn't contain any variable data.
  fprintf(fpHex, ":00000001FF\n");
  if (g_fVerbose) fprintf(stderr, PROGRAM ": %d bytes written to %s\n", cbMemory/wIncrement, lpszFile);
  fclose(fpHex);
  return true;
}

void DumpMemory ()
{
  //++
  //   This routine will dump the entire PDP11 memory image, in octal and
  // ASCII, to stdout.  It can be useful to compare this against the assembly
  // listing to verify that we're decoding the object records correctly.
  //--
  for (uint32_t wAddress = 0;  wAddress < PDPMEMSIZE;  wAddress += 16) {
    printf("%06o/ ", wAddress);
    for (uint32_t i = 0; i < 16; i += 2) {
      uint16_t w = GETWORD((wAddress+i));
      printf(" %06o", w);
    }
    printf("  ");
    for (uint32_t i = 0; i < 16; i++) {
      uint8_t b = g_pabMemory[wAddress+i];
      printf("%c", isprint(b) ? b : '.');
    }
    printf("\n");
  }
}

void WriteBlock (FILE *f, uint16_t wAddress, uint8_t *pbBlock, uint32_t cbBlock)
{
  //++
  //--
  fprintf(f, "\t.WORD\t%06o\t; byte count\n", cbBlock);
  fprintf(f, "\t.WORD\t%06o\t; load address\n", wAddress);
  for (uint32_t cb = 0;  cb < cbBlock;) {
    fprintf(f, "\t.BYTE\t%03o", pbBlock[cb++]);
    for (uint32_t i = 0;  (i < 8) && (cb < cbBlock);  ++i) {
      fprintf(f, ", %03o", pbBlock[cb++]);
    }
    fprintf(f, "\n");
  }
  if (ISODD(cbBlock)) fprintf(f, "\t.EVEN\n");
  fprintf(f, "\n");
}

void WriteAssembler(char *lpszFile)
{
  //++
  //--
  FILE *fpAsm;

  // Open the output file for writing...
  if ((fpAsm=fopen(lpszFile, "wt")) == NULL)
    FAIL1("unable to write file %s\n", lpszFile);

  //
  for (uint32_t wAddress = 0;  wAddress < PDPMEMSIZE;) {
    if (g_pabMemory[wAddress] == 0) {++wAddress;  continue;}
    uint32_t w = wAddress;
    while ((w < PDPMEMSIZE) && (g_pabMemory[w] != 0)) ++w;
    if ((w-wAddress) < 8)  w = wAddress+8;
    if (w >= PDPMEMSIZE) w = PDPMEMSIZE-1;
    WriteBlock(fpAsm, wAddress, &g_pabMemory[wAddress], w-wAddress);
    wAddress = w;
  }

  //
  if (g_wStartAddress != 0)
    WriteBlock(fpAsm, g_wStartAddress, NULL, 0);

  //
  fclose(fpAsm);
}


///////////////////////////////////////////////////////////////////////////////
/////////////////////////   M A I N   P R O G R A M   /////////////////////////
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
  //++
  //--

  // Parse the command line...
  ParseCommand(argc, argv);

  // Allocate the PDP-11 memory image ...
  g_pabMemory = malloc(PDPMEMSIZE);
  if (g_pabMemory == NULL) FAIL("unable to allocate memory image");
  memset(g_pabMemory, 0, PDPMEMSIZE);

  // Read and process the object file ...
  ReadObjectFile();
  if (g_fChecksum) CalculateChecksum();
  if (g_fDumpMemory) DumpMemory();

  // Write out the HEX files ...
  if (g_fAssembler) {
    WriteAssembler(g_szLowFile);
  } else if (g_fEightBit) {
    WriteHex(g_szLowFile,  g_pabMemory, g_lROMsize, 1);
  } else {
    WriteHex(g_szLowFile,  &g_pabMemory[0], g_lROMsize*2, 2);
    WriteHex(g_szHighFile, &g_pabMemory[1], g_lROMsize*2, 2);
  }

  // And we're done!
  return EXIT_SUCCESS;
}

