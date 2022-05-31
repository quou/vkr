#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

	bool read_raw(const char* path, u8** buffer, usize* size) {
		FILE* file = fopen(path, "rb");
		if (!file) {
			error("Failed to fopen `%s' for reading.", path);
			return false;
		}

		fseek(file, 0, SEEK_END);
		usize file_size = ftell(file);
		fseek(file, 0, SEEK_SET);

		*size = file_size;

		*buffer = new u8[file_size];
		if (fread(*buffer, 1, file_size, file) < file_size) {
			warning("Couldn't read all of `%s'.", path);
		}

		return true;
	}

	bool read_raw_text(const char* path, char** buffer) {
		FILE* file = fopen(path, "r");
		if (!file) {
			error("Failed to fopen `%s' for reading.", path);
			return false;
		}

		fseek(file, 0, SEEK_END);
		usize file_size = ftell(file);
		fseek(file, 0, SEEK_SET);

		*buffer = new char[file_size + 1];
		if (fread(*buffer, 1, file_size, file) < file_size) {
			warning("Couldn't read all of `%s'.", path);
		}

		(*buffer)[file_size] = '\0';

		return true;
	}

	bool write_raw(const char* path, u8* buffer, usize* size) {
		return false;
	}

	bool write_raw_text(const char* path, char* buffer) {
		return false;
	}

	u64 elf_hash(const u8* data, usize size) {
		u64 hash = 0, x = 0;

		for (u32 i = 0; i < size; i++) {
			hash = (hash << 4) + data[i];
			if ((x = hash & 0xF000000000LL) != 0) {
				hash ^= (x >> 24);
				hash &= ~x;
			}
		}

		return (hash & 0x7FFFFFFFFF);
	}

	u64 hash_string(const char* str) {
		return elf_hash(reinterpret_cast<const u8*>(str), strlen(str));
	}
}
