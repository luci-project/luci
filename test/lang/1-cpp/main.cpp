#include <iostream>
#include <chrono>
#include <thread>

#include "fib.h"

int main() {
	std::cout << "[C++ main]" << std::endl;
	for (long i = 0; i < 3; i++) {
		if (i)
			std::this_thread::sleep_for(std::chrono::seconds(10));
		std::cout << "fib(" << i << ") = " << fib(i) << std::endl << std::flush;
		printfib(21 + i);
	}
	return 0;
}
