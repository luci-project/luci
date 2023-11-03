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

#ifndef CHESS_AI_H
#define CHESS_AI_H

#include <stdint.h>
#include <stdbool.h>

void ai_reset();
int16_t ai_move(int8_t, uint8_t);

#endif
