//++                                                                    
// pdpfile.c
//                                                                      
//      Copyright (c) 1999 by Robert Armstrong.  All rights reserved.
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
// DESCRIPTION:
//   This module implements a loader for PDP-8 BIN format paper tape    
// images.  It's amazing to me that the DEC BIN loader, written in      
// PDP-8 assembly language, was so small given how messy the format     
// is!                                                                  
//                                                                      
// REVISION HISTORY:
// 02-JAN-00    RLA     Stolen from the Eight project...
//--                                                                    

#include <stdio.h>              // printf(), fprintf(), etc...
#include <stdint.h>		// uint16_t, uint8_t, etc ...
#include <stdbool.h>		// bool, true, false ...
#ifdef __linux__
#include <sys/io.h>             // _read(), _open(), et al...
#include <unistd.h>		// open(), read(), close(), etc ...
#else
#include <io.h>                 // _read(), _open(), et al...
#endif
#include <fcntl.h>              // _O_RDONLY, _O_BINARY, etc...
#include "pdp2hex.h"            // hex tools library declarations

#ifdef __linux__
#define O_BINARY	0
#endif

// Private variables global to this module...
PRIVATE int      hFile;         // handle, from _open_ of the BIN file
PRIVATE uint8_t  abBuffer[512]; // buffer for reading data blocks
PRIVATE int      cbBuffer;      // number of bytes in the buffer now
PRIVATE int      nBufPos;       // next byte in the buffer to be used
PRIVATE uint16_t wFrame;        // current tape frame
PRIVATE uint16_t nChecksum;     // checksum accumulator


//++ 
//   This function reads and returns the next byte from the BIN file.   
// If there are no more bytes, it returns FALSE.  It's fairly simple... 
//--
PRIVATE bool ReadPDPByte (uint8_t *pb)
{
  int Count;
  if (nBufPos >= cbBuffer) {
    Count = read(hFile, abBuffer, sizeof(abBuffer));
    if (Count <= 0) return false;
    cbBuffer = Count;  nBufPos = 0;
  }
  *pb = abBuffer[nBufPos++];
  return true;
}


//++ 
//   This routine reads the next frame from the BIN file, which is a    
// little more complex.  Since paper tape is only 8 bits wide, twelve   
// bit PDP-8 words are split up into two six bit bytes which together   
// are called a "frame".  The upper two bits of the first byte are used 
// as a code to describe the type of the frame.  The upper bits of the  
// second byte are, so far as I know, unused and are always zero.  This 
// function returns a 14 bit tape frame - 12 bits of data plus the type 
// bits from the first byte.  A complication is that the BIN checksum   
// accumulates the bytes on the tape, not the frames, so we have to     
// calculate it here.  The caller can't do it...                        
//--
PRIVATE bool ReadPDPFrame (uint16_t *pwFrame)
{
  uint8_t b;

  // Read the first byte - this is the high order part of the frame... 
  if (!ReadPDPByte(&b)) return false;
  *pwFrame = b << 6;
  
  //   Frame types 2 (leader/trailer) and 3 (field settings) are single 
  // bytes - there is no second data byte in these frames!  More over,  
  // these frame types are _not_ counted towards the checksum!          
  if ((*pwFrame & 020000) != 0) return true;
  
  // Frame types 0 (data) and 1 (address) are normal 12 bit data.... 
  nChecksum += b;
  if (!ReadPDPByte(&b)) return false;
  *pwFrame |= b & 077;  nChecksum += b;

  return true;
}


//++
//   This function loads one segment of a BIN format tape image.  Most  
// tapes have only one segment (i.e. <leader> <data> <trailer> <EOT>),  
// but a few (e.g. FOCAL69 with the INIT segment) are <leader> <data-1> 
// <trailer-1>/<leader-2> <data-2> ... <trailer-n> <EOT>.  When this    
// function is called the leader has already been skipped and the first 
// actual data frame is passed in wFrame. When we return, the data will 
// have been read and wFrame will contain a leader/trailer code.  The   
// big problem is the checksum, which looks just like a data frame.  In 
// fact, the only way we can tell that it is a checksum and not data is 
// because of its position as the very last frame on the tape.  This    
// means that every time we find a data frame, we have to look ahead at 
// the next frame to see whether it's a leader/trailer.  If it is, then 
// the current frame is a checksum. If it isn't, then the current frame 
// is data to be stored!                                                
//                                                                      
//   The return value is the number of data words stored in memory, or  
// -1 if the tape format is bad (e.g. a mismatched checksum).           
//--
PRIVATE int LoadPDPSegment (uint16_t *pwMemory, uint16_t cwMemory)
{
  uint16_t  nAddress = 00200;   // current load address (00200 default) 
  uint16_t  nCount   = 0;       // count of data words read and stored  
  uint16_t  wNextFrame;         // next tape frame, for look ahead      

  // Load tape frames until we hit the end!   
  while (true) {
    switch (wFrame & 030000) {

      case 000000:
        //   This is a data frame. It could be either data to be stored 
        // in memory, or a checksum.  The only way to know is to peek   
        // at the next frame...                                         
        if (!ReadPDPFrame(&wNextFrame)) return -1;
        if (wNextFrame == 020000) {
          //   This is the end of the tape, so this frame must be the   
          // checksum.  Make sure that it matches our value and then we 
          // are done loading.  One slight problem is that the checksum 
          // bytes shouldn't be added to the checksum accumulator, but  
          // ReadBINFrame() has already done it.  We'll subtract them 
          // back out to compensate...                                  
          nChecksum = (nChecksum - (wFrame >> 6) - (wFrame & 077)) & 07777;
          if (nChecksum != wFrame) return -1;
          return nCount;
        } else if (nAddress >= cwMemory) {
          // Real data, but the load address is invalid... 
          fprintf(stderr, "address %06o exceeds memory", nAddress);
          return -1;
        } else {
          // This is, as they say, the good stuff... 
          pwMemory[nAddress] = wFrame;  ++nAddress;  ++nCount;
        }
        wFrame = wNextFrame;
        // Don't read the next frame this time - we already did that! 
        continue;

      case 010000:
        //   Frame type 1 sets the loading origin.  The address is only 
        // twelve bits - the field can be set by frame type 3...        
        nAddress = (nAddress & 070000) | (wFrame & 07777);
        break;
        
      case 020000:
        //  Frame type 2 is leader/trailer codes, and these are usually 
        // handled under case 000000, above.  If we find one here it    
        // means that the tape didn't end with a data frame, and hence  
        // there was no checksum.  I don't think this ever happens in   
        // real life, so we'll consider it a bad tape...                
        return -1;
        
      case 030000:
        //  Type 3 frames set the loading field... 
        nAddress = (nAddress & 07777) | ((wFrame & 07000) << 3);
        break;
    }

    // Read the next tape frame and keep on going. 
    if (!ReadPDPFrame(&wFrame)) return -1;

  }
}


//++
//   This function will load an entire BIN tape, given its file name.  It
// returns TRUE if the file is loaded successfully and FALSE if there was
// some error.
//--
PUBLIC bool LoadPDP (char *lpszFileName, uint16_t *pwMemory, uint16_t cwMemory)
{
  uint16_t nSegments = 0;  // number of segments in this tape            
  uint16_t nWords    = 0;  //   "     " words     "   "    "             
  int      cwSegment;      // number of words loaded from this segment   
  uint8_t  b;              // temporary 8 bit data                       

  // Open the file (after giving it the default type, of course). 
  hFile = open(lpszFileName, O_RDONLY | O_BINARY);
  if (hFile == -1) {
    fprintf(stderr, "%s: unable to read file\n", lpszFileName);
    return false;
  }
  cbBuffer = nBufPos = 0;

  //   Some tape images begin with the name of the actual name of the   
  // program, in plain ASCII text.  Apparently the real DEC BIN loader  
  // ignores anything before the start of the leader, so we'll do the   
  // same...                                                            
  do
    if (!ReadPDPByte(&b)) return -1;
  while (b != 0200);

  while (true) {
    nChecksum = 0;

    // Skip the leader and find the first data frame... 
    do
      if (!ReadPDPFrame(&wFrame)) {
        fprintf(stderr, "%s: %u segments, %u words loaded\n", lpszFileName, nSegments, nWords);
        close(hFile);
        return true;
      }
    while (wFrame == 020000);
  
    // Load this segment of the tape... 
    cwSegment = LoadPDPSegment(pwMemory, cwMemory);  ++nSegments;
    if (cwSegment <= 0) {
      fprintf(stderr,"%s: error loading segment %d\n", lpszFileName, nSegments);
      return 0;
    }
    nWords += cwSegment;
  }
}
