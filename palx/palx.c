//++
// palx - PDP-8/IM6100/HD6120 Cross Assembler
//
//    PPPPPPPPPP        AAAAAAAA      LL              XX          XX
//    PPPPPPPPPPP      AAAAAAAAAA     LL              XX          XX
//    PP        PP    AA        AA    LL               XX        XX
//    PP        PP    AA        AA    LL                XX      XX
//    PP        PP    AA        AA    LL                 XX    XX
//    PP        PP    AA        AA    LL                  XX  XX
//    PPPPPPPPPPP     AA        AA    LL                    XX
//    PPPPPPPPPP      AA        AA    LL                    XX
//    PP              AAAAAAAAAAAA    LL                  XX  XX
//    PP              AAAAAAAAAAAA    LL                 XX    XX
//    PP              AA        AA    LL                XX      XX
//    PP              AA        AA    LL               XX        XX
//    PP              AA        AA    LLLLLLLLLLLL     XX        XX
//    PP              AA        AA    LLLLLLLLLLLL     XX        XX
//
//                            Bob Armstrong
//                August 1981/February 1999/December 2022
//              Copyright (C) 1999-2022 by Robert Armstrong
//
//   This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
//   This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License along with
// this program; if not, visit the website of the Free Software Foundation, Inc.,
// www.gnu.org.
//
// DESCRIPTION:
//   PALX is a PDP-8 cross assembler.  it was specifically intended for the
// Intersil IM6100 and Harris HD6120 chips, although it would work for a "real"
// PDP-8 just as well.  The PALX syntax differs somewhat from the DEC PAL PDP-8
// assembler, and PALX is closer in look and feel to MACRO-10.  Whether that's
// good or bad depends on your point of view, but PALX certainly cannot assemble
// a traditional PDP-8 PAL source file, nor can a PALX source be assembled by PAL.
//
//   Refer to the file PALX.HLP for a description of the assembler syntax.
//
// USAGE:
//   palx [-p nn] [-w nnn] [-8] [-l listfile] [-b binaryfile] sourcefile
//      -l file -> specify listing file name
//      -b file -> specify binary file name
//      -p nn   -> set listing page length to nn lines
//      -w nnn  -> set listing page width to nnn columns
//      -8      -> use OS/8 style for .SIXBIT/.SIXBIZ
//      -a      -> use ASR33 "always mark" ASCII
//
// NOTES:
//
// REVISION HISTORY
//                        - be sure an origin setting gets sent to the .bin file
//                          if the program starts at address 0001
//    3-Apr-00    RLA     - Make it compile on VMS under DEC C...
//    4-Apr-00    RLA     - fix up the listing slightly so that tabs line up.
//                        - add the .PAGE pseudo op
//                        - add the .IM6100 and .HD6120 pseudo ops
//                        - make .SIXBIT fold the string to upper case
//                        - make the literals show up in the listing before
//                          the .END, not after!
//                        - make PALX support multiple fields and add the
//                          .FIELD pseudo op
//                        - add the memory bitmap to the listing
//                          (this is a way cool feature!)
//                        - Under some conditions, EvaluateOperand() fails to
//                          return a value.  This leads to some very puzzling
//                          and undeserved error messages when assembling!
//    5-Apr-00    RLA     - Add a symbol cross reference to the listing
//    6-Apr-00    RLA     - Finally, finally, fix the nagging "gratuitous P
//                          error when a page is exactly full" problem!
//                        - If the syntax for .HD6120 or .IM6100 was illegal,
//                          the line wouldn't get printed in the listing.
//                        - Add the .VECTOR pseudo op to set the reset vector
//                          on the 6100 and 6120 microprocessors...
//    7-Apr-00    RLA     - Store the "." in pseudo ops as part of the actual
//                          name in the symbol table.  This way there's no
//                          conflict if the user happens to define a symbol in
//                          his program with the same name.
//                        - Do away with the PSEUDO_OPS enumeration and just
//                          store the address of the routine directly in
//                          the symbol table.
//                        - Add real support for both the 6120 hardware stack,
//                          and software stack emulations on the 6100, with
//                          the .STACK, .PUSH, .POP, .PUSHJ and .POPJ pseudo ops.
//                        - Add the Harris mnemonics for HLTFLG, PNLTRP, PWRON
//                          and BTSTRP to the 6120 extended symbols.
//                        - I forgot to specify _O_TRUNC when opening the BIN
//                          file, so if you assembled a program, edited it to
//                          make it shorter and assembled it again, part of the
//                          original BIN file would be left at the end of the
//                          new one!  This lead to one very hard to find bug!
//    8-Apr-00    RLA     - The value of R3L (7014) was incorrect
//    9-Apr-00    RLA     - .FIELD needs to dump out the literals _before_
//                          sending the field change frame to the binary file,
//                          otherwise the literals go into the new field!
//   10-Apr-00    RLA     - Add the table of contents (aka TOC) to the listing.
//   14-Apr-00    RLA     - Remove the "smart" P error test generated by
//                          CheckBitMap - it prevented us from doing to many
//                          clever things in BOOTS 6120.
//   15-Apr-00    RLA     - Add the .TEXT pseudo op to generate packed ASCII.
//   19-Apr-00    RLA     - Remove the BTSTRP, PWRON, PNLTRP and HLTFLG mnemonics.
//   21-Apr-00    RLA     - Always start the table of contents on an odd numbered
//                          page.  That'll be an upward facing page if a listing
//                          is printed double sided.
//                        - Add the Memory Map and Symbol Table headings to the
//                          table of contents.
//                        - Add escape codes (e.g. \t, \r, \d, etc) to the strings
//                          for .ASCIZ, .TEXT and .SIXBIT.
//    3-May-00    RLA     - Add the & (logical AND) and | (logical OR) operators
//                          to arithmetic expressions.
//                        - If there are no literals on the current page, then
//                          allow the PC to flow freely onto the next page.  This
//                          makes it possible to generate long strings of data or
//                          text without PALX requiring arbitrary .PAGE statements
//                          to break them up.
//                        - In .END, if there are no literals just leave the final
//                          PC it is, but if there are literals then dump them and
//                          advance the PC to the start of the next page.  This is
//                          important because the PC at .END is the program break.
//    4-May-00    RLA     - Fix the \t string escape - the code for tab is 011, not
//                          010 (there was a day when I would have just known that,
//                          but these days I have to look it up!).
//                        - Modify the .TEXT, .ASCIZ and .SIXBIT pseudo ops to list
//                          list only the address and not all the individual words
//                          of code generated.  It just made the listing file way
//                          too long to show all those words of code!
//   22-Jan-01    RLA     - To help with spliting SBC6120 into multiple fields,
//                          add the "F" error to flag off-field references.
//                        - Since off-field references aren't always an error, add
//                          the .NOWARN psuedo op to allow specified error codes
//                          to be explicitly suppressed.
//   30-Jan-01    RLA     - If we encounter a .BLOCK statement which would cause
//                          the page to overflow, we helpfully try to calculate
//                          the maximum space that we could reserve and allocate
//                          that.  Unfortunately, if the page is already past full
//                          we calculate a negative number, which ends up reserving
//                          a very large number of words!
//   23-Feb-10  RLA     - Increase the HASHSIZE to 3079 - BTS6120 finally has
//                        too many symbols for the original limit of 1037!!
//   14-Feb-11  RLA     - Implement the multiply, divide and modulus ("*", "/"
//                        and "%") operators.  And while I'm at it, add
//                        parenthesis too so you can now say something like
//                        "-(X/2)"....  We still don't have any operator
//                        precedence, though.
//   08-Dec-22    RLA   - Change the number of lines per page so that listings fit
//                        on an 8-1/2x11" paper in landscape mode.
//                      - Change .PUSHJ so that it puts the full address of the
//                        destination in the second word, ON THE 6100!  .PUSHJ
//                        still generates a JMP instruction on the 6120.
//  18-Dec-22     RLA   - Add macros, including named arguments and generated
//                        labels.  Macro definitions and expansions may be nested
//                        and macros may be redefined...
//                      - Add conditional assembly pseudo ops .IFDEF, .IFNDEF
//                        .IFEQ, .IFNE, .IFGT, .IFGE, .IFLT and .IFLE
//  20-DEC-22     RLA   - Add new listing options .LIST and .NOLIST.
//                      - Add .ERROR pseudo op.
//                      - Don't require a "." for macro names.
//  21-DEC-22     RLA   - invent MyAlloc() and use it everywhere
//                      - replace _strdup() with MyStrDup()
//                      - invent the FatalError() and Message() routines
//                        (since VAXC/DECC doesn't have variadic macros!)
//                      - change .HM6120 th .HD6120
//                      - (re)port to VMS and Linux
//                      - fix .IFLT (the test was wrong!)
//  26-DEC-22     RLA   - Add .NLOAD pseudo-op
//                      - Add -p and -w command line options to set the
//                        listing page width and length
//                      - truncate titles (on the right) and file names
//                        (on the left) if they are too long for the listing
// 27-DEC-22      RLA   - Add .EJECT pseudo operation.
//                      - Invent .LIST/.NOLIST TXB to control listing of binary
//                        expansion of .TEXT/.ASCIZ/.SIXBIT and .DATA.
// 30-DEC-22      RLA   - Consider '>' to be an EOL character too.
//                        Add 'T' error for illegal character in SIXBIT string.
//                        Add .SIXBIZ pesudo-op to generate terminated SIXBIT.
//                        Add "-8" command line option to generate OS/8 SIXBIT.
// 31-DEC-22      RLA   - Detect and warn if any location is loaded more than once.
//                        .BLOCK inadvertently generates D errors.
//                        .SIXBIT/Z list the source with the first line of code.
// 12-JAN-23      RLA   - Change .OPDEF to .MRI.
//                        Add -a option to generate "always mark" ASCII
// 13-JAN-23      RLA   - Allow actual arguments to be enclosed in "< ... >"
//                          This permits any characters - commas, semicolons,
//                          whatever, in the actual.
// 20-MAR-23	  RLA	- Miscellaneous code cleanup to compile clean with gcc.
//			  Add .ENABLE and .DISABLE to allow the OS8 and ASR33
//			    options to be set from within the source file.
// 21-MAR-23      RLA   - if the source line ends with "\r\n" (as DOS files do
//                          under Linux!) then ingore the "\r".
// 23-JUL-23      RLA   - if the string contains exactly 1 character, .SIXBIT
//                          doesn't list the source line!
// 27-JUL-23      RLA   - Allow unary "~" as ones complement.
//
// BUGS
//
// SUGGESTIONS
//   The possibility exists that we could use the bitmap to determine which
//   locations have been filled with code and which are unused, and this
//   information could be used to generate P errors rather than always
//   checking the PC against the LiteralBase.  This has the advantage that
//   it could detect code that gets overwritten later in the assembly.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdio.h>      // printf(), scanf(), fopen(), etc...
#include <stdlib.h>     // exit(), malloc(), free(), ...
#include <stdbool.h>    // bool, true and false ...
#include <stdarg.h>     // va_list, va_start(), va_end(), etc...
#include <string.h>     // strlen(), strcpy(), strcat(), etc...
#include <ctype.h>      // islower(), toupper(), etc...
#include <assert.h>     // assert() (what else???)
#include <time.h>       // asctime(), localtime(), struct tm, etc...
#ifdef VMS
#include <inttypes.h>   // VAXC/DECC does not know about stdint!
#include <unixio.h>     // Unix equivalents for DECC/VAXC...
#include <fcntl.h>      // same as the U*nx counter part
#include <stat.h>       // ditto, but note the absence of the "sys/"!
#else
#include <stdint.h>     // uint8_t, uint32_t, etc ...
#include <fcntl.h>      // _O_TRUNC, _O_WRONLY, etc...
#include <sys/stat.h>   // _S_IWRITE, _S_IREAD, etc...
#ifdef __linux__
#include <limits.h>     // PATH_MAX ...
#include <unistd.h>     // open(), write(), close(), etc ...
#include <sys/io.h>     // _open(), _write(), _close(), etc...
#include <libgen.h>     // basename() and dirname() ...
#include <errno.h>      // errno, ENOENT, etc ...
#else
#include <io.h>         // _open(), _write(), _close(), etc...
#endif
#endif

// Linux and VMS/DECC has slightly different names for these symbols...
#if defined(__linux__)
#define _S_IREAD        S_IRUSR
#define _S_IWRITE       S_IWUSR
#elif defined(VMS)
#define _S_IREAD        S_IREAD
#define _S_IWRITE       S_IWRITE
#endif
#if defined(VMS) || defined(__linux__)
#define _O_CREAT        O_CREAT
#define _O_WRONLY       O_WRONLY
#define _O_TRUNC        O_TRUNC
#define _O_BINARY       0
#define _write          write
#define _read           read
#define _open           open
#define _close          close
#endif


////////////////////////////////////////////////////////////////////////////////
///////////////////////   CONSTANTS AND DEFINITIONS   //////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Constants ...
#define TITLE   "IM6100/HD6120 Cross Assembler"
#define PALX            "PALX"  // our name for messages
#define VERSION            423  // our version number
#define HASHSIZE          3079  // maximum entries in the symbol table
#define MAXSTRING          256  // longest string (e.g. source line) allowed!
#define IDLEN               12  // longest identifier allowed!
#define MAXARG              10  // maximum number of macro arguments
#define MAXBODY           4096  // longest macro body allowed (in characters)
#define LINES_PER_PAGE      60  // default number of lines per listing page
#define COLUMNS_PER_PAGE   120  // default number of columns per listing page
#define LIST_TYPE       ".lst"  // default type for listing files
#define BINARY_TYPE     ".bin"  //    "      "   "  binary    "
#define SOURCE_TYPE     ".plx"  //    "      "   "  source    "

// Error codes ...
//   The following constants determine the error codes printed in the
// listing at the start of the bad line....
#define ER_RAN          'A'     // number out of range
#define ER_MAC          'C'     // too many macro arguments or macro too long
#define ER_DUP          'D'     // location loaded more than once
#define ER_ERR          'E'     // .ERROR pseudo op
#define ER_OFF          'F'     // off field reference warning
#define ER_LST          'L'     // unknown .LIST/.NOLIST option
#define ER_MDF          'M'     // multiply defined
#define ER_IFN          'N'     // illegal format for number
#define ER_MIC          'O'     // illegal micro-coded combination
#define ER_PAF          'P'     // page full error
#define ER_SYM          'S'     // a symbol has been improperly defined
#define ER_UDF          'U'     // undefined symbol
#define ER_TXT          'T'     // illegal character in SIXBIT string
#define ER_SYN          'X'     // syntax error
#define ER_SCT          'W'     // illegal off page reference
#define ER_POP          'Z'     // badly formed or unknown pseudo op

// Primitive string types...
typedef char string_t[MAXSTRING]; // generic character strings
typedef char identifier_t[IDLEN]; // any identifier (aka symbol)
#define EOS  '\0'                 // end of string character
#define EOL  '\n'                 // standard end of line character
#define STREQL(x,y)    (strcmp(x,y) == 0)
#define STRNEQL(x,y,n) (strncmp(x,y,n) == 0)


////////////////////////////////////////////////////////////////////////////////
////////////////////////////   DATA STRUCTURESS   //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//++
// Symbol Table Data
//   One table is used to store all identifiers, including both built
// in machine opcodes, pseudo-operations, in addition to user defined
// symbols.  Symbol table entries contain a name, a value and a symbol
// type.  The latter comes from the following enum and defines the
// usage of the symbol.  The value depends on the type, as follows:
//
//  * For user defined symbols, Value.bin is simply the "value" defined
//  * For machine instructions, Value.bin is the "value" of the opcode
//  * For pseudo ops, Value.pop is the address of an internal routine
//  * For macros, Value.mac is a pointer to the macro text
//--
enum _SYMBOL_TYPES {    // Types of symbols in the symbol table:
                        // User defined symbol table entries...
  SF_UDF,               //   an undefined symbol
  SF_TAG,               //   a label (aka tag)
  SF_EQU,               //   an equate
  SF_OPDEF,             //   a user defined opcode (by .OPDEF xyz)
  SF_MACRO,             //   a .DEFINE macro
  SF_MDF,               //   a multiply defined symbol
                        // Built in machine opcode entries...
  OP_MRI,               //   memory reference instruction
  OP_OPR,               //   operate microinstruction
  OP_IOT,               //   standard (traditional) input/output
  OP_PIE,               //   peripheral interface element (IM6101)
  OP_PIO,               //   parallel I/O (IM6103)
  OP_CXF,               //   change field (CDF/CIF) instruction
                        // Built in pseudo operators...
  PO_POP                //   and pseudo operation
};
typedef enum _SYMBOL_TYPES SYMBOL_TYPE;

// Symbol cross reference table entries ...
struct _CREF {                // Cross reference data for symbols...
  uint16_t      nLine;        //   source line number of this reference
  bool          fDefinition;  //   true if this reference defines the symbol
  struct _CREF *pNext;        //   next reference to this symbol
};
typedef struct _CREF CREF;

// Table of contents entries ...
struct _TOC {                 // Table of contents (TOC) entries
  char        *pszTitle;      //   the string (from .TITLE) of this section
  uint16_t     nPage;         //   corresponding listing page number
  struct _TOC *pNext;         //   next table of contents entry
};
typedef struct _TOC TOC;

// Macro definitions and expansions ...
typedef struct _MACDEF MACDEF;
struct _MACDEF {                  // macro definition ...
  uint8_t         nFormals;       //   number of formal arguments
  identifier_t aszFormals[MAXARG];//   names of the formal arguments
  char           *pszBody;        //   the actual text of the macro body
  struct _SYMBOL *pSymbol;        //   symbol table entry for this macro
};
struct _MACEXP {                  // macro expansion
  MACDEF     *pDefinition;        //   macro definition we're expanding
  uint8_t     nActuals;           //   number of actual parameters
  string_t    aszActuals[MAXARG]; //   actual arguments
  char       *pszNextLine;        //   pointer to next line in macro body
  struct _MACEXP *pPreviousMacro; //   next outer nested expansion
};
typedef struct _MACEXP MACEXP;

// Pointer to a pseudo operation routine ...
typedef void (*POPPTR) (char *);

// Symbol table entries ...
struct _SYMBOL {        // Data associated with a user symbol...
  char        *pszName; //   the name of the symbol
  SYMBOL_TYPE  nType;   //   symbol type (pseudo-op, label, etc)
  union {
    uint16_t  bin;      //   symbol value (all symbols except PO_POP)
    MACDEF   *mac;      //   symbol value (macro body)
    POPPTR    pop;      //   symbol value (pseudo ops)
  } Value;
  CREF    *pFirstRef;   //   first CREF block in the reference chain
  CREF    *pLastRef;    //   last    "    "    "  "      "       "
};
typedef struct _SYMBOL SYMBOL;


////////////////////////////////////////////////////////////////////////////////
////////////////////////////   GLOBAL VARIABLES   //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

                                // Source file variables...
string_t   szSourceFile;        //   name of the current source file
FILE      *pSourceFile;         //   file handle of the source
uint16_t   nSourceLine;         //   count of source lines read
string_t   szSourceText;        //   text of the current source line
                                // Listing file variables...
string_t   szListFile;          //   name of the listing file
FILE      *pListFile;           //   listing file handle
uint16_t   nLinesPerPage;       //   listing lines per page
uint16_t   nColumnsPerPage;     //   listing columns per page
uint16_t   nListPages;          //   count of pages printed
uint16_t   nLinesThisPage;      //   count of lines on this page
bool       fNewPage;            //   start a new page in the listing
bool       fListSymbols;        //   true to list the symbol table 
bool       fListMap;            //   true to list the memory bitmap
bool       fListExpansions;     //   true to list macro expansions
bool       fListText;           //   list code for .TEXT/.ASCIZ/.SIXBIT
bool       fListTOC;            //   true to list the table of contents
bool       fPaginate;           //   true to paginate the listing
string_t   szProgramTitle;      //   current .TITLE string for listing
string_t   szErrorFlags;        //   error flags for this source line
                                // Binary file variables...
string_t   szBinaryFile;        //   name of the binary file
int        fdBinaryFile;        //   binary (object) file descriptor
uint16_t   nLastBinaryAddress;  //   address of last word punched
uint16_t   nBinaryChecksum;     //   running checksum of binary data
uint8_t    abBinaryData[64];    //   buffer for writing BIN data
uint16_t   cbBinaryData;        //   number of bytes used in the buffer
                                // Literal pool variables...
uint16_t   nLiteralBase;        //   first free literal pool address
uint16_t   anLiteralData[0200]; //   literal pool for this page
                                // Symbol table variables...
SYMBOL    *Symbols[HASHSIZE];   //   symbols (all of 'em!)
TOC       *pFirstTOC;           //   first table of contents entry
TOC       *pLastTOC;            //   last    "    "    "       "
                                // Software stack opcodes from .STACK
uint16_t   nPUSH, nPOP;         //   opcodes for .PUSH and .POP
uint16_t   nPUSHJ, nPOPJ;       //   opcodes for .PUSHJ and .POPJ
                                // Macro expansion variables...
MACEXP    *pCurrentMacro;       //   current macro expansion (NULL if none)
uint32_t  nGeneratedLabel;      //   current generated label number
                                // Miscellaneous...
uint16_t   nPass;               //   current assembler pass (1 or 2)
uint16_t   nPC;                 //   current instruction location
uint16_t   nField;              //   current instruction field (0..7)
uint16_t   nCPU;                //   selected CPU (either 6100 or 6120)
bool       fOS8SIXBIT;          //   use OS/8 style SIXBIT code
bool       fASCII8BIT;          //   use ASR33 always mark 8 bit ASCII
uint16_t   nErrorCount;         //   count of errors in this file
uint8_t    abBitMap[32768/8];   //   bitmap of unused memory words
string_t   szIgnoredErrors;     //   error flags that we can ignore


////////////////////////////////////////////////////////////////////////////////
////////////////////////////   UTILITY ROUTINES   //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void MessageList (const char *pszMsg, va_list ap)
{
  //++
  //   Handle the actual printing of error and iformational messages.  This
  // routine is shared between Message() and FatalError() ...
  //--
  fprintf(stderr, "%s - ", PALX);
  vfprintf(stderr, pszMsg, ap);
  fprintf(stderr, "\n");
}

void Message (const char *pszMsg, ...)
{
  //++
  //   Print an informational or warning message to stderr and return.  The
  // advantage to this over fprintf() is that we format the message nicely!
  //--
  va_list ap;  va_start(ap, pszMsg);
  MessageList(pszMsg, ap);  va_end(ap);
}

void FatalError (const char *pszMsg, ...)
{
  //++
  // Print a fatal error message and then abort this program!
  //--
  va_list ap;  va_start(ap, pszMsg);
  MessageList(pszMsg, ap);  va_end(ap);
  exit(EXIT_FAILURE);
}

void *MyAlloc (size_t cb)
{
  //++
  //   This routine does a malloc(), verifies that it succeeded (and aborts the
  // program if it didn't!), and then zeros the assigned memory block.  Pretty
  // simple, but this sequence happens all the time.
  //--
  void *p = malloc(cb);
  if (p == NULL)  FatalError("out of memory memory");
  memset(p, 0, cb);
  return p;
}

char *MyStrDup (const char *pszSrc)
{
  //++
  //   Allocate exactly enough space to hold a copy of the source string; copy
  // said string, and return a pointer to the copy.  Yes, _strdup() is a
  // standard library function, but it doesn't check for errors and if the
  // malloc() fails you'll get a NULL pointer back!
  //--
  char *pszDst = MyAlloc(strlen(pszSrc)+1);
  strcpy(pszDst, pszSrc);
  return pszDst;
}

char *MyStrPad (const char *pszSrc, int nWidth)
{
  //++
  //   Copy the source string and pad it with spaces so that it is exactly
  // nWidth columns.  If nWidth is positive then the string is left justified
  // and padded on the right with spaces, and if nWidth is negative then it's
  // right justified and padded on the left.  If the length of the source
  // string itself exceeds nWidth then it will be truncated, on the right if
  // nWidth is positive and on the left if nWidth is negative.
  // 
  //   Note that the result is returned in a static buffer owned by this
  // routine, so calling it a second time will destroy the result from the
  // first call.  You can always call MyStrDup() on the result if you really
  // need it to be permanent.
  //--
  static string_t szResult;
  memset(szResult, ' ', MAXSTRING);
  szResult[MAXSTRING-1] = EOS;
  size_t cbSrc = strlen(pszSrc);
  if (nWidth > 0) {
     memcpy(szResult, pszSrc, cbSrc);  szResult[nWidth] = EOS;
  } else {
    size_t cbWidth = -nWidth;
    size_t cbStart = (cbSrc <= cbWidth) ? cbWidth -cbSrc : 0;
    const char *pStart = (cbSrc <= cbWidth) ? pszSrc : pszSrc + (cbSrc-cbWidth);
    strcpy(szResult+cbStart, pStart);
    szResult[cbWidth] = EOS;
  }
  return szResult;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////   SYMBOL TABLE ROUTINES   ////////////////////////////
////////////////////////////////////////////////////////////////////////////////


uint16_t HashCode (const char *pszName)
{
  //++
  //   This routine generates the hash code for a given identifier by
  // forming a RADIX-64 binary equivalent for the ordinal values of the
  // characters, and then dividing by the table length.  It's crude, but
  // good enough for now.  Note that the hashing results are markedly
  // better when the symbol table size (e.g. HASHSIZE) is prime!
  //--
  uint32_t Hash = 0;
  for (;  *pszName != EOS;  ++pszName)
    Hash = (Hash * (uint32_t) '_') + (uint32_t) (*pszName - ' ');
  return (uint16_t) (Hash % (uint32_t) HASHSIZE);
}

SYMBOL *LookupSymbol (const char *pszName, bool fEnter)
{
  //++
  //   This function performs a hashed search of the symbol table to find
  // the specified name.  If the name is in the table, then the address
  // of the corresponding symbol record is returned. If no match is found
  // (i.e the symbol is undefined) and fEnter is true, then a new symbol
  // table entry is created for this name. The new entry will have a type
  // of SF_UDF (undefined) and all other fields zero, and the caller can
  // change these (including the type!) to the correct values.  If the
  // name is not in the symbol table and fEnter is false, then NULL is
  // returned and nothing is entered into the table.
  //
  //   NOTE:  This is a fairly primitive hashed search with linear
  // probing...
  //--
  uint16_t nHash, nInitialHash;  SYMBOL *pNew;  char *pszNameCopy;

  //   Compute the hash code of this symbol and use it for the inital
  // stab into the table.  If that entry doesn't match, then search
  // forward from there.  If we find a NULL entry in the table, then we
  // can quit with the knowledge that this symbol isn't defined.
  nHash = nInitialHash = HashCode(pszName);
  while (Symbols[nHash] != NULL) {
    if (STREQL(Symbols[nHash]->pszName, pszName)) return Symbols[nHash];
    nHash = (nHash + 1) % HASHSIZE;
    if (nHash == nInitialHash) {
       //   The symbol table is completely full and this symbol isn't
       // in there (and, of course, there's no place to add it!).
       FatalError("symbol table full");
    }
  }

  //   This symbol isn't defined, but nHash points to where it should
  // go if it were defined.  Allocate memory for a new record and fill
  // it in with the symbol's name.  Note that we have to create a copy
  // of the name string, since we can't depend on the original being
  // permanent!
  if (!fEnter) return NULL;
  pNew = (SYMBOL *) MyAlloc(sizeof(SYMBOL));
  pszNameCopy = MyStrDup(pszName);
  pNew->pszName = pszNameCopy;  pNew->nType = SF_UDF;
  return Symbols[nHash] = pNew;
}

void AddReference (SYMBOL *pSym, bool fDefinition)
{
  //++
  //   This routine will add another entry to the cross reference chain
  // for the symbol.  It's fairly simple, but a couple of subtle things
  // happen here - first, since we always process the source lines in
  // order (well, duh!) we don't need to sort the CREF chains _if_ we're
  // careful to always add new references to the end of the chain rather
  // than the beginning.  The SYMBOL structure keeps separate pointers to
  // the beginning and end of the CREF chain for just that reason. Second,
  // it's possible for the same source line to reference the same symbol
  // more than once, but in that case we don't want duplicate entries in
  // the chain.  Since the list is in order, if the current source line
  // already added a reference to this symbol we know it will be the last
  // one on the list, and we don't have to do an expensive search of the
  // entire CREF chain to find it. Finally, we accumulate cross reference
  // data only under pass 2 - otherwise we'd have a whole set of complete
  // duplicates from each pass!
  //--
  CREF *pNew;
  if (nPass != 2) return;
  // If we already have a reference to this line, then quit...
  if ((pSym->pLastRef!=NULL) && (pSym->pLastRef->nLine==nSourceLine)) return;
  // Create a new CREF block...
  pNew = (CREF *) MyAlloc(sizeof(CREF));
  pNew->nLine = nSourceLine;  pNew->fDefinition = fDefinition;
  // Add it to the end of the chain...
  if (pSym->pLastRef != NULL) {
    pSym->pLastRef->pNext = pNew;  pSym->pLastRef = pNew;
  } else {
    // If there's no pLastRef, we know there must be no pFirstRef!
    pSym->pFirstRef = pSym->pLastRef = pNew;
  }
}

void AddTOC (const char *pszTitle)
{
  //++
  //   We keep a table of contents (TOC), which is a simple list of every
  // title string we find along with the listing page number where it
  //  occurs. After pass 2, the entire TOC is printed at the end of the
  // listing file.  It'd look better if it appeared at the beginning of
  // the listing, of course, but that would mean we'd have to accumulate
  // the TOC information on pass 1, and there's no convenient way to know
  // the listing page numbers on that pass.  If you print the listing you
  // can always physically move the TOC page back to the beginning!
  //--
  TOC *pTOC;

  // Create a new TOC structure and fill out all the fields...
  pTOC = (TOC *) MyAlloc(sizeof(TOC));
  pTOC->pszTitle = MyStrDup(pszTitle);

  //   The page number we put in the TOC is the current listing page,
  // unless fNewPage is set.  That means we also found a form feed on
  // this source line (which happens all the time with .TITLE!) and in
  // that case this line will actually start the _next_ page...
  pTOC->nPage = fNewPage ? nListPages+1 : nListPages;

  //   Link this entry into the TOC.  Since we want the listing to be
  // sorted by page number, we have to always add new entries to the
  // _end_ of the list rather than the beginning...
  if (pLastTOC == NULL) {
    pTOC->pNext = NULL;  pFirstTOC = pLastTOC = pTOC;
  } else {
    pLastTOC->pNext = pTOC;  pLastTOC = pTOC;
  }
}

// Forward declarations for all pseudo operations...
void DotEND    (char *);  void DotORG    (char *);  void DotDATA   (char *);
void DotTITLE  (char *);  void DotASCIZ  (char *);  void DotTEXT   (char *);
void DotBLOCK  (char *);  void DotSIXBIT (char *);  void DotMRI    (char *);
void DotPAGE   (char *);  void DotFIELD  (char *);  void DotIM6100 (char *);
void DotHD6120 (char *);  void DotVECTOR (char *);  void DotSTACK  (char *);
void DotPUSH   (char *);  void DotPOP    (char *);  void DotPUSHJ  (char *);
void DotPOPJ   (char *);  void DotDEFINE (char *);  void DotIFDEF  (char *);
void DotIFNE   (char *);  void DotIFLT   (char *);  void DotIFGT   (char *);
void DotIFEQ   (char *);  void DotIFLE   (char *);  void DotIFGE   (char *);
void DotIFDEF  (char *);  void DotIFNDEF (char *);  void DotERROR  (char *);
void DotNOWARN (char *);  void DotLIST   (char *);  void DotNOLIST (char *);
void DotNLOAD  (char *);  void DotEJECT  (char *);  void DotSIXBIZ (char *);
void DotENABLE (char *);  void DotDISABLE(char *);

void InitializeSymbols (void)
{
  //++
  //   This function is called once, at startup, to add all the built in
  // names to the symbol table, including both machine instructions and
  // pseudo operations.  We'd like these to be compiled into a static
  // table, of course, but with a hash table that's not easily done...
  //--
  SYMBOL *pSym;  uint16_t i;
  for (i = 0;  i < HASHSIZE;  ++i)  Symbols[i] = NULL;

  // These macros reduce the amount of typing...
#define SYM(n,v,t)      \
  pSym = LookupSymbol(n, true);  pSym->nType = t;  pSym->Value.bin = v;
#define MRI(n,v)        SYM(n, v, OP_MRI)
#define OPR(n,v)        SYM(n, v, OP_OPR)
#define IOT(n,v)        SYM(n, v, OP_IOT)
#define PIE(n,v)        SYM(n, v, OP_PIE)
#define PIO(n,v)        SYM(n, v, OP_PIO)
#define CXF(n,v)        SYM(n, v, OP_CXF)
#define EQU(n,v)        SYM(n, v, SF_EQU)
#define POP(n,v)        \
  pSym = LookupSymbol(n, true);  pSym->nType = PO_POP;  pSym->Value.pop = v;

  // PDP-8 memory reference instructions
  MRI("AND",  00000);   MRI("TAD",  01000);   MRI("ISZ",  02000);
  MRI("DCA",  03000);   MRI("JMS",  04000);   MRI("JMP",  05000);

  // PDP-8 operate instructions
  OPR("NOP",  07000);   OPR("IAC",  07001);   OPR("RAL",  07004);
  OPR("RTL",  07006);   OPR("RAR",  07010);   OPR("RTR",  07012);
  OPR("BSW",  07002);   OPR("CML",  07020);   OPR("CMA",  07040);
  OPR("CIA",  07041);   OPR("CLL",  07100);   OPR("STL",  07120);
  OPR("CLA",  07200);   OPR("GLK",  07204);   OPR("STA",  07240);
  OPR("HLT",  07402);   OPR("OSR",  07404);   OPR("SKP",  07410);
  OPR("SNL",  07420);   OPR("SZL",  07430);   OPR("SZA",  07440);
  OPR("SNA",  07450);   OPR("SMA",  07500);   OPR("SPA",  07510);
  OPR("LAS",  07604);   OPR("MQL",  07421);   OPR("MQA",  07501);
  OPR("SWP",  07521);   OPR("CAM",  07621);   OPR("ACL",  07701);

  // Standard PDP-8 memory extension instructions
  CXF("CDF",  06201);   CXF("CIF",  06202);   CXF("CXF",  06203);
  IOT("RDF",  06214);   IOT("RIF",  06224);   IOT("RIB",  06234);
  IOT("RMF",  06244);

  // Standard PDP-8 processor IOT instructions
  IOT("SKON", 06000);  IOT("ION",  06001);  IOT("IOF",  06002);
  IOT("SRQ",  06003);  IOT("GTF",  06004);  IOT("RTF",  06005);
  IOT("SGT",  06006);  IOT("CAF",  06007);

  // Pseudo operations...
  POP(".END",    DotEND);     POP(".ORG",    DotORG);
  POP(".DATA",   DotDATA);    POP(".TITLE",  DotTITLE);
  POP(".ASCIZ",  DotASCIZ);   POP(".BLOCK",  DotBLOCK);
  POP(".SIXBIT", DotSIXBIT);  POP(".SIXBIZ", DotSIXBIZ);
  POP(".MRI",    DotMRI);     POP(".NLOAD",  DotNLOAD);
  POP(".PAGE",   DotPAGE);    POP(".FIELD",  DotFIELD);
  POP(".HD6120", DotHD6120);  POP(".IM6100", DotIM6100);
  POP(".VECTOR", DotVECTOR);  POP(".STACK",  DotSTACK);
  POP(".PUSH",   DotPUSH);    POP(".POP",    DotPOP);
  POP(".PUSHJ",  DotPUSHJ);   POP(".POPJ",   DotPOPJ);
  POP(".TEXT",   DotTEXT);    POP(".DEFINE", DotDEFINE);
  POP(".IFDEF",  DotIFDEF);   POP(".IFNDEF", DotIFNDEF);
  POP(".IFEQ",   DotIFEQ);    POP(".IFNE",   DotIFNE);
  POP(".IFLT",   DotIFLT);    POP(".IFLE",   DotIFLE);
  POP(".IFGT",   DotIFGT);    POP(".IFGE",   DotIFGE);
  POP(".NOWARN", DotNOWARN);  POP(".ERROR",  DotERROR);
  POP(".LIST",   DotLIST);    POP(".NOLIST", DotNOLIST);
  POP(".ENABLE", DotENABLE);  POP(".DISABLE",DotDISABLE);
  POP(".EJECT",  DotEJECT);

  //   Accept .HM6120 as a synonym for .HD6120 for backward
  // compatibility with old (e.g. BTS6120!) source files.
  POP(".HM6120", DotHD6120);
}

void IntersilMnemonics (void)
{
  //++
  //   This routine will load the extra symbols used in the Intersil
  // manuals for the IM6100, IM6101, IM6102 and IM6103 parts...
  //--
  SYMBOL *pSym;

  // IM6101 Peripheral Interface Element (PIE) instructions
  PIE("READ1", 06000);  PIE("READ2", 06010);  PIE("WRITE1",06001);
  PIE("WRITE2",06011);  PIE("SKIP1", 06002);  PIE("SKIP2", 06003);
  PIE("SKIP3", 06012);  PIE("SKIP4", 06013);  PIE("RCRA",  06004);
  PIE("WCRA",  06005);  PIE("WCRB",  06015);  PIE("WVR",   06014);
  PIE("SFLAG1",06006);  PIE("SFLAG3",06016);  PIE("CFLAG1",06007);
  PIE("CFLAG3",06017);

  // IM6103 Parallel I/O (PIO) instructions
  PIO("SETPA", 06300);  PIO("CLRPA", 06301);  PIO("WPA",   06302);
  PIO("RPA",   06303);  PIO("SETPB", 06304);  PIO("CLRPB", 06305);
  PIO("WPB",   06306);  PIO("RPB",   06307);  PIO("SETPC", 06310);
  PIO("CLRPC", 06311);  PIO("WPC",   06312);  PIO("RPC",   06313);
  PIO("SKPOR", 06314);  PIO("SKPIR", 06315);  PIO("WSR",   06316);
  PIO("RSR",   06317);

  // IM6102 Memory Extension, DMA and clock (MEDIC) instructions
  IOT("LIF",  06254);
  IOT("CLZE", 06130);  IOT("CLSK", 06131);  IOT("CLOE", 06132);
  IOT("CLAB", 06133);  IOT("CLEN", 06134);  IOT("CLSA", 06135);
  IOT("CLBA", 06136);  IOT("CLCA", 06137);
  IOT("LCAR", 06205);  IOT("RCAR", 06215);  IOT("LWCR", 06225);
  CXF("LEAR", 06206);  IOT("REAR", 06235);  IOT("LFSR", 06245);
  IOT("RFSR", 06255);  IOT("WRVR", 06275);  IOT("SKOF", 06265);
}

void HarrisMnemonics (void)
{
  //++
  //   Like IntersilMnemonics(), this routine will load the special names
  // used by Harris in the 6120 documentation.  The other member of the
  // Harris family, the 6121, didn't have any special mnemonics!
  //--
  SYMBOL *pSym;

  // HD6120 "Extra" instructions
  OPR("R3L",  07014);  IOT("WSR",  06246);  IOT("GCF",  06256);
  IOT("PR0",  06206);  IOT("PR1",  06216);  IOT("PR2",  06226);
  IOT("PR3",  06236);  IOT("PRS",  06000);  IOT("PGO",  06003);
  IOT("PEX",  06004);  IOT("CPD",  06266);  IOT("SPD",  06276);

  // HD6120 stack (Yes - a PDP-8 with a stack!) instructions
  IOT("PPC1", 06205);  IOT("PPC2", 06245);  IOT("PAC1", 06215);
  IOT("PAC2", 06255);  IOT("RTN1", 06225);  IOT("RTN2", 06265);
  IOT("POP1", 06235);  IOT("POP2", 06275);  IOT("RSP1", 06207);
  IOT("RSP2", 06227);  IOT("LSP1", 06217);  IOT("LSP2", 06237);

  //   These flags are returned by the 6120 after a PRS instruction.
  // They aren't really opcodes, but these mnemonics do appear in the
  // Harris databooks, so I guess we'll define them!
  //EQU("BTSTRP", 04000);  EQU("PNLTRP", 02000);
  //EQU("PWRON",  00400);  EQU("HLTFLG", 00200);
}

int CompareSymbols (const void *p1, const void *p2)
{
  //++
  // Compare two symbols for sorting ...
  //--
  const SYMBOL *s1 = *((SYMBOL **) p1);
  const SYMBOL *s2 = *((SYMBOL **) p2);
  if ((s1 == NULL) && (s2 == NULL)) return  0;
  if ((s1 == NULL) && (s2 != NULL)) return  1;
  if ((s1 != NULL) && (s2 == NULL)) return -1;
  return strcmp((s1)->pszName, (s2)->pszName);
}

void SortSymbols (void)
{
  //++
  //   This function will sort the symbol table (using the qsort() from
  // the C RTL).  Since symbol table entries are normally placed based
  // on hash codes, this routine ruins that and LookupSymbol() will no
  // longer function after it is used.   SortSymbols() is called only
  // after assembly is completed and just before listing the symbols.
  //--
  qsort(&Symbols, HASHSIZE, sizeof(SYMBOL *), &CompareSymbols);
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////   LISTING FILE ROUTINES   ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

void GetSystemDate (char *pszText)
{
  //++
  // Return the system date in the format "dd-mmm-yy" ...
  //--
  time_t tTime;  struct tm *pTM;
  char *szMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                      "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  time(&tTime);  pTM = localtime(&tTime);
  sprintf(pszText, "%02d-%3s-%02d",
    pTM->tm_mday, szMonths[pTM->tm_mon], (pTM->tm_year % 100));
}

void GetSystemTime (char *pszText)
{
  //++
  // Return the system time in the format "hh:mm:ss" ...
  //--
  time_t tTime;  struct tm *pTM;
  time(&tTime);  pTM = localtime(&tTime);
  sprintf(pszText, "%02d:%02d:%02d", pTM->tm_hour, pTM->tm_min, pTM->tm_sec);
}

bool Flag (char ch)
{
  //++
  //   Errors in the source file are handled in a fairly trivial way -
  // lines which have errors will show one or more error characters next
  // to the line number in the listing file.  These error characters are
  // single character mnemonics for the type of error (e.g. "U" for an
  // undefined symbol, "X" for a syntax error, etc).  It's fairly simple
  // minded, but this is the traditional way of handling errors in PDP
  // assemblers!
  //
  //   A list of the error characters for the current source line is kept
  // in szErrorFlags, and this routine will add a new error to the list.
  // The error flags are then dumped to the listing file via List() and
  // reset to a null string for the next time around.
  //
  //   The user may specify, via the .NOWARN pseudo-op, a list of one or
  // more error codes that are to be ignored. If the error being reported
  // is one of those then we just forget it, but it's the user's job to
  // ensure that the correct code is being generated!
  //--
  size_t nLen = strlen(szErrorFlags);

  // If this is one of the errors to be ignored, then so be it...
  if (strchr(szIgnoredErrors, ch) != NULL) return false;

  // If this character is already in the flags, then don't add it twice!
  if (strchr(szErrorFlags, ch) != NULL) return false;

  // Otherwise, add this one to the list...
  if (nLen < sizeof(szErrorFlags)-1) {
    szErrorFlags[nLen] = ch;  szErrorFlags[nLen+1] = EOS;
  }

  // Count the total number of errors in this file
  ++nErrorCount;

  //   This function always returns false, which makes it convenient to
  // say things like "return Fail(ER_SYN)" elsewhere in this code.  The
  // return value of this function has no other significance...
  return false;
}

void NewPage (void)
{
  //++
  //   This routine starts a new page in the listing file and prints the
  // pretty header for it...
  //--
  string_t szDate, szTime;
  if (!fPaginate) return;
  // Get a timestamp for the listing...
  GetSystemDate(szDate);  GetSystemTime(szTime);
  // Start a new page and print our name and the page number...
  if (nListPages > 0)  fprintf(pListFile, "\f");
  fprintf(pListFile, "%s - %s V%d.%02d RLA ", PALX, TITLE, (VERSION / 100), (VERSION % 100));
  fprintf(pListFile, "%s %8s    Page %3d\n",
    MyStrPad(szDate, -(nColumnsPerPage-68)), szTime, ++nListPages);
  // Print the second line with the file name and program title...
  int nWidth = nColumnsPerPage / 2;
  fprintf(pListFile, "%s", MyStrPad(szProgramTitle, nWidth));
  fprintf(pListFile, "%s\n", MyStrPad(szSourceFile, -nWidth));
  // A few blank lines and we're ready...
  fprintf(pListFile, "\n");  nLinesThisPage = 3;
  fNewPage = false;
}

void ListLine (FILE *hList, uint16_t *pField, uint16_t *pAddress, uint16_t *pCode, bool fSource)
{
  //++
  //   All output to the listing file, with the exception of the new page
  // header and the symbol table dump, is done by this routine to give
  // everything consistent formatting.  This routine prints a line number
  // the error characters, if any, and then an address field, a generated
  // code file, and a source text field.  Any or all of the last four
  // items may be omitted when appropriate (e.g. lines which contain only
  // comments show only the source text but no address or code)...
  //--

  //   If this line comes from a macro expansion, then append a "+" to
  // the error flags just to make it clear where it came from.  Note some
  // code checks szErrorFlags for a null string to determine whether the
  // current line has any errors, so we can't just change the global value -
  // we've got to make a local copy.
  string_t szLocalErrors;
  strcpy(szLocalErrors, szErrorFlags);
  if (pCurrentMacro != NULL) {
    strcat(szLocalErrors, "+");
    if (!fListExpansions) {
      //   If we're not listing macro expansion text, then never list the
      // source code.  More over, if there's no address or generated code,
      // then don't list anything at all!
      if ((pAddress == NULL) && (pCode == NULL)) return;
      fSource = false;
    }
  }

  //   Print the error characters and, if we will be showing the source
  // text, the source line number as well...
  if (fSource && (pCurrentMacro == NULL))
    fprintf(hList, "%4d%-4s", nSourceLine, szLocalErrors);
  else
    fprintf(hList, "    %-4s", szLocalErrors);

  // Print the address field (in octal), if needed...
  if ((pField != NULL) && (pAddress != NULL))
    fprintf(hList, "%01o%04o", *pField, *pAddress);
  else
    fprintf(hList, "     ");
  fprintf(hList, "    ");

  // And print the generated code (also in octal)....
  if (pCode != NULL)
    fprintf(hList, "%04o", *pCode);
  else
    fprintf(hList, "    ");

  //   And finally print the source text, if it exists.  Note that if
  // do print the source text, we don't print a newline since there's
  // already one at the end of the source line!
  if (fSource) {
    fprintf(hList, /*"\t"*/ "   ");  fputs(szSourceText, hList);
  } else
    fprintf(hList, "\n");
}

void List (uint16_t *pField, uint16_t *pAddress, uint16_t *pCode, bool fSource)
{
  //++
  //   This function has the same parameters as ListLine(), however this
  // one will print the line both to the listing file and, if this line
  // contains any errors, to stderr.  After printing it always clears the
  // error flags...
  //--
  if ((++nLinesThisPage > nLinesPerPage) || fNewPage)  NewPage();
  ListLine(pListFile, pField, pAddress, pCode, fSource);
  if (strlen(szErrorFlags) > 0)
    ListLine(stderr, pField, pAddress, pCode, fSource);
  szErrorFlags[0] = EOS;
}

void ListCommandLine (void)
{
  //++
  //   Construct a facsimile of the original command line and print it
  // to the listing.  This can be very helpful if you need to recompile
  // 
}

void ListSummary (void)
{
  //++
  //   This function will write a summary of the program size and the
  // total error count to both the listing file and to stderr...
  //--
  // See if there's enough room on this page for the summary...
  nLinesThisPage += 5;
  if (nLinesThisPage > nLinesPerPage)  NewPage();
  fprintf(pListFile, "\n\n\n");

  // Print the program break message to the listing and stderr...
  fprintf(pListFile, "Program break is %05o\n", (nField<<12) + nPC);
  Message(           "Program break is %05o",   (nField<<12) + nPC);

  // Now summarize the number of errors found...
  if (nErrorCount > 0) {
    fprintf(pListFile, "%d error(s) detected\n", nErrorCount);
    Message(           "%d error(s) detected",   nErrorCount);
  } else {
    fprintf(pListFile, "No errors detected\n");
    Message(           "No errors detected");
  }
}

void ListSymbols (void)
{
  //++
  //   This routine dumps the symbol table along with the cross reference
  // information to the listing file.  Each symbol is listed, one per
  // line, with its value and a list of the source lines that refer to
  // it.  In the cross reference, the source line that defines a symbol
  // is indicated by an asterisk.  Undefined symbols and multiply defined
  // symbols are also listed and shown as such.  Built in symbols, such
  // as machine ops and pseudo ops, are also listed _if_ they are used at
  // least once in the source.  Even though the values of these symbols
  // aren't exciting, the cross reference information can be useful. Note
  // that this routine does NOT sort the symbol table - you'll probably
  // want to do that, by calling SortSymbols(), first.
  //--
  uint16_t nSymbol, nCREF;  SYMBOL *pSym;  CREF *pCREF;
  strcpy(szProgramTitle, "Symbol Table");  NewPage();  AddTOC(szProgramTitle);

  for (nSymbol = 0;  nSymbol < HASHSIZE;  ++nSymbol) {
    if ((pSym = Symbols[nSymbol]) == NULL) continue;

    // Print the symbol's type and value...
    switch (pSym->nType) {
      // User defined symbols...
      case SF_UDF:
        fprintf(pListFile, "%-10s -UDF-    ", pSym->pszName);
        break;
      case SF_MDF:
        fprintf(pListFile, "%-10s -MDF-    ", pSym->pszName);
        break;
      case SF_TAG:
        fprintf(pListFile, "%-10s %05o    ", pSym->pszName, (uint16_t) pSym->Value.bin);
        break;

      // Macros ...
      case SF_MACRO:
        if (pSym->pFirstRef == NULL) continue;
        fprintf(pListFile, "%-10s -MAC-    ", pSym->pszName);
        break;

      // Built in symbols...
      case OP_MRI:  case OP_OPR:  case OP_IOT:  case OP_PIE:
      case OP_PIO:  case OP_CXF:  case SF_EQU:  case SF_OPDEF:
        if (pSym->pFirstRef == NULL) continue;
        fprintf(pListFile, "%-10s  %04o    ", pSym->pszName, pSym->Value.bin);
        break;
      case PO_POP:
        if (pSym->pFirstRef == NULL) continue;
        fprintf(pListFile, "%-10s -POP-    ", pSym->pszName);
        break;

        // All other symbol types (are there any others?) are suppressed...
      default:
        continue;
    }

    // Now display the cross reference chain for this symbol...
    for (pCREF=pSym->pFirstRef, nCREF=0;  pCREF!=NULL;  pCREF=pCREF->pNext) {
      if (++nCREF > (nColumnsPerPage-20)/7) {
        fprintf(pListFile, "\n");  nCREF = 1;
        if (++nLinesThisPage > nLinesPerPage)  NewPage();
        fprintf(pListFile, "                    ");
      }
      fprintf(pListFile, "%6d%c", pCREF->nLine, (pCREF->fDefinition ? '*' : ' '));
    }

    // Finish this line and we're done...
    fprintf(pListFile, "\n");
    if (++nLinesThisPage > nLinesPerPage)  NewPage();

  }
}

void ListTOC (void)
{
  //++
  //   This routine dumps the table of contents to the listing file. It's
  // pretty simple, even though it does go to a little extra effort to
  // make the output pretty...
  //--
  TOC *pTOC;    char szBuffer[80];
  //   We always start the TOC listing on an odd numbered page.  That
  // way, if the listing is printed on double sided paper, the TOC will
  // be on an upward facing page.
  if ((nListPages & 1) != 0)
    {strcpy(szProgramTitle, "");  NewPage();}
  strcpy(szProgramTitle, "Table of Contents");  NewPage();
  for (pTOC = pFirstTOC;  pTOC != NULL;  pTOC = pTOC->pNext) {
    strcpy(szBuffer, pTOC->pszTitle);
    if ((strlen(szBuffer) & 1) != 0) strcat(szBuffer, " ");
    while (strlen(szBuffer) < 64) strcat(szBuffer, " .");
    if (++nLinesThisPage > nLinesPerPage)  NewPage();
    fprintf(pListFile, "\t%s%4d\n", szBuffer, pTOC->nPage);
  }
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////   MEMORY BITMAP ROUTINES   ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

void MarkBitMap (uint16_t nField, uint16_t nAddress)
{
  //++
  //   We keep a map, one bit per PDP-8 word, of all the words in the
  // 32KW PDP-8 memory space.  Every time a word is filled by assembled
  // code, we mark that location by setting its bit and at the end of the
  // assembly we can print a nice map of all the free memory locations.
  //--
  uint16_t  nIndex = ((nField << 12) | nAddress) / 8;
  uint8_t nMask = (uint8_t) (1 << (nAddress & 7));
  if ((abBitMap[nIndex] & nMask) != 0) Flag(ER_DUP);
  abBitMap[nIndex] |= nMask;
}

#ifdef UNUSED
bool CheckBitMap (uint16_t nField, uint16_t nAddress)
{
  //++
  //   This routine tests the memory bitmap entry for a specific location
  // and returns true if that location is marked as "in use" already...
  //--
  uint16_t  nIndex = ((nField << 12) | nAddress) / 8;
  uint8_t nMask = (uint8_t) (1 << (nAddress & 7));
  return (abBitMap[nIndex] & nMask) != 0;
}
#endif

void ClearBitMap (void)
{
  //++
  //   This clears all the bits in the memory map.  It's normally called
  // only once, at the very beginning of the assembly...
  //--
  memset(abBitMap, 0, sizeof(abBitMap));
}

uint16_t CountBitMapEmpty (uint16_t nStart)
{
  //++
  //   This routine will count and return the number of empty words in
  // the bitmap starting from nStart.  Note that this doesn't count the
  // exact number of words - it rounds off to the nearest multiple of 8
  // so it can count whole bytes!
  //--
  uint16_t nCount = 0;  nStart = nStart / 8;
  while (   ((nStart+nCount) < sizeof(abBitMap))
         && (abBitMap[nStart+nCount] == 0))
    ++nCount;
  return nCount*8;
}

void ListBitMapLine (uint16_t nStart)
{
  //++
  //   This routine writes one line of the memory map to the listing
  // file.  A line contains 64 bits corresponding to one half of a PDP-8
  // memory page.  A bit is a one if the word is used, and zero if it is
  // free.
  //--
  uint16_t nWord, i;
  fprintf(pListFile, "%05o/", nStart);
  // Each byte contains eight bits, for eight PDP-8 words...
  for (nWord = 0;  nWord < 64;  nWord += 8) {
    uint8_t b = abBitMap[(nStart+nWord)/8];
    fprintf(pListFile, " ");
    // Unfortunately there's no "%b", we we have to do it the hard way!
    for (i = 0;  i < 8;  ++i) {
      fprintf(pListFile, "%1d", b & 1);  b >>= 1;
    }
  }
  fprintf(pListFile, "\n");
}

void ListBitMap (void)
{
  //++
  //   This routine dumps the memory bitmap to the listing file in a
  // format that's familiar to anyone who has ever used the OS/8 BITMAP
  // utility.  Note that it displays the bitmap only for memory fields
  // that have at least one word actually used.
  //--
  uint16_t nField, nPage;
  strcpy(szProgramTitle, "Memory Map");
  fNewPage = true;  AddTOC(szProgramTitle);

  for (nField = 0;  nField < 8;  ++nField) {
    // If this whole field is unused, then just skip it completely...
    if (CountBitMapEmpty(nField<<12) >= 4096) continue;
    //   At least one page in this field is used, so we'll print all of
    // them.  Each page takes two lines at 64 words per line, and we
    // add a blank line in between, so a complete field takes two pages
    // in the listing.
    for (nPage = 0;  nPage < 32;  ++nPage) {
      if ((nPage == 0) || (nPage == 16))  NewPage();
      ListBitMapLine((nField<<12) | (nPage<<7));
      ListBitMapLine((nField<<12) | (nPage<<7) | 64);
      fprintf(pListFile, "\n");
    }
  }

  //fprintf(pListFile, "\n");
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////////   BINARY FILE ROUTINES   ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

void FlushBinary (void)
{
  //++
  //   This procedure will write the current contents of the binary file
  // buffer to the file. Don't forget to call it before closing the file!
  //--
  int nCount;
  nCount = _write(fdBinaryFile, abBinaryData, cbBinaryData);
  if ((uint16_t) nCount != cbBinaryData)
    FatalError("error writing %s", szBinaryFile);
  cbBinaryData = 0;
}

void PutBinary (uint8_t nByte)
{
  //++
  //   This procedure will write one byte to the binary file.  As a side
  // effect, it also accumulates a checksum of the bytes punched...
  //--
  if (cbBinaryData >= sizeof(abBinaryData))  FlushBinary();
  abBinaryData[cbBinaryData++] = nByte;
  //   Note: Leader/trailer bytes (0200) and field setting bytes (03xx)
  // are _not_ included in the checksum!!!
  if ((nByte & 0200) == 0)  nBinaryChecksum += nByte;
}

void PunchLeader (void)
{
  //++
  // Punch leader/trailer frames on binary "tape"...
  //--
  uint16_t i;
  for (i = 0;  i < 32;  ++i)  PutBinary(0200);
}

void PunchField (uint16_t nField)
{
  //++
  // Punch a field change frame to the binary "tape"...
  //--
  PutBinary((uint8_t) (0300 | (nField << 3)));
}

void Punch (uint16_t nAddress, uint16_t nCode)
{
  //++
  //   This function will write a word to the binary file.  This file is
  // kept in standard PDP-8 BIN loader format, which lets us store 12 bit
  // words, address, and field information, in 8 bit bytes.
  //--
  if (nAddress != (nLastBinaryAddress+1)) {
    PutBinary((uint8_t) (((nAddress >> 6) & 077) | 0100));
    PutBinary((uint8_t) (nAddress & 077));
  }
  nLastBinaryAddress = nAddress;
  PutBinary((uint8_t) ((nCode >> 6) & 077));
  PutBinary((uint8_t) (nCode & 077));
}

void PunchChecksum (void)
{
  //++
  // Punch the current checksum to the binary file...
  //--
  uint16_t nSum = nBinaryChecksum & 07777;
  PutBinary((uint8_t) (nSum >> 6));
  PutBinary((uint8_t) (nSum & 077));
  PunchLeader();  FlushBinary();
}

void OutputCode (uint16_t nCode, bool fList, bool fSource)
{
  //++
  //   This function will send the word of code to both the object file
  // and the listing and then increment the PC. This function is normally
  // used only by pseudo-ops such as .ASCIZ or .SIXBIT to show the extra
  // words of code generated, and the listing line will contain only the
  // address and generated code with no source text.  On pass 1, this
  // function increments the PC only and nothing is sent to either output
  // file.
  //--

  //   If this code is going to overwrite the literal pool, then cause
  // a P (page full) error.  However, if there are no literals on this
  // page then we want to let the PC flow freely from one page to the
  // next, because this lets us generate long strings of data or text
  // without needing arbitrary .PAGE statements to break it up.  If the
  // PC does flow onto the next page, however, we do have to be careful
  // to reset the literal base - just because there aren't any literals
  // on this page doesn't mean there won't be any on the next!
  if (nPC >= nLiteralBase) {
    if ((nLiteralBase & 0177) == 0)
      nLiteralBase = (nPC & 07600) + 0200;
    else
      Flag(ER_PAF);
  }
  //   If this word is already marked as used (if, for example, two
  // separate pieces of code are accidentally origined to the same
  // place) then flag it as an error...
  //if (CheckBitMap(nField, nPC)) Flag(ER_PAF);
  // List and punch the generated code...
  if (nPass == 2) {
    if (fList) List(&nField, &nPC, &nCode, fSource);
    Punch(nPC, nCode);  MarkBitMap(nField, nPC);
  }
  ++nPC;
}

void DumpLiterals(void)
{
  //++
  //   This routine will dump the current literal pool into the object
  // and the listing file.  This is done whenever the programmer uses the
  // .ORG pseudo-op to move to another page, or when we reach the end of
  // the source file.
  //--
  uint16_t nLoc, i;

  // there is no literal processing under pass 1....
  if (nPass != 2) return;

  // output all the literals that we have...
  for (nLoc = nLiteralBase;  (i = nLoc & 0177) != 0;  ++nLoc) {
    List(&nField, &nLoc, &(anLiteralData[i]), false);
    Punch(nLoc, anLiteralData[i]);  MarkBitMap(nField, nLoc);
  }
}

bool SetPC (uint16_t nNew)
{
  //++
  //   This function is similar to IncrementPC, except that it sets the
  // current location to the absolute value given.  If the new PC is on a
  // different page than the current PC, the literal pool must be dumped
  // and re-initialized before the location can be changed.  If the new
  // location specified is greater than 07777, an error is flagged and
  // nothing is changed.
  //--
  if (nNew > 07777) {
    Flag(ER_RAN);  return false;
  } else {
    //   There's a small (if there is such a thing) hack here - if the
    // previous page was exactly full and had no literals, then the PC
    // will actually be the first word of the _next_ page now.  If the
    // .ORG is also to the next page, then testing for the new PC and
    // the old PC on the same page actually succeeds. This is a problem
    // because the literal base never gets reset in that case. The hack
    // is to test the PC against the old literal base as well to see if
    // the previous page was indeed full!
    if (((nNew & 07600) != (nPC & 07600)) || (nPC == nLiteralBase)) {
      DumpLiterals();  nLiteralBase = (nNew & 07600) + 0200;
    }
    nPC = nNew;  return true;
  }
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////   SIMPLE LEXICAL FUNCTIONS   ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

// iseol - return true at the end of the parseable line...
bool iseol (char c) {return (((c) == ';')  || ((c) == '>') || ((c) == EOL) || ((c) == EOS)); }

// isid1 - return true if c is a legal start-of-identifier character...
bool isid1 (char c) {return (isalpha(c) || ((c)=='%') || ((c)=='$') || ((c)=='_'));}

// isid2 - return true if c is a legal identifier character...
bool isid2 (char c) {return (isid1(c) || isdigit(c) || ((c)=='.'));}

char SpanWhite (char **pszText)
{
  //++
  // Skip over any white space characters on the line ...
  //--
  // In assembly language, newline is not a white space!!
  while (isspace(**pszText)  &&  (**pszText != EOL)) ++*pszText;
  return **pszText;
}

void TrimString (char *pszText)
{
  //++
  // Trim trailing white space from a string...
  //--
  size_t cb = strlen(pszText);
  while ((cb > 0) && isspace(pszText[cb-1])) pszText[(cb--)-1] = EOS;
}

bool ScanName (char **pszText, char *pszName, uint16_t Max)
{
  //++
  //   This function will scan any string of alphanumeric characters from
  // the source line and return in in pszName. If the string is longer than
  // allowed by nMax, then only the first nMax-1 (to allow for a EOS) are
  // actually returned in pszName, _however_ they will all be scanned and
  // skipped.  Note that the first character of the identifier must be
  // alphabetic (i.e. not a digit).  If this routine can't find at least
  // one alphabetic character then it will return false and a null name.
  //--
  SpanWhite(pszText);  *pszName = EOS;
  if (!isid1(**pszText)) return false;
  while (isid2(**pszText)) {
    if (Max > 1)  {
      *pszName++ = toupper(**pszText);  --Max;
    }
    ++*pszText;
  }
  *pszName = EOS;
  return true;
}

bool ScanNumber (char **pszText, uint16_t *pnValue)
{
  //++
  //   This routine scans a number from the input line. and the number
  // may be in either the decimal or octal radixes, as indicated by a '.'
  // or  'd'  suffix for decimal, or 'b' for octal.  If no radix is given
  // then the octal radix will be used.  This procedure will return false
  // if the number is illegal for some reason (e.g. '8' or '9' in octal
  // number or if not even one digit can be found).
  //--
  bool fDecimal, fNull;
  uint16_t nOctal, nDecimal;

  // initialize everything...
  SpanWhite(pszText);
  nOctal = nDecimal = 0;  fDecimal = false;  fNull = true;

  // scan all the digits in the number...
  while (isdigit(**pszText)) {
    nDecimal = (nDecimal*10) + (**pszText-'0');
    nOctal   = (nOctal<<3) | (**pszText-'0');
    if (**pszText > '7')  fDecimal = true;
    fNull = false;  ++*pszText;
  }

  // is there a radix suffix ??
  if (fNull) return Flag(ER_IFN);
  if (toupper(**pszText) == 'B') {
    // B for Octal ?????
    if (fDecimal) return Flag(ER_IFN);
    *pnValue = nOctal;  ++*pszText;
  } else if ((toupper(**pszText) == 'D')  ||  (**pszText == '.')) {
    *pnValue = nDecimal;  ++*pszText;
  } else
    *pnValue = fDecimal ? nDecimal : nOctal;

  return true;
}

bool GetArgumentString (char **pszText, char *pszString, size_t nMax)
{
  //++
  //  This function parses the argument string for the .ASCIZ and .SIXBIT
  // pseudo-ops.  Either of these accepts a string of characters quoted
  // by the first non-blank character following the pseudo-op. It returns
  // false if there are any syntax errors in the statement...
  //--
  char chQuote;  size_t nLen=0;
  pszString[0] = EOS;

  // The first non-blank character is the string quote character...
  chQuote = SpanWhite(pszText);
  if (iseol(chQuote))  return Flag(ER_SYN);
  ++*pszText;

  // Accumulate the characters out of the string...
  while ((**pszText != chQuote) && (**pszText != EOS)) {
    if (nLen >= nMax-1) return Flag(ER_SYN);
    pszString[nLen++] = **pszText;  ++*pszText;
  }
  ++*pszText;
  if (!iseol(SpanWhite(pszText))) return Flag(ER_SYN);

  // Terminate the string and return its length...
  pszString[nLen] = EOS;
  return true;
}

bool ExpandEscapes (const char *pszOld, char *pszNew, size_t nMax)
{
  //++
  //   This function expands the escape sequences allowed in PALX strings
  // for the .SIXBIT, .ASCIZ and .TEXT pseudo ops.  The escape sequences
  // currently recognized are:
  //
  //      \r - replaced by a carriage return (015) character
  //      \n - replaced by a line feed (012) character
  //      \t - replaced by a horizontal tab (010) character
  //      \d - replaced by the current date
  //      \h - replaced by the current time
  //      \\ - replaced by a single backslash ("\") character
  //--
  string_t sz;  size_t nLen = 0;
  pszNew[0] = EOS;
  while (*pszOld != EOS) {

    // Any non-escape characters are just copied literally to the result...
    if (*pszOld != '\\') {
      if (nLen >= nMax-1) return Flag(ER_SYN);
      pszNew[nLen++] = *pszOld++;
      continue;
    }

    if (nLen >= nMax-1) return Flag(ER_SYN);
    switch ((*++pszOld)) {
      // These cases are all fairly easy...
      case 'r':  pszNew[nLen++] = (char) 015;  break;
      case 'n':  pszNew[nLen++] = (char) 012;  break;
      case 't':  pszNew[nLen++] = (char) 011;  break;

      // Date and time are a little harder...
      case 'd':  case 'h':
        if (*pszOld == 'd')  GetSystemDate(sz);  else GetSystemTime(sz);
        if ((strlen(sz)+nLen) >= nMax-1) return Flag(ER_SYN);
        strcpy(pszNew+nLen, sz);  nLen += strlen(sz);
        break;

      // Illegal escape sequence...
      default: return Flag(ER_SYN);
    }
    ++pszOld;
  }

  pszNew[nLen] = EOS;
  return true;
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////   ARITHMETIC EXPRESSION SCANNER   ///////////////////////
////////////////////////////////////////////////////////////////////////////////

// Forward references...
bool EvaluateExpression (char **pszText, uint16_t *pszValue);
bool EvaluateOpcode (char **pszText, SYMBOL *pOP, uint16_t *pnValue);

bool EvaluateSymbol (char **pszText, const char *pszName, uint16_t *pnValue)
{
  //++
  //   This function is used by the expression scanner to evaluate the
  // value of any symbol that it finds.  The symbol might be something
  // simple like a user defined label or equate, it might be an opcode
  // (e.g. "TAD xyz"), or it might be something undefined or illegal. One
  // way or another, this routine will compute a value for the symbol and
  // return it.  This function returns true for success and false for any
  // error...
  //--
  SYMBOL *pSym = LookupSymbol(pszName, true);
  // Under pass 2, add a cross reference entry for this symbol...
  AddReference(pSym, false);

  switch (pSym->nType) {

    //   Labels return a 12 bit address, minus the field information,
    // but if the field of the symbol does not match the current field,
    // give an "off field" warning.
    case SF_TAG:
      if ((((pSym->Value.bin) >> 12) & 7) != nField)  Flag(ER_OFF);
      *pnValue = (pSym->Value.bin) & 07777;  return true;

    // Equated symbols return their value, as-is...
    case SF_EQU:
      *pnValue = pSym->Value.bin;  return true;

    // Opcodes evaluate their operand and return a complete instruction...
    case SF_OPDEF:
    case OP_MRI:
    case OP_OPR:
    case OP_IOT:
    case OP_PIE:
    case OP_PIO:
    case OP_CXF:
      return EvaluateOpcode (pszText, pSym, pnValue);

    // Undefined and multiply defined symbols flag errors...
    case SF_UDF:
      *pnValue = 0;  Flag(ER_UDF);  return false;
    case SF_MDF:
      *pnValue = 0;  Flag(ER_MDF);  return false;

    // Any other symbol type is bad news...
    default:
      *pnValue = 0;  Flag(ER_SYM);  return false;
  }
}

bool EvaluateLiteral (char **pszText, uint16_t *pnValue)
{
  //++
  //   This function is invoked whenever the expression scanner finds a
  // literal value (i.e. "[...]").  This routine skips over the opening
  // left bracket (we know it's there!), evaluates the expression, enters
  // it into the literal table, and then returns the address of that
  // entry (which is the actual value of the literal!).
  //--
  uint16_t nValue, nLoc, i;

  // Skip the '[', evaluate the expression, and the match the ']'...
  assert(**pszText == '[');  ++*pszText;  *pnValue = 0;
  if (!EvaluateExpression(pszText, &nValue)) return Flag(ER_SYN);
  if (**pszText != ']') return Flag(ER_SYN);
  ++*pszText;


  //   See if this value is already in the literal table.  If it is,
  // then we can reuse the existing entry...
  for (nLoc = nLiteralBase;  (i = nLoc & 0177) != 0;  ++nLoc) {
    if (nValue == anLiteralData[i]) {
      *pnValue = nLoc;  return true;
    }
  }

  //   Otherwise enter this data into the literal pool and return its
  // address.  Be sure that the literals don't overrun the code already
  // generated on this page!
  if (nLiteralBase <= nPC+1)
    return Flag(ER_PAF);
  else {
    --nLiteralBase;  *pnValue = nLiteralBase;
    anLiteralData[nLiteralBase & 0177] = nValue;
    return true;
  }
}

bool EvaluateString (char **pszText, uint16_t *pnValue)
{
  //++
  //   This routine gets called when the expression scanner finds a quote
  // mark.  It will grab the next character, return its ASCII value (no
  // big trick there!), and then check the next character for the closing
  // quote mark.  If no closing quote is found then it flags an error and
  // returns false....
  //--
  assert(**pszText == '\"');  ++*pszText;
  *pnValue = (uint16_t) **pszText;  ++*pszText;
  if (fASCII8BIT) *pnValue |= 0200;
  if (**pszText != '\"') return Flag(ER_SYN);
  ++*pszText;
  return true;
}

bool EvaluateOperand (char **pszText, uint16_t *pnValue)
{
  //++
  //   This routine gets called by the expression scanner to evaluate
  // any form of arithmetic operand.  It looks at the next non-space
  // character to figure out what the operand might be - a number, a
  // quoted string, a symbol, etc... It returns true and the value of the
  // operand if all is well, and false if there's a problem.  In the
  // latter case an error code will have already been added via Flag().
  //--
  bool fNegative=false;  bool fComplement=false;  identifier_t szName;
  *pnValue = 0;

  // is this a leading sign (+, -, or ~) ???
  if ((SpanWhite(pszText) == '+')  ||  (**pszText == '-')  ||  (**pszText == '~')) {
    fNegative = **pszText == '-';
    fComplement = **pszText == '~';
    ++*pszText;  SpanWhite(pszText);
  }

  if (**pszText == '(') {
    // Handle nested expressions ...
    ++*pszText;
    if (!EvaluateExpression(pszText, pnValue)) return Flag(ER_SYN);
    if (**pszText != ')') return Flag(ER_SYN);
    ++*pszText;
  } else if ((**pszText == '*')  ||  (**pszText == '.')) {
    // return the current location
    *pnValue = nPC;  ++*pszText;
  } else if (**pszText == '[') {
    // literal expression
    if (!EvaluateLiteral(pszText, pnValue)) return false;
  } else if (**pszText == '\"') {
    // quoted string
    if (!EvaluateString(pszText, pnValue)) return false;
  } else if (isdigit(**pszText)) {
    // a number
    if (!ScanNumber(pszText, pnValue)) return false;
  } else if (isid1(**pszText)) {
    // an identifier
    ScanName(pszText, szName, sizeof(szName));
    if (!EvaluateSymbol(pszText, szName, pnValue)) return false;
  } else {
    // otherwise this must be an error...
    Flag(ER_SYN);  return false;
  }

  // Be sure to apply the sign to the number..
  if (fNegative) *pnValue = (4096-*pnValue) & 07777;
  if (fComplement) *pnValue = (~*pnValue) & 07777;
  return true;
}

bool EvaluateExpression (char **pszText, uint16_t *pnValue)
{
  //++
  //   This routine puts all those above ones together to scan a complete
  // arithmetic expression. This scanner will stop on the first character
  // that it can't interpret as an operator - in a normally formed expr-
  // ession this might be a ']', a ')', or the end of the line.  If any
  // errors are found it will return false and flag the appropriate error
  // character to the listing...
  //--
  uint16_t nOpnd;  char chOP;

  // We always start out with an operand...
  *pnValue = 0;
  if (!EvaluateOperand(pszText, pnValue)) return false;

  while (true) {

    //   The next non-blank character must be an operator, so if it's
    // not one we recognize then this is as far as we can parse...
    chOP = SpanWhite(pszText);
    if (    (chOP != '-')  &&  (chOP != '+')
         && (chOP != '&')  &&  (chOP != '|')
         && (chOP != '*')  &&  (chOP != '/')
         && (chOP != '%')                    ) return true;
    ++*pszText;

    //   If it's a legal operator, then evaluate the operand and
    // calculate the result...
    if (!EvaluateOperand(pszText, &nOpnd)) return false;
    switch (chOP) {
      case '+':  *pnValue = (*pnValue +       nOpnd)  & 07777;    break;
      case '-':  *pnValue = (*pnValue + (4096-nOpnd)) & 07777;    break;
      case '&':  *pnValue = (*pnValue & nOpnd)        & 07777;    break;
      case '|':  *pnValue = (*pnValue | nOpnd)        & 07777;    break;
      case '*':  *pnValue = (*pnValue * nOpnd)        & 07777;    break;
      case '/':  *pnValue = (*pnValue / nOpnd)        & 07777;    break;
      case '%':  *pnValue = (*pnValue % nOpnd)        & 07777;    break;
    }

  }
}

bool EvaluateMRI (char **pszText, const SYMBOL *pMRI, uint16_t *pnValue)
{
  //++
  //   This routine will parse an MRI (memory reference) instruction and
  // compute the final opcode.  It handles the indirect addressing flag
  // (@), scans the operand to determine its address, and decides whether
  // current page or zero page addressing is appropriate...
  //--
  uint16_t nAddress;
  *pnValue = pMRI->Value.bin;

  if (SpanWhite(pszText) == '@') {
    // Indirect addressing...
    *pnValue |= 0400;  ++*pszText;
  }

  // Evaluate the operand and figure out the addressing mode...
  if (!EvaluateExpression(pszText, &nAddress)) return false;
  if ((nAddress & 07600) == 0) {
    // Page zero addressing ...
    *pnValue |= nAddress;
  } else if ((nAddress & 07600) == (nPC & 07600)) {
    // Current page addressing ...
    *pnValue |= 0200 | (nAddress & 0177);
  } else
    return Flag(ER_SCT);

  return true;
}

uint16_t OPRGroup (uint16_t nOpcode)
{
  //++
  // Return the group (1, 2 or 3) of an operate micro instruction
  //--
  if ((nOpcode & 07400) == 07000) return 1;
  if ((nOpcode & 07401) == 07400) return 2;
  if ((nOpcode & 07401) == 07401) return 3;
  return 0;
}

bool EvaluateOPR (char **pszText, SYMBOL *pOPR, uint16_t *pnValue)
{
  //++
  //   This routine will parse an operate microinstruction and return its
  // final value.  Operate instructions can be combined on the same line
  // by simply writing them with spaces in between (e.g. "CLA CLL CML" or
  // "SPA SNA"), and because of that this function just keeps reading and
  // parsing names from the source line until it finds some special
  // character, such as EOS, ']', ';', etc...
  //--
  identifier_t szName;

  *pnValue = pOPR->Value.bin;
  while (true) {
    if (!ScanName(pszText, szName, sizeof(szName))) return true;
    pOPR = LookupSymbol(szName, true);  AddReference(pOPR, false);
    if (pOPR->nType != OP_OPR) return Flag(ER_MIC);
    //   Check for a combination of operate instructions from different
    // groups and flag an error. The CLA instruction is a special case,
    // since it exists in all three groups (and hence can be combined
    // with anything).  The symbol table, however, contains only the
    // group 1 version (opcode 7200) of CLA.
    if (    (*pnValue != 07200) && ((pOPR->Value.bin) != 07200)
         && (OPRGroup(*pnValue) != OPRGroup(pOPR->Value.bin))) Flag(ER_MIC);
    *pnValue |= pOPR->Value.bin;
  }
}

bool EvaluateCXF (char **pszText, const SYMBOL *pCXF, uint16_t *pnValue)
{
  //++
  //   This routine parses change field instructions (e.g. CIF, CDF, or
  // both). The argument is just the new field number, however PALX
  // differs from PAL8 in how it is handled.  In PALX the field number is
  // from 0 to 7 and we'll position it correctly in the instruction, but
  // in PAL8 the field number is simply OR'ed with the instruction so it
  // must be multiplied by 8 (e.g. 10, 20, 30, etc. in octal).
  //--
  uint16_t nField;
  if (!EvaluateExpression(pszText, &nField)) return false;
  if (nField > 7)  return Flag(ER_RAN);
  *pnValue = (pCXF->Value.bin) | (nField << 3);
  return true;
}

bool EvaluateEIO (char **pszText, const SYMBOL *pEIO, uint16_t *pnValue)
{
  //++
  //   This function evaluates the IM6101 (PIE) and IM6103 (PIO)
  // instructions.  These take an argument which is the select address
  // for the chip, and this address must be shifted over and positioned
  // correctly in the opcode...
  //--
  uint16_t nAddress;
  if (!EvaluateExpression(pszText, &nAddress)) return false;
  if (pEIO->nType == OP_PIE) {
    if ((nAddress == 0)  ||  (nAddress > 31)) return Flag(ER_RAN);
    *pnValue = (pEIO->Value.bin) | (nAddress << 4);
  } else if (pEIO->nType == OP_PIO) {
    if ((nAddress == 0)  ||  (nAddress > 3)) return Flag(ER_RAN);
    *pnValue = (pEIO->Value.bin) | (nAddress << 4);
  }
  return true;
}

bool EvaluateOpcode (char **pszText, SYMBOL *pOP, uint16_t *pnValue)
{
  //++
  //   This routine is called when we find that a machine instruction
  // needs to be evaluated.  This function will parse the operands for
  // the isntruction (if any) and assemble the complete opcode which is
  // returned in pnValue.  The parsing stops when it finds any character
  // that can't be interpreted as part of the instruction - usually it's
  // EOS, ';', ')' or ']', but it's up to the caller to verify that the
  // expected terminator is found.  As usual, this routine returns true
  // if all is well and false if there is some kind of syntax error...
  //--
  switch (pOP->nType) {

    // process a memory reference instruction...
    case OP_MRI:
    case SF_OPDEF:
      if (!EvaluateMRI(pszText, pOP, pnValue)) return false;
      break;

    // this routine processes an operate micro-instruction
    case OP_OPR:
      if (!EvaluateOPR(pszText, pOP, pnValue)) return false;
      break;

    // here to process a cdf or cif instruction...
    case OP_CXF:
      if (!EvaluateCXF(pszText, pOP, pnValue)) return false;
      break;

    // here to process a pie or pio instruction...
    case OP_PIE:
    case OP_PIO:
      if (!EvaluateEIO(pszText, pOP, pnValue)) return false;
      break;

    // here to precess an iot instructon...
    case OP_IOT:
      *pnValue = pOP->Value.bin;
      break;

    // should never get here, but ...
    default: return false;
  }

  return true;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////   LISTING CONTROL PSEUDO OPERATIONS   /////////////////////
////////////////////////////////////////////////////////////////////////////////

void DotTITLE (char *pszText)
{
  //++
  //   This routine is called to process the .TITLE pseudo-op.  We ignore
  // any white space after the pseudo-op, and then any remaining text on
  // the line becomes the title for this program.  The program title is
  // printed on the banner of every page in the listing.
  //--
  size_t nLen;

  //  This pseudo op doesn't generate any code, so we can safely ignore
  // it under pass 1...
  if (nPass == 1) return;

  // Make sure we have some kind of operand.
  if (iseol(SpanWhite(&pszText)))
    Flag(ER_SYN);
  else {
    // The rest of the line becomes the title.
    memset(szProgramTitle, 0, sizeof(szProgramTitle));
    strcpy(szProgramTitle, pszText);
    // Remove the newline from the end of the string...
    nLen = strlen(szProgramTitle);
    if ((nLen > 0) && (szProgramTitle[nLen-1] == '\n'))
      szProgramTitle[nLen-1] = EOS;
    // Create a new TOC entry for this string.
    AddTOC(szProgramTitle);
  }
  List(NULL, NULL, NULL, true);
}

void DotERROR (char *pszText)
{
  //++
  //   The .ERROR pseudo op always flags an error, which will cause the source
  // line to be listed on stderr.  This can be used with the .IFxxxx statements
  // to check for any user defined error conditions in the assembly.
  //--
  if (nPass == 2) {
    Flag(ER_ERR);  List(NULL, NULL, NULL, true);
  }
}

void DotNOWARN (char *pszText)
{
  //++
  //   The .NOWARN pseudo op gives a list of one or more error codes,
  // specified by their single letter mnemonic, which are to be ignored.
  // Needless to say, it's still the user's responsibility to ensure that
  // the right code is being generated!
  //--
  uint16_t nLen;  char ch;
  szIgnoredErrors[0] = EOS;  nLen = 0;  ch = SpanWhite(&pszText);

  //   The remainder of the line after .NOWARN specifies one or more
  // error letters to be ignored, optionally separated by spaces. Note
  // that a null argument isn't illegal - it simply says that all
  // errors are to be reported!
  while (!iseol(ch)) {
    if (!isalpha(ch)) Flag(ER_SYN);
    szIgnoredErrors[nLen++] = toupper(ch);  szIgnoredErrors[nLen] = EOS;
    ++pszText;  ch = SpanWhite(&pszText);
  }

  if (nPass == 2)  List(NULL, NULL, NULL, true);
}

void ListOptions (char *pszText, bool fEnable)
{
  //++
  //   The .LIST and .NOLIST pseudo ops allow various listing options to be
  // enabled or suppressed.  Each pseudo op should be followed by one or more
  // mnemonics for the option to be enabled/disabled.
  // 
  //    MET     - macro expansion text (binary is always listed!)
  //    TXB     - list binary for .TEXT/.ASCIZ/.SIXBIT and .DATA
  //    TOC     - table of contents
  //    MAP     - memory bitmap
  //    SYM     - symbol table including cross reference
  //    PAG     - paginate the listing
  //    ALL     - enable everything (.LIST only!)
  //--
  identifier_t szName;
  while (true) {
    if (!ScanName(&pszText, szName, IDLEN)) {Flag(ER_SYN);  break;}
    if      (STREQL(szName, "MET"))  fListExpansions = fEnable;
    else if (STREQL(szName, "TXB"))  fListText       = fEnable;
    else if (STREQL(szName, "TOC"))  fListTOC        = fEnable;
    else if (STREQL(szName, "MAP"))  fListMap        = fEnable;
    else if (STREQL(szName, "SYM"))  fListSymbols    = fEnable;
    else if (STREQL(szName, "PAG"))  fPaginate       = fEnable;
    else if (STREQL(szName, "ALL") && fEnable)
      fListExpansions = fListTOC = fListMap = fListSymbols = fListText = true;
    else Flag(ER_LST);
    if (SpanWhite(&pszText) != ',') break;
    ++pszText;
  }
  if (!iseol(SpanWhite(&pszText))) Flag(ER_SYN);
  if (nPass == 2) List(NULL, NULL, NULL, true);
}
void DotLIST (char *pszText) {ListOptions(pszText, true);}
void DotNOLIST (char *pszText) {ListOptions(pszText, false);}

void DotEJECT (char *pszText)
{
  //++
  //   .EJECT starts a new listing page and is equivalent to a form feed
  // character in the source file.  There are no operands and it couldn't be
  // easier.  EXCEPT, remember that the new page is started AFTER the .EJECT
  // (i.e. .EJECT is the last line on the previous page, NOT the first line
  // on the new page!).
  //--
  if (!iseol(SpanWhite(&pszText))) Flag(ER_SYN);
  if (nPass == 2) List(NULL, NULL, NULL, true);
  fNewPage = true;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////   CODE GENERATING PSEUDO OPERATIONS   /////////////////////
////////////////////////////////////////////////////////////////////////////////

void DotASCIZ (char *pszText)
{
  //++
  //   This routine processes the .ASCIZ pseudo-op, which generates one
  // or more data words with each word containing a single ASCII char-
  // acter from the argument string...
  //--
  string_t szText, szData;  uint16_t i;

  // Parse the source statement...
  GetArgumentString (&pszText, szText, sizeof(szText));
  ExpandEscapes(szText, szData, sizeof(szData));

  // Under pass 2, list the line containg the text itself first...
  if (nPass == 2)  List(&nField, &nPC, NULL, true);

  //   And finally we can output the data.  Note that if there were any
  // syntax errors, the string will be null so nothing gets output...
  for (i = 0;  szData[i] != EOS;  ++i) {
    uint8_t bChar = szData[i] | (fASCII8BIT ? 0200 : 0);
    OutputCode((uint16_t) bChar, fListText, false);
  }

  // Be sure to terminate the string with a null byte...
  OutputCode(0, fListText, false);
}

void DotTEXT (char *pszText)
{
  //++
  //   The .TEXT pseudo op generates literal ASCII text strings, packed
  // three characters into two words in the standard OS/8 packing scheme.
  // Contrast this to .ASCIZ, which generates ASCII strings one character
  // per word.  Like .ASCIZ, .TEXT also marks the end of the string with
  // a null byte, however it always guarantees that an entire full twelve
  // bit word of zeros ends the string, not just a byte.
  //--
  string_t szText, szData;  uint16_t i;

  // Parse the source statement and, under pass 2, list it...
  GetArgumentString (&pszText, szText, sizeof(szText));
  ExpandEscapes(szText, szData, sizeof(szData));
  if (nPass == 2)  List(&nField, &nPC, NULL, true);

  if (fASCII8BIT) {
    for (i = 0; i < strlen(szData); ++i) szData[i] |= 0200;
  }

  for (i = 0;  i < strlen(szData);  i += 3) {
    int nLeft = strlen(&(szData[i]));
    if (nLeft >= 3) {
      OutputCode((((szData[i+2] >> 4) & 0xF) << 8) | szData[i],   fListText, false);
      OutputCode((( szData[i+2]       & 0xF) << 8) | szData[i+1], fListText, false);
    } else {
      if (nLeft > 0) OutputCode(szData[i],   fListText, false);
      if (nLeft > 1) OutputCode(szData[i+1], fListText, false);
    }
  }

  // Always end the string with a full word of zeros... */
  OutputCode(0, fListText, false);
}

void DoSIXBIT (char *pszText, bool fTerminate)
{
  //++
  //   This routine processes the .SIXBIT pseudo-op, which is essentially
  // the same as .ASCIZ except that it outputs pairs of SIXBIT characters
  // per word rather than single ASCII bytes.  Either the OS/8 SIXBIT code
  // or the better DECsystem-10 SIXBIT code can be used, depending on the
  // -8 command line option.  The difference is simple but significant -
  // OS/8 ANDs the ASCII character with 077 to get the SIXBIT code, but the
  // DECsystem-10 subtracts 040 (space) from the ASCII to get SIXBIT.  To
  // convert the DECsystem-10 version back to ASCII is easy, just add 040.
  // Converting the OS/8 version back to ASCII is harder, since you have
  // test whether the code is .GE. 40 (and do nothing) or .LT. 40 (and add
  // 100).
  // 
  //    In the case of .SIXBIT, no special terminating character is added
  // to the string.  A string of two characters, for example .SIXBIT /AB/,
  // generates exactly one word of code.  The .SIXBIZ peseudo-op is identical
  // to .SIXBIT, however it forces a special terminating byte or word.  In
  // OS/8 SIXBIT the terminator is zero, so the last word of the string will
  // be either cc00 (for an odd number of characters) or just 0000 (for an
  // even number).  In DECsystem-10 SIXBIT the terminator is 077 and so the
  // last word will be either cc77 or 7777, depending on whether the string
  // length is even or odd.
  //--
  string_t szText, szData;  uint16_t i, nByte, nCode = 0;  bool fFirst = true;

  // Parse the source statement...
  GetArgumentString (&pszText, szText, sizeof(szText));
  ExpandEscapes(szText, szData, sizeof(szData));

  //   Generate data for all the sixbit characters.  Note that we list the
  // source text for the first line, but none of the subsequent lines.
  for (i = 0;  szData[i] != EOS;  ++i) {
    char ch = toupper(szData[i]);
    if ((ch < ' ') || (ch > '_')) Flag(ER_TXT);
    nByte = fOS8SIXBIT ? (ch & 077) : (ch - ' ');
    if (i & 1) {
      nCode |= nByte;  OutputCode(nCode, fListText, fFirst);
      fFirst = false;
    } else
      nCode = nByte << 6;
  }

  // Handle the last word, generating a terminator if necessary ...
  if (i & 1) {
    if (fTerminate && !fOS8SIXBIT) nCode |= 077;
    OutputCode(nCode, fListText, fFirst);
  } else {
    if (fTerminate) OutputCode(fOS8SIXBIT ? 0 : 07777, fListText, false);
  }
}
void DotSIXBIT (char *pszText) {DoSIXBIT(pszText, false);}
void DotSIXBIZ (char *pszText) {DoSIXBIT(pszText, true);}

void DotBLOCK (char *pszText)
{
  //++
  //   This procedure implements the .BLOCK pseudo-op, which reserves one
  // or more words of memory for storage.  This pseudo-op actually causes
  // a jump in the binary file addresses, and the memory words reserved
  // are not initialized to any particular value.  Note that .BLOCK 0 is
  // specifically allowed as a no-op place holder...
  //--
  uint16_t nLen, i;

  // Evaluate the argument and if it's malformed, treat it as zero...
  if (!EvaluateExpression(&pszText, &nLen) || !iseol(*pszText))  nLen = 0;

  // Make sure the range doesn't exceed the current page... */
  if ((nPC + nLen) > nLiteralBase) {
    Flag(ER_PAF);
    nLen = (nLiteralBase > nPC) ? (nLiteralBase - nPC) : 0;
  }

  //   Even though .BLOCK doesn't generate any code, we still want to
  // show these locations as used in the bitmap!
  if (nPass == 2) {
    for (i = 0; i < nLen; ++i)  MarkBitMap(nField, nPC + i);
  }

  // List the line and bump the PC, but don't actually generate any code... */
  if (nPass == 2)  List(&nField, &nPC, NULL, true);
  nPC += nLen;
}

void DotDATA (char *pszText)
{
  //++
  //   This routine handles the .DATA pseudo-op, which generates one or
  // more words of data.  It isn't all that useful, since the line ".DATA
  // XYZ" is equivalent to just writing "XYZ", however one advantage is
  // that .DATA allows several words of code to be generated (e.g. ".DATA
  // A, B, C".
  //--
  uint16_t nCode, nWords, i;  char *p;

  //   There must be a better way!!  This doesn't even really work -
  // consider the statement ".DATA 1, [.DATA 2,3], 4" for example...

  //   The first thing we want to do is go thru and count the number of
  // words this statement will generate.  We do this by simply counting
  // the commas, but this is trickier than it sounds because there may
  // be ASCII strings in expressions that contain commas (e.g. .DATA 1,
  // ",", 2 ...).
  nWords = 1;  p = pszText;
  while (!iseol(*p)) {
    if (*p == ',') {
      ++nWords;  ++p;
    } else if (*p == '\"') {
      for (++p;  ((*p != '\"') && (*p != EOL));  ++p)  ;
      if (*p == '\"') ++p;
    } else
      ++p;
  }

  //   On pass 1, all we care about is the number of words generated so
  // we can just increment the PC and quit now.  In fact, we don't want
  // to evaluate the expressions because they likely contain symbols
  // that aren't defined yet on pass 1, and we don't want them entered
  // into the symbol table as undefined!
  if (nPass == 1) {nPC += nWords;  return;}

  //   Now go thru and parse all the expressions once, but don't store
  // any of the code words generated.  We do this so that if there are
  // any syntax errors in the expressions, the error characters will be
  // printed on the same line with the .DATA statement.  Just for fun,
  // we also count the number of data words we find this way - if it
  // doesn't agree with what we used on pass 1 then we flag an error.
  for (p = pszText, i = 1;  ;  ++i, ++p) {
    EvaluateExpression(&p, &nCode);
    if (iseol(SpanWhite(&p))) break;
    if (*p != ',')  Flag(ER_SYN);
  }
  if (i != nWords) Flag(ER_SYN);
  List(NULL, NULL, NULL, true);

  //   Now generate the actual binary code.  Note that we always _must_
  // generate exactly as many words as we found on pass 1, or we'll
  // cause phase errors in the assembly!
  for (p = pszText, i = 0;  i < nWords;  ++i, ++p) {
    if (!EvaluateExpression(&p, &nCode)) nCode = 0;
    OutputCode(nCode, fListText, false);
    if (iseol(SpanWhite(&p))) break;
  }
}

void DotNLOAD (char *pszText)
{
  //++
  //   The ".NLOAD num" pseudo operation evaluates its operand and generates
  // a combination of OPR instructions to load the AC with that value.  This
  // only works for certain special cases, as follows -
  // 
  // CONSTANT    OPCODE                         MODELS
  // --------    --------------------------     --------------------------------------
  //     0000    7200 CLA			all
  //     0001    7201 CLA IAC			all
  //     0002    7326 CLA CLL CML RTL		all
  //     2000    7332 CLA CLL CML RTR		all
  //     3777    7350 CLA CLL CMA RAR		all
  //     4000    7330 CLA CLL CML RAR		all
  //     5777    7352 CLA CLL CMA RTR		all
  //     7775    7352 CLA CLL CMA RTL		all
  //     7776    7346 CLA CLL CMA RAL		all
  //     7777    7240 CLA CMA			all
  //     0003    7325 CLA CLL CML IAC RAL	IM6100, HD6120, PDP-8/I and up
  //     0004    7307 CLA CLL IAC RTL		IM6100, HD6120, PDP-8/I and up
  //     0006    7327 CLA CLL CML IAC RTL	IM6100, HD6120, PDP-8/I and up
  //     6000    7333 CLA CLL CML IAC RTR	IM6100, HD6120, PDP-8/I and up
  //     0100    7203 CLA IAC BSW		IM6100, HD6120, PDP-8/E and up
  //     0010    7315 CLA CLL IAC R3L		HD6120 only
  //
  // NOTE:
  //   -3 = 7775 -> NL7775
  //   -2 = 7776 -> NL7776
  //   -1 = 7777 -> NL7777
  // 
  //   If the .NLOAD argument is not one of these magic values, an "A" error
  // occurs and we generate a NOP (7000) opcode.
  //--

  //   On pass 1, all we care about is the number of words generated so
  // we can just increment the PC and quit now.
  if (nPass == 1) {++nPC;;  return;}

  // Evaluate the argument (there had better be only one!)
  uint16_t nValue = 0, nCode = 07000;
  EvaluateExpression(&pszText, &nValue);
  if (!iseol(SpanWhite(&pszText))) Flag(ER_SYN);

  // Figure out the correct OPR instruction to generate this value ...
  switch (nValue) {
    case 00000:  nCode = 07200;	 break;  // CLA
    case 00001:  nCode = 07201;	 break;  // CLA IAC
    case 00002:  nCode = 07326;	 break;  // CLA CLL CML RTL
    case 02000:  nCode = 07332;	 break;  // CLA CLL CML RTR
    case 03777:  nCode = 07350;	 break;  // CLA CLL CMA RAR
    case 04000:  nCode = 07330;	 break;  // CLA CLL CML RAR
    case 05777:  nCode = 07352;	 break;  // CLA CLL CMA RTR
    case 07775:  nCode = 07346;	 break;  // CLA CLL CMA RTL
    case 07776:  nCode = 07344;	 break;  // CLA CLL CMA RAL
    case 07777:  nCode = 07240;	 break;  // CLA CMA
    case 00003:  nCode = 07325;	 break;  // CLA CLL CML IAC RAL
    case 00004:  nCode = 07307;	 break;  // CLA CLL IAC RTL
    case 00006:  nCode = 07327;	 break;  // CLA CLL CML IAC RTL
    case 06000:  nCode = 07333;	 break;  // CLA CLL CML IAC RTR
    case 00100:  nCode = 07203;	 break;  // CLA IAC BSW
    case 00010:  if (nCPU == 6120) {
    		  nCode = 07315;	 // CLA CLL IAC R3L
		 } else {
		  nCode = 07000;  Flag(ER_RAN);
		 }
		 break;
    default:  nCode = 07000;  Flag(ER_RAN);  break;
  }
  OutputCode(nCode, true, true);
}

////////////////////////////////////////////////////////////////////////////////
///////////////////   6100/6120 SPECIAL PSEUDO OPERATIONS   ////////////////////
////////////////////////////////////////////////////////////////////////////////

void ChangeCPU (char *pszText, uint16_t n, void (*pMnemonics) (void))
{
  //++
  // DotIM6100 and DotIM6120
  //   These pseudo ops select the CPU (either the Intersil IM6100 or the
  // Harris HD6120) and add the appropriate CPU specific symbols to the
  // symbol table. We also remember the current CPU in nCPU in case other
  // pseudo ops want to behave differently on differnt CPUs, but none
  // currently do!
  //--
  if (iseol(SpanWhite(&pszText))) {
    (*pMnemonics)();  nCPU = n;
  } else
    Flag(ER_SYN);
  if (nPass == 2) List(NULL, NULL, NULL, true);
}
void DotIM6100 (char *pszText) {ChangeCPU(pszText, 6100, &IntersilMnemonics);}
void DotHD6120 (char *pszText) {ChangeCPU(pszText, 6120, &HarrisMnemonics);}

void DotVECTOR (char *pszText)
{
  //++
  //   The .VECTOR pseudo op generates a reset vector for the IM6100 and
  // HD6120 microprocessors.  On reset, these chips set the PC to 7777
  // and execute whatever instruction is there - the convention being to
  // put a JMP to actual program start in that location.  If the start
  // address is also on the current page, .VECTOR generates a simple JMP
  // to this address in the last location of the page.  If the start
  // address is on another page, .VECTOR generates two words of code - a
  // JMP indirect in the last location of the page, and the actual start
  // address in the next to last location.
  //
  //   Normally this pseudo-op would be used in code page 7600, however
  // this implementation does not require that and .VECTOR may be used on
  // any page.  It works the same way regardless of the page - the JMP
  // intstruction goes in the last word on the page, and the indrect
  // vector (if any) in the second to last location.  This is necessary
  // for systems like the JR2K, where some fancy EPROM mapping means that
  // the code which executes on page 7600 isn't actually on page 7600 of
  // the EPROM image.
  //
  //   You might be wondering why we need a special pseudo op for this
  // at all - can't we do the same thing with a .ORG or two?  Well, you
  // can but it effectively prevents you from using any literals on page
  // 7600.  The advantage to .VECTOR is that it actually takes advantage
  // of the literal pool to generate the vectors, and still leaves it
  // available for other code on page 7600 to use.  It also looks cool!
  //--
  uint16_t nVector = 0;  uint16_t nCurrentPage = nPC & 07600;

  // This can't be used on a "generic" PDP-8...
  if (nCPU == 0) Flag(ER_POP);

  //   If we have already assembled some literals on this page, then
  // we're going to overwrite some of those literals.  There's nothing
  // we can do to avoid this, but we can at least generate an error
  // message so the user knows he messed up..
  if (nLiteralBase != (nCurrentPage+0200)) Flag(ER_PAF);

  //   On pass 2, try to evaluate the actual vector address.  On pass 1
  // we don't care what it is...
  if (nPass == 2) {
    if (!EvaluateExpression(&pszText, &nVector) || !iseol(*pszText)) Flag(ER_SYN);
  }

  //   And now the magic part.  If the startup address is on this page,
  // then put a literal in the last location with a simple JMP (5200)
  // to this location.  If the startup address is on another page, then
  // put an indirect JMP (5776) in the last location and the actual
  // address in second to last location.  In either case, leave the
  // literal base set just below the words we generated so other
  // literals can be generated on this page..
  if ((nVector & 07600) != nCurrentPage) {
    anLiteralData[0177] = 05776;
    anLiteralData[0176] = nVector;
    nLiteralBase = nCurrentPage+0176;
  } else {
    anLiteralData[0177] = 05200 | (nVector & 0177);
    nLiteralBase = nCurrentPage + 0177;
  }

  //   There's no need to actually output any code here - sooner or
  // later we will get around to calling DumpLiterals() and that will
  // actually output the vectors.

  // Under pass two, list this line and we're done.
  if (nPass == 2) List(NULL, NULL, &nVector, true);
}

void DotSTACK (char *pszText)
{
  //++
  //  .STACK lets the programmer set the opcodes associated with the four
  // stack pseudo ops - .PUSH, .POP, .PUSHJ and .POPJ.  Because the 6120
  // has not one but _two_ hardware stacks, this pseudo op can be used to
  // select the stack in use.  Although the IM6100 (and a real PDP-8 for
  // that matter) don't have a hardware stack they can emulate one with
  // software and this same pseudo op can be used to set the instructions
  // (almost always JMS or JMPs) associated with the software simulation.
  // This makes it easy to port the same source code to either CPU.
  //--

  //   This pseudo op doesn't generate any code, so on pass 1 we can
  // safely skip it...
  if (nPass == 1) return;
  if (nCPU == 0) Flag(ER_POP);

  // There always have to be exactly four operands...
  if (!EvaluateExpression(&pszText, &nPUSH)  || (*pszText++ != ',')) Flag(ER_SYN);
  if (!EvaluateExpression(&pszText, &nPOP)   || (*pszText++ != ',')) Flag(ER_SYN);
  if (!EvaluateExpression(&pszText, &nPUSHJ) || (*pszText++ != ',')) Flag(ER_SYN);
  if (!EvaluateExpression(&pszText, &nPOPJ)  || !iseol(*pszText))    Flag(ER_SYN);

  //   List the four opcodes defined, and that's all until we encounter
  // a .PUSH, .POP, .PUSHJ or .POPJ...
  List(NULL, NULL, NULL, true);
  List(NULL, NULL, &nPUSH, false);
  List(NULL, NULL, &nPOP, false);
  List(NULL, NULL, &nPUSHJ, false);
  List(NULL, NULL, &nPOPJ, false);
}

void StackFunction (char *pszText, uint16_t nOpcode)
{
  //++
  // DotPUSH, DotPOP, and DotPOPJ
  //   These pseudo ops output the opcode for stack operations, as set by
  // the .STACK pseudo op.  They're all basically the same and don't
  // really need to be a pseudo op - a plain, old fashioned symbol table
  // entry would suffice.  However there's no easy way in PALX to get the
  // pseudo op notation (e.g. ".XYZ") for a plain symbol table entry, so
  // we'll have to invest a few lines of code...
  //--
  
  // There should never be any arguments for this one...
  if (nCPU == 0) Flag(ER_POP);
  if (!iseol(SpanWhite(&pszText))) Flag(ER_SYN);
  // If the stack hasn't been defined with .STACK, then flag an error.
  if (nOpcode == 0) Flag(ER_POP);
  OutputCode(nOpcode, true, true);
}
void DotPUSH (char *pszText) {StackFunction(pszText, nPUSH);}
void DotPOP  (char *pszText) {StackFunction(pszText, nPOP);}
void DotPOPJ (char *pszText) {StackFunction(pszText, nPOPJ);}

void DotPUSHJ (char *pszText)
{
  //++
  //  .PUSHJ is slightly more complicated, enough so that this one really
  // does need to be a pseudo op, because it takes an argument which is
  // the address of the subroutine to call.
  //
  //   On the 6120, .PUSHJ first outputs a word of code containing only
  // the opcode for PUSHJ as defined by.STACK, and then a second word
  // with a JMP instruction to the subroutine entry point.  This is
  // because the 6120 hardware PPC instruction just pushes the PC and
  // then expects the user to provide their own JMP inline.  The argument
  // to.PUSHJ works exactly as the operand for a JMP instruction, and if
  // it is off page then then it must be either a zero page reference or
  // an "@[...]" link.
  //
  //   On the 6100, the stack is implemented entirely in software and the
  // PUSHJ instruction is just a JMS the subroutine call routine. In this
  // case the convention is to put the complete 12 bit address of the
  // destination in the next word after the PUSHJ.  No JMP in needed. In
  // theory we could make the 6100 work the same way as the 6120, but
  // this plan saves a word of code when the destination is off page.
  //--
  uint16_t nJMP;

  //  This always generates two words of code, so on pass 1 we can just
  // leave it at that...
  if (nPass == 1) {nPC += 2;  return;}
  if (nCPU == 0) Flag(ER_POP);

  //   Output the opcode for PUSHJ.  If it hasn't been defined yet by a
  // .STACK invocation, then flag an error too.
  if (nPUSHJ == 0) Flag(ER_POP);
  OutputCode(nPUSHJ, true, true);

  // Now evaluate the operand and generate a second word of code...
  if (nCPU == 6100) {
    if (!EvaluateExpression(&pszText, &nJMP) || !iseol(*pszText)) Flag(ER_SYN);
    OutputCode(nJMP, true, false);
  } else if (nCPU == 6120) {
    SYMBOL *pJMP = LookupSymbol("JMP", false);
    EvaluateMRI(&pszText, pJMP, &nJMP);
    if (!iseol(SpanWhite(&pszText))) Flag(ER_SYN);
    OutputCode(nJMP, true, false);
  }
}


////////////////////////////////////////////////////////////////////////////////
///////////////////   6100/6120 SPECIAL PSEUDO OPERATIONS   ////////////////////
////////////////////////////////////////////////////////////////////////////////

void DotFIELD (char *pszText)
{
  //++
  //   The ".FIELD n" pseudo op changes the current assembly field to n.
  // Since almost all the assembly is don in terms of twelve bit PDP-8
  // addresses, this has suprisingly little effect.  About all it does is
  // output a field change frame to the binary file and reset the PC to
  // 0200 of the new field...
  //--
  uint16_t nNew=0;
  if (EvaluateExpression(&pszText, &nNew) && iseol(*pszText)) {
    if (nNew < 010) {
      //   We have to be a little careful about the order we do things -
      // first, we set the PC off page to force out the literal pool, then
      // we output a field change frame, and finally we reset the origin to
      // 0200 in the new field!
      SetPC(0);  nField = nNew;
      if (nPass == 2) PunchField(nField);
      SetPC(0200);
    } else
      Flag(ER_RAN);
  } else
    Flag(ER_SYN);
  if (nPass == 2)  List(&nField, &nPC, NULL, true);
}

void DotORG (char *pszText)
{
  //++
  //   This routine handles the .ORG pseudo-op, which takes a single
  // numeric argument that is the new PC.  If the new location is in a
  // different page, then the current literal pool is dumped first...
  //--
  uint16_t nLoc=0;

  if (EvaluateExpression(&pszText, &nLoc) && iseol(*pszText))
    SetPC(nLoc);
  else
    Flag(ER_SYN);
  if (nPass == 2)  List(&nField, &nPC, NULL, true);
}

void DotPAGE (char *pszText)
{
  //++
  //   The .PAGE pseudo-op performs essentially the same function as ORG,
  // but in this case we work with PDP-8 code pages instead. A .PAGE with
  // no operand advances the location counter to the start of the next
  // code page, and ".PAGE n" moves to the start of page number n.
  //--
  uint16_t nPage=0;

  if (iseol(SpanWhite(&pszText)))
    SetPC((nPC + 0177) & 07600);
  else if (EvaluateExpression(&pszText, &nPage) && iseol(*pszText))
    SetPC(nPage << 7);
  else
    Flag(ER_SYN);
  if (nPass == 2)  List(&nField, &nPC, NULL, true);
}

void DotMRI (char *pszText)
{
  //++
  //   This function processes .MRI, which allows user define MRI style
  // instructions to be added to the symbol table.  The syntax of this
  // statement goes something like ".MRI MYMRI=1000" ...
  //--
  identifier_t szName;  SYMBOL *pSym;  uint16_t nValue=0;

  // Parse the name of the symbol and then its value...
  if (   ScanName(&pszText, szName, sizeof(szName))
      && (SpanWhite(&pszText) == '=')
      && !iseol(*++pszText)
      && EvaluateExpression(&pszText, &nValue)
      && iseol(*pszText)) {
    // The statement is syntactically legal - define the new MRI...
    pSym = LookupSymbol(szName, true);  AddReference(pSym, true);
    if (nPass == 1) {
      if (pSym->nType == SF_UDF) {
        pSym->nType = SF_OPDEF;  pSym->Value.bin = nValue;
      } else
        pSym->nType = SF_MDF;
    } else {
      if (pSym->nType != SF_OPDEF)  Flag(ER_SYM);
    }
  } else
    // The syntax is invalid
    Flag(ER_SYN);

  if (nPass == 2)  List(NULL, NULL, &nValue, true);
}

void DotEND (char *pszText)
{
  //++
  //   This routine processes the .END pseudo-op.  Suprisingly, this
  // currently does nothing - assembly ends when we reach the end of the
  // source file...
  //--
  if (!iseol(SpanWhite(&pszText))) Flag(ER_SYN);
  //   If there are literals on this page, then do the equivalent of a
  // .PAGE to get the literal pool dumped out in the right place...
  if ((nLiteralBase & 0177) != 0)  SetPC((nPC + 0177) & 07600);
  if (nPass == 2)  List(NULL, NULL, NULL, true);
}

void AssemblyOptions (char *pszText, bool fEnable)
{
  //++
  //   The .ENABLE and .DISABLE pseudo ops allow various assembly options to
  // be turned on or off.  Each pseudo op should be followed by one or more
  // mnemonics for the option to be enabled/disabled.
  // 
  //    OS8	- enable OS/8 coding for .SIXBIT/.SIXBIZ pseudo ops
  //    ASR	- enable ASR33 "always mark" coding for ASCII text
  //
  //  Note that ".ENABLE OS8" is equivalent to the "-8" command line option,
  // and "ASR" is equivalent to "-a".
  //--
  identifier_t szName;
  while (true) {
    if (!ScanName(&pszText, szName, IDLEN)) {Flag(ER_SYN);  break;}
    if      (STREQL(szName, "OS8"))  fOS8SIXBIT = fEnable;
    else if (STREQL(szName, "ASR"))  fASCII8BIT = fEnable;
    else Flag(ER_LST);
    if (SpanWhite(&pszText) != ',') break;
    ++pszText;
  }
  if (!iseol(SpanWhite(&pszText))) Flag(ER_SYN);
  if (nPass == 2) List(NULL, NULL, NULL, true);
}
void DotENABLE (char *pszText) {AssemblyOptions(pszText, true);}
void DotDISABLE (char *pszText) {AssemblyOptions(pszText, false);}


////////////////////////////////////////////////////////////////////////////////
/////////////////////   MACRO DEFINITION AND EXPANSION   ///////////////////////
////////////////////////////////////////////////////////////////////////////////

void CheckFormFeed (void)
{
  //++
  //   This function will check the current source line for the presence
  // of an ASCII form feed character.  If it finds one, then it deletes
  // the form feed and sets the fNewPage flag which will cause the List()
  // procedure to force a new page in the listing after we've processed
  // this line...
  //--
  char *p;
  while ((p=strchr(szSourceText, '\f')) != NULL) {
    //  You might be tempted to use strcpy() here, but the offical ANSI
    // standard says of strcpy() "the source and destination must not
    // overlap."  You might think "Nah - they're not serious.  What could
    // go wrong?"  Well, it works fine with MSVC (up to the 2022 version,
    // at least) and also with VMS/DECC. However with Linux and gcc it'll
    // work most of the time, but when the string has just the right length
    // you'll get some really wierd results.  Not kidding... We'll play it
    // safe and copy the characters the hard way...
    while ((*p=*(p+1)) != EOS) ++p;
    fNewPage = true;
  }
}

MACDEF *NewMacro (SYMBOL *pSym)
{
  //++
  //   Create a new macro definition (MACDEF) block and attach it to the
  // specified symbol table entry.  If the symbol is already defined (as
  // evidenced by nType != SF_UDF) AND it's already a macro definition,
  // then delete the previous body and reuse the MACDEF block.  This
  // allows macros to be redefined.  If the symbol is already defined
  // and it's NOT a macro, then that's an error and we return NULL.
  //--
  MACDEF *pDef = NULL;
  if (pSym->nType == SF_UDF) {
    // Create a brand new macro definition ...
    pDef = (MACDEF *) MyAlloc(sizeof(MACDEF));
    pSym->nType = SF_MACRO;  pSym->Value.mac = pDef;
    pDef->pSymbol = pSym;  return pDef;
  } else if (pSym->nType == SF_MACRO) {
    // This is aleady defined as a macro...
    pDef = pSym->Value.mac;  free(pDef->pszBody);
    memset(pDef, 0, sizeof(MACDEF));
    pDef->pSymbol = pSym;  return pDef;
  } else {
    // Already define and it's NOT a macro ...
    Flag(ER_MDF);  return NULL;
  }
}

void PopExpansion (void)
{
  //++
  //   This routine will delete the current macro expansion level and "pop"
  // the macro expansion stack one level.  The previous expansion, if any,
  // becomes current again.
  //--
  if (pCurrentMacro == NULL) return;
  MACEXP *p = pCurrentMacro;
  pCurrentMacro = p->pPreviousMacro;
  free(p);
}

bool ParseFormals (MACDEF *pDef, char **pszText)
{
  //++
  //   This will parse the formal argument list for a macro definition.
  // This is simply a list of identifiers enclosed in parenthesis and
  // separated by commas.  The formal list may be empty, as indicated
  // by either "()" or simply no parenthesis at all. The formal argument
  // names will be stored in the macro definition block.  If there is
  // any syntax error then false is returned.
  //--
  bool fParenthesis = false;  uint8_t nArgs = 0;

  // If there's nothing more there, then there are no arguments...
  pDef->nFormals = 0;
  if (iseol(SpanWhite(pszText))) return true;

  // Parenthesis are optional, but they need to match!
  if (**pszText == '(') {fParenthesis = true;  ++* pszText;}

  // Try to scan a list of names ...
  for (nArgs = 0;  nArgs < MAXARG;  ++nArgs) {
    ScanName(pszText, (pDef->aszFormals[nArgs]), sizeof(identifier_t));
    if (SpanWhite(pszText) != ',') break;
    ++*pszText;
  }
  pDef->nFormals = nArgs+1;

  // Make sure the parenthesis are balanced and we're done ...
  if (fParenthesis) {
    if (**pszText != ')') {Flag(ER_SYN);  return false;}
    ++*pszText;
  }
  return true;
}

const char *SearchFormals (const MACEXP *pExp, const char *pszName)
{
  //++
  //   Search the list of formal arguments for a matching name and, if we can
  // find one, return a pointer to the actual argument string.  If the formal
  // name isn't found then return NULL.  Note that this differs from the case
  // a formal argument name that's known, but the associated actual argument is
  // a null string.  In that case we'd return a pointer to a null string...
  //--
  MACDEF *pDef = pExp->pDefinition;
  for (uint8_t i=0;  i < pDef->nFormals;  ++i) {
    //   Special hack here - if the formal name begins with a "$", then don't
    // compare that as part of the name.  Formals that begin with "$" are
    // generated labels, and the actual "$" is not part of the name.
    char *pszFormal = pDef->aszFormals[i];
    if (*pszFormal == '$') ++pszFormal;
    if (STREQL(pszName, pszFormal)) return pExp->aszActuals[i];
  }
  return NULL;
}

bool GetMacroLine()
{
  //++
  //   This routine will copy the next line from the current macro body (as
  // pointed to pCurrentMacro) into the szSourceText buffer, expanding any
  // parameters as it goes.  Remember that the macro body is stored as a big
  // block of text with newline characters between lines, so we stop when we
  // find the next newline.  If there are no more characters left in the
  // macro body (i.e. the next character is EOS) then we return false and
  // no text is copied to szSourceText.
  //
  //   Macro actual argument references are written inline as "$name", where
  // "name" matches one of the formal arguments given to the macro definition.
  // Argument expansion happens anywhere inside the macro body - strings,
  // comments - anywhere!  A literal dollar sign character may be included in
  // the macro expansion by doubling it (e.g. "$$").
  //--
  uint32_t nPos = 0;  char ch;  identifier_t szName;  const char *pszArg;
  memset(szSourceText, 0, sizeof(szSourceText));
  if (*(pCurrentMacro->pszNextLine) == EOS) return false;

  while (true) {
    if ((ch = *(pCurrentMacro->pszNextLine)++) == '$') {
      // Expand an actual argument - get the argument number ...
      if (*(pCurrentMacro->pszNextLine) == '$') {
        szSourceText[nPos++] = '$';  ++(pCurrentMacro->pszNextLine);  continue;
      }
      if (!ScanName(&(pCurrentMacro->pszNextLine), szName, IDLEN)) {Flag(ER_SYN);  continue;}
      if ((pszArg = SearchFormals(pCurrentMacro, szName)) != NULL) {
        // Copy in the actual text ...
        size_t cbArg = strlen(pszArg);
        if ((nPos+cbArg) > MAXSTRING-1) break;
        strcpy(&szSourceText[nPos], pszArg);
        nPos += cbArg;
      }
    } else {
      // It's just a regular character - copy it and check for EOL ...
      if (nPos >= MAXSTRING - 1) break;
      szSourceText[nPos++] = ch;
      if (ch == EOL) return true;
    }
  }

  // Here if anything goes wrong with the macro expansion ...
  FatalError("macro expansion too long from line %d", nSourceLine);
  return false;
}

bool GetSourceLine()
{
  //++
  //   This routine will load the next source line into szSourceText.  Usually
  // it just reads the line from the source file, BUT if a macro expansion is
  // currently active it will return the next line from the macro body instead.
  // If we've run out of text in the current expansion it will "pop" the macro
  // expansion stack and try to return a line from the next outer nested macro.
  // When we run out of macros it will go back to reading the source file, and
  // if there are no more lines in the source file it returns false.
  //--
  while (pCurrentMacro != NULL) {
    // Return the next line from the current macro expansion ...
    if (GetMacroLine()) return true;
    // This expansion is done - try to pop the macro expansion "stack" ...
    PopExpansion();
  }
  // There are no more macros - try to read the source file ...
  memset(szSourceText, 0, sizeof(szSourceText));
  if (fgets(szSourceText, sizeof(szSourceText), pSourceFile) == NULL) return false;
  //  If the line ends with "\r\n" (e.g. it's an MSDOS text file) then convert
  // that to just "\n" ...
  size_t cbLine = strlen(szSourceText);
  if (   (cbLine >= 2)
      && (szSourceText[cbLine-2] == '\r')
      && (szSourceText[cbLine-1] == '\n')) {
    szSourceText[cbLine-2] = '\n';  szSourceText[cbLine-1] = '\0';
  }
  // Count the lines read and we're done ...
  ++nSourceLine;  CheckFormFeed();  return true;
}

char GetSourceChar (char **pszText)
{
  //++
  //   Return the next source character.  Normally this would be as simple as
  // *pszText++, however macro definitions and conditional blocks can extend
  // across multiple lines.  If we've reached the end of the current line then
  // this routine will read the next line and start returning characters from
  // that one.
  //--
  char ch;
  while ((ch = *(*pszText)++) == EOS) {
    if (nPass == 2)  List(NULL, NULL, NULL, true);
    if (!GetSourceLine())
      FatalError("end of file while reading text block");
    *pszText = szSourceText;
  }
  return ch;
}

void ReadBlock (char **pszText, char **pszBody, bool fAddNewline)
{
  //++
  //   This routine will read a block of text, that is any section enclosed
  // by angle brackets ("<...>"), pack it into a text buffer, and return it.
  // Everything else in the assembler takes place on a single source line,
  // but text blocks might occupy only part of a source line, or they may
  // extend across multiple lines.  It's used to read the body of macro
  // definitions and failing (i.e. condition not true) conditional blocks.
  // 
  //   If pszBody is NULL then the text read is simple discarded; this is
  // used for conditionals.  However if pszBody is not NULL then the text
  // accumulated will be copied into a block on the heap and the address of
  // that block returned.  
  //--
  char ch;  char szBody[MAXBODY];  size_t cbBody = 0;  int32_t nLevel = 0;
  memset(szBody, 0, sizeof(szBody));

  // Skip everything until we find the next '<' ...
  do {
    do ch = GetSourceChar(pszText); while (isspace(ch) || (ch == EOL));
    if (ch != '<') Flag(ER_SYN);
  } while (ch != '<');

  // If the next character after the '<' is a newline, then skip it ...
  ch = GetSourceChar(pszText);
  if (ch == EOL) ch = GetSourceChar(pszText);

  // Store characters in the body until we find the matching '>' ...
  while ((ch != '>') || (nLevel != 0)) {
    if (cbBody >= sizeof(szBody)-2) FatalError("macro body too long");
    if (pszBody != NULL) szBody[cbBody++] = ch;
    if (ch == '<') ++nLevel;
    if ((ch == '>') && (nLevel > 0)) --nLevel;
    ch = GetSourceChar(pszText);
  }

  if (pszBody != NULL) {
    // Make sure the definition ended with a newline ...
    if (fAddNewline) {
      if ((cbBody == 0) || (szBody[cbBody-1] != EOL)) szBody[cbBody++] = EOL;
    }
    szBody[cbBody] = EOS;

    // Copy the body to the heap and return a pointer to it...
    *pszBody = MyStrDup(szBody);
  }
}

bool ParseActual (char **pszText, char *pszActual, size_t cbActual)
{
  //++
  //   This routine will parse a single actual macro argument string.  This is
  // a bit tough, since arguments are mostly free form and can consist of any
  // string of characters.  There are, however, a few rules - macro arguments
  // cannot span multiple lines (at least for now), and macro arguments cannot
  // contain a comma (which separates multiple arguments) EXCEPT under certain
  // conditions.  Commas ARE allowed in an argument if they're part of a string
  // enclosed by matching pairs of Parenthesis or double quotes.  For example,
  // 
  //    A, B, C     - has three arguments, "A", "B" and "C"
  //    A, (B, C)   - has two arguments, "A" and "(B, C)"
  //    A B C       - has one argument, "A B C"
  //    A "B,C"     - has one argument, "A ""B, C""
  //    A, "B,C"    - has two arguments, "A" and ""B, C""
  //--
  char ch;  size_t cb = 0;
  bool fInQuotes = false;  int32_t nParenthesis = 0;
  memset(pszActual, 0, cbActual);
  ch = SpanWhite(pszText);
  if (ch == '<') {
    //   This is a bit of a hack, but it allows an actual argument to be
    // enclosed in angle brackets.  And in that case the actual can contain
    // ANY characters - commas, quotes, unmatched parenthesis, whatever...
    char *pszBlock;
    ReadBlock(pszText, &pszBlock, false);
    if (strlen(pszBlock) > (cbActual - 1)) {
      Flag(ER_MAC);  free(pszBlock);  return false;
    }
    strcpy(pszActual, pszBlock);  free(pszBlock);
  } else {
    while (((ch != ',') || fInQuotes || (nParenthesis > 0)) && !iseol(ch)) {
      if (ch == '"') fInQuotes = !fInQuotes;
      if (ch == '(') ++nParenthesis;
      if ((ch == ')') && (nParenthesis == 0)) break;
      if (cb == cbActual) {Flag(ER_MAC);  return false; }
      if (ch == ')') --nParenthesis;
      pszActual[cb++] = ch;  ch = *++(*pszText);
    }
    TrimString(pszActual);
  }
  return true;
}

void GeneratedActuals (MACEXP *pExp)
{
  //++
  //   This routine will go thru the formal and actual argument lists for this
  // macro expansion and, for any formal name which begins with a "$" but which
  // has a null actual argument, it will generate an actual of the form "$nnnnn"
  // where "nnnnn" is a sequentially assigned number.  Why?  These arguments can
  // be used as generated labels inside of macros to guarantee unique names.
  //--
  MACDEF *pDef = pExp->pDefinition;
  for (uint8_t i = 0;  i < pDef->nFormals;  ++i) {
    if (pDef->aszFormals[i][0] != '$') continue;
    if (pExp->aszActuals[i][0] != EOS) continue;
    sprintf(pExp->aszActuals[i], "$%05d", ++nGeneratedLabel);
  }
}

void DotDEFINE (char *pszText)
{
  //++
  //   This routine will parse a macro definition and enter it into the symbol
  // table.  The macro definition syntax looks like -
  //
  //      .DEFINE   NAME (ARG1, ARG2, ARG3, ...)
  //      <
  //        ... macro body line 1 ...
  //        ...
  //        ... macro body line n ...
  //      >
  //
  //   The formal argument list is optional and may be omitted (or replaced by
  // "()") if the macro has no arguments.  The macro body consists of all text
  // between the "<" and ">" and can span multiple lines or be only part of a
  // single line.  There can be nothing except spaces and/or newlines between
  // the end of the formal argument list and the opening "<", and there can be
  // nothing except for white space and/or a newline after the closing ">".
  //--
  identifier_t szMacro;  SYMBOL *pSym;  MACDEF *pDef;  char *pszBody;

  // First parse the name of the macro.
  if (!ScanName(&pszText, szMacro, IDLEN)) {
    //   If no name can be found then just flag an error and quit.  This may
    // lead to other errors (e.g. for the line with the ">") but his source
    // code is screwed up and there's no way around that.
    Flag(ER_SYN);  return;
  }

  // Add this macro definition to the symbol table ...
  pSym = LookupSymbol(szMacro, true);
  pDef = NewMacro(pSym);
  AddReference(pSym, true);

  // Parse the formal argument list ...
  ParseFormals(pDef, &pszText);

  // Read (and store) the body of the macro ...
  ReadBlock(&pszText, &pszBody, true);
  if (!iseol(SpanWhite(&pszText))) Flag(ER_SYN);
  if (pDef != NULL) pDef->pszBody = pszBody;

  // List this line (the one containing the ">") and we're done ...
  if (nPass == 2)  List(NULL, NULL, NULL, true);
}

void DotMACRO (const SYMBOL *pMac, char *pszText)
{
  //++
  //   This routine is called when we find a macro invocation.  It parses the
  // actual argument list, sets up the array of argument strings in aszActuals[],
  // and then sets up the global pointers so GetSourceLine() will start reading
  // text from the macro body.
  //--
  bool fParenthesis = false;  uint8_t nArg = 0;

  // Create a new macro expansion block ...
  MACEXP *pExp = (MACEXP *) MyAlloc(sizeof(MACEXP));
  pExp->pDefinition = pMac->Value.mac;
  pExp->pszNextLine = pExp->pDefinition->pszBody;

  //   There are a couple of special cases we need to deal with here.  First,
  // if parenthesis are used around the argument list then we need to remember
  // that so we can look for the matching ")".  Second, if the argument list is
  // empty then we need to quit now.  And lastly, "()" counts as an empty
  // argument list!
  if (iseol(SpanWhite(&pszText))) goto noargs;
  if (*pszText == '(') {
    fParenthesis = true;  ++pszText;
    if (SpanWhite(&pszText) == ')') goto noargs;
  }

  //   Parse the remainder of this source line and make copies of all the
  // actual argument strings.
  for (nArg = 0;  nArg < MAXARG;  ++nArg) {
    if (!ParseActual(&pszText, pExp->aszActuals[nArg], MAXSTRING)) break;
    if (*pszText != ',') break;
    ++pszText;
  }

  // Make sure the parenthesis are balanced ...
  if (fParenthesis) {
    if (*pszText != ')') Flag(ER_SYN);
    ++pszText;
  }
  if (!iseol(SpanWhite(&pszText))) Flag(ER_SYN);
  pExp->nActuals = nArg+1;

noargs:
  // Handle any generated labels in this macro ...
  GeneratedActuals(pExp);

  // List the macro call under pass 2 ...
  if (nPass == 2)  List(NULL, NULL, NULL, true);

  //  And finally, push this macro onto the current macro expansion stack.  We
  // don't want to do this until AFTER we've listed the source line, because
  // otherwise this macro call will show up in the listing with a "+" and no
  // line number, as if it were part of its own expansion!
  pExp->pPreviousMacro = pCurrentMacro;  pCurrentMacro = pExp;
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////   CONDITIONAL ASSEMBLY   ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Forward references...
void Assemble (char *pszText);

void DoConditional (char *pszText, bool fSuccess)
{
  //++
  //    This routine is called to handle all the conditional assembly pseudo
  // ops.  If fSuccess is true then the conditional test was met and we
  // assemble the code enclosed in the angle brackets, and if fSuccess is
  // false then we skip over the text block in the angle brackets.
  //--
  char ch;
  if (fSuccess) {
    //   Here if the conditional succeeds and we want to assemble the text
    // block.  There had better be a "<" next.  Skip white space and EOL until
    // we find it, and if we find anything else then that's an error.
    do {
      do ch = GetSourceChar(&pszText); while (isspace(ch) || (ch == EOL));
      if (ch != '<') Flag(ER_SYN);
    } while (ch != '<');
    //   Assemble the rest of this line (anything left after the "<") as well
    // as all subsequent lines.  Eventually we'll run into the matching ">",
    // but Assemble() will just ignore that.
    Assemble(pszText);
  } else {
    //   And here if the conditional fails and we want to skip (i.e. do not
    // assemble!) the text block.  We can use ReadBlock() for this, since it
    // will automatically handle nested text blocks for us.
    ReadBlock(&pszText, NULL, false);
    //   But be sure to assemble the rest of this line (any part that comes
    // after the closing ">") ...
    Assemble(pszText);
  }
}

void DotIFDEF (char *pszText)
{
  //++
  //   The ".IFDEF name <code>" pseudo op assembles the "code" text block IF
  // the specified symbol name is defined.  "Defined" means anything in our
  // symbol table, including user labels and equates, macros, machine opcodes,
  // and even other pseudo-ops.
  //--
  identifier_t szName;
  if (!ScanName(&pszText, szName, IDLEN)) {Flag(ER_SYN);  return;}
  DoConditional(pszText, (LookupSymbol(szName, false) != NULL));
}

void DotIFNDEF (char *pszText)
{
  //++
  //   This is the same as .IFDEF except, as you probably guessed, it reverses
  // the sense of the test and assembles the text block if the symbol is NOT
  // defined.
  //--
  identifier_t szName;
  if (!ScanName(&pszText, szName, IDLEN)) {Flag(ER_SYN);  return;}
  DoConditional(pszText, (LookupSymbol(szName, false) == NULL));
}

void DotIFEQ (char *pszText)
{
  //++
  //   The ".IFEQ expr <code>" pseudo-op assembles the "code" text block if
  // the value of expr is zero.  This can be any expression, including symbols
  // opcodes, and even (though I wouldn't recommend it!) literals.  If there
  // are any errors in the expression, like undefined symbols or syntax errors,
  // then we list this line and ignore it.  That'll probably lead to subsequent
  // errors as well, but there's not much else we can do.
  //--
  uint16_t nCode;
  if (!EvaluateExpression(&pszText, &nCode)) {
    if (nPass == 2)  List(NULL, NULL, NULL, true);
  } else
    DoConditional(pszText, (nCode == 0));
}

void DotIFNE (char *pszText)
{
  //++
  //   And .IFNE (note the missing "E"!) is identical to .IFEQ except that it
  // will assemble the code block if the value of the expression is NOT zero.
  //--
  uint16_t nCode;
  if (!EvaluateExpression(&pszText, &nCode)) {
    if (nPass == 2)  List(NULL, NULL, NULL, true);
  } else
    DoConditional(pszText, (nCode != 0));
}

void DotIFGT (char *pszText)
{
  //++
  //   Test if expr is .GT. 0.  We have to be careful here because we're
  // dealing with 12 bit numbers!
  //--
  uint16_t nCode;
  if (!EvaluateExpression(&pszText, &nCode)) {
    if (nPass == 2)  List(NULL, NULL, NULL, true);
  } else
    DoConditional(pszText, (nCode != 0) && ((nCode & 04000) == 0));
}

void DotIFGE (char *pszText)
{
  //++
  // Test if expr is .GE. 0.
  //--
  uint16_t nCode;
  if (!EvaluateExpression(&pszText, &nCode)) {
    if (nPass == 2)  List(NULL, NULL, NULL, true);
  } else
    DoConditional(pszText, (nCode & 04000) == 0);
}

void DotIFLE (char *pszText)
{
  //++
  // Test if expr is .LE. 0.
  //--
  uint16_t nCode;
  if (!EvaluateExpression(&pszText, &nCode)) {
    if (nPass == 2)  List(NULL, NULL, NULL, true);
  } else
    DoConditional(pszText, (nCode == 0) || ((nCode & 04000) != 0));
}

void DotIFLT (char *pszText)
{
  //++
  // Test if expr is .LT. 0.
  //--
  uint16_t nCode;
  if (!EvaluateExpression(&pszText, &nCode)) {
    if (nPass == 2)  List(NULL, NULL, NULL, true);
  } else
    DoConditional(pszText, (nCode != 0) && ((nCode & 04000) != 0));
}


////////////////////////////////////////////////////////////////////////////////
////////////////////   ASSEMBLY FIRST AND SECOND PASSES   //////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CheckLabel (char **pszText)
{
  //++
  //   This function will check the current source line for the presence
  // of one or more labels (aka tags, e.g. "XYZ:") and will process any
  // that it finds.  On pass 1, labels are entered into the symbol table
  // with a value equated to the current location.  On pass 2 there's no
  // need to re-define the tags, but we do check their entry in the
  // symbol table to see if they have any errors (e.g. multiply defined)
  // associated with them so that these errors can be flagged in the
  // listing file.  This function returns true if it finds and processes
  // at least one label...
  //--
  identifier_t szLabel;  char *pLabel;  SYMBOL *pSym;  bool fFound=false;

  while (true) {
    //   The only way to know whether there's a label is to scan over
    // the name and then look for the ":".  If it turns out that there
    // is no label, then the original pszText pointer is left unchaged so
    // there's no need to back up...
    pLabel = *pszText;
    if (!ScanName(&pLabel, szLabel, sizeof(szLabel))) return fFound;
    SpanWhite (&pLabel);
    if (*pLabel != ':') return fFound;

    // We've found a label. Advance the text pointer to skip over it...
    *pszText = pLabel + 1;  pSym = LookupSymbol(szLabel, true);
    AddReference(pSym, true);
    if (nPass == 1) {
      // On pass 1, enter this label into the symbol table...
      if (pSym->nType == SF_UDF) {
        pSym->nType = SF_TAG;  pSym->Value.bin = (nField<<12) | nPC;
      } else
        pSym->nType = SF_MDF;
    } else {
      // On pass 2, check the symbol for errors...
      if (pSym->nType != SF_TAG)  Flag(ER_SYM);
    }
    fFound = true;
  }
}

bool CheckDefinition (char *pszText)
{
  //++
  //   This function will test the current source line to see if it is a
  // symbol definition of the form "XYZ=123", and process it if so.  This
  // statement simply evaluates the expression on the right side of the
  // equal sign and defines the symbol to the left of the equal with this
  // value.  This routine returns true if the source line is a definition
  // and in this case the entire line has been processed (and listed if
  // this is pass 2) when we return....
  //--
  identifier_t szName;  uint16_t nValue;  SYMBOL *pSym;

  // See if this even is a symbol definition...
  if (!ScanName(&pszText, szName, sizeof(szName))) return false;
  SpanWhite (&pszText);
  if (*pszText != '=') return false;

  //   It is - parse the expression and calculate its value.  Note that
  // we evaluate symbol definitions on pass 1, but we still parse the
  // expression all over again on pass 2.  This ensures that any errors
  // in the expression get reported in the listing file!
  ++pszText;  nValue = 0;
  if (EvaluateExpression(&pszText, &nValue)) {
    if (!iseol(*pszText))  Flag(ER_SYN);
  }

  // Enter the symbol into the symbol table...
  pSym = LookupSymbol(szName, true);  AddReference(pSym, true);
  if (nPass == 1) {
    if (pSym->nType == SF_UDF) {
      pSym->nType = SF_EQU;  pSym->Value.bin = nValue;
    } else
      pSym->nType = SF_MDF;
  } else {
    if (pSym->nType != SF_EQU)  Flag(ER_SYM);
  }

  // If this is pass 2, then list this line...
  if (nPass == 2)  List(NULL, NULL, &nValue, true);

  return true;
}

bool CheckMacroPseudo (char *pszText)
{
  //++
  //   This function will check the source line for a pseudo op (e.g. .TITLE,
  // .END, etc) or a macro invocation.  If it finds one, then it will invoke
  // the correct routine to process it.  Pseudo-ops always begin with a "."
  // (which is a dead giveaway!) but macros don't.  true is returned if we
  // find a pseudo op or macro, and false otherwise ...
  //--
  identifier_t szName;  SYMBOL *pSym;
  char ch = SpanWhite(&pszText);

  // If it's a "." then we know we're looking for a pseudo-op ...
  if (ch == '.') {
    ++pszText;  szName[0] = '.';
    if (ScanName(&pszText, szName+1, sizeof(szName)-1)) {
      pSym = LookupSymbol(szName, true);
      AddReference(pSym, false);
      if (pSym->nType == PO_POP)
        {(*pSym->Value.pop) (pszText);  return true;}
    }
    Flag(ER_POP);
    if (nPass == 2)  List(NULL, NULL, NULL, true);
    return true;
  } else if (isid1(ch)) {
    if (ScanName(&pszText, szName, sizeof(szName))) {
      pSym = LookupSymbol(szName, true);
      AddReference(pSym, false);
      if (pSym->nType == SF_MACRO)
        {DotMACRO(pSym, pszText);  return true;}
    }
  }
  return false;
}

bool CheckPseudoOp (char *pszText)
{
  //++
  //   This function will check the source line for a pseudo op (e.g.
  // .TITLE, .END, .BLOCK, etc) and, if it finds one, then it will invoke
  // the correct routine to process it.  Since the pseudo-ops do many
  // different functions, everything that happens, including listing the
  // source line, is determined by the pseudo op's routine.  true is
  // returned if we find a pseudo op, and false if this is a normal kind
  // of statement...
  //--
  identifier_t szName;  SYMBOL *pSym = NULL;

  //   All pseudo ops current start with a ".", so we can exploit this
  // to trivialize the test for the presence of a pseudo-op...
  if (SpanWhite(&pszText) != '.')  return false;

  // Get the name and look it up in the symbol table...
  ++pszText;  szName[0] = '.';
  if (   !ScanName(&pszText, szName+1, sizeof(szName))
      || ((pSym=LookupSymbol(szName, true))->nType != PO_POP)) {
    //   Either the "." isn't followed by an identifier, or the ident-
    // ifier isn't defined as a pseudo-op.  In either case this state-
    // ment is bogus.  On pass 1 we can just ignore it, but on pass 2
    // we have to list it so that the user can see!
    Flag(ER_POP);  AddReference(pSym, true);
    if (nPass == 2)  List(NULL, NULL, NULL, true);
    return true;
  }

  // Dispatch to the correct pseudo op function...
  AddReference(pSym, false);
  ((void (*) (char *)) (pSym->Value.pop)) (pszText);
  return true;
}

void Assemble (char *pszText)
{
  //++
  //   This routine will assemble a single source statement.  This may be an
  // equate (e.g. "FOO=123"), a pseudo operation, a macro call, a regular
  // machine opcode, or just a data word.  
  //--
  uint16_t nCode;  bool fLabel;

  //   First, check for a symbol definition (e.g. "XYZ=123").  We do
  // this first, before even checking for labels, because putting a
  // label on a symbol definition (e.g. "ABC: XYZ=123") is not legal!
  // If this source line is a symbol definition, then there's nothing
  // more that we need to do...
  if (CheckDefinition(pszText)) return;

  //   Next, check to see if this line has any labels on it and if it
  // does then define them.  We need to remember if this line has any
  // label with the fLabel flag...
  fLabel = CheckLabel(&pszText);

  //   Now, if this source line is null (e.g. it's blank or contains
  // only a comment) then we can just list it and forget it.  Usally
  // these lines don't show either an address or generated code in
  // the listing, _however_, if the line contains a label then we go
  // ahead and show the address alone...
  if (iseol(SpanWhite(&pszText)) || (*pszText == '>')) {
    if (nPass == 2)  List(&nField, (fLabel ? &nPC : NULL), NULL, true);
    return;
  }

  //   See if this line contains a pseudo-op (e.g. .TITLE, .END, etc)
  // If it does, then the pseudo op routine is responsible for every-
  // thing that happens next, including listing the line.  This is
  // because some pseudo-ops generate more than one word of code.
  if (CheckMacroPseudo(pszText)) return;

  //   If it's none of these things then this line must generate some
  // kind of code.  It could be a machine instruction, or it could
  // just be some numeric expression (e.g. "X: 0077") - it doesn't
  // matter either way.  On pass 1 we don't try to evaluate the
  // expression because it may contain symbols that aren't defined
  // yet, but on pass 2 we do evaluate it and output the code word
  // generated.  On either pass, however, we increment the PC.
  if (nPass == 2) {
    if (!EvaluateExpression(&pszText, &nCode)) Flag(ER_SYN);
    if (SpanWhite(&pszText) == '>') ++pszText;
    if (!iseol(SpanWhite(&pszText))) Flag(ER_SYN);
    OutputCode(nCode, true, true);
  } else
    ++nPC;
}

void Pass (uint16_t n)
{
  //++
  //   This routine will read and parse the entire source file.  It gets
  // called twice, once for each assembler pass. The caller is responsible
  // for opening the source, listing and binary files before calling this
  // procedure.
  //--
  char *pszText;

  // Initialize all the global variables...
  nPass = n;  nCPU = 0;  nErrorCount = nSourceLine = 0;
  nPC = 0200;  nField = 0;  nLiteralBase = nPC + 0200;
  fNewPage = fListExpansions = fPaginate = true;
  fListSymbols = fListMap = fListTOC = fListText = true;
  szErrorFlags[0] = EOS;  szIgnoredErrors[0] = EOS;
  pCurrentMacro = NULL;  nGeneratedLabel = 0;
  nPUSH = nPOP = nPUSHJ = nPOPJ = 0;
  nLastBinaryAddress = 010000;  nBinaryChecksum = 0;
  Message("%s, pass %d", szSourceFile, n);

  while (GetSourceLine()) {
    pszText = szSourceText;
    Assemble(pszText);
  }

  // Make sure there are no left over literals on the last page!
  if (n == 2) DumpLiterals();
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////   COMMAND LINE PARSER   /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool ParseOptions (int argc, char *argv[])
{
  //++
  //   This function will parse the command line options and set up the
  // source, binary and list file names.  A source name is required, but
  // either or both of the listing and binary file names may be omitted.
  // It returns false if the arguments are invalid...
  //--
  int i;  char *pArg, *pEnd;
  szSourceFile[0] = szListFile[0] = szBinaryFile[0] = EOS;
  nLinesPerPage = LINES_PER_PAGE;  nColumnsPerPage = COLUMNS_PER_PAGE;
  fOS8SIXBIT = fASCII8BIT = false;

  for (i = 1;  i < argc;  ++i) {
    if (STREQL(argv[i], "-l")) {
      if ((++i >= argc) || (strlen(szListFile) > 0)) return false;
      strcpy(szListFile, argv[i]);
    } else if (STREQL(argv[i], "-b")) {
      if ((++i >= argc) || (strlen(szBinaryFile) > 0)) return false;
      strcpy(szBinaryFile, argv[i]);
    } else if (STREQL(argv[i], "-8")) {
      fOS8SIXBIT = true;
    } else if (STREQL(argv[i], "-a")) {
      fASCII8BIT = true;
    } else if (STRNEQL(argv[i], "-p", 2)) {
      if (argv[i][2] == EOS) {
        if (++i >= argc) return false;
        pArg = argv[i];
      } else
        pArg = argv[i]+2;
      nLinesPerPage = (uint16_t) strtol(pArg, &pEnd, 10);
      if (*pEnd != EOS) return false;
    } else if (STRNEQL(argv[i], "-w", 2)) {
      if (argv[i][2] == EOS) {
        if (++i >= argc) return false;
        pArg = argv[i];
      } else
        pArg = argv[i]+2;
      nColumnsPerPage = (uint16_t) strtol(pArg, &pEnd, 10);
      if (*pEnd != EOS) return false;
    } else if (argv[i][0] == '-') {
      return false;
    } else {
      if (strlen(szSourceFile) > 0) return false;
      strcpy(szSourceFile, argv[i]);
    }
  }
  return strlen(szSourceFile) > 0;
}

#ifdef VMS
void _splitpath (const char *pszPath, char *pszDrive, char *pszDirectory, char *pszName, char *pszType)
{
  //++
  //   This routine sill split up a VMS file specification into component
  // parts.  It's equivalent to the MSDOS library function of the same
  // name.  It assumes that the VMS specification is syntactically legal-
  // what happens to illegal names is anybody's guess...
  //--
  const char *pStart, *p;  uint16_t nLen;
  pszDrive[0] = pszDirectory[0] = pszName[0] = pszType[0] = EOS;  pStart = pszPath;
  // First try to extract a device name...
  p = strchr(pStart, ':');
  if (p != NULL) {
    nLen = p - pStart+1;  strncpy(pszDrive, pStart, nLen);
    pszDrive[nLen] = EOS;  pStart = p + 1;
  }
  // Now the directory, if any...
  p = strchr(pStart, ']');
  if (p != NULL) {
    nLen = p - pStart+1;  strncpy(pszDirectory, pStart, nLen);
    pszDirectory[nLen] = EOS;  pStart = p + 1;
  }
  //  Next the file name.  This time, unlike the others, the terminator
  // (i.e. ".") is _not_ part of the name.  Also, if no dot is found,
  // then the entire remainder of ths string is the name.
  p = strchr(pStart, '.');
  if (p != NULL) {
    nLen = p - pStart;  strncpy(pszName, pStart, nLen);
    pszName[nLen] = EOS;  pStart = p;
  } else {
    strcpy(pszName, pStart);  pStart = pStart + strlen(pStart);
  }
  //   And finally the extension (type).  There may be a VMS version
  // number too, but we strip that off and discard it...
  strcpy(pszType, pStart);  char *pv = strchr(pszType, ';');
  if (pv != NULL) *pv = EOS;
}
#endif

#ifdef __linux__
void _splitpath (const char *pszPath, char *pszDrive, char *pszDirectory, char *pszName, char *pszType)
{
  //++
  //   And now the Linux version.  Note that the dirname() and basename()
  // library functions are a bit, well, funky.  dirname() modifies the caller's
  // buffer by replacing the last "/" in the path with a null and then it
  // returns a pointer to start of the string.  basename() doesn't modify the
  // buffer it's passed and just returns a pointer to the first character after
  // the last "/" in the name.  Calling dirname() before basename() doesn't
  // work because the latter will be unable to find the actual file name after
  // dirname terminates the string just in front of it.  Calling them in the
  // reverse order, however, works fine.
  //--
  *pszDrive = *pszDirectory = *pszName = *pszType = EOS;
  if (strlen(pszPath) == 0) return;
  char szPath[PATH_MAX];
  strcpy(szPath, pszPath);
  strcpy(pszName, basename(szPath));
  strcpy(pszDirectory, dirname(szPath));
  // dirname() DOES NOT include the trailing "/", so we need to add one!
  strcat(pszDirectory, "/");
  //   Now we play the same game ourselves to separate the extension (if any)
  // from the file name.  Note that we extract the extension string BEFORE
  // replacing the "." with an EOS because the dot is part of the extension!
  char *psz = strrchr(pszName, '.');
  if (psz != NULL) {
    strcpy(pszType, psz);  *psz = EOS;
  }
}
#endif

void DefaultFile (char *pFileName, const char *pRelatedName, const char *pDefaultType)
{
  //++
  //   This routine applies a set of "defaults" to a file specification -
  // specifically a default extension (e.g. ".plx", ".bin", etc) and a
  // default path.  The latter is derived from a "related file", which is
  // usually the source file.  This done so that binary files and listing
  // files are by default placed in the same path as the source file.
  //--
  string_t szDrive, szDir, szName, szType;
  string_t szDefaultDrive, szDefaultDir, szDefaultName, szDummy;
  _splitpath(pFileName, szDrive, szDir, szName, szType);
  _splitpath(pRelatedName, szDefaultDrive, szDefaultDir, szDefaultName, szDummy);
  strcpy(pFileName, (strlen(szDrive) > 0) ? szDrive : szDefaultDrive);
  strcat(pFileName, (strlen(szDir)   > 0) ? szDir   : szDefaultDir);
  strcat(pFileName, (strlen(szName)  > 0) ? szName  : szDefaultName);
  strcat(pFileName, (strlen(szType)  > 0) ? szType  : pDefaultType);
}

void OpenFiles (void)
{
  //++
  //   This routine opens all the necessary input and output files for
  // assembly - the source file, the binary file, and the listing file.
  // If any of the files cannot be opened, an error message is printed on
  // stderr and this program is terminated.
  //--

  // Apply the default extension, ".plx", to the input file...
  DefaultFile(szSourceFile, "", SOURCE_TYPE);
  pSourceFile = fopen(szSourceFile, "r");
  if (pSourceFile == NULL) FatalError("unable to read %s", szSourceFile);

  //   Derive the full path of the input file, including the actual
  // drive and directory, for the listing.  This turns, for example,
  // "myfile.plx" into "C:\BOB\PROJECTS\MYFILE.PLX" ...
#if defined(VMS)
  char *p;
  fgetname(pSourceFile, szSourceFile);  p = strchr(szSourceFile, ';');
  if (p != NULL) *p = EOS;
#elif defined(_MSC_VER)
  string_t szTemp;
  _fullpath(szTemp, szSourceFile, sizeof(szTemp));
  strcpy(szSourceFile, szTemp);  _strupr(szSourceFile);
#elif defined(__linux__)
  char sz[PATH_MAX];
  if (realpath(szSourceFile, sz) != NULL) strcpy(szSourceFile, sz);
#endif

  //  The listing file defaults to the same name and path as the source
  // file with the extension .LST...
  DefaultFile(szListFile, szSourceFile, LIST_TYPE);
  pListFile = fopen(szListFile, "w");
  if (pListFile == NULL) FatalError("unable to write %s", szListFile);

  // Ditto for the binary file, except with ".BIN".
  DefaultFile(szBinaryFile, szSourceFile, BINARY_TYPE);
  fdBinaryFile = _open(szBinaryFile,
    _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
  if (fdBinaryFile == -1) FatalError("unable to write %s", szBinaryFile);
  cbBinaryData = 0;  PunchLeader();
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////   MAIN PROGRAM   ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char *argv[])
{
  //++
  // And at last - main() !!
  //--

  // Parse the command line...
  if (!ParseOptions(argc, argv)) {
    fprintf(stderr, "Usage:\t%s [-w nnn] [-p nnn] [-l file] [-b file] sourcefile\n", PALX);
    fprintf(stderr, "\n");
    fprintf(stderr, "\t-b file - specify binary file name\n");
    fprintf(stderr, "\t-l file - specify listing file name\n");
    fprintf(stderr, "\t-w nnn  - listing page width in columns\n");
    fprintf(stderr, "\t-p nn   - listing page length in lines\n");
    fprintf(stderr, "\t-8      - use OS/8 style for .SIXBIT/.SIXBIZ\n");
    fprintf(stderr, "\t-a      - use ASR33 \"always mark\" ASCII\n");
    exit(EXIT_FAILURE);
  }

  // Initialize...
  Message("%s V%d.%02d RLA", TITLE, (VERSION / 100), (VERSION % 100));
  InitializeSymbols();  ClearBitMap();  pFirstTOC = pLastTOC = NULL;
  OpenFiles();

  // Make the first pass and rewind the source file...
  Pass(1);
  if (fseek(pSourceFile, 0L, SEEK_SET) != 0)
    FatalError("error rewinding source file");

  // Make the second pass and generate the binary and listing files.
  Pass(2);  PunchChecksum();
  ListSummary();
  if (fListMap) ListBitMap();
  if (fListSymbols) {SortSymbols();  ListSymbols();}
  if (fListTOC) ListTOC();

  // Close all the files and we're done... */
  fclose(pSourceFile);  fclose(pListFile);  _close(fdBinaryFile);
  return EXIT_SUCCESS;
}
