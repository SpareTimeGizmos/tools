//++
// serial.h -> declarations for the serial port module
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
extern void serOpen (const char *pszName, uint32_t nBaud);
extern void serClose();
extern void serFlush();
extern void serSend (const uint8_t *pabBuffer, size_t cbBuffer);
extern void serSendByte (uint8_t bData);
extern size_t serReceive (uint8_t *pabBuffer, size_t cbBuffer, uint32_t lTimeout);
extern bool serReceiveByte (uint8_t *pbData, uint32_t lTimeout);
extern void serSetDTR (bool fDTR);
extern void serSleep (uint32_t lDelay);
