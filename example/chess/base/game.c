/*
 * This file is part of the Luci Project (https://gitlab.cs.fau.de/luci-project/).
 * Copyright (c) 2023 by Bernhard Heinloth
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

#include <assert.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#include "tui.h"
#include "ai.h"
#include "board.h"
#include "openings.h"

const char * const game_playerName[2] = { "Alice" , "Bob" };
const uint8_t game_playerStrength[2] = { 4, 0 };

void game_describe(int8_t player, int8_t piece, uint8_t move) {
	// Output stream
	FILE * out = stdout;
	fputc('\n', out);

	// Long algebraic notation
	if ((move & MOVE_CASTELING) != 0){
		fputs((move & MOVE_CASTELING_QUEENSIDE) == MOVE_CASTELING_QUEENSIDE ? "0-0-0" : "0-0", out);
		if (board_isCheck(player * (-1)))
			fputc('#', out);
	} else {
		if (player * piece > 1)
			fputc("  NBRQK"[player * piece], out);
		fprintf(out, "%c%d%c%c%d", 'a' + selected.from.x, 8 - selected.from.y, (move & MOVE_CAPTURE) != 0 ? 'x' : '-', 'a' + selected.to.x, 8 - selected.to.y);
		if (board_isCheck(player * (-1)))
			fputc('+', out);
		else if ((move & MOVE_PROMOTION) == MOVE_PROMOTION)
			fputc('Q', out);
		else if ((move & MOVE_EN_PASSANT) == MOVE_EN_PASSANT)
			fputs(" e.p.", out);
	}
	fflush(out);
}


bool game_move(int8_t player) {
	uint8_t strength = game_playerStrength[player > 0];

	if (strength == 0) {
		// weak Human
		while (true) {
			chess_pos input = POS_INVALID;
			tui_resetMove();
			while (tui_readMove(&input)) {
				int8_t i = board[input.y][input.x];
				if (POS_IS_INVALID(selected.from) && i == 0) {
					fputs("You cannot choose an empty field!\n", stderr);
					break;
				} else if (POS_IS_INVALID(selected.from) && i * player < 0) {
					fputs("You cannot choose your enemies piece!\n", stderr);
					break;
				} else if (i * player > 0) {
					selected.from = input;
				} else if (input.y == selected.from.y && input.x == selected.from.x) {
					fputs("You didn't move the piece...\n", stderr);
				} else {
					selected.to = input;
					int8_t piece = BOARD_SELECTED_FROM;
					uint8_t move = board_move(player);
					if (move == MOVE_NOT_ALLOWED) {
						fputs("This move is not allowed!", stderr);
					} else {
						game_describe(player, piece, move);
						return true;
					}
				}
			}
		}
	} else {
		// stronger "AI"
		ai_move(player, strength);
		if (SELECTED_IS_INVALID)
			return false;
		sleep(2); // Make it slower so it is not so depressing to loose :)
		int8_t piece = BOARD_SELECTED_FROM;
		uint8_t move = board_move(player);
		assert(move != MOVE_NOT_ALLOWED && "AI did illegal move");
		game_describe(player, piece, move);
	}
	return true;
}


bool game_step(int8_t player) {
	if (board_isDraw()) {
		// Draw game
		puts("Draw game!");
		return false;
	} else if (!board_hasKing(player) || !game_move(player)) {
		// Winner is the other player
		printf("%s wins the game!", game_playerName[player < 0]);
		return false;
	} else {
		return true;
	}
}


void game() {
	int8_t player = 1;
	board_reset();
	do {
		// switch player
		player *= -1;
	} while (game_step(player));
}


int main() {
	// Check opening database
	openings_validate();

	// No buffering on stdin;
	tui_init();

	for (unsigned n = 1; ; n++) {
		printf("Starting chess game round #%u\n", n);
		// Wait 2 seconds;
		sleep(2);

		// Play one game
		game();

		// Wait 8 seconds
		sleep(8);
	}

	return 0;
}

