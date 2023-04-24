package main

/*
#cgo CFLAGS: -I.
#cgo LDFLAGS: -L. -lfib
#include "fib.h"
*/
import "C"

import (
	"fmt"
	"time"
)

func main() {
	fmt.Println("[Go main]")
	for i := C.long(0); i < 3; i++ {
		if i != 0 {
			time.Sleep(10 * time.Second)
		}
		fmt.Printf("fib(%d) = %d\n", i, C.fib(i))
		C.printfib(21 + i)
	}
}