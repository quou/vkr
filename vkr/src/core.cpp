#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "vkr.hpp"

namespace vkr {
	void error(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		verror(fmt, args);
		va_end(args);
	}

	void warning(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		vwarning(fmt, args);
		va_end(args);
	}

	void info(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		vinfo(fmt, args);
		va_end(args);
	}

	void abort_with(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		vabort_with(fmt, args);
		va_end(args);
	}

	void verror(const char* fmt, va_list args) {
		printf("\033[31;31merror \033[0m");
		vprintf(fmt, args);
		printf("\n");
	}

	void vwarning(const char* fmt, va_list args) {
		printf("\033[31;35mwarning \033[0m");
		vprintf(fmt, args);
		printf("\n");
	}

	void vinfo(const char* fmt, va_list args) {
		printf("\033[31;32minfo \033[0m");
		vprintf(fmt, args);
		printf("\n");
	}

	void vabort_with(const char* fmt, va_list args) {
		verror(fmt, args);
		exit(1);
	}
}
