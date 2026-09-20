"""Incrementally build EngineLib, then run the actor naming checks without a renderer."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / 'Tests/bin/ActorNameSmoke'
build.mkdir(parents=True, exist_ok=True)
directxtk = root / 'packages/directxtk_desktop_2019.2025.10.28.2'
project = build / 'ActorNameSmoke.vcxproj'
project.write_text(f'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v145</PlatformToolset><UseDebugLibraries>true</UseDebugLibraries></PropertyGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)</OutDir><IntDir>$(ProjectDir)obj/</IntDir></PropertyGroup>
<ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><RuntimeTypeInfo>false</RuntimeTypeInfo><RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary><AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions><AdditionalIncludeDirectories>{root / 'EngineLib'};{directxtk / 'include'}</AdditionalIncludeDirectories></ClCompile>
<Link><SubSystem>Console</SubSystem><AdditionalDependencies>{root / 'bin/Debug/EngineLib/EngineLib.lib'};DirectXTK.lib;d3d11.lib;d3dcompiler.lib;dxgi.lib;dxguid.lib;windowscodecs.lib;ole32.lib;user32.lib;comdlg32.lib;%(AdditionalDependencies)</AdditionalDependencies><AdditionalLibraryDirectories>{directxtk / 'native/lib/x64/Debug'}</AdditionalLibraryDirectories></Link></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="{root / 'Tests/ActorNameSmoke.cpp'}" /></ItemGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.targets" /></Project>''', encoding='utf-8')

env = {key.upper(): value for key, value in os.environ.items()}
msbuild = r'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
for target in [root / 'build/EngineLib.vcxproj', project]:
    subprocess.run([msbuild, str(target), '/p:Configuration=Debug', '/p:Platform=x64',
                    '/m', '/v:minimal', '/clp:ErrorsOnly', '/nologo'], env=env, check=True)
subprocess.run([str(build / 'ActorNameSmoke.exe')], cwd=root, env=env, check=True, timeout=10)
