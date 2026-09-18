"""Isolated test for the texture asset while the rest of the renderer is being ported."""
import os
import sys
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / 'Tests/bin-texture'
build.mkdir(exist_ok=True)
sources = ['Tests/TextureAssetSmoke.cpp', 'EngineLib/Core/AssetSystem/Asset.cpp',
           'EngineLib/Core/AssetSystem/Asset/Texture2DAsset.cpp', 'EngineLib/Core/Core.cpp',
           'EngineLib/Core/Name.cpp', 'EngineLib/Core/Object/Object.cpp',
           'EngineLib/Core/IO/FileManager.cpp', 'EngineLib/Engine/EngineStatics.cpp']
target = 'FontAtlasAssetSmoke' if '--font' in sys.argv else 'TextureAssetSmoke'
if '--font' in sys.argv:
    sources[0] = 'Tests/FontAtlasAssetSmoke.cpp'
    sources += ['EngineLib/Core/AssetSystem/Asset/FontAtlasAsset.cpp', 'EngineLib/Rendering/FontResource.cpp']
items = ''.join(f'<ClCompile Include="{root / name}" />' for name in sources)
project = build / f'{target}.vcxproj'
project.write_text(f'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v145</PlatformToolset><UseDebugLibraries>true</UseDebugLibraries></PropertyGroup>
<Import Project="$(VCTargetsPath)/Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)</OutDir><IntDir>$(ProjectDir)obj/{target}/</IntDir></PropertyGroup>
<ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><RuntimeTypeInfo>false</RuntimeTypeInfo><RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary><AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions><AdditionalIncludeDirectories>{root / 'EngineLib'};C:/vcpkg/installed/x64-windows/include</AdditionalIncludeDirectories></ClCompile>
<Link><SubSystem>Console</SubSystem><AdditionalDependencies>DirectXTK.lib;d3d11.lib;dxgi.lib;dxguid.lib;windowscodecs.lib;ole32.lib;user32.lib;%(AdditionalDependencies)</AdditionalDependencies><AdditionalLibraryDirectories>C:/vcpkg/installed/x64-windows/debug/lib</AdditionalLibraryDirectories></Link></ItemDefinitionGroup>
<ItemGroup>{items}</ItemGroup><Import Project="$(VCTargetsPath)/Microsoft.Cpp.targets" /></Project>''', encoding='utf-8')
env = {k.upper(): v for k, v in os.environ.items()}
msbuild = r'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
subprocess.run([msbuild, str(project), '/p:Configuration=Debug', '/p:Platform=x64', '/v:minimal', '/nologo'], env=env, check=True)
subprocess.run([str(build/f'{target}.exe')], cwd=root, env=env, check=True)
