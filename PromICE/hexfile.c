//++                                                                    
//hexfile.c - Intel .hex file input and output routines
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
//   This module contains routines to read and write files in the standard
// Intel HEX format.  Only the 00 (data) and 01 (end of file) record types
// are recognized.
//
//      hexDump() - write a .HEX file from memory
//      hexLoad() - load a .HEX file into memory
//
// NOTE:
//   Although PromICE in general supports ROM sizes larger than 64K, this code
// won't handle HEX files with more than 16 bit addresses.  It can't - I don't
// know how the Intel standard deals with ROMs larger than 64K!
//                                                                      
// REVISION HISTORY:
// 04-APR-23    RLA     Stolen from the ROMCKSUM project...
//--
#include <stdio.h>              // printf(), FILE, etc ...
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>             // uint8_t, uint32_t, etc ...
#include <stdbool.h>            // bool, true, false, etc ...
#include <stdarg.h>             // va_arg, va_list, et al ...
#include <assert.h>             // assert() (what else??)
#include <memory.h>             // memset(), et al ...
#include "PromICE.h"            // global declarations for this project
#include "hexfile.h"            // declarations for this module


PUBLIC uint32_t hexLoad (
  const char *pszFile,      // name of the file to read
  uint8_t    *pabMemory,    // memory image to receive the data
  uint32_t    cbOffset,     // offset to be applied to addresses
  uint32_t    cbMemory)     // maximum size of the memory image
{
  //++
  //   This function will load a standard Intel format HEX file into memory.
  // It returns the number of bytes actually read from the file (which may not
  // be the same as the ROM size since, unlike a binary file, the bytes don't
  // have to be contiguous).  If any error occurs an error message is printed
  // and the program exits.
  //--
  FILE       *fpHex;        // handle of the file we're reading
  uint32_t    cbTotal;      // number of bytes loaded from file
  uint32_t    cbRecord;     // length       of the current .HEX record
  uint32_t    nRecAddr;     // load address "   "     "      "     "
  uint32_t    nType = 0;    // type         "   "     "      "     "
  uint32_t    nChecksum;    // checksum     "   "     "      "     "
  uint32_t    b;            // temporary for the data byte read...
  uint32_t    nLine;        // current line number (for error messages)
  
  // Open the hex file and, if we can't, then we can go home early...
  if ((fpHex=fopen(pszFile, "rt")) == NULL)
    FatalError("unable to read %s", pszFile);
  
  for (cbTotal = 0, nLine = 1;  nType != 1;  ++nLine) {
    // Read the record header - length, load address and record type...
    if (fscanf(fpHex, ":%2x%4x%2x", &cbRecord, &nRecAddr, &nType) != 3)
      FatalError("Intel format error (1) in file %s line %d", pszFile, nLine);

    // The only allowed record types (now) are 1 (EOF) and 0 (DATA)...
    if (nType > 1)
      FatalError("Intel unknown record type (0x%02X) in file %s line %d", nType, pszFile, nLine);
    
    // Begin accumulating the checksum, which includes all these bytes...
    nChecksum = cbRecord + HIBYTE(nRecAddr) + LOBYTE(nRecAddr) + nType;

    // Read the data part of this record and load it into memory...
    for (; cbRecord > 0;  --cbRecord, ++nRecAddr, ++cbTotal) {
      if (fscanf(fpHex, "%2x", &b) != 1)
        FatalError("Intel format error (2) in file %s line %d\n", pszFile, nLine);
      if ((nRecAddr+cbOffset < 0) || (nRecAddr+cbOffset >= cbMemory)) 
        FatalError("Intel address (0x%04X) out of range in file %s line %d\n", nRecAddr, pszFile, nLine);
      pabMemory[nRecAddr+cbOffset] = b;  nChecksum += b;
    }

    //   Finally, read the checksum at the end of the record.  That byte,
    // plus what we've recorded so far, should add up to zero if everything's
    // kosher.
    if (fscanf(fpHex, "%2x\n", &b) != 1)
      FatalError("Intel format error (3) in file %s line %d\n", pszFile, nLine);
    nChecksum = (nChecksum + b) & 0xFF;
    if (nChecksum != 0)
      FatalError("Intel checksum error (0x%02X) in file %s line %d", nChecksum, pszFile, nLine);
  }

  // Everything was fine...
  fclose(fpHex);
  return cbTotal;
}

PUBLIC bool hexDump (
  const char *pszFile,      // name of the file to read
  uint8_t    *pabMemory,    // memory image to receive the data
  uint32_t    cbOffset,     // offset to be applied to addresses
  uint32_t    cbMemory)     // maximum size of the memory image
{
  //++
  //   This procedure writes a memory dump in standard Intel HEX file format.
  // It's the logical inverse of LoadHex() and, like that routine, it's limited
  // to 64K maximum.  It returns FALSE if there's any kind of error while writing
  // the file.
  //--
  FILE       *fpHex;        // handle of the file we're reading
  uint32_t    cbRecord;     // length       of the current .HEX record
  uint32_t    nRecAddr;     // load address "   "     "      "     "
  int32_t     nChecksum;    // checksum     "   "     "      "     "
  uint32_t    nMemAddr;     // current memory address
  uint32_t    i;            // temporary...

  // Open the output file for writing...
  if ((fpHex=fopen(pszFile, "wt")) == NULL)
    FatalError("unable to write %s", pszFile);

  // Dump all of memory...
  for (nMemAddr = 0;  nMemAddr < cbMemory;  nMemAddr += cbRecord) {

    // Figure out the record size and address for that record.  
    nRecAddr = (uint32_t) (nMemAddr + cbOffset);
    cbRecord = 16;
    if (cbRecord > (cbMemory-nMemAddr))  cbRecord = (cbMemory-nMemAddr);

    // Write the record header (for a type 0 record)...
    fprintf(fpHex, ":%02X%04X00", cbRecord, nRecAddr); 
    
    // Start the checksum calculation...
    nChecksum = cbRecord + HIBYTE(nRecAddr) + LOBYTE(nRecAddr) + 00 /*nType*/;

    // And dump all the data bytes in this record.
    for (i = 0;  i < cbRecord;  ++i) {
      fprintf(fpHex, "%02X", pabMemory[nMemAddr+i]);
      nChecksum += pabMemory[nMemAddr+i];
    }

    // Write out the checksum byte and finish off this record.
    fprintf(fpHex,"%02X\n", (-nChecksum) & 0xFF);
  }

  //   Don't forget to write an EOF record, which is easy in our case, since it
  // doesn't contain any variable data.
  fprintf(fpHex, ":00000001FF\n");
  fclose(fpHex);
  return true;
}
