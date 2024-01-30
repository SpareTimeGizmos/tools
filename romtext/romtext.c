//++
//romtext
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
//    This program will convert a plain ASCII text file into an Intel format
// HEX file.  It's pretty simple - the only real nifty things it does are to
// allow you to specify the starting address for the HEX file image, and to
// ingore comments in the source file.
//
// USAGE:
//  romtext [-annnn] input-file output-file ...
//
//	     -annnn - set the address of the output image
//
// REVISION HISTORY
// 22-Feb-06	RLA	New file...
// 21-Mar-23    RLA     When reading DOS text files on Linux, they already
//                        end with \r\n - don't add another <CR>!
//--
#include <stdio.h>		// printf(), scanf(), et al.
#include <stdlib.h>		// exit(), ...
#include <stdint.h>             
#include <malloc.h>		// malloc(), _fmalloc(), etc...
#include <memory.h>		// memset(), etc...
#include <string.h>

#define ROMSIZE	((unsigned) 65535)	// largest file we can convert!
#define MAXLINE 512			// longest line possible

typedef unsigned char uchar;

// Globals...
uint16_t uROMAddress;		// offset of the EPROM in memory



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
  FILE      *fOutput,	        // handle of the .HEX file to be written
  uint8_t   *pbData,		// array of bytes to be saved
  uint32_t   cbData,		// number of bytes to write
  uint16_t   uOffset)		// offset applied to input records
{
  uint16_t   uAddress = 0;	// address of the current record
  uint16_t   cbRecord;  	// size of the current record
  uint16_t   uRecordAddress;
  uint16_t   uChecksum;	        // checksum "  "     "       "
  uint16_t   i;			// temporary...


  while (cbData > 0) {
    cbRecord = (cbData > 16) ? 16 : (unsigned) cbData;
    uRecordAddress = (uAddress+uOffset) & 0xFFFF;
    fprintf(fOutput, ":%02X%04X00", cbRecord, uRecordAddress);
    uChecksum = cbRecord + (uRecordAddress >> 8) + (uRecordAddress & 0xFF) + 00 /* Type */;
    for (i = 0; i < cbRecord; ++i) {
      fprintf(fOutput,"%02X", *(pbData+uAddress+i));
      uChecksum += *(pbData+uAddress+i);
    }
    fprintf(fOutput,"%02X\n", (-uChecksum) & 0xFF);
    cbData -= cbRecord;  uAddress += cbRecord;
  }
  
  fprintf(fOutput, ":00000001FF\n");
}


//++
//   This function parses the command line and initializes all the global
// variables accordingly.  It's tedious, but fairly brainless work.  Note
// that NO file names are required - if none are supplied, stdin and stdout
// are used instead...
//--
int ParseOptions (int argc, char *argv[])
{
  int nArg;  char *psz;

  // First, set all the defaults...
  uROMAddress = 0;

  // If there are no arguments, then just print the help and exit...
  if (argc == 1) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr,"romtext [-annnn] [input-file] [output-file]\n");
    fprintf(stderr,"\t-annnn - set the address of the ROM image\n");
    exit(EXIT_SUCCESS);
  }

  for (nArg = 1;  nArg < argc;  ++nArg) {
    // If it doesn't start with a "-" character, then it must be a file name.
    if (argv[nArg][0] != '-') return nArg;

    // Handle the -a (address) option...
    if (strncmp(argv[nArg], "-a", 2) == 0) {
      uROMAddress = (unsigned) strtoul(argv[nArg]+2, &psz, 10);
      if ((*psz == 'x') && (uROMAddress == 0))
        uROMAddress = (unsigned) strtoul(psz+1, &psz, 16);
      if ((*psz != '\0') || (uROMAddress > 0xFFFF)) {
        fprintf(stderr, "romtext: illegal address: \"%s\"", argv[nArg]);
        exit(EXIT_FAILURE);
      }
      continue;
    }

    // Otherwise it's an illegal option...
    fprintf(stderr, "romtext: unknown option - \"%s\"\n", argv[nArg]);
    exit(EXIT_FAILURE);
  }

  return nArg;  
}


//++
// ReadText
//--
unsigned ReadText (FILE *fInput, uint8_t *pbData, uint16_t uMaxSize)
{
  char szLine[MAXLINE];  uint16_t uTextSize = 0;
  
  while (fgets(szLine, MAXLINE, fInput) != NULL) {
    char *psz;  int len = strlen(szLine);
    // Ignore comments...
    if ((len > 0) && (szLine[0] == '#')) continue;
    // Convert the line ending to <CRLF> regardless of what we read...
    if ((len > 0) && (szLine[len-1] == '\n')) szLine[--len] = 0;
    if ((len > 0) && (szLine[len-1] == '\r')) szLine[--len] = 0;
    strcat(szLine, "\r\n");  len += 2;
    // Append the line to the buffer ...
    for (psz = szLine;  *psz != 0;  ++psz) {
      if (uTextSize++ < uMaxSize) *pbData++ = *psz;
    }
  } 
  // Always end with a null byte!
  if (uTextSize++ < uMaxSize) *pbData++ = 0;
  return uTextSize;
}


//++
//main
//--
int main (int argc, char *argv[])
{
  uint16_t uTextSize;  FILE *fInput, *fOutput;
  char *pszInput;  uint8_t *pbData;  int nArg;
 
  nArg = ParseOptions(argc, argv);
  //fprintf(stderr,"Start Address = %u (0x%04x)\n", uROMAddress, uROMAddress);
  
  //   If either the input file, the output file, or both is absent then
  // they default to stdin and stdout.  If only one is present, then its
  // the input file...
  if (nArg <argc) {
    //fprintf(stderr,"Input file = %s\n", argv[nArg]);
    fInput = fopen(argv[nArg], "r");  pszInput = argv[nArg];
    if (fInput == NULL) {
      fprintf(stderr,"romtext: can't read %s\n", argv[nArg]);
      exit(EXIT_FAILURE);
    }
    ++nArg;
  } else {
    fInput = stdin;  pszInput = "";
  }
  if (nArg < argc) {
    //fprintf(stderr,"Output file = %s\n", argv[nArg]);
    fOutput = fopen(argv[nArg], "w");
    if (fOutput == NULL) {
      fprintf(stderr,"romtext: can't write %s\n", argv[nArg]);
      exit(EXIT_FAILURE);
    }
    ++nArg;
  } else
    fOutput = stdout;

  // If there are any arguments left, it's an error...
  if (nArg < argc) {
    fprintf(stderr,"romtext: extra arguments \"%s\"\n", argv[nArg]);
    exit(EXIT_FAILURE);
  }
      
  // Allocate a buffer to hold the ROM image ...
  pbData = malloc((size_t) ROMSIZE);
  if (pbData == NULL) {
    fprintf(stderr,"romtext: failed to allocate memory\n");
    exit(1);
  } 
  memset(pbData, 0xFF, (size_t) ROMSIZE);

  // Load the input file...
  //..
  uTextSize = ReadText(fInput, pbData, ROMSIZE);

  // Dump out the ROM image and we're all done...
  WriteHex(fOutput, pbData, uTextSize, uROMAddress);
  printf("%s: %u bytes from 0x%04X to 0x%04X\n",
    pszInput, uTextSize, uROMAddress, uROMAddress+uTextSize-1);

  exit(0);  
}
