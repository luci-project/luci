#include "util.h"

const char * text[] = {
	"Hello world!",
	"Hallo Welt!",
	"Hola Mundo!",
	"Bonjour Monde!",
	"Hei Verden!"
};

const int delay = 3;

int main(int argc, char **argv) {
	println("Util example\n============");

	print("[Started '");
	for (int i = 0; i < argc; i++) {
		if (i > 0)
			print(" ");
		print(argv[i]);
	}
	println("']");

	const size_t num = sizeof(text)/sizeof(text[0]);

	log_message(INFO, "[Preparing log level...]\n");
	log_level = INFO;
	log_message(INFO, "[Log level set!]\n");
	log_version();

	for (size_t i = 0; i < num; ++i) {
		println(text[i]);
		log_message(VERBOSE, "[sleeping few seconds]\n");
		sleep(delay);
	}
	log_message(INFO, "[Fail?]\n");
	println(NULL);
	log_version();
	return 0;
}
