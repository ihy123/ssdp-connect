-- premake project file for including as a dependency to my project
project "ssdp-connect"
	kind "StaticLib"
	language "C"
	targetdir "lib/%{cfg.buildcfg}"
	
	-- set up files
	files { "ssdp.h" "ssdp.c" "ssdp-connect.h" "ssdp-connect.c" }
	
	-- set up configurations
	filter "configurations:Release"
		optimize "Speed"
		runtime "Release"
		staticruntime "On"
	filter "configurations:Debug"
		symbols "On"
		runtime "Debug"
	
