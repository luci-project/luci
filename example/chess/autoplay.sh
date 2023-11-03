#!/usr/bin/env bash

# This just outputs prescripted moves for player 2 (with some delay),
# completely ignoring the actual moves of player 1 / ai

DELAY=7
function move() {
	sleep ${DELAY}
	echo $2
}

## Move 1
move "d2-d4" "d7-d5"

## Move 2
move "c2-c4" "e7-e6"

## Move 3
move "Nb1-c3" "c7-c6"

## Move 4
move "c4xd5" "c6xd5"

## Move 5
move "Bc1-f4" "Nb8-c6"

## Move 6
move "a2-a3" "Ke8-d7"

## Move 7
move "e2-e4" "a7-a6"

## Move 8
move "e4xd5" "e6xd5"

## Move 9
move "Ke1-d2" "Ra8-a7"

## Move 10
move "Qd1-b1" "Nc6xd4"

## Move 11
move "Ra1-a2" "Nd4-b3"

while true ; do
	## Move 12...
	move "Kd2-c2" "Nb3-d4"

	## Move 13...
	move "Kc2-d2" "Nd4-b3"
done

