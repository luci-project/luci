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

#include "board.h"

#include <assert.h>

#include "ai.h"
#include "openings.h"


const int8_t board_default[8][8] = {
	{  4,  2,  3,  5,  6,  3,  2,  4 },
	{  1,  1,  1,  1,  1,  1,  1,  1 },
	{  0,  0,  0,  0,  0,  0,  0,  0 },
	{  0,  0,  0,  0,  0,  0,  0,  0 },
	{  0,  0,  0,  0,  0,  0,  0,  0 },
	{  0,  0,  0,  0,  0,  0,  0,  0 },
	{ -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -4, -2, -3, -5, -6, -3, -2, -4 }
};

int8_t board[8][8];
uint16_t board_counter[3];
chess_move selected;
chess_move selected_last;

#define ROCHADE_ROOK_DISABLE(X, P)  board_rochade |= 1 << ((((P) >> 7) & 1) * 2 + ((X) & 1))
#define ROCHADE_ROOK_POSSIBLE(X, P)  (((board_rochade >> ((((P) >> 7) & 1) * 2 + ((X) & 1))) & 1) == 0)
uint8_t board_rochade;

void board_reset() {
	openings_reset();
	board_counter[0] = 0;
	board_counter[1] = 0;
	board_counter[2] = 0;
	for (uint8_t y = 0; y < 8; y++)
		for (uint8_t x = 0; x < 8; x++)
			board[y][x] = board_default[y][x];
	selected_last = selected = MOVE_INVALID;
	board_rochade = 0;
}

bool board_isDraw() {
	return board_counter[1] - board_counter[0] > 100 || board_counter[1] - board_counter[2] > 100;
}

bool board_hasKing(int8_t player) {
	for (uint8_t y = 0; y < 8; y++)
		for (uint8_t x = 0; x < 8; x++)
			if (board[y][x] == KING * player)
				return true;
	return false;
}


uint16_t board_moveCounter() {
	return board_counter[1];
}


#define CAPTURE_MARK(Y, X) if (VALID_FIELD(Y, X)) captureBoard |= ((uint64_t)1 << ((Y) * 8 + (X)))
#define CAPTURE_CHECK(Y, X) (((captureBoard >> ((Y) * 8 + (X))) & 1) == 1)
uint64_t board_capture(int8_t player) {
	uint64_t captureBoard = 0llu;
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++) {
			uint8_t fig = board[y][x] * player * (-1);
			switch (fig) {
				case PAWN:
					CAPTURE_MARK(y - player, x + 1);
					CAPTURE_MARK(y - player, x - 1);
					break;
				case KNIGHT:
					CAPTURE_MARK(y + 2, x + 1);
					CAPTURE_MARK(y + 2, x - 1);
					CAPTURE_MARK(y - 2, x + 1);
					CAPTURE_MARK(y - 2, x - 1);
					CAPTURE_MARK(y + 1, x + 2);
					CAPTURE_MARK(y + 1, x - 2);
					CAPTURE_MARK(y - 1, x + 2);
					CAPTURE_MARK(y - 1, x - 2);
					break;
				case KING:
					CAPTURE_MARK(y, x + 1);
					CAPTURE_MARK(y + 1, x + 1);
					CAPTURE_MARK(y + 1, x);
					CAPTURE_MARK(y + 1, x - 1);
					CAPTURE_MARK(y, x - 1);
					CAPTURE_MARK(y - 1, x + 1);
					CAPTURE_MARK(y - 1, x);
					CAPTURE_MARK(y - 1, x - 1);
					break;
				case QUEEN:
				case ROOK:
					for (int8_t i = x + 1; i < 8 && (captureBoard |= (uint64_t) 1 << ((y) * 8 + (i))) != 0 && board[y][i] == NONE; i++) ;
					for (int8_t i = x - 1; i >= 0 && (captureBoard |= (uint64_t) 1 << ((y) * 8 + (i))) != 0 && board[y][i] == NONE; i--) ;
					for (int8_t i = y + 1; i < 8 && (captureBoard |= (uint64_t) 1 << ((i) * 8 + (x))) != 0 && board[i][x] == NONE; i++) ;
					for (int8_t i = y - 1; i >= 0 && (captureBoard |= (uint64_t) 1 << ((i) * 8 + (x))) != 0 && board[i][x] == NONE; i--) ;
					if (fig == ROOK)
						break;
				case BISHOP:
					// bishop or queen
					for (int8_t i = 1; y + i < 8 && x + i < 8 && (captureBoard |= (uint64_t) 1 << ((y + i) * 8 + (x + i))) != 0 && board[y + i][x + i] == NONE; i++) ;
					for (int8_t i = 1; y - i >= 0 && x - i >= 0 && (captureBoard |= (uint64_t) 1 << ((y - i) * 8 + (x - i))) != 0 && board[y - i][x - i] == NONE; i++) ;
					for (int8_t i = 1; y + i < 8 && x - i >= 0 && (captureBoard |= (uint64_t) 1 << ((y + i) * 8 + (x - i))) != 0 && board[y + i][x - i] == NONE; i++) ;
					for (int8_t i = 1; y - i >= 0 && x + i < 8 && (captureBoard |= (uint64_t) 1 << ((y - i) * 8 + (x + i))) != 0 && board[y - i][x + i] == NONE; i++) ;
					break;
			}
		}
	return captureBoard;
}

bool board_isCheck(int8_t player) {
	for (uint8_t y = 0; y < 8; y++)
		for (uint8_t x = 0; x < 8; x++)
			if (board[y][x] == KING * player) {
				uint64_t captureBoard = board_capture(player);
				return CAPTURE_CHECK(y, x);
			}
	return false;
}

bool board_canMove(chess_move move, int8_t player) {
	if (!VALID_MOVE(move) || player * player != 1 || (move.from.y == move.to.y && move.from.x == move.to.x))
		return false;

	int8_t from = board[move.from.y][move.from.x];
	int8_t to = board[move.to.y][move.to.x];
	pieces_t piece = (pieces_t)(from * player);
	if (from == 0 || from * player <= 0 || to * player > 0) {
		return false;
	} else if (piece == PAWN) {
		if (move.from.x == move.to.x && to == 0 && (move.to.y - move.from.y == player || (move.to.y - move.from.y == 2 * player && board[player + move.from.y][move.from.x] == 0 && move.from.y == (player > 0 ? 1 : 6))))
			return true;
		// capture (inc. en passant)
		else
			return ((move.from.x - move.to.x) * (move.from.x - move.to.x) == 1 && move.to.y - move.from.y == player) && (to * player < 0 || (board[selected_last.to.y][selected_last.to.x] == PAWN * (-1) * player && (selected_last.to.y - selected_last.from.y) * (selected_last.to.y - selected_last.from.y) == 4 && selected_last.to.y == move.from.y && selected_last.to.x == selected_last.from.x && selected_last.to.x == move.to.x));
	} else if (piece == QUEEN || piece == ROOK || piece == BISHOP) {
		// rook - and queen
		if (piece != BISHOP) {
			if (move.from.x == move.to.x) {
				for (uint8_t i = (move.from.y < move.to.y ? move.from.y : move.to.y) + 1; i < (move.from.y > move.to.y ? move.from.y : move.to.y); i++)
					if (board[i][move.from.x] != NONE)
						return false;
				return true;
			} else if (move.from.y == move.to.y) {
				for (uint8_t i = (move.from.x < move.to.x ? move.from.x : move.to.x) + 1; i < (move.from.x > move.to.x ? move.from.x : move.to.x); i++)
					if (board[move.from.y][i] != NONE)
						return false;
				return true;
			}
		}
		// bishop - and queen
		if (piece != ROOK) {
			if ((move.from.y - move.to.y) * (move.from.y - move.to.y) != (move.from.x - move.to.x) * (move.from.x - move.to.x))
				return false;
			for (int8_t i = 1; i < (move.from.y > move.to.y ? move.from.y - move.to.y : move.to.y - move.from.y); i++)
				if (board[move.from.y + i * (move.to.y - move.from.y > 0 ? 1 : -1)][move.from.x + i * (move.to.x - move.from.x > 0 ? 1 : -1)] != NONE)
					return false;
			return true;
		}
		// nothing found? wrong move.
		return false;
	} else if (piece == KNIGHT) {
		return (((move.from.y - move.to.y == 2 || move.from.y - move.to.y == -2) && (move.from.x - move.to.x == 1 || move.from.x - move.to.x == -1)) || ((move.from.y - move.to.y == 1 || move.from.y - move.to.y == -1) && (move.from.x - move.to.x == 2 || move.from.x - move.to.x == -2)));
	} else if (piece == KING) {
		uint64_t captureBoard = board_capture(player);
		// king must not be in check after move
		if (CAPTURE_CHECK(move.to.y, move.to.x))
			return false;
		// Check castling
		if (move.from.x == 4 && move.from.y == (player < 0 ? 7 : 0) && move.to.y == move.from.y && !CAPTURE_CHECK(move.from.y, move.from.x) && ((move.to.x == 2 && board[move.from.y][0] == ROOK * player && board[move.from.y][1] == NONE && board[move.from.y][2] == NONE && board[move.from.y][3] == NONE && ROCHADE_ROOK_POSSIBLE(0, player) && !CAPTURE_CHECK(move.from.y, 3)) || (move.to.x == 6 && board[move.from.y][7] == ROOK * player && board[move.from.y][6] == NONE && board[move.from.y][5] == NONE && ROCHADE_ROOK_POSSIBLE(7, player) && !CAPTURE_CHECK(move.from.y, 5))))
			return true;
		else
			return (((move.from.x - move.to.x) * (move.from.x - move.to.x)) <= 1 && ((move.from.y - move.to.y) * (move.from.y - move.to.y)) <= 1);
	} else {
		// invalid piece
		assert(false && "Invalid piece");
	}
}

uint8_t board_move(int8_t player) {
	if (board_canMove(selected, player)) {
		uint8_t m = MOVE_VALID;

		// En Passant
		if (BOARD_SELECTED_FROM == player && (selected.from.x - selected.to.x) * (selected.from.x - selected.to.x) == 1 && selected.to.y - selected.from.y == player && (selected_last.to.y - selected_last.from.y) * (selected_last.to.y - selected_last.from.y) == 4 && board[selected_last.to.y][selected_last.to.x] == -1 * player && selected_last.to.y == selected.from.y && selected_last.to.x == selected_last.from.x && selected_last.to.x == selected.to.x) {
			board[selected_last.to.y][selected_last.to.x] = 0;
			m |= MOVE_EN_PASSANT;
		} else if (BOARD_SELECTED_FROM == ROOK * player) {
			// castling disable figures
			ROCHADE_ROOK_DISABLE(selected.from.x, player);
		} else if (BOARD_SELECTED_FROM == KING * player && selected.from.x == 4) {
			ROCHADE_ROOK_DISABLE(0, player);
			ROCHADE_ROOK_DISABLE(7, player);
			// Perform castling
			if (selected.to.x == 2) {
				assert(board[selected.to.y][0] == ROOK * player);
				board[selected.to.y][3] = board[selected.to.y][0];
				board[selected.to.y][0] = NONE;
				m |= MOVE_CASTELING_QUEENSIDE;
			} else if (selected.to.x == 6) {
				assert(board[selected.to.y][7] == ROOK * player);
				board[selected.to.y][5] = board[selected.to.y][7];
				board[selected.to.y][7] = NONE;
				m |= MOVE_CASTELING_KINGSIDE;
			}
		}
		// Fifty moves rule
		if (BOARD_SELECTED_TO != NONE || (m & MOVE_EN_PASSANT) != 0) {
			board_counter[player + 1] = board_counter[1];
			m |=  MOVE_CAPTURE;
		}

		// Pawn to Queen
		if (BOARD_SELECTED_FROM == player && selected.to.y == (player > 0 ? 7 : 0)) {
			BOARD_SELECTED_TO = QUEEN * player;
			m |= MOVE_PROMOTION;
		} else {
			// Move
			BOARD_SELECTED_TO = BOARD_SELECTED_FROM;
		}
		BOARD_SELECTED_FROM = 0;
		// Copy move
		selected_last = selected;
		board_counter[1]++;
		return m;
	} else {
		return MOVE_NOT_ALLOWED;
	}
}
