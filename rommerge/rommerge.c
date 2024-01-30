//++
//rommerge
//
//      Copyright (C) 2004-2024 By Spare Time Gizmos, Milpitas CA.
//
//   This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
//   You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// DESCRIPTION:
//   This program will merge multiple Intel .HEX files and write a new .HEX
// file for the resulting ROM image.  One side effect of this program is that
// it also fills unused bytes in the ROM image, and writes these fillers out
// to the new HEX file.  Normally the filler is 0x00, but an alternate value
// may be specified on the command line.
//
// USAGE:
//  rommerge [-snnnn] [-onnnn] [-fnn] output-file input-file-1 input-file-2 input-file-3 ...
//
//	-snnnn - set the ROM size in bytes (e.g. -s32k or -s32768)
//      -onnnn - set the offset for the output image
//	         (e.g. -o32k starts the output image at 0x8000)
//	-fnn   - set the filler byte to nn decimal (e.g. -f0 or -f255)
//
// NOTE:
//   This program will work for ROMs up to 64K, which requires that longs
// be used to sizes and counts...
//
// REVISION HISTORY
// 24-Feb-05	RLA	New file...
//--
#include <stdio.h>		// printf(), scanf(), et al.
#include <stdlib.h>		// exit(), ...
#include <stdint.h>             // uint8_t, uint16_t, etc ...
#include <malloc.h>		// malloc(), _fmalloc(), etc...
#include <memory.h>		// memset(), etc...
#include <string.h>

typedef unsigned char uchar;

// Globals...
uint32_t  lROMSize;     // size of the ROM, in bytes (e.g. 65536)
uint8_t   uFillByte;	// filler value for unused ROM locations
uint16_t  uROMOffset;	// offset of the EPROM in memory
uint8_t  *pbData;	// pointer to the ROM image buffer
uint32_t lByteCount;	// count of bytes loaded from the .HEX file
char    *szOutputFile;	// output file name



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
// be read!
//--
long ReadHex (		// returns count of bytes read, or zero if error
  char      *pszName,	// name of the .HEX file to be loaded
  uint8_t   *bData,	// this array receives the data loaded
  uint32_t   lSize,	// maximum size of the array, in bytes
  uint16_t   uOffset)	// offset applied to input records
{
  FILE	   *f;		// handle of the input file
  uint32_t  lCount = 0;	// number of bytes loaded from file
  unsigned  cbLength;	// length       of the current .HEX record
  unsigned  uAddress;	// load address "   "     "      "     "
  unsigned  uType;	// type         "   "     "      "     "
  unsigned  uChecksum;	// checksum     "   "     "      "     "
  unsigned  uByte;	// temporary for the data byte read...
  
  if ((f=fopen(pszName, "rt")) == NULL)
    {fprintf(stderr,"%s: unable to open file\n", pszName);  return 0;}

  while (1) {
    if (fscanf(f,":%2x%4x%2x",&cbLength,&uAddress,&uType) != 3)
      {fprintf(stderr,"%s: bad .HEX file format (1)\n", pszName);  return 0;}
    if (uType > 1)
      {fprintf(stderr,"%s: unknown record type\n", pszName);  return 0;}
    uChecksum = cbLength + (uAddress >> 8) + (uAddress & 0xFF) + uType;
    for (;  cbLength > 0;  --cbLength, ++uAddress, ++lCount) {
      if (fscanf(f,"%2x",&uByte) != 1)
        {fprintf(stderr,"%s: bad .HEX file format (2)\n", pszName);  return 0;}
      if ((uint32_t)((uAddress - uOffset) & 0xFFFF) >= lSize)
        {fprintf(stderr,"%s: address %04X outside ROM\n", pszName, uAddress);  return 0;}
      if (pbData[(uAddress-uOffset) & 0xFFFF] == uFillByte) 
        pbData[(uAddress-uOffset) & 0xFFFF] = (uchar) uByte;
      else if (uByte != uFillByte) {
        printf("%s: conflict at address 0x%04X\n", pszName, uAddress);  return 0;
      }
      uChecksum += uByte;
    }
    if (fscanf(f,"%2x\n",&uByte) != 1)
      {fprintf(stderr,"%s: bad .HEX file format (3)\n", pszName);  return 0;}
    uChecksum = (uChecksum + uByte) & 0xFF;
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
void WriteHex (
  char      *pszName,	// name of the .HEX file to be written
  uint8_t   *pbData,	// array of bytes to be saved
  long       lCount,	// number of bytes to write
  uint16_t   uOffset)	// offset applied to input records
{
  FILE *f;		// handle of the output file
  unsigned uAddress = 0;// address of the current record
  unsigned cbRecord;	// size of the current record
  unsigned uRecordAddress;
  int      uChecksum;	// checksum "  "     "       "
  unsigned i;		// temporary...

  if ((f=fopen(pszName, "wt")) == NULL)
    {fprintf(stderr,"%s: unable to write file\n", pszName);  return;}

  while (lCount > 0) {
    cbRecord = (lCount > 16) ? 16 : (unsigned) lCount;
    uRecordAddress = (uAddress+uOffset) & 0xFFFF;
    fprintf(f, ":%02X%04X00", cbRecord, uRecordAddress);
    uChecksum = cbRecord + (uRecordAddress >> 8) + (uRecordAddress & 0xFF) + 00 /* Type */;
    for (i = 0; i < cbRecord; ++i) {
      fprintf(f,"%02X", *(pbData+uAddress+i));
      uChecksum += *(pbData+uAddress+i);
    }
    fprintf(f,"%02X\n", (-uChecksum) & 0xFF);
    lCount -= cbRecord;  uAddress += cbRecord;
  }
  
  fprintf(f, ":00000001FF\n");  fclose(f);
}


//++
//   This function parses the command line and initializes all the global
// variables accordingly.  It's tedious, but fairly brainless work.  All
// options, if there are any, are required to be first on the command line.
// The first non-option parameter is the output file name, and all parameters
// after that are input files.  This routine parses just the options and it
// returns the index of the output file name.
//--
int ParseOptions (int argc, char *argv[])
{
  int nArg;  char *psz;

  // First, set all the defaults...
  lROMSize = 65536;  uROMOffset = 0;  uFillByte = 0xFF;

  // If there are no arguments, then just print the help and exit...
  if (argc == 1) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr,"rommerge [-snnnn] [-onnnn] [-fnn] output-file input-file-1 input-file-2 ...\n");
    fprintf(stderr,"\t-snnnn - set the ROM size in bytes (e.g. -s32k or -s32768)\n");
    fprintf(stderr,"\t-onnnn - set the offset for the output image (e.g. -o32768)\n");
    fprintf(stderr,"\t-fnn   - set the filler byte to nn decimal (e.g. -f0 or -f255)\n");
    exit(EXIT_SUCCESS);
  }

  for (nArg = 1;  nArg < argc;  ++nArg) {
    // If it doesn't start with a "-" character, then it must be a file name.
    if (argv[nArg][0] != '-') {
      //   At least two MORE arguments - this output file and two input files - are
      // still required!
      if (nArg+2 > argc) {
        fprintf(stderr,"rommerge: not enough file names\n");
        exit(EXIT_FAILURE);
      }
      return nArg;
    }

    // Handle the -o (offset) option...
    if (strncmp(argv[nArg], "-o", 2) == 0) {
      uROMOffset = (unsigned) strtoul(argv[nArg]+2, &psz, 10);
      if (*psz == 'k' || *psz == 'K')   uROMOffset <<= 10, ++psz;
      if ((*psz != '\0') || (uROMOffset > 0xFFFF)) {
        fprintf(stderr, "rommerge: illegal offset: \"%s\"", argv[nArg]);
        exit(EXIT_FAILURE);
      }
      continue;
    }

    // Handle the -s (ROM size) option...
    if (strncmp(argv[nArg], "-s", 2) == 0) {
      lROMSize = strtoul(argv[nArg]+2, &psz, 10);
      if (*psz == 'k' || *psz == 'K')   lROMSize <<= 10, ++psz;
      if (*psz != '\0') {
        fprintf(stderr,"rommerge: invalid ROM size \"%s\"\n", argv[nArg]);
        exit(EXIT_FAILURE);
      }
      continue;
    }

    // Handle the -f (fill byte) option...
    if (strncmp(argv[nArg], "-f", 2) == 0) {
      uFillByte = (unsigned) strtol(argv[nArg]+2, &psz, 10);
      if (*psz != '\0') {
        fprintf(stderr,"rommerge: invalid fill byte \"%s\"\n", argv[nArg]);
        exit(EXIT_FAILURE);
      }
      continue;
    }

    // Otherwise it's an illegal option...
    fprintf(stderr, "rommerge: unknown option - \"%s\"\n", argv[nArg]);
    exit(EXIT_FAILURE);
  }
  
  // If we get here, there's no file name...
  fprintf(stderr,"rommerge: specify a file name\n");
  exit(EXIT_FAILURE);
}


//++
//main
//--
int main (int argc, char *argv[])
{
  int nFile;  uint32_t i;
 
  nFile = ParseOptions(argc, argv);
  szOutputFile = argv[nFile++];
  //fprintf(stderr,"Output file = %s\n", szOutputFile);
  //fprintf(stderr,"ROM Size        = %ld (0x%05lx)\n", lROMSize, lROMSize);
  //fprintf(stderr,"Fill Byte       = %u   (0x%02x)\n", uFillByte, uFillByte);
  //fprintf(stderr,"ROM Offset      = %u (0x%05x)\n", uROMOffset, uROMOffset);
 
  // Allocate a buffer to hold the ROM image and fill it with the filler value.
  pbData = malloc((size_t) lROMSize);
  if (pbData == NULL) {
    fprintf(stderr,"rommerge: failed to allocate memory\n");
    exit(1);
  }
  for (i = 0;  i < lROMSize;  ++i)  pbData[i] = uFillByte;

  // Load all the input files...
  for (lByteCount = 0; nFile < argc;  ++nFile) {
    long lBytes;
    lBytes = ReadHex(argv[nFile], pbData, lROMSize, uROMOffset);
    printf("%s: %ld bytes read\n", argv[nFile], lBytes);
    lByteCount += lBytes;
  }
  if (lByteCount == 0)  exit(1);

  // Dump out the new ROM image and we're all done...
  WriteHex(szOutputFile, pbData, lROMSize, uROMOffset);
  printf("%s: %d bytes written\n", szOutputFile, lByteCount);

  return 0;  
}
