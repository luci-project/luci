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
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <termios.h>

#include "tui.h"
#include "board.h"
#include "openings.h"


bool tui_init() {
	struct termios term;
	if (tcgetattr(STDIN_FILENO, &term))
		return false;

	// No line oriented (= non-canonical) input
	term.c_lflag &= ~ICANON;
	// No echo of pressed keys
	term.c_lflag &= ~ECHO;

	term.c_cc[VMIN] = 0;
	term.c_cc[VTIME] = 0;

	return !tcsetattr(STDIN_FILENO, TCSANOW, &term);
}


void tui_resetMove() {
	selected = MOVE_INVALID;
}


bool tui_readMove(chess_pos * input) {
	*input = POS_INVALID;
	// Input loop
	while (!VALID_POS(*input)) {
		// Wait for input
		char c;
		while (read(STDIN_FILENO, &c, 1) != 1)
			// delay du to non-blocking input
			usleep(10000);

		// analyze input
		switch(c) {
			case 'A' ... 'H':
				// select row
				input->x = c - 'A';
				break;

			case 'a' ... 'h':
				// select row as well
				input->x = c - 'a';
				break;

			case '1' ... '8':
				// select column
				input->y = 8 - (c - '0');
				break;
		}
	}
	return true;
}

