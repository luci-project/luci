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

#include "ai.h"
#include <assert.h>

#include "board.h"
#include "openings.h"

#define mininf ((int16_t)-32500)
#define maxinf ((int16_t)32500)

// Taken from http://chessprogramming.wikispaces.com/Simplified+evaluation+function
const int16_t eval_values[] = { 0, 100, 320, 330, 500, 900, 20000 };

const int8_t eval_table[][8][8] = {
	{	// king (end game - delta)
		{-70,-60,-40,-30,-30,-40,-60,-70 },
		{-50,-50,  0,  0,  0,  0,-50,-50 },
		{-20, 10, 40, 50, 50, 40, 10,-20 },
		{-10, 20, 60, 80, 80, 60, 20,-10 },
		{  0, 30, 70, 90, 90, 70, 30,  0 },
		{  0, 30, 60, 80, 80, 60, 30,  0 },
		{  0, 20, 30, 50, 50, 30, 20,  0 },
		{-20,  0, 10, 30, 30, 10,  0,-20 }
	},
	{	// Pawns
		{  0,  0,  0,  0,  0,  0,  0,  0 },
		{  5, 10, 10,-20,-20, 10, 10,  5 },
		{  5, -5,-10,  0,  0,-10, -5,  5 },
		{  0,  0,  0, 20, 20,  0,  0,  0 },
		{  5,  5, 10, 25, 25, 10,  5,  5 },
		{ 10, 10, 20, 30, 30, 20, 10, 10 },
		{ 50, 50, 50, 50, 50, 50, 50, 50 },
		{  0,  0,  0,  0,  0,  0,  0,  0 }
	},
	{	// Knights
		{-50,-40,-30,-30,-30,-30,-40,-50 },
		{-40,-20,  0,  5,  5,  0,-20,-40 },
		{-30,  5, 10, 15, 15, 10,  5,-30 },
		{-30,  0, 15, 20, 20, 15,  0,-30 },
		{-30,  5, 15, 20, 20, 15,  5,-30 },
		{-30,  0, 10, 15, 15, 10,  0,-30 },
		{-40,-20,  0,  0,  0,  0,-20,-40 },
		{-50,-40,-30,-30,-30,-30,-40,-50 }
	},
	{ // Rooks
		{  0,  0,  0,  5,  5,  0,  0,  0 },
		{ -5,  0,  0,  0,  0,  0,  0, -5 },
		{ -5,  0,  0,  0,  0,  0,  0, -5 },
		{ -5,  0,  0,  0,  0,  0,  0, -5 },
		{ -5,  0,  0,  0,  0,  0,  0, -5 },
		{ -5,  0,  0,  0,  0,  0,  0, -5 },
		{  5, 10, 10, 10, 10, 10, 10,  5 },
		{  0,  0,  0,  0,  0,  0,  0,  0 }
	},
	{ // Queen
		{-20,-10,-10, -5, -5,-10,-10,-20 },
		{-10,  0,  5,  0,  0,  0,  0,-10 },
		{-10,  5,  5,  5,  5,  5,  0,-10 },
		{  0,  0,  5,  5,  5,  5,  0, -5 },
		{ -5,  0,  5,  5,  5,  5,  0, -5 },
		{-10,  0,  5,  5,  5,  5,  0,-10 },
		{-10,  0,  0,  0,  0,  0,  0,-10 },
		{-20,-10,-10, -5, -5,-10,-10,-20 }
	},
	{ // King (middle game)
		{ 20, 30, 10,  0,  0, 10, 30, 20 },
		{ 20, 20,  0,  0,  0,  0, 20, 20 },
		{-10,-20,-20,-20,-20,-20,-20,-10 },
		{-20,-30,-30,-40,-40,-30,-30,-20 },
		{-30,-40,-40,-50,-50,-40,-40,-30 },
		{-30,-40,-40,-50,-50,-40,-40,-30 },
		{-30,-40,-40,-50,-50,-40,-40,-30 },
		{-30,-40,-40,-50,-50,-40,-40,-30 }
	}
};

void ai_reset() {
	openings_reset();
}

#define AI_NEGAMAXMOV_CALL()	int16_t val = (-1) * ai_negamax(depth - 1, (-1) * b, (-1) * a, player * (-1), false);

#define AI_NEGAMAXMOV_EVAL(TOY, TOX)								\
			if ( val > bestVal ){									\
				bestVal = val;										\
				if (record)											\
					selected = MOVE(fromY, fromX, (TOY), (TOX));	\
			}														\
			if (val > a)											\
				a = val;											\
			if (a >= b )											\
				return bestVal;										\

#define AI_NEGAMAXMOV(TOY, TOX)																		\
	do {																							\
		if (VALID_FIELD((TOY),(TOX)) && board_canMove(MOVE(fromY, fromX, (TOY), (TOX)), player)){	\
			int8_t oldFrom = board[fromY][fromX];													\
			int8_t oldTo = board[(TOY)][(TOX)];														\
			board[fromY][fromX] = NONE;																\
			board[(TOY)][(TOX)] = oldFrom;															\
			if (oldFrom == player && TOY == ( player > 0 ? 7 : 0))									\
				board[(TOY)][(TOX)] = QUEEN * player;												\
			AI_NEGAMAXMOV_CALL();																	\
			board[fromY][fromX] = oldFrom;															\
			board[(TOY)][(TOX)] = oldTo;															\
			AI_NEGAMAXMOV_EVAL((TOY),(TOX))															\
		}																							\
	} while(0)
__attribute__ ((hot))
int16_t ai_negamax(int8_t depth, int16_t a, int16_t b, int8_t player, bool record) {
	if (!board_hasKing(player))
		return mininf;
	else if (depth <= 0) {
		int16_t values = 0;
		uint8_t pieces[3] = { 0 };
		uint8_t queens[3] = { 0 };
		for (int8_t y = 0; y < 8; y++)
			for (int8_t x = 0; x < 8; x++) {
				int8_t fig = board[y][x];
				if (fig != NONE) {
					int8_t figPlayer = fig < 0 ? -1 : 1;
					values += (eval_values[fig * figPlayer] + eval_table[fig * figPlayer][figPlayer < 0 ? 7 - y : y][x]) * figPlayer;
					// Endgame detection
					if (fig == QUEEN)
						queens[figPlayer + 1]++;
					else
						pieces[figPlayer + 1]++;
				}
			}
		// Endgame check
		if ((queens[0] == 0 || (queens[0] == 1 && pieces[0] <= 3)) && (queens[2] == 0 || (queens[2] == 1 && pieces[2] <= 3)))
			for (int8_t y = 0; y < 8; y++)
				for (int8_t x = 0; x < 8; x++) {
					// Use the delta table for the king endgame
					if (board[y][x] == KING)
						values += eval_table[0][y][x];
					else if (board[y][x] == -1 * KING)
						values -= eval_table[0][7 - y][x];
				}
		return values * player;
	}
	// calculate best move
	else {
		int16_t bestVal = mininf;
		for (uint8_t fromY = 0; fromY < 8; fromY++)
			for (uint8_t fromX = 0; fromX < 8; fromX++) {
				int8_t fig = board[fromY][fromX] * player;
				if (fig > 0)
					switch (fig) {
						case PAWN:
							AI_NEGAMAXMOV(player + fromY, fromX);
							AI_NEGAMAXMOV(2 * player + fromY, fromX);
							// Note: en passant moves might not be done correctly - for efficiency reasons we ignore it.
							AI_NEGAMAXMOV(player + fromY, fromX + 1);
							AI_NEGAMAXMOV(player + fromY, fromX - 1);
							break;
						case KNIGHT:
							for (int8_t i = -1; i <= 1; i = i + 2)
								for (int8_t j = -1; j <= 1; j = j + 2) {
									AI_NEGAMAXMOV(fromY + 1 * i, fromX + 2 * j);
									AI_NEGAMAXMOV(fromY + 2 * i, fromX + 1 * j);
								}
							break;
						case KING:
							for (int8_t i = -1; i <= 1; i++)
								for (int8_t j = -1; j <= 1; j++)
									AI_NEGAMAXMOV(fromY + i, fromX + j);
							// Try Castling
							if (fromX == 4 && fromY == (player > 0 ? 7 : 0)) {
								// ... Queenside
								if (board_canMove(MOVE(fromY, fromX, fromY, 2), player)) {
									assert(board[fromY][0] == ROOK * player && board[fromY][1] == NONE && board[fromY][2] == NONE && board[fromY][3] == NONE && board[fromY][4] == KING * player);
									board[fromY][0] = NONE;
									board[fromY][2] = KING * player;
									board[fromY][3] = ROOK * player;
									board[fromY][4] = NONE;
									AI_NEGAMAXMOV_CALL();
									board[fromY][0] = ROOK * player;
									board[fromY][2] = NONE;
									board[fromY][3] = NONE;
									board[fromY][4] = KING * player;
									AI_NEGAMAXMOV_EVAL(fromY, 2)
								}
								// ... Kingside
								if (board_canMove(MOVE(fromY, fromX, fromY, 6), player)) {
									assert(board[fromY][7] == ROOK * player && board[fromY][6] == NONE && board[fromY][5] == NONE && board[fromY][4] == KING * player);
									board[fromY][7] = NONE;
									board[fromY][6] = KING * player;
									board[fromY][5] = ROOK * player;
									board[fromY][4] = NONE;
									AI_NEGAMAXMOV_CALL();
									board[fromY][4] = KING * player;
									board[fromY][5] = NONE;
									board[fromY][6] = NONE;
									board[fromY][7] = ROOK * player;
									AI_NEGAMAXMOV_EVAL(fromY, 6)
								}
							}
							break;
						default:
							assert(fig == ROOK || fig == BISHOP || fig == QUEEN);
							// bishop or queen
							if (fig != ROOK)
								for (int8_t i = -8; i <= 8; i++)
									if (i != 0) {
										AI_NEGAMAXMOV(fromY + i, fromX + i);
										AI_NEGAMAXMOV(fromY - i, fromX - i);
										AI_NEGAMAXMOV(fromY + i, fromX - i);
										AI_NEGAMAXMOV(fromY - i, fromX + i);
									}
							// rook or queen
							if (fig != BISHOP)
								for (int8_t i = -8; i <= 8; i++)
									if (i != 0) {
										AI_NEGAMAXMOV(fromY + i, fromX);
										AI_NEGAMAXMOV(fromY - i, fromX);
										AI_NEGAMAXMOV(fromY, fromX + i);
										AI_NEGAMAXMOV(fromY, fromX - i);
									}
					}
			}
		return bestVal;
	}
}

int16_t ai_move(int8_t player, uint8_t level) {
	assert(level > 0);
	if (!openings_move(player)){
		selected = MOVE_INVALID;
		return ai_negamax(level, mininf, maxinf, player, true) * player;
	}
	return 0;
}
