//++
// pdp2hex.c
//
// Copyright (C) 2000 by Robert Armstrong.  All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANT-
// ABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
// Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, visit the website of the Free
// Software Foundation, Inc., www.gnu.org.
//
// DESCRIPTION:
//   This program converts a PDP-8 paper tape image, in the traditional DEC
// BIN loader format, into two binary image files suitable for burning into
// a pair of EPROMs.  The twelve bit PDP-8 words are split in half, with
// the lower six bits going into the lower six bits of one ROM and the upper
// six bits into the other ROM.  The two most significant bits of each EPROM
// are unused.
//
// Usage:
//      pdp2hex [-onnnn] [-snnnn] [-cnnnn] [-r] [-p] input-file low-file high-file
//      pdp2hex [-onnnn] [-snnnn] [-cnnnn] -m input-file output-file [high-file]
//                                                       
//      -onnnnn specifies the base address, in octal, where the two ROMs will
//      be mapped in PDP-8 address space.  This offset is subtracted from the
//      addresses of all data loaded.
//
//      -snnnn specifies the size of the EPROMs, in octal.
//
//      -cnnnn specifies that PDP2HEX should compute a twelve bit checksum for
//      the ROM image and store its two's complement at the ROM offset specified
//      by nnnn.  The PDP-8 program can then verify the ROM checksum simply by
//      summing all ROM words and testing that the result is zero.  If this
//      option is omitted no checksum is computed.
//
//      -r specifies that the bit order should be reversed - DX0 and DX6 become
//      the LSBs of the two ROMs rather than the MSBs.
//
//      -p specifies that the address order should be permuted as in the
//      SBC6100 model 1
//
//	-m outputs a .mem file suitable for use with Verilog $readmemh().  .
//
//   The output files may be any one of three formats -
//
//	Intel HEX file format (".hex")
//	raw binary format (".bin")
//	Verilog memory image (".mem")
//
// as determined by the file extension given.  Note that in the case of a
// Verilog memory image, either one or two output files may be given.  If two
// output files are used, the "high" file gets the upper 4 bits of each word
// and the "low" file the lower 8 bits.  If only one Verilog output file is
// used, then it receives all twelve bits.  All other output formats require
// two files.  Naturally the input file is always in PDP-8 BIN loader format.
//
//   Note that this program treats all PDP-8 memory as a 32K "flat" address
// space.  It has no special handling for fields, except what's needed to
// correctly load the BIN format tape images.
//
//   To put it all another way, this program loads the input file into a 32KW
// buffer that is an image of the PDP-8 memory.  All words between the ROM
// offset (the -o option) and the offset plus the ROM size (the -s option)
// are then split and dumped to the two BIN files.
//
// REVISION HISTORY
// 10-Feb-00    RLA             New file.
//--
#include <stdio.h>              // printf(), scanf(), etc...
#include <stdlib.h>             // malloc(), free(), exit(), ...
#include <stdint.h>		// uint16_t, uint8_t, etc ...
#include <stdbool.h>		// bool, true, false ...
#include <malloc.h>             // _halloc(), _hfree(), etc...
#include <string.h>             // strcmp(), etc...
#ifdef __linux__
#include <linux/limits.h>	// PATH_MAX ...
#endif
#include "pdp2hex.h"            // standard ROM utilities (e.g. LoadHex(), etc)


// Global variables...
char szInputFile[PATH_MAX];     // name of the input (PDP-8 BIN) file
char szHighFile [PATH_MAX];     // name of the high byte output file
char szLowFile  [PATH_MAX];     //   "   "   " low   "     "     "
uint16_t  nROMOffset;           // ROM address offset, if any
uint16_t  nROMSize;             // maximum ROM address
int16_t   nChecksumOffset;      // word to receive the checksum (-1 if none)
bool      fReverse;             // TRUE to reverse the bit order, FALSE otherwise
bool      fSBC6100;		// TRUE to prevert the bits for the SBC6100
bool      fVerilog;		// TRUE to output Verilog .mem files
uint16_t *pwPDP;                // PDP-8 memory image
uint8_t  *pbHigh, *pbLow;       // memory buffers for 8 bit ROM images


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
  szInputFile[0] = szHighFile[0] = szLowFile[0] = '\0';
  nROMOffset = 0;  nROMSize = PDP_MEM_SIZE;
  nChecksumOffset = -1;  fReverse = fSBC6100 = fVerilog = false;
  
  // If there are no arguments, then just print the help and exit...
  if (argc == 1) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr,"\tpdp2hex [-onnnn] [-snnnn] [-cnnnn] [-r] [-p] input-file low-file high-file\n");
    fprintf(stderr,"\tpdp2hex [-onnnn] [-snnnn] [-cnnnn] -m input-file output-file [high-file]\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Options:\n");
    fprintf(stderr,"\t-onnnnn\t- set ROM offset, in octal\n");
    fprintf(stderr,"\t-snnnnn\t- set ROM size, in octal\n");
    fprintf(stderr,"\t-cnnnnn\t- set checksum location, in octal\n");
    fprintf(stderr,"\t-r\t- reverse bit order\n");
    fprintf(stderr,"\t-p\t- SBC6100 model 1 addressing\n");
    fprintf(stderr,"\t-m\t- output in Verilog $readmemh() format\n");
    exit(EXIT_SUCCESS);
  }

  for (nArg = 1;  nArg < argc;  ++nArg) {
    // If it doesn't start with a "-" character, then it must be a file name.
    if (argv[nArg][0] != '-') {
           if (strlen(szInputFile) == 0) strcpy(szInputFile, argv[nArg]);
      else if (strlen(szLowFile)   == 0) strcpy(szLowFile, argv[nArg]);
      else if (strlen(szHighFile)  == 0) strcpy(szHighFile, argv[nArg]);
      else FAIL1("PDP2HEX", "too many files specified: \"%s\"", argv[nArg]);
      continue;
    }
    
    // Handle the -s (size) option...
    if (strncmp(argv[nArg], "-s", 2) == 0) {
      nROMSize = (uint16_t) strtoul(argv[nArg]+2, &psz, 8);
      if ((*psz != '\0')  ||  (nROMSize == 0))
        FAIL1("PDP2HEX", "illegal size: \"%s\"", argv[nArg]);
      continue;
    }
    
    // Handle the -o (offset) option...
    if (strncmp(argv[nArg], "-o", 2) == 0) {
      nROMOffset = (uint16_t) strtoul(argv[nArg]+2, &psz, 8);
      if (*psz != '\0')
        FAIL1("PDP2HEX", "illegal offset: \"%s\"", argv[nArg]);
      continue;
    }
    
    // Handle the -c (checksum) option...
    if (strncmp(argv[nArg], "-c", 2) == 0) {
      nChecksumOffset = (int) strtoul(argv[nArg]+2, &psz, 8);
      if ((*psz != '\0') || (nChecksumOffset == 0) || (nChecksumOffset > 7777))
        FAIL1("PDP2HEX", "illegal offset: \"%s\"", argv[nArg]);
      continue;
    }
    
    // Handle the -r (reverse) option...
    if (strcmp(argv[nArg], "-r") == 0) {
      fReverse = true;
      continue;
    }
    
    // Handle the -p (pervert) option...
    if (strcmp(argv[nArg], "-p") == 0) {
      fSBC6100 = true;
      continue;
    }
    
    // Handle the -m (Verilog .mem) option...
    if (strcmp(argv[nArg], "-m") == 0) {
      fVerilog = true;
      continue;
    }

    // Otherwise it's an illegal option...
    FAIL1("PDP2HEX", "unknown option - \"%s\"\n", argv[nArg]);
  }

  // Make sure all the file names were specified...
  if (fVerilog) {
    if (strlen(szLowFile) == 0)
      FAIL("PDP2HEX", "required file names missing");
  } else {
    if (strlen(szHighFile) == 0)
      FAIL("PDP2HEX", "required file names missing");
  }
}


//++
//   This routine will reverse the order of the bits in a SIXBIT value.  We
// need it because some unfortunate fool (me!) forgot that the PDP-8 reverses
// its bit order!
//--
uint8_t Reverse (uint8_t x)
{
  if (fReverse) {
    uint8_t b0 = (x   ) & 1;
    uint8_t b1 = (x>>1) & 1;
    uint8_t b2 = (x>>2) & 1;
    uint8_t b3 = (x>>3) & 1;
    uint8_t b4 = (x>>4) & 1;
    uint8_t b5 = (x>>5) & 1;
    return (b0<<5) | (b1<<4) | (b2<<3) | (b3<<2) | (b4<<1) | b5;
  } else
    return x;
}

//++
//   This routine fixes the address bits for the SBC6100 model 1.  It's
// here for the same reason we have the Reverse() routine!
//--
uint16_t FixSBC6100Address (uint16_t x)
{
  if (fSBC6100) {
    uint16_t b0 = (x   )  & 1;
    uint16_t b1 = (x>>1)  & 1;
    uint16_t b2 = (x>>2)  & 1;
    uint16_t b3 = (x>>3)  & 1;
    uint16_t b4 = (x>>4)  & 1;
    uint16_t b5 = (x>>5)  & 1;
    uint16_t b6 = (x>>6)  & 1;
    uint16_t b7 = (x>>7)  & 1;
    uint16_t b8 = (x>>8)  & 1;
    uint16_t b9 = (x>>9)  & 1;
    uint16_t b10= (x>>10) & 1;
    uint16_t b11= (x>>11) & 1;
    uint16_t b12= (x>>12) & 1;
    uint16_t b13= (x>>13) & 1;
    uint16_t b14= (x>>14) & 1;
    uint16_t w =  (b0 <<14) | (b1 <<13) | (b2 <<12) | (b3 <<11) | (b4 <<10)
                | (b5 << 9) | (b6 << 8) | (b7 << 7) | (b8 << 6) | (b9 << 5)
                | (b10<< 4) | (b11<< 3) | (b12<< 2) | (b13<< 1) |  b14;
    fprintf(stderr,"FixSBC6100Address() x=0x%04X w=0x%04X\n", x, w);
    return w;
  } else
    return x;
}


//++
//   This routine will compute the checksum of the ROMs as a two's complement
// twelve bit value.
//--
uint16_t CalculateChecksum (uint16_t *pwMemory, uint16_t nSize)
{
  int nChecksum = 0;  uint16_t i;
  for (i = 0;  i < nSize;  ++i)  nChecksum += pwMemory[i];
  return (uint16_t) ((-nChecksum) & 07777);
}


//++
//   Write a Verilog .mem file for the entire twelve bit word.  Verilog wants
// one line for every word of the simulated memory, and one hex value per line.
// It's pretty simple.
//--
void WriteOneMemFile (char *lpszFileName, uint16_t *pwMemory, uint16_t cwMemory)
{
  FILE *f;  uint16_t i;

  f = fopen(lpszFileName, "wt");
  if (f == NULL) {
    FAIL1("PDP2HEX", "unable to write %s", lpszFileName);
    return;
  }

  for (i = 0;  i < cwMemory;  ++i)
	  fprintf(f, "%03X\n", pwMemory[i]);

  fclose(f);
}

//++
//   Write the memory image split into two Verilog files - a "high" file
// containing the upper four bits of each word, and a "low" file containing
// the low eight bits.  
//--
void WriteTwoMemFiles (char *lpszLowFile, char *lpszHighFile, uint16_t *pwMemory, uint16_t cwMemory)
{
  FILE *fLow, *fHigh;  uint16_t i;

  fLow = fopen(lpszLowFile, "wt");
  if (fLow == NULL) {
    FAIL1("PDP2HEX", "unable to write %s", lpszLowFile);
    return;
  }
  fHigh = fopen(lpszHighFile, "wt");
  if (fHigh == NULL) {
    FAIL1("PDP2HEX", "unable to write %s", lpszHighFile);
    return;
  }

  for (i = 0;  i < cwMemory;  ++i) {
	fprintf(fLow, "%02X\n",  pwMemory[i]     & 0xFF);
	fprintf(fHigh, "%01X\n", (pwMemory[i]>>8) & 0x0F);
  }
  
  fclose(fLow);  fclose(fHigh);
}


//++
// The main program...
//--
int main (int argc, char *argv[])
{
  uint16_t n;
  
  // Parse the command line...
  ParseCommand(argc, argv);
  
  // Allocate memory for the three buffers used...
  pwPDP  = malloc(PDP_MEM_SIZE * sizeof(uint16_t));
  pbHigh = malloc(PDP_MEM_SIZE * sizeof(uint8_t));
  pbLow  = malloc(PDP_MEM_SIZE * sizeof(uint8_t));
  if ((pwPDP == NULL) || (pbHigh == NULL) || (pbLow == NULL))
    FAIL("PDP2HEX", "out of memory");

  // Load the input file...
  SetFileType(szInputFile, ".bin");
  if (!LoadPDP(szInputFile, pwPDP, PDP_MEM_SIZE)) return EXIT_FAILURE;
  
  //   If we want a checksum, then compute them now.  "Them", because each
  // field is checksummed separately.
  if (nChecksumOffset != -1) {
    uint16_t wChecksum, wBase;
    for (wBase = 0;  wBase < nROMSize;  wBase += 4096) {
      pwPDP[nChecksumOffset+wBase] = 0;
      wChecksum = CalculateChecksum(pwPDP+wBase, 4096);
      pwPDP[nChecksumOffset+wBase] = wChecksum;
      fprintf(stderr,"PDP2HEX: field %d checksum = %04o\n", wBase/4096, wChecksum);
    }
  }
  
  // If the "-m" option was given, write either 1 or 2 Verilog .mem files...
  if (fVerilog) {
    if (strlen(szHighFile) == 0) {
      SetFileType(szLowFile, ".mem");
      WriteOneMemFile(szLowFile, pwPDP, nROMSize);
    } else {
      SetFileType(szLowFile, ".mem");
      SetFileType(szHighFile, ".mem");
      WriteTwoMemFiles(szLowFile, szHighFile, pwPDP, nROMSize);
    }
    return EXIT_SUCCESS;
  }

  //   Split the PDP-8 memory image into two 8 bit ROM images and, at the same
  // time, check to see if there are any words loaded which don't fit into the
  // ROM specified.  This is just a precaution to make sure the user didn't
  // accidentally generate some code in his PDP-8 BIN file that's outside the
  // area occupied by the ROM.
  for (n = 0;  n < PDP_MEM_SIZE;  ++n) {
    if (((n<nROMOffset) || (n>(nROMOffset+nROMSize-1))) && (pwPDP[n]!=0))
      fprintf(stderr, "PDP2HEX: address %05o is used and outside the ROM image\n", n);
    pbHigh[FixSBC6100Address(n)] = Reverse((uint8_t) ((pwPDP[n] >> 6) & 077));
    pbLow[FixSBC6100Address(n)]  = Reverse((uint8_t) (pwPDP[n] & 077));
   }

  // Write the output files, release the buffers and we're done!  
  if (!DumpHexOrBinary(szHighFile, pbHigh+nROMOffset, 0, nROMSize)) return EXIT_FAILURE;
  if (!DumpHexOrBinary(szLowFile,  pbLow +nROMOffset, 0, nROMSize)) return EXIT_FAILURE;
  free(pwPDP);  free(pbHigh);  free(pbLow);
  return EXIT_SUCCESS;
}
