project "stb"
	kind "StaticLib"
	language "C"
	staticruntime "on"

	architecture "x64"

	pic "on"

	targetdir "../bin"
	objdir "obj"

	files {
		"include/**.h",
		"src/stb.c"
	}

	includedirs {
		"include"
	}

	filter "system:windows"
		defines {
			"_CRT_SECURE_NO_WARNINGS"
		}

	filter "configurations:debug"
		runtime "debug"
		symbols "on"

	filter "configurations:release"
		runtime "release"
		optimize "on"
