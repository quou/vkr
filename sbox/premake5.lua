project "sbox"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

	targetdir "../bin"

	architecture "x86_64"

	pic "on"

	files {
		"src/**.hpp",
		"src/**.cpp",
	}

	includedirs {
		"../vkr/include"
	}

	links {
		"vkr"
	}

	defines {
		"VKR_IMPORT_SYMBOLS"
	}

	filter "configurations:debug"
		runtime "debug"
		symbols "on"

		defines {
			"DEBUG"
		}

	filter "configurations:release"
		runtime "release"
		optimize "on"

		defines {
			"RELEASE"
		}
