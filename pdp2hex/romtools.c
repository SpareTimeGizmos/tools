//++                                                                    
// romtools.c
//                                                                      
//   Copyright (C) 2000 by Robert Armstrong.  All rights reserved.
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License as
//   published by the Free Software Foundation; either version 2 of the
//   License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful, but
//   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANT-
//   ABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
//   Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, visit the website of the Free
//   Software Foundation, Inc., www.gnu.org.
//
// REVISION HISTORY:
// 02-JAN-00    RLA     Stolen from the ROMCKSUM project...
//--
#include <stdio.h>      	// printf(), fprintf(), etc...
#include <stdint.h>		// uint16_t, uint8_t, etc ...
#include <stdbool.h>		// bool, true, false ...
#include <stdlib.h>		// exit(), EXIT_SUCCESS, etc ...
#include <string.h>		// memset(), strcpy(), etc ...
#include <strings.h>		// strcasecmp(), ...
#include <malloc.h>		// malloc() (what else?)
#include <fcntl.h>      	// O_RDONLY, _O_TRUNC, etc...
#include <sys/stat.h>   	// S_IREAD, _S_IWRITE, ..
#ifdef __linux__
#include <sys/io.h>             // _read(), _open(), et al...
#include <unistd.h>		// read(), write(), open(), etc ...
#include <libgen.h>		// basename(), dirname(), ...
#include <linux/limits.h>	// PATH_MAX ...
#define O_BINARY 0
#define stricmp(str1,str2) strcasecmp(str1,str2)
#else
#include <io.h>                 // _read(), _open(), et al...
#endif
#include "pdp2hex.h"


//++
//   This routine will return the current extension (e.g. ".hex", ".bin", etc)
// of the file name specified ...
//--
PUBLIC void GetExtension(char *lpszName, char *lpszType)
{
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


//++
//   This routine will apply a default extension to a file that doesn't
// already have one...
//--
PUBLIC void SetFileType (char *lpszName, char *lpszType)
{
  char szExt[PATH_MAX];
  GetExtension(lpszName, szExt);
  if (strlen(szExt) == 0) strcat(lpszName, lpszType);
}


//++
//LoadHex
//
//   This function will load a standard Intel format HEX file into memory.
// It returns the number of bytes actually read from the file (which may not
// be the same as the ROM size since, unlike a binary file, the bytes don't
// have to be contiguous), or -1 if any error occurs.  In the latter case,
// an error message is also printed.
//--
PUBLIC uint32_t LoadHex (
  char    *lpszFile,            // name of the file to read
  uint8_t *pbMemory,            // memory image to receive the data
  uint32_t cbOffset,            // offset to be applied to addresses
  uint32_t cbMemory)            // maximum size of the memory image
{
  FILE     *fpHex;              // handle of the file we're reading
  uint32_t  cbTotal;            // number of bytes loaded from file
  int       cbRecord;           // length       of the current .HEX record
  int       nRecAddr;           // load address "   "     "      "     "
  int       nType;              // type         "   "     "      "     "
  int       nChecksum;    	// checksum     "   "     "      "     "
  int       b;                  // temporary for the data byte read...
  uint16_t  nLine;              // current line number (for error messages)
  
  // Open the hex file and, if we can't, then we can go home early...
  if ((fpHex=fopen(lpszFile, "rt")) == NULL) {
    fprintf(stderr, "%s: unable to read file\n", lpszFile);
    return -1;
  }
  
  for (cbTotal = 0, nLine = 1;  nType != 1;  ++nLine) {
    // Read the record header - length, load address and record type...
    if (fscanf(fpHex, ":%2x%4x%2x", &cbRecord, &nRecAddr, &nType) != 3) {
      fprintf(stderr, "%s: format error (1) in line %d\n", lpszFile, nLine);
      fclose(fpHex);  return -1;
    }

    // The only allowed record types (now) are 1 (EOF) and 0 (DATA)...
    if (nType > 1) {
      fprintf(stderr, "%s: unknown record type (0x%02X) in line %d\n", lpszFile, nType, nLine);
      fclose(fpHex);  return -1;
    }
    
    // Begin accumulating the checksum, which includes all these bytes...
    nChecksum = cbRecord + HIBYTE(nRecAddr) + LOBYTE(nRecAddr) + nType;

    // Read the data part of this record and load it into memory...
    for (; cbRecord > 0;  --cbRecord, ++nRecAddr, ++cbTotal) {
      if (fscanf(fpHex, "%2x", &b) != 1) {
        fprintf(stderr, "%s: format error (2) in line %d\n", lpszFile, nLine);
        fclose(fpHex);  return -1;
      }
      if ((nRecAddr+cbOffset < 0) || (nRecAddr+cbOffset >= cbMemory)) {
        fprintf(stderr, "%s: address (0x%04X) out of range in line %d\n", lpszFile, nRecAddr, nLine);
        fclose(fpHex);  return -1;
      }
      pbMemory[nRecAddr+cbOffset] = b;  nChecksum += b;
    }

    //   Finally, read the checksum at the end of the record.  That byte,
    // plus what we've recorded so far, should add up to zero if everything's
    // kosher.
    if (fscanf(fpHex, "%2x\n", &b) != 1) {
      fprintf(stderr, "%s: format error (3) in line %d\n", lpszFile, nLine);
      fclose(fpHex);  return -1;
    }
    nChecksum = (nChecksum + b) & 0xFF;
    if (nChecksum != 0) {
      fprintf(stderr, "%s: checksum error (0x%02X) in line %d\n",  lpszFile, nChecksum, nLine);
      fclose(fpHex);  return -1;
    }
  }

  // Everything was fine...
  fprintf(stderr, "%s: %d bytes loaded\n", lpszFile, cbTotal);
  fclose(fpHex);
  return cbTotal;
}


//++
//DumpHex
//
//   This procedure writes a memory dump in standard Intel HEX file format.
// It's the logical inverse of LoadHex() and, like that routine, it's limited
// to 64K maximum.  It returns FALSE if there's any kind of error while writing
// the file.
//--
PUBLIC bool DumpHex (
  char    *lpszFile,     // name of the file to read
  uint8_t *pbMemory,     // memory image to receive the data
  uint32_t cbOffset,     // offset to be applied to addresses
  uint32_t cbMemory)     // maximum size of the memory image
{
  FILE    *fpHex;        // handle of the file we're reading
  int      cbRecord;     // length       of the current .HEX record
  uint16_t nRecAddr;     // load address "   "     "      "     "
  int      nChecksum;    // checksum     "   "     "      "     "
  uint32_t nMemAddr;     // current memory address
  int      i;            // temporary...

  // Open the output file for writing...
  if ((fpHex=fopen(lpszFile, "wt")) == NULL) {
    fprintf(stderr, "%s: unable to write file\n", lpszFile);
    return false;
  }

  // Dump all of memory...
  for (nMemAddr = 0;  nMemAddr < cbMemory;  nMemAddr += cbRecord) {

    // Figure out the record size and address for that record.  
    nRecAddr = (uint16_t) (nMemAddr + cbOffset);
    cbRecord = 16;
    if ((uint32_t) cbRecord > (cbMemory-nMemAddr))  cbRecord = (int) (cbMemory-nMemAddr);

    // Write the record header (for a type 0 record)...
    fprintf(fpHex, ":%02X%04X00", cbRecord, nRecAddr); 
    
    // Start the checksum calculation...
    nChecksum = cbRecord + HIBYTE(nRecAddr) + LOBYTE(nRecAddr) + 00 /*nType*/;

    // And dump all the data bytes in this record.
    for (i = 0;  i < cbRecord;  ++i) {
      fprintf(fpHex, "%02X", pbMemory[nMemAddr+i]);
      nChecksum += pbMemory[nMemAddr+i];
    }

    // Write out the checksum byte and finish off this record.
    fprintf(fpHex,"%02X\n", (-nChecksum) & 0xFF);
  }

  //   Don't forget to write an EOF record, which is easy in our case, since it
  // doesn't contain any variable data.
  fprintf(fpHex, ":00000001FF\n");
  fprintf(stderr,"%s: %d bytes written\n", lpszFile, cbMemory);
  fclose(fpHex);
  return true;
}


//++
//LoadBinary
//
//   This function reads the memory image from a binary file.  It's not much
// harder than just calling _read(), but we have to handle opening the file
// and some special kludges for images larger than 32K.  It returns the number
// of bytes actually read, or -1 if any error occurs.
//--
PUBLIC uint32_t LoadBinary (
  char    *lpszFile,    // name of the file to read
  uint8_t *pbMemory,    // memory image to receive the data
  uint32_t cbMemory)    // maximum size of the memory image
{
  int      hFile;       // handle of the binary file
  uint32_t cbTotal;     // total number of bytes read
  int      cbRead;      // number of bytes read this time around
   
  // Open the file in binary, untranslated mode...
  if ((hFile = open(lpszFile, O_BINARY | O_RDONLY)) == -1) {
    fprintf(stderr, "%s: unable to read file\n", lpszFile);
    return -1;
  }
  
  //   We'd really love to just read the entire file in one single, simple,
  // operation, but in the 16 bit C RTL the _read() function can't read more
  // than 65534 bytes in a single call.  There's nothing we can do except to
  // split it up into several smaller reads.
  for (cbTotal = cbRead = 0;  (cbMemory > 0) && (cbRead > 0); ) {
    cbRead = 4096;
    if ((uint32_t) cbRead > cbMemory)  cbRead = (int) cbMemory;
    cbRead = read(hFile, pbMemory, cbRead);
    if (cbRead < 0)  {
      fprintf(stderr, "%s: error reading file\n", lpszFile);
      close(hFile);  return -1;
    }
    cbTotal += cbRead;  cbMemory -= cbRead;  cbMemory += cbRead;
  }

  // It's an error if we ran out of memory without finding the EOF...
  if ((cbMemory == 0) && (cbRead != 0)) {
    fprintf(stderr, "%s: too large for memory\n", lpszFile);
    close(hFile);  return -1;
  }
  
  // All done - close the file and return success...
  fprintf(stderr, "%s: %d bytes loaded\n", lpszFile, cbTotal);
  close(hFile);
  return cbTotal;
}


//++
// DumpBinary
//
//   This routine dumps a ROM image verbatim to a simple binary file.  It would
// be as simple as just calling _write(), but we've got to deal with opening the
// file and breaking up images larger than 32K.  It will return FALSE if there's
// any error while writing the file.
//--
PUBLIC bool DumpBinary (
  char    *lpszFile,    // name of the file to write
  uint8_t *pbMemory,    // memory image to receive the data
  uint32_t cbMemory)    // maximum size of the memory image
{
  int      hFile;       // handle of the binary file
  uint32_t cbTotal;     // total number of bytes read
  int      cbWrite;     // number of bytes written this time around
   
  // Open the file in binary, untranslated mode...
  hFile = open(lpszFile, O_BINARY|O_CREAT|O_TRUNC|O_WRONLY, 0666);
  if (hFile == -1) {
    fprintf(stderr, "%s: unable to write file\n", lpszFile);
    return false;
  }

  //   We have the same problem with writing that we did with reading, so we're
  // forced to write the file in several chunks...
  for (cbTotal = 0;  cbTotal < cbMemory;  ) {
    cbWrite = 123;
    if ((uint32_t) cbWrite > (cbMemory-cbTotal))  cbWrite = (int) (cbMemory-cbTotal);
    cbWrite = write(hFile, pbMemory, cbWrite);
    if (cbWrite <= 0)  {
      fprintf(stderr, "%s: error writing file\n", lpszFile);
      close(hFile);  return false;
    }
    cbTotal += cbWrite;  pbMemory += cbWrite;
  }

  // All done - close the file and return success...
  fprintf(stderr,"%s: %d bytes written\n", lpszFile, cbMemory);
  close(hFile);
  return true;
}


//++
//   This routine will attempt to figure out whether the file is in Intel Hex
// or raw binary format and load it accordingly.  It guesses the file's format
// based on the extension - .HEX for an Intel file and .BIN for binary.  If the
// file has no extension, then it will test to see if either a .HEX or .BIN
// file exists and go from there.  If neither a .HEX or .BIN file exists or
// the file has an unrecognized extension (.e.g ".TXT" or something) then an
// error message is printed and -1 is returned.
//--
PUBLIC uint32_t LoadHexOrBinary (char *lpszName, uint8_t *pMemory, uint32_t lOffset, uint32_t lSize)
{
  char szExt[PATH_MAX];  uint16_t cbName;
  GetExtension(lpszName, szExt);

  // If the user specified the file's type, then do what he said...
  if (stricmp(szExt, ".hex") == 0)
    return LoadHex(lpszName, pMemory, lOffset, lSize);
  if (stricmp(szExt, ".bin") == 0)
    return LoadBinary(lpszName, pMemory, lSize);
    
  // If the file has an unknown extension, then punt...
  if (strlen(szExt) > 0) {
    fprintf(stderr, "%s: unknown file type\n", lpszName);  return -1;
  }

  //   Otherwise, give the file a default extension of .HEX and see if that
  // actually exists.  If it doesn't, then try .BIN in the same way.
  cbName = strlen(lpszName);  strcat(lpszName, ".hex");
  if (access(lpszName, 0) != -1)
    return LoadHex(lpszName, pMemory, lOffset, lSize);
  lpszName[cbName] = '\0';  strcat(lpszName, ".bin");
  if (access(lpszName, 0) != -1)
    return LoadBinary(lpszName, pMemory, lSize);
    
  // The file is nowhere to be found...
  lpszName[cbName] = '\0';
  fprintf(stderr,"%s: can not find either .hex or .bin file\n", lpszName);
  return -1;
}


//++
//   This routine will write either an Intel Hex format file or a raw binary
// file depending on the extension of the filename - .HEX for Intel or .BIN
// for binary.  If the file has an extension but it isn't one of these two, 
// then we'll print an error message and quit.  If the file has no extension
// at all, then we'll default to writing a .HEX file...
//--
PUBLIC bool DumpHexOrBinary (char *lpszName, uint8_t *pMemory, uint32_t lOffset, uint32_t lSize)
{
  char szExt[PATH_MAX];
  GetExtension(lpszName, szExt);

  // If there's no extension, default to .HEX...
  if (strlen(szExt) == 0) {
    strcat(lpszName, ".hex");
    return DumpHex(lpszName, pMemory, lOffset, lSize);
  }

  //   Otherwise the file has an extension specified - if it's one we know how
  // to deal with, then do that.
  if (stricmp(szExt, ".hex") == 0)
    return DumpHex(lpszName, pMemory, lOffset, lSize);
  if (stricmp(szExt, ".bin") == 0)
    return DumpBinary(lpszName, pMemory, lSize);
  
  // If we don't recognize the extension, then punt...
  fprintf(stderr,"%s: specify either .hex or .bin\n", lpszName);
  return false;
}
