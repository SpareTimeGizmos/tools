//++
//romcksum - add a checksum to an EPROM image
//
// Copyright (C) 2017 by Spare Time Gizmos.  All rights reserved.
//
// This firmware is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 59 Temple
// Place, Suite 330, Boston, MA  02111-1307  USA.
//
//DESCRIPTION:
//   This program will read a ROM image from an Intel .HEX file, calculate a
// 16 bit checksum for it, and then store that checksum in the last two bytes
// of the ROM.  The checksum is calculated so that the 16 bit unsigned sum of
// all the bytes in the ROM, _including_ the checksum in the last two bytes,
// is equal to the last two bytes.
//
//   Since the checksum is included in itself, we have to go to some lengths
// to prevent the checksum value from affecting its own calculation.  The way
// that's done is to actually use the last _four_ bytes of the ROM - the last
// two contain the checksum and the two before that contain the two's complement
// of each byte in the checksum.  The sum of a byte and its complement is always
// 0x0100, and since there are two such bytes, adding a checksum to the ROM in
// this way always adds 0x0200 to the original checksum _regardless_ of what
// the actual checksum value may be.
//
//   This rather complicated system has a couple of advantages.  First, the
// checksum calculated by the Data IO and EETools TopMax programmers will
// always agree with the checksum calculated by this program.  Second, since
// the checksum is not zero (as it would be if we simply stored the two's
// complement of the checksum in the ROM), the checksum can be used to uniquely
// identify ROM versions.
//
//    The observant reader may notice that there's a tiny little problem with
// this algorithm.  If either byte of the checksum just happens to be 0x00,
// then the two's complement of 0 is 0, and needless to say, 0 plus 0 does not
// give 0x0100!  This is an unavoidable consequence of using two's complement.
// One fix would be to use the one's complement instead, which has no such 
// pitfall, but the weight of historical precendent prevents us from taking
// the easy way out.  Instead, the alorithm checks for this case (either or
// both bytes zero) and adjusts the checksum correction accordingly.
//
//   One final side effect of this program is that it also fills unused bytes
// in the ROM image, and writes these filler characters out to the new HEX
// file.  Normally the filler is 0xFF, but an alternate value may be specified
// on the command line.
//
// USAGE:
//    romcksum input-file [-cnnnn] [-snnnn] [-onnnn] [-fnn] [-e|-b] [-v] output-file
//      -cnnnn  - set the offset of the checksum to nnnn
//      -snnnn  - set the ROM size to nnnn bytes
//      -onnnn  - set the offset applied to input files
//      -fnn    - fill unused ROM locations with nn
//      -e      - store the checksum in little-endian format (default)
//      -b      - store the checksum in big-endian format
//      -v      - verbose output
//
// NOTE:
//   This program will work for ROMs up to 64K bytes.  In theory it would work
// for bigger EPROMs, but the HEX file reader/writer can only handle four digit
// addresses.
//
// REVISION HISTORY
// 12-May-98	RLA	New file...
//  3-May-17    RLA     Update to Visual Studio 2013
// 29-Nov-24    RLA     Change #include <limits.h> to <linux/limits.h>
//--
#include <stdio.h>		// printf(), scanf(), et al.
#include <stdlib.h>		// exit(), ...
#include <stdint.h>             // uint8_t, uint32_t, etc ...
#include <stdbool.h>            // bool, TRUE, FALSE, etc ...
#include <string.h>		// strlen(), etc ...
#ifdef __linux__		// ...
#include <linux/limits.h>	// PATH_MAX ...
#define _MAX_PATH PATH_MAX
#endif

// Useful definitions....
#define MAXROM    65536         // the biggest image we can accomodate (64Kb)

#define HIBYTE(x) 	((uint8_t)  (((x) >> 8) & 0xFF))
#define LOBYTE(x) 	((uint8_t)  ((x) & 0xFF))
#define HIWORD(x)	((uint16_t) (((x) & 0xFFFF0000L) >> 16))
#define LOWORD(x) 	((uint16_t) ( (x) & 0xFFFF))
#define MKWORD(h,l)	((uint16_t) ((((h) & 0xFF) << 8) | ((l) & 0xFF)))

// Globals...
uint32_t lROMSize;		// size of the ROM, in bytes (e.g. 65536)
uint32_t lROMOffset;		// offset applied to HEX file addresses
uint32_t lChecksumOffset;	// position of the checksum bytes
uint8_t  bFillByte;		// filler value for unused ROM locations
bool	 fVerbose;		// true for verbose output
bool     fLittleEndian;		// store checksum in little endian format
char szInputFile[_MAX_PATH];	// input file specification
char szOutputFile[_MAX_PATH];	// output file specification



//++
//ReadHex
//
//   This function will load a standard Intel format .HEX file into memory.
// Only the traditional 16-bit address format is supported, which puts an
// upper limit of 64K on the amount of data which may be loaded.  Only
// record types 00 (data) and 01 (end of file) are recognized.
//
//   The number of bytes read will be returned as the function's value,
// and this will be zero if any error occurs.  Note that all counts, sizes
// and offsets must be longs on the off chance that exactly 64K bytes will
// be read!  It returns the count of bytes read, or zero if an error occurs.
//--
uint32_t ReadHex (char *pszName, uint8_t *pabData, uint32_t cbData, uint32_t lOffset)
{
  FILE	   *f;		// handle of the input file
  uint32_t  lCount = 0;	// number of bytes loaded from file
  unsigned  uLength;	// length       of the current .HEX record
  unsigned  uAddress;	// load address "   "     "      "     "
  unsigned  uType;	// type         "   "     "      "     "
  unsigned  uChecksum;	// checksum     "   "     "      "     "
  unsigned  uByte;	// temporary for the data byte read...
  if ((f=fopen(pszName, "rt")) == NULL)
    {fprintf(stderr,"%s: unable to open file\n", pszName);  return 0;}

  while (true) {
    if (fscanf(f,":%2x%4x%2x", &uLength, &uAddress, &uType) != 3)
      {fprintf(stderr,"%s: bad .HEX file format (1)\n", pszName);  return 0;}
    if (uType > 1)
      {fprintf(stderr,"%s: unknown record type %d\n", pszName, uType);  return 0;}
    uChecksum = uLength + HIBYTE(uAddress) + LOBYTE(uAddress) + uType;
    for (;  uLength > 0;  --uLength, ++uAddress, ++lCount) {
      if (fscanf(f,"%2x",&uByte) != 1)
        {fprintf(stderr,"%s: bad .HEX file format (2)\n", pszName);  return 0;}
      if (LOWORD(uAddress+lOffset) >= cbData)
        {fprintf(stderr,"%s: address outside EPROM\n", pszName);  return 0;}
      pabData[LOWORD(uAddress+lOffset)] = (uint8_t) uByte;
      uChecksum += uByte;
    }
    if (fscanf(f,"%2x\n",&uByte) != 1)
      {fprintf(stderr,"%s: bad .HEX file format (3)\n", pszName);  return 0;}
    uChecksum = LOBYTE(uChecksum+uByte);
    if (uChecksum != 0)
      {fprintf(stderr,"%s: checksum error\n", pszName);  return 0;}
    if (uType == 1) break;
  }
  
  fclose(f);
  return lCount;
}


//++
//WriteHex
//
//   This function will write an array of bytes to a file in standard Intex
// .HEX file format.  Only the traditional 16 bit format is supported and
// so the array must be 64K or less.  The only records generated are type 00
// (data) and 01 (end of file).  This routine always writes everything in
// the array and doesn't attempt to remove filler bytes...
//--
void WriteHex (char *pszName, uint8_t *pabData,	uint32_t cbData)
{
  FILE *f;		// handle of the output file
  uint16_t uAddress = 0;// address of the current record
  uint16_t cbRecord;	// size of the current record
  uint16_t uChecksum;	// checksum "  "     "       "
  unsigned i;		// temporary...
  if ((f=fopen(pszName, "wt")) == NULL)
    {fprintf(stderr,"%s: unable to write file\n", pszName);  return;}

  while (cbData > 0) {
   cbRecord = (cbData > 16) ? 16 : (unsigned) cbData;
    fprintf(f,":%02X%04X00", cbRecord, uAddress);
    uChecksum = cbRecord + HIBYTE(uAddress) + LOBYTE(uAddress) + 00 /* Type */;
    for (i = 0; i < cbRecord; ++i) {
      fprintf(f,"%02X", *(pabData+uAddress+i));
      uChecksum += *(pabData+uAddress+i);
    }
    fprintf(f,"%02X\n", LOBYTE(-uChecksum));
    cbData -= cbRecord;  uAddress += cbRecord;
  }
  
  fprintf(f, ":00000001FF\n");  fclose(f);
}


//++
//   This function parses the command line and initializes all the global
// variables accordingly.  It's tedious, but fairly brainless work.  If there
// are any errors in the command line, then it simply prints an error message
// and exits - there is no error return from this function!
//--
void ParseCommand (int argc, char *argv[])
{
  int nArg;  char *psz;

  // First, set all the defaults...
  szInputFile[0] = szOutputFile[0] = '\0';
  lROMSize = lChecksumOffset = lROMOffset = 0L;  bFillByte = 0xFF;
  fLittleEndian = fVerbose = false;

  // If there are no arguments, then just print the help and exit...
  if (argc == 1) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr,"  romcksum input-file [-cnnnn] [-snnnn] [-onnnn] [-fnn] [-e] [-v] output-file\n");
    fprintf(stderr,"\t-cnnnn\t- set the offset of the checksum to nnnn\n");
    fprintf(stderr,"\t-snnnn\t- set the ROM size to nnnn bytes\n");
    fprintf(stderr,"\t-onnnn\t- set the offset applied to input files\n");
    fprintf(stderr,"\t-fnn\t- fill unused ROM locations with nn\n");
    fprintf(stderr,"\t-e\t- store the checksum in little-endian format\n");
    fprintf(stderr,"\t-b\t- store the checksum in big-endian format\n");
    fprintf(stderr,"\t-v\t- verbose output\n");
    exit(EXIT_SUCCESS);
  }

  for (nArg = 1;  nArg < argc;  ++nArg) {
    // If it doesn't start with a "-" character, then it must be a file name.
    if (argv[nArg][0] != '-') {
      if (strlen(szInputFile) == 0) strcpy(szInputFile, argv[nArg]);
      else if (strlen(szOutputFile)   == 0) strcpy(szOutputFile, argv[nArg]);
      else {
        fprintf(stderr, "romcksum: too many files specified: \"%s\"", argv[nArg]);
	exit(EXIT_FAILURE);
      }
      continue;
    }

    // Handle the -c (checksum) option...
    if (strncmp(argv[nArg], "-c", 2) == 0) {
      lChecksumOffset = strtoul(argv[nArg]+2, &psz, 10);
      if ((*psz != '\0') || (lChecksumOffset == 0) || (lChecksumOffset > 0xFFFF)) {
        fprintf(stderr, "romcksum: illegal offset: \"%s\"", argv[nArg]);
	exit(EXIT_FAILURE);
      }
      continue;
    }

    // Handle the -o (offset) option...
    if (strncmp(argv[nArg], "-o", 2) == 0) {
      lROMOffset = strtoul(argv[nArg]+2, &psz, 10);
      if ((*psz != '\0') || (lROMOffset > 0xFFFF)) {
        fprintf(stderr, "romcksum: illegal offset: \"%s\"", argv[nArg]);
	exit(EXIT_FAILURE);
      }
      continue;
    }

    // Handle the -s (ROM size) option...
    if (strncmp(argv[nArg], "-s", 2) == 0) {
      lROMSize = strtoul(argv[nArg]+2, &psz, 10);
      if (*psz == 'k' || *psz == 'K')   lROMSize <<= 10, ++psz;
      if (*psz != '\0') {
        fprintf(stderr,"romcksum: invalid ROM size \"%s\"\n", argv[nArg]);
        exit(EXIT_FAILURE);
      }
      continue;
    }

    // Handle the -f (fill byte) option...
    if (strncmp(argv[nArg], "-f", 2) == 0) {
      bFillByte = (unsigned) strtol(argv[nArg]+2, &psz, 10);
      if (*psz != '\0') {
        fprintf(stderr,"romcksum: invalid fill byte \"%s\"\n", argv[nArg]);
        exit(EXIT_FAILURE);
      }
      continue;
    }

    // Handle the -e (little endian) and -b (big endian) options...
    if (strcmp(argv[nArg], "-e") == 0) {
      fLittleEndian = true;
      continue;
    }
    if (strcmp(argv[nArg], "-b") == 0) {
      fLittleEndian = false;
      continue;
    }

    // Handle the -v (verbose) option...
    if (strcmp(argv[nArg], "-v") == 0) {
      fVerbose = true;
      continue;
    }

    // Otherwise it's an illegal option...
    fprintf(stderr, "romcksum: unknown option - \"%s\"\n", argv[nArg]);
    exit(EXIT_FAILURE);
  }

  // Make sure all the file names were specified...
  if (strlen(szOutputFile) == 0) {
    fprintf(stderr, "romcksum: required file names missing");
    exit(EXIT_FAILURE);
  }
}


//++
//   This routine calculates the two checksum bytes for the EPROM and returns
// them in CheckH (high byte) and CheckL (low byte), AND it calculates two
// "correction" bytes which are returned in CorrH and CorrL.  The correction
// bytes are calculated so that when they're added to the sum of all the other
// EPROM bytes, plus the checksum bytes, the total is the checksum.  Usually
// this is simple because a byte plus its complement is always 0x0100, but
// there are special cases when that byte is zero.
//--
void CalculateChecksum (uint16_t uSum,
       uint8_t *pbCheckH, uint8_t *pbCheckL, uint8_t *pbCorrH, uint8_t *pbCorrL)
{
  *pbCheckL = LOBYTE(uSum);  *pbCorrL = -*pbCheckL;
  if (uSum == 0xFE01) {
    //   This is the case where everything falls apart.  The correct checksum
    // in this case should be 0x0001, but to get from 0xFE01 to 0x0001 we need
    // two correction bytes that total 0x0200.  That's not going to happen, and
    // I don't see any games we could play to make it happen!
    fprintf(stderr, "UNABLE TO CALCULATE CHECKSUM!!!!!\n");  exit(0);
    //*pbCheckH = 0x00;  *pbCorrH = 0xFF;  *pbCorrL = 0xFF;
  } else if ((uSum > 0xFE01) && (uSum < 0xFF01)) {
    //   If the checksum is in this range, then adding 0x0200 would produce a
    // high byte of zero.  That's not bad by itself, but the complement of zero
    // is zero and 0+0 doesn't produce any carry!  The trick is to set the
    // high order correction byte to 0xFF instead of zero, and then ALSO bump
    // the low byte by one.  This produces 0xFF+1, which gives another carry.
    *pbCheckH = 0x00;  *pbCorrH = 0xFF;  ++*pbCorrL;
  } else {
    //   This is the easy case - we add 0x0100 to the checksum to account for
    // the carry from the high byte and, unless the low byte of the checksum
    // happens to be zero, another 0x0100 for the carry from the low byte.
    // Remember that adding 1 to the high byte alone is the same as adding
    // 0x0100 to the entire checksum!
    *pbCheckH = HIBYTE(uSum) + 1;
    if (*pbCheckL != 0) ++*pbCheckH;
    *pbCorrH = -*pbCheckH;
  }
}


//++
//main
//--
int main (int argc, char *argv[])
{
  uint8_t   abData[MAXROM]; // ROM image buffer
  uint32_t  cbData;         // count of bytes loaded from the .HEX file
  uint16_t  uSum;	    // computed sum of all EPROM bytes
  unsigned  i;              // temporaries...	
  uint8_t bCheckH, bCheckL; // two checksum bytes, high and low
  uint8_t bCorrH, bCorrL;   // two "correction" bytes, high and low
 
  ParseCommand(argc, argv);
  if (lROMSize == 0) lROMSize = 32768L;
  if (lChecksumOffset == 0) lChecksumOffset = lROMSize-4;
  if (fVerbose) {
    fprintf(stderr,"Input file  = %s\n", szInputFile);
    fprintf(stderr,"Output file = %s\n", szOutputFile);
    fprintf(stderr,"ROM Size        = %d (0x%05x)\n", lROMSize, lROMSize);
    fprintf(stderr,"Fill Byte       = %u (0x%02x)\n", bFillByte, bFillByte);
    fprintf(stderr,"Checksum Offset = %d (0x%05x)\n", lChecksumOffset, lChecksumOffset);
    fprintf(stderr,"ROM Offset      = %d (0x%05x)\n", lROMOffset, lROMOffset);
    fprintf(stderr,"Checksum Order  = %s\n", fLittleEndian ? "Little Endian" : "Big Endian");
  }

  // Fill the EPROM with the filler and then load the .HEX file over that...
  for (i = 0; i < lROMSize; ++i)  abData[i] = bFillByte;
  cbData = ReadHex(szInputFile, abData, lROMSize, lROMOffset);
  if (cbData == 0)  exit(1);

  // Force the bytes occupied by the checksum to always be zeros...
  abData[lChecksumOffset+0] = abData[lChecksumOffset+1] = 0;
  abData[lChecksumOffset+2] = abData[lChecksumOffset+3] = 0;

  //   Calculate the sum of the entire EPROM image so far, before we add our
  // magic four bytes to the total...
  for (i = 0, uSum = 0;  i < lROMSize;  ++i) uSum += abData[i];

  // Now calculate the magic bytes ...
  CalculateChecksum(uSum, &bCheckH, &bCheckL, &bCorrH, &bCorrL);
  
  // Put the checksum and its complement in the top of the ROM image...
  if (fLittleEndian) {
    abData[lChecksumOffset+0] = bCorrL;  abData[lChecksumOffset+1] = bCorrH;
    abData[lChecksumOffset+2] = bCheckL;  abData[lChecksumOffset+3] = bCheckH;
  } else {
    abData[lChecksumOffset+0] = bCorrH;  abData[lChecksumOffset+1] = bCorrL;
    abData[lChecksumOffset+2] = bCheckH;  abData[lChecksumOffset+3] = bCheckL;
  }

  // Dump out the new ROM image and we're all done...
  WriteHex(szOutputFile, abData, lROMSize);
  printf("%s: %d bytes loaded, ROMsize=%d, checksum=0x%02X%02X\n", szOutputFile, cbData, lROMSize, bCheckH, bCheckL);
  return 0;
}
