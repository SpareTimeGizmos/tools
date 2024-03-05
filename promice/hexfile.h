//++
// hexfile.h -> declarations for the Intel .HEX file routines
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

// Global methods ...
extern bool hexDump (const char *pszFile, uint8_t *pabMemory, uint32_t cbOffset, uint32_t cbMemory);
extern uint32_t hexLoad (const char *pszFile, uint8_t *pabMemory, uint32_t cbOffset, uint32_t cbMemory);
