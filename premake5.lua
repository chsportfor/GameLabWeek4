workspace "GameLabWeek4"
	architecture "x64"
	configurations { "Debug", "Release", "ObjViewerDebug", "ObjViewerRelease" }
	toolset "msc-v145"
	location "build"
	startproject "EngineApp"

local DirectXTK = "packages/directxtk_desktop_2019.2025.10.28.2"
local GoogleTest = "packages/Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn.1.8.1.8"

local function applyCommonSettings()
	language "C++"
	cppdialect "C++20"
	characterset "Unicode"
	systemversion "latest"
	warnings "Default"
	multiprocessorcompile "On"
	targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
	objdir "bin-int/%{cfg.buildcfg}/%{prj.name}"

	filter "configurations:Debug"
		defines { "_DEBUG", "IS_OBJ_VIEWER=0" }
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG", "IS_OBJ_VIEWER=0" }
		runtime "Release"
		optimize "Off"

	filter "configurations:ObjViewerDebug"
		defines { "_DEBUG", "IS_OBJ_VIEWER=1" }
		runtime "Debug"
		symbols "On"

	filter "configurations:ObjViewerRelease"
		defines { "NDEBUG", "IS_OBJ_VIEWER=1" }
		runtime "Release"
		optimize "Speed"

	filter {}
end

local function linkEngineDependencies()
	links {
		"d3d11",
		"d3dcompiler",
		"dxgi",
		"user32",
		"DirectXTK",
		"DirectXTex",
		"windowscodecs",
		"ole32",
	}
	filter "configurations:Debug"
		libdirs { DirectXTK .. "/native/lib/x64/Debug" }
	filter "configurations:ObjViewerDebug"
		libdirs { DirectXTK .. "/native/lib/x64/Debug" }
	filter "configurations:Release"
		libdirs { DirectXTK .. "/native/lib/x64/Release" }
	filter "configurations:ObjViewerRelease"
		libdirs { DirectXTK .. "/native/lib/x64/Release" }
	filter {}
end

group "Engine"
project "EngineLib"
	kind "StaticLib"
	applyCommonSettings()
	rtti "Off"

	includedirs {
		"EngineLib",
		DirectXTK .. "/include",
	}

	files {
		"EngineLib/**.h",
		"EngineLib/**.hpp",
		"EngineLib/**.inl",
		"EngineLib/**.cpp",
		"EngineLib/Assets/**",
		"EngineLib/Config/**",
	}

	filter "files:EngineLib/Assets/**"
		buildaction "None"

	filter "files:EngineLib/Config/**"
		buildaction "None"

	filter "files:EngineLib/ThirdParty/ImGui/**.cpp"
		enablepch "Off"

	filter {}

group "Applications"
project "EngineApp"
	kind "WindowedApp"
	applyCommonSettings()
	debugdir "EngineLib"

	includedirs { "EngineLib" }
	files {
		"EngineApp/**.h",
		"EngineApp/**.cpp",
	}
	links { "EngineLib" }
	linkEngineDependencies()

group "Tests"
project "UnitTest"
	kind "ConsoleApp"
	applyCommonSettings()

	includedirs {
		"EngineLib",
		GoogleTest .. "/build/native/include",
	}
	files {
		"UnitTest/**.h",
		"UnitTest/**.cpp",
	}
	pchheader "pch.h"
	pchsource "UnitTest/pch.cpp"
	links { "EngineLib" }
	linkEngineDependencies()

	filter "configurations:Debug"
		libdirs { GoogleTest .. "/lib/native/v140/windesktop/msvcstl/static/rt-dyn/x64/Debug" }
		links { "gtestd", "gtest_maind" }

	filter "configurations:Release"
		libdirs { GoogleTest .. "/lib/native/v140/windesktop/msvcstl/static/rt-dyn/x64/Release" }
		links { "gtest", "gtest_main" }

	filter "configurations:ObjViewerDebug"
		libdirs { GoogleTest .. "/lib/native/v140/windesktop/msvcstl/static/rt-dyn/x64/Debug" }
		links { "gtestd", "gtest_maind" }

	filter "configurations:ObjViewerRelease"
		libdirs { GoogleTest .. "/lib/native/v140/windesktop/msvcstl/static/rt-dyn/x64/Release" }
		links { "gtest", "gtest_main" }

	filter {}
