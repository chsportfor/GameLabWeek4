"""Build/run a hidden-window docking check without modifying the editor ini file."""
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
build = root / 'Tools/bin/DockingCheck'
build.mkdir(parents=True, exist_ok=True)
project = build / 'DockingCheck.vcxproj'
project.write_text(f'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v145</PlatformToolset><UseDebugLibraries>true</UseDebugLibraries></PropertyGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)</OutDir><IntDir>$(ProjectDir)obj/</IntDir></PropertyGroup>
<ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><RuntimeTypeInfo>false</RuntimeTypeInfo><RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary><AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions><AdditionalIncludeDirectories>{root / 'EngineLib'}</AdditionalIncludeDirectories></ClCompile>
<Link><SubSystem>Console</SubSystem><AdditionalDependencies>{root / 'bin/Debug/EngineLib/EngineLib.lib'};DirectXTK.lib;DirectXTex.lib;d3d11.lib;d3dcompiler.lib;dxgi.lib;dxguid.lib;windowscodecs.lib;ole32.lib;user32.lib;comdlg32.lib;%(AdditionalDependencies)</AdditionalDependencies></Link></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="{root / 'Tools/DockingCheck.cpp'}" /></ItemGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.targets" /></Project>''', encoding='utf-8')
env = {k.upper(): v for k, v in os.environ.items()}
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
installation = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
    'Microsoft.Component.MSBuild', '-property', 'installationPath'], text=True).strip()
msbuild = Path(installation) / 'MSBuild/Current/Bin/MSBuild.exe'
subprocess.run([str(root / 'premake5.exe'), 'vs2026'], cwd=root, env=env, check=True)
for p in [root / 'build/EngineLib.vcxproj', project]:
    subprocess.run([str(msbuild), str(p), '/p:Configuration=Debug', '/p:Platform=x64',
        '/m', '/v:minimal', '/clp:ErrorsOnly', '/nologo'], cwd=root, env=env, check=True)
subprocess.run([str(build / 'DockingCheck.exe'), *sys.argv[1:]], cwd=root / 'EngineLib', env=env, check=True, timeout=60)
