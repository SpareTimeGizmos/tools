//++
// PromICE.h -> global declarations for the PromICE project
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

// Parameters ...
#define PROMICE            "PromICE"  // name of this program (for messages)
#define VERSION                   1   // current version 
#define MAXUNIT                   2   // maximum number of PromICE units
#define SERIALPORTENV "PROMICE_PORT"  // environment variable for default port
#define SERIALBAUDENV "PROMICE_BAUD"  // environment variable for default bau
#define DEFAULTBAUD           57600   // default baud rate (if nothing else)

// PromICE commands ...
enum _PROMICE_COMMANDS {
  CMD_NONE,                 // no command specified
  CMD_VERIFY,               // verify communication with PromICE
  CMD_RESET,                // reset target without downloading
  CMD_TEST,                 // execute self test
  CMD_DOWNLOAD,             // download file(s)
  CMD_HELP,                 // show the help text
};
typedef enum _PROMICE_COMMANDS PROMICE_COMMAND;

// Define global and local (to one source file) routines ...
#define PRIVATE static
#define PUBLIC

// Common string functions ...
#define EOS  '\0'                 // end of string character
#define EOL  '\n'                 // standard end of line character
#define STREQL(x,y)     (strcmp(x,y)     == 0)
#define STRIEQL(x,y)    (_stricmp(x,y)    == 0)
#define STRNEQL(x,y,n)  (strncmp(x,y,n)  == 0)
#define STRNIEQL(x,y,n) (strnicmp(x,y,n) == 0)

// Common bit manipulations ...
#define ISSET(v,m) (((v) & (m)) != 0)
#define SETBIT(v,m) v |= m
#define CLRBIT(v,m) v &= ~(m)

// Standard HIBYTE() and LOBYTE() macros ...
#ifndef HIBYTE
#define LOBYTE(x) ( (x)       & 0xFF)
#define HIBYTE(x) (((x) >> 8) & 0xFF)
#endif

// Definitions for Linux compatibility ...
#ifdef __linux__
#define _stricmp	strcasecmp
#define _strnicmp	strncasecmp
#define _MAX_PATH	PATH_MAX
#define errno_t		int
#define strcat_s(d,l,s)	strcat(d,s)
#define strcpy_s(d,l,s)	strcpy(d,s)
#endif

// Global methods ...
extern void Message (const char *pszMsg, ...);
extern void FatalError (const char *pszMsg, ...);
