//++
// mkid01.c
//
// Copyright (C) 2003 by Robert Armstrong.
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
// DESCRIPTION
//   This program is able to read and write SBC6120 ID01 partitions on an IDE
// disk attached to the PC.  
//
// Usage:
//      mkid01 -rnnnn -ud <file>
//      mkid01 -wnnnn -ud <file>
//
//   The -r option reads ID01 partition nnnn (octal!) from IDE drive d and
// writes it to the file given.  The -w option writes ID01 partition nnnn
// (octal!) of IDE drive d from the file given.  ID01 partitions are always
// exactly 2Mb long and this program always transfers an entire partition.
// The PC disk files created are in the same format used by the WinEight
// emulator.
//
//   WARNING - you'll want to be a little careful with this program as it's
// only too easy to write all over your PC's boot drive.  Ordinarily the 
// IDE unit parameter will be 1 (i.e. "-u1"), which specifies the slave drive
// attached to the primary controller.  It's possible, if you boot your PC
// from a floppy or SCSI disk, that you might want to use "-u0" to select
// unit zero, but use care!
//
// REVISION HISTORY
// 16-Sep-01    RLA     New file.
//--

// Include files...
#include <stdio.h>      // NULL, printf(), scanf(), et al
#include <string.h>     // strlen(), strcpy(), strcat(), etc...
#include <stdlib.h>     // malloc(), exit(), etc...
#include <io.h>         // open(), read(), write(), etc...
#include <fcntl.h>      // O_RDWR, O_CREAT, O_BINARY, etc...
#include <sys\stat.h>   // S_IWRITE, S_IREAD, et al...
#ifdef MSDOS
#include <dos.h>        // _int86(), _int86x(), _REGS, etc...
#endif
#if defined(__WIN32__) || defined(_WIN32)
#include "windows.h"	/* FindFirstFile(), CreateFile, etc...		*/
#endif
#if defined(_WIN32)
#include <winioctl.h>	/* DISK_GEOMETRY, et al...			*/
#endif
#include <assert.h>     // assert() macro (what else??)


#define OS8_BLOCK_SIZE	  256
#define ID01_SECTOR_SIZE  512
typedef unsigned	  UINT;
typedef unsigned short	  UWORD;
typedef unsigned char	  UBYTE;
#if !defined(FALSE)
typedef int		  BOOL;
#define FALSE		  (0)
#define TRUE		  (~FALSE)
#endif

#ifdef MSDOS
#pragma pack(1)
//   This structure is used by BIOS INT 13h, subfunction 48h, to obtain the
// characteristics of an IDE disk drive...
struct _EXTENDED_DRIVE_PARAMETERS {
  unsigned short nParamSize;            // size of this structure (001Eh)
  unsigned short nFlags;                // information flags
  unsigned long  lPhysicalCylinders;    // number of physical cylinders
  unsigned long  lPhysicalHeads;        // number of physical heads on drive
  unsigned long  lSectorsPerTrack;      // number of physical sectors per track
  unsigned long  lTotalSectors[2];      // total number of sectors on drive
  unsigned short nSectorSize;           // bytes per sector (should be 512!)
  unsigned long  lEDDConfig;            // EDD configuration parameters
};

//   This structure is used by BIOS INT 13h, subfunctions 42h and 43h, to
// specify the parameters for extended disk reads and writes...
struct _DISK_ADDRESS_PACKET {
  unsigned char  bPacketSize;           // size of this structure (10h)
  unsigned char  bReserved;             // reserved (must be zero)
  unsigned short nTransferCount;        // number of sectors to transfer
  void __far    *lpBuffer;              // segmented address of buffer
  unsigned long  lLBA[2];               // starting absolute block number
};
#pragma pack()
#endif

#define FAIL(msg)                   \
  {fprintf(stderr, "mkid01: " msg "\n");  exit(EXIT_FAILURE);}
#define FAIL1(msg,arg)              \
  {fprintf(stderr, "mkid01: " msg "\n", arg);  exit(EXIT_FAILURE);}
#define WARN(msg)                   \
  fprintf(stderr,  "mkid01: " msg "\n");


#if defined(__WIN32__) || defined(_WIN32)
HANDLE driveHandle;		/* handle to drive device */

char* Win32Error(void)
{
    LPVOID lpMsgBuf;
    char* eol;

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		  NULL, GetLastError(),
		  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		 (LPTSTR) &lpMsgBuf, 0, NULL);

    if (eol = strchr(lpMsgBuf, '\r'))
      *eol = '\0';

    return lpMsgBuf;
}
#endif


//   The ID01 disk format, whether physical or virtual, couldn't be
// simpler. IDE disks naturally use 512 byte sectors, and we just treat
// each sector as 256 sixteen bit words.  The upper four bits of each
// word are ignored, and the remainder make a single OS/8 block of 256
// twelve bit words.  The SBC6120 hardware and BTS6120 firmware do it
// exactly the same way...
//
//   Since all disk acess is done LBA mode, we can simply use the OS/8
// block number directly as the disk address.  No conversion to track,
// head and sector is required.  The partitioning scheme is equally
// simple - each ID01 partition is always exactly 4096 blocks/sectors,
// so we can compute the physical disk LBA as 4096*nPart + nBlock .
//
//   Note that FLX8 supports ID01 partitions only for physical disks -
// virtual ID01 images are always exactly one partition.

// ReadID01Block - read one OS/8 block from the selected ID01 partition
BOOL ReadID01Block (UINT nDrive, UINT nPart, UINT nBlock, UWORD *pwData)
{
  UINT i;
  long lLBA = (((long) nPart) <<12) + (long) nBlock;

#ifdef MSDOS
  // Read a physical sector from an IDE drive attached to this PC!
  union _REGS inregs, outregs;  struct _SREGS segregs;
  struct _DISK_ADDRESS_PACKET DiskAddress;
  void __far *lpDAP = &DiskAddress;
  memset(&DiskAddress, 0, sizeof(DiskAddress));
  DiskAddress.bPacketSize = sizeof(DiskAddress);
  DiskAddress.nTransferCount = 1;  DiskAddress.lpBuffer = pwData;
  DiskAddress.lLBA[1] = 0;  DiskAddress.lLBA[0] = lLBA;
  inregs.h.ah = 0x42;  inregs.h.dl = nDrive | 0x80;
  inregs.x.si = _FP_OFF(lpDAP);  segregs.ds  = _FP_SEG(lpDAP);
  _int86x (0x13, &inregs, &outregs, &segregs);
  if (outregs.x.cflag != 0) return FALSE;
#elif defined(__WIN32__) || defined(_WIN32)
    /* Read a physical sector from a raw drive attached to this PC */
    LARGE_INTEGER qLBA;		/* use 64-bit math to compute offset */
    DWORD read;
    qLBA.QuadPart = (LONGLONG) lLBA * ID01_SECTOR_SIZE;
    assert(0xFFFFFFFF != SetFilePointer(driveHandle, qLBA.LowPart, &qLBA.HighPart, FILE_BEGIN));
    assert(ReadFile(driveHandle, pwData, ID01_SECTOR_SIZE, &read, NULL));
#else
    assert(FALSE);  /* physical disk operations not implemented */
#endif

  //   Mask all the data words in the block to 12 bits.  For a real,
  // legitimate drive written by the SBC6120 this shouldn't be needed,
  // but just to be safe we'll do it in case this drive's never been
  // near a SBC6120 before...
  for (i = 0;  i < OS8_BLOCK_SIZE;  ++i)  pwData[i] &= 07777;
  return TRUE;
  
} //ReadID01Block


// WriteID01Block - write one OS/8 block to the selected ID01 partition
BOOL WriteID01Block (UINT nDrive, UINT nPart, UINT nBlock, UWORD *pwData)
{
  long lLBA = (((long) nPart) << 12) + (long) nBlock;

#ifdef MSDOS
  // Write a physical sector to an IDE drive attached to this PC!
  union _REGS inregs, outregs;  struct _SREGS segregs;
  struct _DISK_ADDRESS_PACKET DiskAddress;
  void __far *lpDAP = &DiskAddress;
  memset(&DiskAddress, 0, sizeof(DiskAddress));
  DiskAddress.bPacketSize = sizeof(DiskAddress);
  DiskAddress.nTransferCount = 1;  DiskAddress.lpBuffer = pwData;
  DiskAddress.lLBA[1] = 0;  DiskAddress.lLBA[0] = lLBA;
  inregs.h.ah = 0x43;  inregs.h.al = 0;  inregs.h.dl = nDrive | 0x80;
  inregs.x.si = _FP_OFF(lpDAP);  segregs.ds  = _FP_SEG(lpDAP);
  _int86x (0x13, &inregs, &outregs, &segregs);
  if (outregs.x.cflag != 0) return FALSE;
#elif defined(__WIN32__) || defined(_WIN32)
    /* Write a physical sector to a raw drive attached to this PC */
    LARGE_INTEGER qLBA;		/* use 64-bit math to compute offset */
    DWORD wrote;
    qLBA.QuadPart = (LONGLONG) lLBA * ID01_SECTOR_SIZE;
    assert(0xFFFFFFFF != SetFilePointer(driveHandle, qLBA.LowPart, &qLBA.HighPart, FILE_BEGIN));
    assert(WriteFile(driveHandle, pwData, ID01_SECTOR_SIZE, &wrote, NULL));
#else
    assert(FALSE);  /* physical disk operations not implemented */
#endif

  return TRUE;
} //WriteID01Block


// OpenPhysicalID01
//   This procedure will open a physical ID01 drive under MSDOS.  There
// isn't anything that actually needs to be done to open the drive, but
// we can take this opportunity to verify that this BIOS supports the
// INT 13 Enhanced Drive Services.  We need these extensions to be able
// to access the drive directly in LBA mode without any CHS translation
// getting in the way.  We also determine the capacity of the drive and
// save that away for later range checking of the partition numbers.
int OpenPhysicalID01 (UINT nDrive)
{
#ifdef MSDOS
  union _REGS inregs, outregs;  struct _SREGS segregs;
  struct _EXTENDED_DRIVE_PARAMETERS DriveParams;
  void __far *lpParams = &DriveParams;

  //  INT 13h, function 41h, is used to test for the presence of a BIOS
  // with Enhanced Disk Drive support.
  inregs.h.ah = 0x41;  inregs.x.bx = 0x55AA;  inregs.h.dl = 0x80;
  _int86 (0x13, &inregs, &outregs);
  //   If the carry flag is set, then this BIOS doesn't know anything
  // about enhanced disk services.  However even if the carry flag is
  // clear, we still need to check the bitmap of supported functions
  // (returned in CX) to verify that extended disk access functions are
  // present...
  if (outregs.x.cflag || ((outregs.x.cx & 1) == 0))
    FAIL("This BIOS does not support extended disk access\n");

  //   Now use INT 13h, subfunction 48H, to find the size of the drive
  // in physical sectors.  If this function fails, then the unit most
  // likely doesn't exist.  Note that the BIOS actually allows for 64
  // bits in the drive size, but that's _huge_ !!  We only use 32 bits
  // of the drive size, and the SBC6120/BTS6120 actually only uses 24.
  inregs.h.ah = 0x48;  inregs.h.dl = nDrive | 0x80;
  inregs.x.si = _FP_OFF(lpParams);  segregs.ds  = _FP_SEG(lpParams);
  memset(&DriveParams, 0, sizeof(DriveParams));
  DriveParams.nParamSize = sizeof(DriveParams);
  _int86x (0x13, &inregs, &outregs, &segregs);
  if (outregs.x.cflag)  return -1;
  assert(DriveParams.lTotalSectors[1] == 0);
  return (int) (DriveParams.lTotalSectors[0] >> 12);
#elif defined(__WIN32__) || defined(_WIN32)
    static char daName[] = "\\\\.\\X:";	/* special name format to open volume */
    DISK_GEOMETRY layout;
    DWORD read;

    if (islower(nDrive)) nDrive = toupper(nDrive);
    if ((nDrive <= 'C') || (nDrive > 'Z')) {
	FAIL("Use <drive letter>: as the device name to open a physical drive");
	return FALSE;
	}

    daName[4] = nDrive;
fprintf(stderr," Opening \"%s\"\n", daName);
    driveHandle = CreateFile(daName, GENERIC_READ|GENERIC_WRITE,
			     FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			     0, NULL);
    if (driveHandle == INVALID_HANDLE_VALUE){
	FAIL1("Error opening the drive: %s", Win32Error());
	return FALSE;
	}

    /* read the geometry, and incidentally make sure there's a disk in it,
       if it's removable */
    if (DeviceIoControl(driveHandle, IOCTL_DISK_GET_DRIVE_GEOMETRY,
		    NULL, 0,
		    &layout, sizeof(layout),
		    &read, NULL)){
	/* compute the number of 4096 block partitions it will hold */
//	nMaxPartition = (UINT)
//	       (((layout.Cylinders.QuadPart *
//		  layout.TracksPerCylinder *
//		  layout.SectorsPerTrack *
//		  layout.BytesPerSector) / ID01_SECTOR_SIZE) >> 12);
//	Message('I', "ID01 supports %d OS/8 partitions", nMaxPartition);
	return TRUE;
	}
    else {
	FAIL1("Error opening drive for I/O: %s", Win32Error());
	return FALSE;
	}
#else
  /* Non-MSDOS machines don't support ID01 (at least not yet!)... */
  return FALSE;
#endif
}

void ClosePhysicalID01 ()
{
#if defined(__WIN32__) || defined(_WIN32)
  /* give the disk back to the operating system */
  DWORD ret;
  /* eject it if it's ejectable, just for fun */
  DeviceIoControl(driveHandle, IOCTL_STORAGE_EJECT_MEDIA,
  		  NULL, 0, NULL, 0, &ret, NULL);
  CloseHandle(driveHandle);
  driveHandle = INVALID_HANDLE_VALUE;
#endif
}



// Read an OS/8 disk block from the image file...
BOOL ReadImageBlock (int fd, UINT nBlock, UWORD *pwData)
{
  UINT nSize = OS8_BLOCK_SIZE * 2;
  long lOffset = ((long) nBlock) * ((long) nSize);
  if (_lseek(fd, lOffset, SEEK_SET) != lOffset) return FALSE;
  if (_read(fd, pwData, nSize) != (int) nSize) return FALSE;
  return TRUE;
}


// Write an OS/8 disk block to the image file...
BOOL WriteImageBlock (int fd, UINT nBlock, UWORD *pwData)
{
  UINT nSize = OS8_BLOCK_SIZE * 2;
  long lOffset = ((long) nBlock) * ((long) nSize);
  if (_lseek(fd, lOffset, SEEK_SET) != lOffset) return FALSE;
  if (_write(fd, pwData, nSize) != (int) nSize) return FALSE;
  return TRUE;
}



// Write an entire ID01 image to the IDE drive...
void WritePartition (int nDrive, int nPartition, char *lpszFile)
{
  UWORD awData[OS8_BLOCK_SIZE];  int fd, nBlock;
  fd = _open(lpszFile, _O_RDONLY|_O_BINARY, _S_IREAD|_S_IWRITE);
  if (fd == -1) FAIL1("Unable to read %s", lpszFile);

  for (nBlock = 0;  nBlock < 4096;  ++nBlock) {
    if ((nBlock & 0177) == 0)
      fprintf(stderr,"\rWriting block %d ... ", nBlock);
    if (!ReadImageBlock(fd, nBlock, awData))
      FAIL1("Error reading file %s", lpszFile);
    if (!WriteID01Block(nDrive, nPartition, nBlock, awData))
      FAIL1("Error writing drive %d", nDrive);
  }

  fprintf(stderr,"\rWriting block %d ... Done!\n", nBlock);
  _close(fd);
}

// Read an entire ID01 image from the IDE drive...
void ReadPartition (char *lpszFile, int nDrive, int nPartition)
{
  UWORD awData[OS8_BLOCK_SIZE];  int fd, nBlock;
  fd = _open(lpszFile, _O_CREAT|_O_WRONLY|_O_BINARY, _S_IREAD|_S_IWRITE);
  if (fd == -1) FAIL1("Unable to write %s", lpszFile);

  for (nBlock = 0;  nBlock < 4096;  ++nBlock) {
    if ((nBlock & 0177) == 0)
      fprintf(stderr,"\rReading block %d ... ", nBlock);
    if (!ReadID01Block(nDrive, nPartition, nBlock, awData))
      FAIL1("Error reading drive %d", nDrive);
    if (!WriteImageBlock(fd, nBlock, awData))
      FAIL1("Error writing file %s", lpszFile);
  }

  fprintf(stderr,"\rReading block %d ... Done!\n", nBlock);
  _close(fd);
}



// ShowUsage - print a "Usage: ..." message and quit...
void ShowUsage (void)
{
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "\tmkid01 -rnnnn -ud <image-file>\n");
  fprintf(stderr, "\tmkid01 -wnnnn -ud <image-file>\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "\t-rnnnn\t- read partition nnnn (octal!)\n");
  fprintf(stderr, "\t-wnnnn\t- write partition nnnn (octal!)\n");
  fprintf(stderr, "\t-ud\t- select IDE drive d\n");
  exit(EXIT_SUCCESS);
}

// ParseArguments - Parse the command line arguments...
void ParseArguments (int argc, char *argv[], int *pnPartition,
  int *pnDrive, int *pnDirection, char **pszFileName)
{
  int nArg;  char *psz;
  *pnPartition = *pnDrive = *pnDirection = -1;  *pszFileName = NULL;

  for (nArg = 1;  nArg < argc;  ++nArg) {
    // If it doesn't start with a "-" character, then it must be a file name.
    if (argv[nArg][0] != '-') {
      if (*pszFileName == NULL) *pszFileName = argv[nArg];
      else FAIL1("too many files specified: \"%s\"", argv[nArg]);
      continue;
    }
    
    // Handle the -r (read) and -w (write) options...
    if (   (strncmp(argv[nArg], "-r", 2) == 0)
        || (strncmp(argv[nArg], "-w", 2) == 0)) {
      *pnDirection = argv[nArg][1];
      *pnPartition = (UINT) strtoul(argv[nArg]+2, &psz, 8);
      if (*psz != '\0')
        FAIL1("illegal partition: \"%s\"", argv[nArg]);
      continue;
    }
    
    // Handle the -u (unit) option...
    if (strncmp(argv[nArg], "-u", 2) == 0) {
      *pnDrive = (UINT) strtoul(argv[nArg]+2, &psz, 10);
      if ((*psz != '\0') || (*pnDrive > 1)) {
        if (isalpha(*psz) && (*(psz+1) == '\0'))
	  *pnDrive = *psz;
	else
          FAIL1("illegal unit: \"%s\"", argv[nArg]);
      }
      continue;
    }
    
    // Otherwise it's an illegal option...
    FAIL1("unknown option - \"%s\"\n", argv[nArg]);
  }
}



//++
// The main program...
//--
int main (int argc, char *argv[])
{
  int nPartition, nDrive, nDirection;  char *pszFileName;

  // All arguments, including -ud and -rnnnn or -wnnnn, are required.  
  // If the they aren't there, then just print the help and exit...
  if (argc != 4) ShowUsage();
  ParseArguments(argc, argv, &nPartition, &nDrive, &nDirection, &pszFileName);

  if (!OpenPhysicalID01(nDrive)) return EXIT_FAILURE;
  if (nDirection == 'r')
    ReadPartition(pszFileName, nDrive, nPartition);
  else if (nDirection == 'w')
    WritePartition(nDrive, nPartition, pszFileName);
  ClosePhysicalID01();

  return EXIT_SUCCESS;
}
