//++
// pdp2hex.h
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
//--
#ifndef _PDP2HEX_H_
#define _PDP2HEX_H_

// Compilation parameters...
#define PDP_MEM_SIZE 32768      // maximum size of PDP-8 memory, ever!

// Handy macros for assembling larger values from small ones...
#define HIBYTE(x)  ((uint8_t) (((x) >> 8) & 0xFF))
#define LOBYTE(x)  ((uint8_t) ((x) & 0xFF))

// Standard MAX() and MIN() macros...
#define MAX(a,b) ( (a) < (b) ? (b) : (a) )
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )

// Error macros ...
#define FAIL(pgm,msg)                   \
  {fprintf(stderr, pgm ": " msg "\n");  exit(EXIT_FAILURE);}
#define FAIL1(pgm,msg,arg)              \
  {fprintf(stderr, pgm ": " msg "\n", arg);  exit(EXIT_FAILURE);}
#define WARN(pgm,msg)                   \
  fprintf(stderr, pgm ": " msg "\n");

// Modifiers for public and private (at the module level) declarations...
#define PRIVATE static
#define PUBLIC

// External functions prototypes...
extern bool LoadPDP (char *lpszFileName, uint16_t *pwMemory, uint16_t cwMemory);
extern uint32_t LoadBinary (char *lpszFileName, uint8_t *pbMemory, uint32_t lSize);
extern bool DumpBinary (char *lpszFileName, uint8_t *pbMemory, uint32_t lSize);
extern uint32_t LoadHex (char *lpszFileName, uint8_t *pbMemory, uint32_t lOffset, uint32_t lSize);
extern bool DumpHex (char *lpszFileName, uint8_t *pbMemory, uint32_t lOffset, uint32_t lSize);
extern void SetFileType (char *lpszName, char *lpszType);
extern uint32_t LoadHexOrBinary (char *lpszName, uint8_t *pbMemory, uint32_t lOffset, uint32_t lSize);
extern bool DumpHexOrBinary (char *lpszName, uint8_t *pbMemory, uint32_t lOffset, uint32_t lSize);

#endif  // ifndef _PDP2HEX_H_
