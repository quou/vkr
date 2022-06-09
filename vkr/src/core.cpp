#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vkr.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

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
#ifdef _WIN32
		auto console = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(console, FOREGROUND_RED);
		printf("error ");
		SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
		printf("\033[31;31merror \033[0m");
#endif
		vprintf(fmt, args);
		printf("\n");
	}

	void vwarning(const char* fmt, va_list args) {
#ifdef _WIN32
		auto console = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_BLUE);
		printf("warning ");
		SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
		printf("\033[31;35mwarning \033[0m");
#endif
		vprintf(fmt, args);
		printf("\n");
	}

	void vinfo(const char* fmt, va_list args) {
#ifdef _WIN32
		auto console = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(console, FOREGROUND_GREEN);
		printf("info ");
		SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
		printf("\033[31;32minfo \033[0m");
#endif
		vprintf(fmt, args);
		printf("\n");
	}

	void vabort_with(const char* fmt, va_list args) {
		verror(fmt, args);
		exit(1);
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

	struct PackHeader {
		u64 table_offset;
		u64 table_count;
		u64 path_offset;
		u64 blob_offset;
	};

	struct {
		FILE* file;

		u64 pack_size;
		u64 self_size;
		u64 pack_offset;

		PackHeader header;
	} packer;

	void init_packer(i32 argc, const char** argv) {
#ifndef DEBUG
		packer.file = fopen(argv[0], "rb");

		if (!packer.file) {
			abort_with("Failed to fopen myself (%s).", argv[0]);
		}

		fseek(packer.file, 0, SEEK_END);
		packer.self_size = ftell(packer.file);

		fseek(packer.file, static_cast<long>(packer.self_size - sizeof(u64)), SEEK_SET);

		fread(&packer.pack_size, 1, sizeof(u64), packer.file);

		packer.pack_offset = packer.self_size - packer.pack_size - sizeof(u64);

		fseek(packer.file, static_cast<long>(packer.pack_offset), SEEK_SET);
		fread(&packer.header.table_offset, 1, sizeof(u64), packer.file);
		fread(&packer.header.table_count,  1, sizeof(u64), packer.file);
		fread(&packer.header.path_offset,  1, sizeof(u64), packer.file);
		fread(&packer.header.blob_offset,  1, sizeof(u64), packer.file);
#endif
	}

	void deinit_packer() {
#ifndef DEBUG
		fclose(packer.file);
#endif
	}

	bool read_raw(const char* path, u8** buffer, usize* size) {
#if DEBUG
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
#else
		/* Search for the file in the package. */
		for (u64 i = 0; i < packer.header.table_count; i++) {
			fseek(packer.file, static_cast<long>(packer.pack_offset + packer.header.table_offset + (i * sizeof(u64) * 5)), SEEK_SET);

			u64 path_hash, path_offset, blob_offset, blob_size, path_size;

			fread(&path_hash,   sizeof(u64), 1, packer.file);
			fread(&path_offset, sizeof(u64), 1, packer.file);
			fread(&blob_offset, sizeof(u64), 1, packer.file);
			fread(&blob_size,   sizeof(u64), 1, packer.file);
			fread(&path_size,   sizeof(u64), 1, packer.file);

			char name[1024];
			fseek(packer.file, static_cast<long>(packer.pack_offset + packer.header.path_offset + path_offset), SEEK_SET);
			fread(name, 1, path_size, packer.file);
			name[path_size] = '\0';

			if (strcmp(path, name) == 0) {
				*buffer = new u8[blob_size];
				fseek(packer.file, static_cast<long>(packer.pack_offset + packer.header.blob_offset + blob_offset), SEEK_SET);
				fread(*buffer, 1, blob_size, packer.file);

				if (size) {
					*size = blob_size;
				}

				return true;
			}
		}

		error("Failed to find `%s' in package.", path);

		return false;
#endif
	}
}
