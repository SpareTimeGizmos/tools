//++
// protocol.h -> declarations for the GEI serial protocol
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
// REVISION HISTORY:
// 24-MAR-23  RLA   New file.
//--
#pragma once

// General protocol parameters ...
#define SHORT_TIMEOUT     100UL // standard timeout for most commands (100ms)
#define LONG_TIMEOUT     1000UL // long timeout for slow commands (1s)
#define CONNECT_TIMEOUT 20000UL // timeout for connecting (20s)
#define RAMTEST_TIMEOUT 30000UL // timeout for RAM TEST and FILL (30s)
#define RESET_LENGTH       55   // approximately 500ms

// "Special" PromICE commands - these are sent without the usual preamble ...
#define GEI_AUTOBAUD      0x03  // what we send to establish the baud rate
#define GEI_IDENTIFY      0x00  // return the number of units daisy chained

// Normal PromICE commands ...
#define	GEI_LOADPOINTER	  0x00	// load data pointer
#define	GEI_WRITEDATA	  0x01	// send RAM data to emulator
#define	GEI_READDATA	  0x02	// read RAM data from emulataor
#define	GEI_RESTART	  0x03	// restart PromICE
#define	GEI_SETMODE	  0x04	// set mode
#define GEI_SETSIZE       0x84  // set emulation size
#define	GEI_TESTRAM	  0x05	// test RAM
#define GEI_FILLRAM       0x15  // fill RAM with a constant
#define	GEI_RESETTARGET	  0x06	// reset target
#define	GEI_TESTPROMICE	  0x07	// test PromICE
#define	GEI_EXTENDED	  0x0E	// extended command
#define	GEI_READVERSION	  0x0F	// read version
#define GEI_READSERIAL    0x1F  // read serial number (not implemented?)

// Flag bits for the message command/response byte ...
#define GEI_CM_RESPONSE   0x80  // this message is a command response
#define GEI_CM_NORESPONSE 0x20  // don't respond to this command
#define GEI_CM_TARGETON   0x40  // target power in (response to GET VERSION)
#define GEI_CM_TARGETACT  0x20  // target active (response to GET VERSION)
#define GEI_CM_MASK       0x0F  // mask for message command bits

// Mode bits for the SET MODE command ...
#define GEI_MD_FASTXMIT 0x80    // don't slow the transmit speed
#define GEI_MD_SENDSTS  0x40    // send target status in each response
#define GEI_MD_TWOBYTES 0x20    // second mode data byte follows
#define GEI_MD_AUTORST  0x02    // enable target auto reset
#define GEI_MD_LOADMODE 0x01    // put the unit in load mode
#define GEI_MD_NOLIGHT  0x80    // disable RUN light (why?)
#define GEI_MD_NOTIMER  0x40    // disable internal timer

// Memory size codes ...
//   Note that converting a size code to the actual number of bytes is
// trivial - it's just (1024 << code).  Converting the other way would
// be equally easy IF you have a LOG2() function, but we don't.  You
// can get by with a right shift and test in a loop, but more work.
#define GEI_SIZE_2K     0x01    //    2K (   2048) bytes
#define GEI_SIZE_4K     0x02    //    4K (   4096) bytes
#define GEI_SIZE_8K     0x03    //    8K (   8192) bytes
#define GEI_SIZE_16K    0x04    //   16K (  16384) bytes
#define GEI_SIZE_32K    0x05    //   32K (  32768) bytes
#define GEI_SIZE_64K    0x06    //   64K (  65536) bytes
#define GEI_SIZE_128K   0x07    //  128K ( 131072) bytes
#define GEI_SIZE_256K   0x08    //  256K ( 262144) bytes
#define GEI_SIZE_512K   0x09    //  512K ( 524288) bytes
#define GEI_SIZE_1M     0x0A    // 1024K (1048576) bytes
#define GEI_SIZE_2M     0x0B    // 2048K (2097152) bytes
#define GEI_SIZE_MAX 2097152UL  // maximum RAM size allowed (2Mb)

//   This same structure is used both for commands sent to the PromICE, and
// for the responses that come back from it.  It's a pretty simple format
// actually, and notice the lack of any checksum or sequence number!
#define GEI_HEADERLEN     3     // count of fixed header bytes in every message
#define GEI_MAXDATALEN  256     // maximum message data length
#pragma pack(push)
#pragma pack(1)
struct _GEI_MESSAGE {
  uint8_t bUnitID;                // unit (0 or 1) this message is for/from
  uint8_t bCommand;               // command code or response byte
  uint8_t bCount;                 // count of data bytes to follow (0 -> 256!)
  uint8_t abData[GEI_MAXDATALEN]; // actual payload/data for this message
};
#pragma pack(pop)
typedef struct _GEI_MESSAGE GEI_MESSAGE;

// Global methods ...
extern uint8_t geiConnect (const char *pszName, uint32_t nBaud);
extern void geiDisconnect();
extern char *geiGetVersion (uint8_t bUnit);
extern uint32_t geiGetSerial (uint8_t bUnit);
extern uint32_t geiGetSize (uint8_t bUnit);
extern uint32_t geiAddressMask (uint32_t lSize);
extern int32_t geiTestRAM (uint8_t bUnit, uint8_t nPasses);
extern int32_t geiFillRAM (uint8_t bUnit, uint8_t bValue);
extern void geiResetTarget();
extern void geiLoadMode ();
extern void geiDownload (uint8_t bUnit, uint8_t *pabData, size_t cbData, uint32_t lAddress);
extern void geiUpload (uint8_t bUnit, uint8_t *pabData, size_t cbData, uint32_t lAddress);
