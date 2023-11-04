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

#include <assert.h>
#include <stdlib.h>

#include "openings.h"
#include "openings_helper.h"

#include "board.h"

#include "openings.db"

#define COUNT(X) (sizeof(X)/sizeof(X[0]))

_Static_assert(COUNT(openings) < OPENING_NONE, "Too much opening moves, cannot handle them");
_Static_assert(RAND_MAX > OPENINGS_GOOD, "Maximum random number is smaller than the possible openings");

uint8_t opening_invalid[COUNT(openings) / 8 + 1];

void openings_reset() {
	for (uint8_t o = 0; o < COUNT(opening_invalid); o++)
		opening_invalid[o] = 0;
}

uint8_t openings_invalidate(uint16_t moveCounter) {
	uint8_t use_opening = OPENING_NONE;
	for (uint8_t o = 0; o < COUNT(openings); o++)
		if (OPENING_IS_VALID(o)) {
			if (OPENING_FROM_X(o, moveCounter) == selected.from.x && OPENING_FROM_Y(o, moveCounter) == selected.from.y && OPENING_TO_X(o, moveCounter) == selected.to.x && OPENING_TO_Y(o, moveCounter) == selected.to.y) {
				if (use_opening == OPENING_NONE)
					use_opening = o;
			} else {
				OPENING_INVALIDATE(o);
			}
		}
	return use_opening;
}

bool openings_move(int8_t player) {
	uint8_t use_opening = OPENING_NONE;
	uint16_t moveCounter = board_moveCounter();
	if (moveCounter < OPENING_ROUNDS * 2)
		use_opening = moveCounter > 0 ? openings_invalidate(moveCounter) : (rand() % OPENINGS_GOOD);
	if (use_opening != OPENING_NONE) {
		selected = MOVE(OPENING_FROM_Y(use_opening, moveCounter + 1), OPENING_FROM_X(use_opening, moveCounter + 1), OPENING_TO_Y(use_opening, moveCounter + 1), OPENING_TO_X(use_opening, moveCounter + 1));
		if (!board_canMove(selected, player))
			assert(false && "invalid opening");
		else
			openings_invalidate(moveCounter + 1);
		return true;
	} else {
		return false;
	}
}


void openings_validate() {
#ifndef NDEBUG
	for (uint8_t o = 0; o < COUNT(openings); o++) {
		board_reset();
		int8_t player = 1;
		for (uint8_t m = 1; m <= OPENING_ROUNDS * 2; m++) {
			player *= -1;
			selected = MOVE(OPENING_FROM_Y(o, m), OPENING_FROM_X(o, m), OPENING_TO_Y(o, m), OPENING_TO_X(o, m));
			assert(board_move(player));
		}
	}
#endif
}
