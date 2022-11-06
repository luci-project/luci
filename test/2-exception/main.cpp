#include <iostream>
#include <unistd.h>

#include "foo.hpp"

int main() {
	for (int i = 0; i < 5; i++) {
		if (i > 0)
			sleep(4);
		std::cout << "Run #" << i << std::endl;
		for (int j = -1; j <= 2; j++) {
			try {
				std::cout << "Executing 'foo(" << j << ")'... " << std::endl;
				int r = foo(j);
				std::cout << "returned " << r << "!" << std::endl;
			} catch (std::exception& e) {
				std::cout << "failed due to uncaught exception: " << e.what() << std::endl;
			}
		}
		std::cout << std::endl;
	}
}
