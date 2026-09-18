"""Build Debug|x64 first. Default: focused collector/quad smoke; --full: all rendering tests."""
import os
import sys
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / 'Tests/bin-renderer-merge'
build.mkdir(exist_ok=True)
project = build / 'RendererMergeSmoke.vcxproj'
project.write_text(f'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v145</PlatformToolset><UseDebugLibraries>true</UseDebugLibraries></PropertyGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)</OutDir><IntDir>$(ProjectDir)obj/</IntDir></PropertyGroup>
<ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><RuntimeTypeInfo>false</RuntimeTypeInfo><RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary><AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions><AdditionalIncludeDirectories>{root / 'EngineLib'};C:/vcpkg/installed/x64-windows/include</AdditionalIncludeDirectories></ClCompile>
<Link><SubSystem>Console</SubSystem><AdditionalDependencies>{root / 'x64/Debug/EngineLib.lib'};DirectXTK.lib;d3d11.lib;d3dcompiler.lib;dxgi.lib;dxguid.lib;windowscodecs.lib;ole32.lib;user32.lib;comdlg32.lib;%(AdditionalDependencies)</AdditionalDependencies><AdditionalLibraryDirectories>C:/vcpkg/installed/x64-windows/debug/lib</AdditionalLibraryDirectories></Link></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="{root / 'Tests/RendererMergeSmoke.cpp'}" /></ItemGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.targets" /></Project>''', encoding='utf-8')

env = {key.upper(): value for key, value in os.environ.items()}
msbuild = r'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
subprocess.run([msbuild, str(project), '/p:Configuration=Debug', '/p:Platform=x64', '/v:minimal', '/nologo'], env=env, check=True)
subprocess.run([str(build / 'RendererMergeSmoke.exe'), *sys.argv[1:]], cwd=root / 'EngineLib', env=env, check=True, timeout=60)
