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
		"../vkr/include",
		"../vkr/ext/ecs/include"
	}

	links {
		"vkr",
		"ecs"
	}

	defines {
		"VKR_IMPORT_SYMBOLS"
	}

	filter { "system:linux", "configurations:release" }
		postbuildcommands {
			"cd ../ && ./bin/packer res/ ./bin/sbox"
		}
	
	filter { "system:windows", "configurations:release" }
		postbuildcommands {
			"cd ..\\ && bin\\packer.exe res bin\\sbox.exe"
		}

	filter "system:windows"
		defines {
			"_CRT_SECURE_NO_WARNINGS"
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
