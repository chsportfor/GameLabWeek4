"""Build and run only the name system and its serialization/map smoke checks."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / 'Tests/bin/NameSmoke'
build.mkdir(parents=True, exist_ok=True)
sources = ['Tests/NameSmoke.cpp', 'EngineLib/Core/Core.cpp', 'EngineLib/Core/Name.cpp']
items = ''.join(f'<ClCompile Include="{root / name}" />' for name in sources)
project = build / 'NameSmoke.vcxproj'
project.write_text(f'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v145</PlatformToolset><UseDebugLibraries>true</UseDebugLibraries></PropertyGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)</OutDir><IntDir>$(ProjectDir)obj/</IntDir></PropertyGroup>
<ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary><AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions><AdditionalIncludeDirectories>{root / 'EngineLib'}</AdditionalIncludeDirectories></ClCompile><Link><SubSystem>Console</SubSystem></Link></ItemDefinitionGroup>
<ItemGroup>{items}</ItemGroup><Import Project="$(VCTargetsPath)/Microsoft.Cpp.targets" /></Project>''', encoding='utf-8')
env = {k.upper(): v for k, v in os.environ.items()}
msbuild = r'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
subprocess.run([msbuild, str(project), '/p:Configuration=Debug', '/p:Platform=x64',
                '/v:minimal', '/nologo'], env=env, check=True)
subprocess.run([str(build / 'NameSmoke.exe')], cwd=root, env=env, check=True)
