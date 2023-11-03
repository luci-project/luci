/*
 * This file is part of the SPiC Project SPiChess  (https://gitlab.cs.fau.de/i4/spic/chess).
 * Copyright (c) 2017 - 2020 by Bernhard Heinloth
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CHESS_OPENINGS_HELPER_H
#define CHESS_OPENINGS_HELPER_H

#define OPENING_IS_VALID(X) ((opening_invalid[(X)/8] & (1 << ((X) % 8))) == 0)
#define OPENING_INVALIDATE(X) do { opening_invalid[(X)/8] |= (1 << ((X) % 8)); } while(0)
#define OPENING_FROM_X(O,M) (((openings[O][(M) * 2 - 2]) >> 4) & 7)
#define OPENING_FROM_Y(O,M) (openings[O][(M) * 2 - 2] & 7)
#define OPENING_TO_X(O,M) (((openings[O][(M) * 2 - 1]) >> 4) & 7)
#define OPENING_TO_Y(O,M) (openings[O][(M) * 2 - 1] & 7)
#define OPENING_NONE (0xff)

#define A1 0x07
#define A2 0x06
#define A3 0x05
#define A4 0x04
#define A5 0x03
#define A6 0x02
#define A7 0x01
#define A8 0x00

#define B1 0x17
#define B2 0x16
#define B3 0x15
#define B4 0x14
#define B5 0x13
#define B6 0x12
#define B7 0x11
#define B8 0x10

#define C1 0x27
#define C2 0x26
#define C3 0x25
#define C4 0x24
#define C5 0x23
#define C6 0x22
#define C7 0x21
#define C8 0x20

#define D1 0x37
#define D2 0x36
#define D3 0x35
#define D4 0x34
#define D5 0x33
#define D6 0x32
#define D7 0x31
#define D8 0x30

#define E1 0x47
#define E2 0x46
#define E3 0x45
#define E4 0x44
#define E5 0x43
#define E6 0x42
#define E7 0x41
#define E8 0x40

#define F1 0x57
#define F2 0x56
#define F3 0x55
#define F4 0x54
#define F5 0x53
#define F6 0x52
#define F7 0x51
#define F8 0x50

#define G1 0x67
#define G2 0x66
#define G3 0x65
#define G4 0x64
#define G5 0x63
#define G6 0x62
#define G7 0x61
#define G8 0x60

#define H1 0x77
#define H2 0x76
#define H3 0x75
#define H4 0x74
#define H5 0x73
#define H6 0x72
#define H7 0x71
#define H8 0x70

#define XX 0xff

#endif
