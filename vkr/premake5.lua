include "ext/glfw"

project "vkr"
	kind "SharedLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

	targetdir "../bin"

	architecture "x86_64"

	pic "on"

	files {
		"src/**.hpp",
		"src/**.cpp",
		"src/**.h",
		"src/**.c",
		"include/vkr/**.hpp",
		"include/vkr/**.h"
	}

	includedirs {
		"include/vkr",
		"ext/glfw/include"
	}

	defines {
		"VKR_EXPORT_SYMBOLS",
		"GLFW_INCLUDE_VULKAN"
	}

	links {
		"glfw"
	}

	filter "system:linux"
		links {
			"X11",
			"dl",
			"m",
			"GL",
			"pthread",
			"vulkan"
		}
	
	filter "system:windows"
		links {
			"opengl32",
			"user32",
			"gdi32",
			"kernel32",
		}

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
