//++
// mkdltxt.c
//
// Copyright (C) 2001 by Robert Armstrong.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
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
//   This program will dump out .VMD or .IDE files in a format acceptable to
// the BTS6120 monitor's DL and RL commands.  VMD files are VM01 RAMDISK
// images and IDE files are ID01 IDE disk images from the WinEight emulator.
// The BTS6120 RL command allows RAM disk images to be downloaded over the
// console serial port and the DL command allows IDE disk partitions to be
// downloaded in the same way, thus you can use WinEight to build a disk image
// and then transfer it to the real hardware.
//
//  Each VM01 RAMDISK page (the SBC6120 deals with RAM disk in 128 word pages
// rather than 256 word OS/8 blocks) is dumped in this format:
//
//      pppp.ooo/ dddd dddd dddd ... dddd
//      pppp.ooo/ ....
//      ....
//      cccc
//
//   Each record consists of a disk address followed by exacly eight words of
// data, and then each group of sixteen records is followed by a checksum of
// the entire 128 word block.  All values are in octal.
//      
//   ID01 IDE images are nearly the same, except that disk blocks contain a
// full 256 twelve bit words.  mkdltxt recognizes the type of input file by
// the extension - .VMD files for RAMDISK and .IDE files for IDE disk.
//
//   The output of this program is simply a text file, written to standard
// output, which then has to be transferred to the SBC6120 using a terminal
// emulator.  To avoid dropping characters while the 6120 processes each
// record, you'll have to program your terminal emulator to insert a delay at
// the end of each line to give the 6120 time to catch up.  BTS6120 prompts
// for each record during a download with a ":" character - some terminal
// emulator programs, like Kermit, allow a prompting character to be specified
// for plain text downloads.  This will save a lot of time over a fixed delay.
//
// EDIT HISTORY
// ------------
// 28-Apr-00    RLA     New file.  
//  4-Mar-10	RLA		Additions to support the SBC6120-RC model...
//--
#include <stdio.h>      // printf(), et al...
#include <stdlib.h>     // exit(), EXIT_FAILURE, etc...
#include <string.h>     // strcmp()...
#include <fcntl.h>      // _O_BINARY, etc...
#include <sys\stat.h>   // _S_IREAD, ...
#include <io.h>         // _read(), _open(), _close(), etc...


//   RAM disk "geometry" parameters.  These must agree with both the WinEight
// emulator AND the BTS6120 ROM monitor if you expect anything to work!
#define VM01_BANKS_PER_DISK      128    // number of banks per 512Kb SRAM "disk"
#define VM01_BANK_SIZE          4096    // size of each RAM disk "bank", in bytes
#define VM01_PAGES_PER_BANK       21    // 128 word PDP-8 memory pages per bank
#define VM01_BYTES_PER_PAGE      192    // number of bytes to hold a 128 word page
#define VM01_SECTOR_SIZE         128    // number of twelve bit words per "sector" 
#define VM01_RC_SIZE			3584	// size of the SBC6120-RC RAM disk, in pages

//   IDE disk "geometry" parameters.  WinEight doesn't actually emulate an
// entire IDE disk but only a single partition of exacty 4096 blocks.
#define ID01_PARTITION_SIZE     4096    // size (in OS/8 blocks) of each partition
#define ID01_SECTOR_SIZE         256    // size (in words) of each IDE sector

// Other useful definitions...
typedef unsigned int   UINT;            // 16 bits of data, unsigned
typedef unsigned char  UCHAR;           //  8  "    "   "      "


//   This routine converts a page, 192 bytes, of data into the corresponding
// twelve bit words and then writes it to standard output in the BTS6120
// format.  Note that the eight bit to twelve conversion used here is the
// conventional OS/8 "three for two" method - this must agree with the method
// used by both the WinEight emulator and the SBC6120 ROM!
void DumpPage (UINT nPage, UCHAR *pabBuffer)
{
  UINT nWord, nByte, nChecksum;
  
  for (nWord = nByte = nChecksum = 0;  nWord < VM01_SECTOR_SIZE;  nWord += 2) {
    UINT w1 = pabBuffer[nByte++];
    UINT w2 = pabBuffer[nByte++];
    w1 |= (pabBuffer[nByte] & 0xF) << 8;
    w2 |= (pabBuffer[nByte++] & 0xF0) << 4;
    nChecksum = (nChecksum + w1 + w2) & 07777;
    if ((nWord & 7) == 0) {
      if (nWord > 0) printf("\n");
      printf("%04o.%03o/ ", nPage, nWord);
    }
    printf("%04o %04o ", w1, w2);
  }
  
  printf("\n%04o\n", nChecksum);
}      


//   This routine converts 512 bytes of data into the corresponding block of 
// 256 twelve bit words and then writes it to standard output in the BTS6120
// format.  The eight to twelve bit conversion used in this instance is simply
// a matter of discarding the upper four bits of every sixteen bit word - 
// exactly what's done by the SBC6120 hardware.
void DumpBlock (UINT nBlock, UINT *pawBuffer, UINT cwBuffer)
{
  UINT nWord, nChecksum;
  
  for (nWord = nChecksum = 0;  nWord < cwBuffer;  nWord++) {
    nChecksum = (nChecksum + pawBuffer[nWord]) & 07777;
    if ((nWord & 7) == 0) {
      if (nWord > 0) printf("\n");
      printf("%04o.%03o/ ", nBlock, nWord);
    }
    printf("%04o ", pawBuffer[nWord]);
  }
  
  printf("\n%04o\n", nChecksum);
}


//   This routine will dump an entire VM01 RAMDISK image...
int DumpVM01 (int fdInput)
{
  UCHAR abBank[VM01_BANK_SIZE];  UINT nCount, nPage, i;
  for (nPage = 0;  nPage < VM01_PAGES_PER_BANK*VM01_BANKS_PER_DISK; ) {
    nCount = _read(fdInput, abBank, sizeof(abBank));
    if (nCount != sizeof(abBank)) return 0;
    for (i = 0;  i < VM01_PAGES_PER_BANK;  ++i, ++nPage)
      DumpPage (nPage, abBank + (i * VM01_BYTES_PER_PAGE));
  }
  return -1;
}
  
  
// This routine will dump an entire ID01 IDE disk image...
int DumpID01 (int fdInput)
{
  UINT awSector[ID01_SECTOR_SIZE];  UINT nBlock, nCount;
  for (nBlock = 0;  nBlock < ID01_PARTITION_SIZE;  ++nBlock) {
    nCount = _read(fdInput, awSector, sizeof(awSector));
    if (nCount != sizeof(awSector)) return 0;
    DumpBlock (nBlock, awSector, ID01_SECTOR_SIZE);
  }
  return -1;
} 

// This routine will dump an SBC6120-RC RAM disk image ...
int DumpRC (int fdInput)
{
  UINT awSector[VM01_SECTOR_SIZE];  UINT nPage, nCount;
  for (nPage = 0;  nPage < VM01_RC_SIZE;  ++nPage) {
    nCount = _read(fdInput, awSector, sizeof(awSector));
    if (nCount != sizeof(awSector)) return 0;
    DumpBlock (nPage, awSector, VM01_SECTOR_SIZE);
  }
  return -1;
} 


int main (int argc, char *argv[])
{
  int fdInput;  char szType[_MAX_EXT];
  
  // Make sure there's always exactly one argument...
  if (argc != 2) {
    fprintf(stderr,"usage: mkdltxt <file>\n");
    return EXIT_FAILURE;
  }

  // Try to open the input file...
  fdInput = _open(argv[1], _O_BINARY | _O_RDONLY, _S_IREAD);
  if (fdInput == -1) {
    fprintf(stderr,"mkdltxt: unable to read %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  // Figure out the type of the input file...
  _splitpath (argv[1], NULL, NULL, NULL, szType); 
  if (stricmp(szType, ".vmd") == 0) {
    if (!DumpVM01(fdInput)) {
      fprintf(stderr,"mkdltxt: error reading %s\n", argv[1]);
      return EXIT_FAILURE;
    }
  } else if (stricmp(szType, ".vmw") == 0) {
    if (!DumpRC(fdInput)) {
      fprintf(stderr,"mkdltxt: error reading %s\n", argv[1]);
      return EXIT_FAILURE;
    }
  } else if (stricmp(szType, ".ide") == 0) {
    if (!DumpID01(fdInput)) {
      fprintf(stderr,"mkdltxt: error reading %s\n", argv[1]);
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr,"mkdltxt: unknown file type %s\n", argv[1]);
    return EXIT_FAILURE;
  }
  
  _close(fdInput);
  return EXIT_SUCCESS;
}
  
