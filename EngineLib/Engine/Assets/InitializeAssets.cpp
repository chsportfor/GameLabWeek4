#include "InitializeAssets.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Core/AssetSystem/AssetSource/FileAssetSource.h"
#include "Core/AssetSystem/AssetSource/StaticMeshAssetSource.h"
#include "Core/AssetSystem/AssetSource/FontAtlasAssetSource.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Rendering/Renderer.h"
#include "Rendering/Primitives/Cube.h"
#include "Rendering/Primitives/Sphere.h"
#include "Rendering/Primitives/GizmoArrow.h"
#include "Rendering/Primitives/Circle.h"
#include "Rendering/Primitives/Triangle.h"
#include "Rendering/Primitives/Primitives.h"
#include "Engine/Assets/ObjImporter.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace
{
	constexpr std::string_view ImportedStaticMeshDirectory = "StaticMeshes";
	constexpr std::string_view ImportManifestFileName = ".objimport";

	std::string ToLower(std::string value)
	{
		for (char& character : value)
		{
			character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
		}
		return value;
	}

	std::string SanitizeFileName(std::string_view value)
	{
		std::string result;
		result.reserve(value.size());
		for (const unsigned char character : value)
		{
			if (std::isalnum(character) || character == '-' || character == '_') result.push_back(character);
			else result.push_back('_');
		}
		if (result.empty()) result = "StaticMesh";
		return result;
	}

	FName RegisterImportedStaticMesh(const std::filesystem::path& ProjectObjPath,
		UAssetManager& Assets, URenderer& Renderer, FFileManager& Files)
	{
		const std::filesystem::path assetsDirectory = Files.GetFileDirectoryPath();
		const std::filesystem::path relativePath = std::filesystem::relative(ProjectObjPath, assetsDirectory);
		const FName assetName = UAssetManager::MakeFileAssetName(ProjectObjPath, Files);
		Assets.RegisterAsset(assetName, MakeShared<FStaticMeshAssetLoader_File>(Renderer, Assets),
			MakeShared<FFileAssetSource>(Files, relativePath));
		return assetName;
	}

	void RegisterImportedStaticMeshes(UAssetManager& Assets, URenderer& Renderer, FFileManager& Files)
	{
		const std::filesystem::path importDirectory = Files.GetFileDirectoryPath() / ImportedStaticMeshDirectory;
		std::filesystem::create_directories(importDirectory);
		for (const std::filesystem::directory_entry& entry
			: std::filesystem::recursive_directory_iterator(importDirectory))
		{
			if (!entry.is_regular_file()) continue;
            const auto extension = ToLower(entry.path().extension().string());
            if (extension == ".obj") RegisterImportedStaticMesh(entry.path(), Assets, Renderer, Files);
            else if (extension == ".mtl") RegisterMaterialLibrary(entry.path(), Assets, Renderer.GetDevice(), Files);
		}
	}

	std::filesystem::path FindExistingImport(const std::filesystem::path& ImportDirectory,
		const std::filesystem::path& CanonicalSourcePath)
	{
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(ImportDirectory))
		{
			if (!entry.is_directory()) continue;
			const std::filesystem::path manifestPath = entry.path() / ImportManifestFileName;
			std::ifstream manifest(manifestPath, std::ios::in | std::ios::binary);
			std::string recordedSource;
			if (!manifest.is_open() || !std::getline(manifest, recordedSource)
				|| recordedSource != CanonicalSourcePath.generic_string()) continue;

			for (const std::filesystem::directory_entry& importedEntry
				: std::filesystem::directory_iterator(entry.path()))
			{
				if (importedEntry.is_regular_file()
					&& ToLower(importedEntry.path().extension().string()) == ".obj")
				{
					return importedEntry.path();
				}
			}
		}
		return {};
	}

	std::filesystem::path MakeUniqueImportDirectory(const std::filesystem::path& ImportDirectory,
		std::string_view BaseName)
	{
		std::filesystem::path candidate = ImportDirectory / BaseName;
		for (uint32 suffix = 1; std::filesystem::exists(candidate); ++suffix)
		{
			candidate = ImportDirectory / (std::string(BaseName) + "_" + std::to_string(suffix));
		}
		return candidate;
	}

	std::string RewriteMaterialLibrary(const FString& ObjText, std::string_view ImportedMtlFileName)
	{
		std::istringstream input(std::string(static_cast<std::string_view>(ObjText)));
		std::ostringstream output;
		output << "mtllib " << ImportedMtlFileName << '\n';
		std::string line;
		while (std::getline(input, line))
		{
			std::istringstream lineStream(line);
			std::string keyword;
			if ((lineStream >> keyword) && keyword == "mtllib") continue;
			output << line << '\n';
		}
		return output.str();
	}

	std::string BuildImportedMaterialLibrary(const FStaticMesh& Mesh,
		const std::filesystem::path& DestinationDirectory)
	{
		const std::filesystem::path texturesDirectory = DestinationDirectory / "Textures";
		std::unordered_map<std::string, std::string> copiedTextures;
		std::unordered_map<std::string, uint32> textureNameCounts;

		auto CopyTexture = [&](const FString& sourcePath, const FString& materialName,
			std::string_view slotName) -> std::string
		{
			if (sourcePath.Len() == 0) return {};
			const std::filesystem::path canonicalSource = std::filesystem::weakly_canonical(
				std::filesystem::path(std::string(static_cast<std::string_view>(sourcePath))));
			if (!std::filesystem::is_regular_file(canonicalSource))
			{
				throw std::runtime_error("Missing imported material texture: " + canonicalSource.string());
			}

			const std::string sourceKey = canonicalSource.generic_string();
			if (const auto found = copiedTextures.find(sourceKey); found != copiedTextures.end())
			{
				return found->second;
			}

			std::filesystem::create_directories(texturesDirectory);
			const std::string materialBase = SanitizeFileName(static_cast<std::string_view>(materialName));
			const std::string baseName = materialBase + "_" + std::string(slotName);
			uint32& nameCount = textureNameCounts[baseName];
			std::string fileName = baseName;
			if (nameCount > 0) fileName += "_" + std::to_string(nameCount);
			++nameCount;
			fileName += canonicalSource.extension().string();

			const std::filesystem::path destination = texturesDirectory / fileName;
			std::filesystem::copy_file(canonicalSource, destination);
			const std::string importedPath = (std::filesystem::path("Textures") / fileName).generic_string();
			copiedTextures.emplace(sourceKey, importedPath);
			return importedPath;
		};

		std::ostringstream output;
		for (const FStaticMaterial& material : Mesh.Materials)
		{
			output << "newmtl " << material.Name.CStr() << '\n';
			output << "Ka " << material.AmbientColor.x << ' ' << material.AmbientColor.y << ' ' << material.AmbientColor.z << '\n';
			output << "Kd " << material.DiffuseColor.x << ' ' << material.DiffuseColor.y << ' ' << material.DiffuseColor.z << '\n';
			output << "Ks " << material.SpecularColor.x << ' ' << material.SpecularColor.y << ' ' << material.SpecularColor.z << '\n';
			output << "Ke " << material.EmissiveColor.x << ' ' << material.EmissiveColor.y << ' ' << material.EmissiveColor.z << '\n';
			output << "Tf " << material.TransmissionFilter.x << ' ' << material.TransmissionFilter.y << ' ' << material.TransmissionFilter.z << '\n';
			output << "Ns " << material.SpecularExponent << '\n';
			output << "Ni " << material.OpticalDensity << '\n';
			if (material.bHasDissolve) output << "d " << material.Dissolve << '\n';
			if (material.bHasTransparency) output << "Tr " << material.Transparency << '\n';
			output << "illum " << material.IlluminationModel << '\n';

			auto WriteMap = [&](std::string_view keyword, const FString& sourcePath, std::string_view slotName)
			{
				const std::string importedPath = CopyTexture(sourcePath, material.Name, slotName);
				if (!importedPath.empty()) output << keyword << ' ' << importedPath << '\n';
			};
			WriteMap("map_Ka", material.AmbientTexturePath, "Ambient");
			WriteMap("map_Kd", material.DiffuseTexturePath, "Diffuse");
			WriteMap("map_Ks", material.SpecularTexturePath, "Specular");
			WriteMap("map_Ns", material.SpecularExponentTexturePath, "SpecularExponent");
			WriteMap("map_Ke", material.EmissiveTexturePath, "Emissive");
			WriteMap("map_d", material.OpacityTexturePath, "Opacity");
			WriteMap("map_Bump", material.NormalTexturePath, "Normal");
			WriteMap("disp", material.DisplacementTexturePath, "Displacement");
			WriteMap("decal", material.DecalTexturePath, "Decal");
			WriteMap("refl", material.ReflectionTexturePath, "Reflection");
			output << '\n';
		}
		return output.str();
	}
}

void RegisterLoadingScreenAssets(UAssetManager& Assets, URenderer& Renderer, FFileManager& Files)
{
    Assets.RegisterAsset(BuiltinAssetNames::LoadingScreen, MakeShared<FTexture2DAssetLoader>(Renderer.GetDevice()),
        MakeShared<FFileAssetSource>(Files, "Textures/LoadingScreen.dds"));
    Assets.RegisterAsset(BuiltinAssetNames::FullscreenMesh, MakeShared<FStaticMeshAssetLoader_Primitive>(Renderer, Assets),
        MakeShared<FStaticMeshAssetSource>(Fullscreen_vertices, Fullscreen_indices));
}

void RegisterSceneAssets(UAssetManager& AssetManager, URenderer& Renderer, FFileManager& Files)
{
    auto MeshLoader = MakeShared<FStaticMeshAssetLoader_Primitive>(Renderer, AssetManager);
    auto RegisterMesh = [&](EPrimitive Type, const auto& Vertices, const auto& Indices)
    {
        AssetManager.RegisterAsset(BuiltinAssetNames::Mesh(Type), MeshLoader,
            MakeShared<FStaticMeshAssetSource>(Vertices, Indices));
    };
    RegisterMesh(EPrimitive::EP_Cube, Cube_vertices, Cube_indices);
    RegisterMesh(EPrimitive::EP_Sphere, Sphere_vertices, Sphere_indices);
    RegisterMesh(EPrimitive::EP_GizmoArrow, GizmoArrow_vertices, GizmoArrow_indices);
    RegisterMesh(EPrimitive::EP_Circle, Circle_vertices, Circle_indices);
    RegisterMesh(EPrimitive::EP_Triangle, Triangle_vertices, Triangle_indices);
    RegisterMesh(EPrimitive::EP_BillboardQuad, Quad_vertices, Quad_indices);

    auto TextureLoader = MakeShared<FTexture2DAssetLoader>(Renderer.GetDevice());
    AssetManager.RegisterAsset(BuiltinAssetNames::Texture(EPrimitive::EP_Cube), TextureLoader,
        MakeShared<FFileAssetSource>(Files, "Textures/CubeTextureSample.dds"));
    AssetManager.RegisterAsset(BuiltinAssetNames::Texture(EPrimitive::EP_Sphere), TextureLoader,
        MakeShared<FFileAssetSource>(Files, "Textures/EarthTexture.dds"));
    AssetManager.RegisterAsset(BuiltinAssetNames::Texture(EPrimitive::EP_BillboardQuad), TextureLoader,
        MakeShared<FFileAssetSource>(Files, "Textures/Explosion_Alpha.dds"));
    AssetManager.RegisterAsset(BuiltinAssetNames::DefaultFont, MakeShared<FFontAtlasAssetLoader>(Renderer.GetDevice()),
        MakeShared<FFontAtlasAssetSource>(Files, "Fonts/KoreanFullAtlas.png", "Fonts/KoreanFullAtlas.json"));

	RegisterImportedStaticMeshes(AssetManager, Renderer, Files);
}

FName ImportStaticMeshObjAsset(const std::filesystem::path& SourcePath,
	UAssetManager& Assets, URenderer& Renderer, FFileManager& Files)
{
	const std::filesystem::path canonicalSource = std::filesystem::weakly_canonical(SourcePath);
	if (!std::filesystem::is_regular_file(canonicalSource)
		|| ToLower(canonicalSource.extension().string()) != ".obj")
	{
		throw std::invalid_argument("Static mesh import requires an existing OBJ file.");
	}

	const std::filesystem::path importDirectory = Files.GetFileDirectoryPath() / ImportedStaticMeshDirectory;
	std::filesystem::create_directories(importDirectory);
	if (const std::filesystem::path existingImport = FindExistingImport(importDirectory, canonicalSource);
		!existingImport.empty())
	{
		return RegisterImportedStaticMesh(existingImport, Assets, Renderer, Files);
	}

	FStaticMesh parsedMesh;
	FString parseError;
	if (!FObjImporter::LoadFromFile(canonicalSource.string(), Files, parsedMesh, parseError))
	{
		throw std::runtime_error(parseError.CStr());
	}

	const std::string baseName = SanitizeFileName(canonicalSource.stem().string());
	const std::filesystem::path destinationDirectory = MakeUniqueImportDirectory(importDirectory, baseName);
	std::filesystem::create_directories(destinationDirectory);
	try
	{
		const std::string objFileName = baseName + ".obj";
		const std::string mtlFileName = baseName + ".mtl";
		const FString sourceObjText = Files.ReadFileToString(canonicalSource);
		Files.WriteStringToFile(destinationDirectory / objFileName,
			RewriteMaterialLibrary(sourceObjText, mtlFileName));
		Files.WriteStringToFile(destinationDirectory / mtlFileName,
			BuildImportedMaterialLibrary(parsedMesh, destinationDirectory));
		Files.WriteStringToFile(destinationDirectory / ImportManifestFileName,
			canonicalSource.generic_string());

		return RegisterImportedStaticMesh(destinationDirectory / objFileName, Assets, Renderer, Files);
	}
	catch (...)
	{
		std::filesystem::remove_all(destinationDirectory);
		throw;
	}
}

FName RegisterObjFileAsset(const std::filesystem::path& Path,
    UAssetManager& Assets, URenderer& Renderer, FFileManager& Files)
{
    return RegisterImportedStaticMesh(Files.ResolvePath(Path), Assets, Renderer, Files);
}
