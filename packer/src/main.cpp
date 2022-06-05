#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <filesystem>
#include <string>
#include <unordered_map>

#include <vkr/vkr.hpp>

using namespace vkr;

#define buffer_size 2048
u8 buffer[buffer_size];

struct Entry {
	u64 path_hash;
	u64 path_offset;
	u64 blob_offset;
	u64 blob_size;
	u64 path_size;

	std::string name;
};

struct Header {
	u64 table_offset;
	u64 table_count;
	u64 path_offset;
	u64 blob_offset;
};

i32 main(i32 argc, const char** argv) {
	if (argc != 3) {
		info("Usage: %s res_dir dst.", argv[0]);
		abort_with("Invalid arguments.");
	}

	std::unordered_map<u64, Entry> resources;
	u64 table_size = 0;
	u64 path_size = 0;
	u64 blob_size = 0;

	for (const auto& resource : std::filesystem::recursive_directory_iterator(argv[1])) {
		if (resource.is_regular_file()) {
			u64 path_hash = hash_string(resource.path().c_str());

			resources[path_hash] = Entry {
				.path_hash = path_hash,
				.path_offset = 0,
				.blob_offset = 0,
				.blob_size = resource.file_size(),
				.path_size = resource.path().string().size(),
				.name = resource.path().string()
			};
		}
	}

	for (auto& resource : resources) {
		resource.second.path_offset = path_size;
		resource.second.blob_offset = blob_size;

		table_size += sizeof(u64) * 5;
		path_size += resource.second.path_size;
		blob_size += resource.second.blob_size;
	}

	FILE* out_file = fopen(argv[2], "ab");
	if (!out_file) {
		abort_with("Failed to fopen `%s'.", argv[2]);
	}

	Header header;
	header.table_offset = 4 * sizeof(u64);
	header.table_count = resources.size();
	header.path_offset = header.table_offset + table_size;
	header.blob_offset = header.path_offset + path_size;

	/* Write the header. */
	fwrite(&header.table_offset, sizeof(u64), 1, out_file);
	fwrite(&header.table_count,  sizeof(u64), 1, out_file);
	fwrite(&header.path_offset,  sizeof(u64), 1, out_file);
	fwrite(&header.blob_offset,  sizeof(u64), 1, out_file);

	/* Write the table. */
	for (auto& resource : resources) {
		fwrite(&resource.second.path_hash,   sizeof(u64), 1, out_file);
		fwrite(&resource.second.path_offset, sizeof(u64), 1, out_file);
		fwrite(&resource.second.blob_offset, sizeof(u64), 1, out_file);
		fwrite(&resource.second.blob_size,   sizeof(u64), 1, out_file);
		fwrite(&resource.second.path_size,   sizeof(u64), 1, out_file);
	}

	/* Write the file paths. */
	for (auto& resource : resources) {
		fwrite(resource.second.name.c_str(), sizeof(char), resource.second.name.size(), out_file);
	}

	/* Write the file blobs. */
	for (auto& resource : resources) {
		FILE* in_file = fopen(resource.second.name.c_str(), "rb");
		if (!in_file) {
			warning("Failed to fopen `%s'.", resource.second.name.c_str());
			continue;
		}

		for (u64 i = 0; i < resource.second.blob_size; i++) {
			u64 read = fread(buffer, 1, buffer_size, in_file);
			fwrite(buffer, 1, read, out_file);
		}

		fclose(in_file);
	}

	/* Write the final package size. */
	u64 final_size = sizeof(u64) * 4 + table_size + blob_size + path_size;
	fwrite(&final_size, sizeof(u64), 1, out_file);

	info("Wrote %llu bytes.", final_size);

	fclose(out_file);
}
