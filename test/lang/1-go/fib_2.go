package main

import "C"
import "fmt"

//export version [not supported yet in Go]
const version C.short = 2;

func fibalgo(value C.long) C.long {
	var l C.long = 0;
	var p C.long = 1;
	var n C.long = value;
	for i := C.long(1); i < value; i++ {
		n = l + p;
		l = p;
		p = n;
	}
	return n;
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