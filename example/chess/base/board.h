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

#ifndef CHESS_BOARD_H
#define CHESS_BOARD_H

#include <stdint.h>
#include <stdbool.h>

#define INVALID (9)

#define VALID_FIELD(Y,X) ((X) >= 0 && (X) < 8 && (Y) >= 0 && (Y) < 8)
#define VALID_POS(P) (VALID_FIELD((P).y,(P).x))
#define VALID_MOVE(M) (VALID_POS((M).from) && VALID_POS((M).to))

#define POS(Y,X) (chess_pos){ .y = (Y), .x = (X) }
#define MOVE(FROMY,FROMX,TOY,TOX) (chess_move){ .from = POS(FROMY,FROMX), .to = POS(TOY,TOX) }

#define POS_INVALID POS(INVALID, INVALID)
#define POS_IS_INVALID(P) ((P).x == INVALID || (P).y == INVALID)

#define MOVE_INVALID MOVE(INVALID, INVALID, INVALID, INVALID)

#define SELECTED_IS_INVALID (POS_IS_INVALID(selected.from) || POS_IS_INVALID(selected.to))


typedef enum {
	NONE = 0,
	PAWN = 1,
	KNIGHT = 2,
	BISHOP = 3,
	ROOK = 4,
	QUEEN = 5,
	KING = 6,
} pieces_t;

typedef enum {
	MOVE_NOT_ALLOWED = 0,
	MOVE_VALID = 1,
	MOVE_CASTELING = 2,
	MOVE_CASTELING_KINGSIDE = 6,
	MOVE_CASTELING_QUEENSIDE = 10,
	MOVE_PROMOTION = 16,
	MOVE_EN_PASSANT = 32,
	MOVE_CAPTURE = 128,
} move_t;

#define BOARD_SELECTED_FROM board[selected.from.y][selected.from.x]
#define BOARD_SELECTED_TO board[selected.to.y][selected.to.x]
extern int8_t board[8][8];

extern int8_t strength;  // AI strength

typedef struct {
	uint8_t y, x;
} chess_pos;

typedef struct {
	chess_pos from, to;
} chess_move;

extern chess_move selected;

void board_reset();
uint16_t board_moveCounter();
bool board_isDraw();
bool board_isCheck(int8_t player);
bool board_hasKing(int8_t player);
bool board_canMove(chess_move move, int8_t player);
uint8_t board_move(int8_t player);

#endif
