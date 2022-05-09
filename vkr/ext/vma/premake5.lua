project "vma"
	kind "StaticLib"
	language "C++"
	staticruntime "on"

	architecture "x64"

	pic "on"

	targetdir "../bin"
	objdir "obj"

	files {
		"include/vk_mem_alloc.h",
		"src/vma.cpp"
	}

	includedirs {
		"include"
	}

	filter "configurations:debug"
		runtime "debug"
		symbols "on"

	filter "configurations:release"
		runtime "release"
		optimize "on"
