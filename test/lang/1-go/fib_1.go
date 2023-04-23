package main

import "C"
import "fmt"

//export version [not supported yet in Go]
const version C.short = 1;

func fibalgo(value C.long) C.long {
	if (value < 0) {
		return 0
	} else if (value < 2) {
		return value
	} else {
		return fibalgo(value - 1) + fibalgo(value - 2)
	}
}

//export fib
func fib(value C.long) C.long {
	return fibalgo(value);
}

//export printfib
func printfib(value C.long) {
	fmt.Printf("[Go Fibonacci Library v%d] fib(%d) = %d\n", version, value, fibalgo(value));
}

func main(){}