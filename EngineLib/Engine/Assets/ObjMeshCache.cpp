// PODOMSH version 2 cache reader extracted from origin/Develops ObjImporter.cpp.
// Keep field order compatible with the colleague's SaveMeshCache writer.
#include "ObjImporter.h"
#include "Platform/WindowsBinArchive.h"
#include <array>
#include <limits>
#include <stdexcept>

namespace
{
	constexpr std::array<char, 8> MeshCacheMagic{ 'P', 'O', 'D', 'O', 'M', 'S', 'H', '\0' };
	constexpr uint32 MeshCacheVersion = 2;
	constexpr uint32 MaxCacheDependencies = 1024;
	constexpr uint32 MaxCacheStrings = 1024 * 1024;
	constexpr uint32 MaxCacheElements = 100 * 1024 * 1024;

	std::filesystem::path Utf8ToPath(const FString& path)
	{
		return std::filesystem::path(Utf2Wide(path));
	}

	struct FMeshCacheDependency
	{
		std::filesystem::path Path;
		uint64 FileSize = 0;
		int64 WriteTime = 0;
	};

	bool SerializeString(FArchive& archive, FString& value)
	{
		uint32 length = archive.IsSaving() ? static_cast<uint32>(value.Len()) : 0;
		archive << length;
		if (archive.IsError() || length > MaxCacheStrings || !archive.CanSerialize(length)) return false;

		if (archive.IsLoading())
		{
			std::string text(length, '\0');
			if (length > 0) archive.Serialize(text.data(), length);
			if (archive.IsError()) return false;
			value = std::string_view(text);
		}
		else if (length > 0)
		{
			archive.Serialize(value.CStr(), length);
		}
		return !archive.IsError();
	}

	bool SerializeCount(FArchive& archive, uint32& count, uint32 maximum,
		uint64 minimumElementSize)
	{
		archive << count;
		return !archive.IsError() && count <= maximum
			&& count <= static_cast<uint32>((std::numeric_limits<int32>::max)())
			&& archive.CanSerialize(static_cast<uint64>(count) * minimumElementSize);
	}

	bool SerializeVector2(FArchive& archive, FVector2& value)
	{
		archive << value.x << value.y;
		return !archive.IsError();
	}

	bool SerializeVector3(FArchive& archive, FVector& value)
	{
		archive << value.x << value.y << value.z;
		return !archive.IsError();
	}

	bool SerializeVector4(FArchive& archive, FVector4& value)
	{
		archive << value.x << value.y << value.z << value.w;
		return !archive.IsError();
	}

	bool QueryDependency(const std::filesystem::path& path, FMeshCacheDependency& dependency)
	{
		std::error_code error;
		const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, error);
		if (error || !std::filesystem::is_regular_file(canonicalPath, error) || error) return false;
		const uintmax_t fileSize = std::filesystem::file_size(canonicalPath, error);
		if (error || fileSize > (std::numeric_limits<uint64>::max)()) return false;
		const auto writeTime = std::filesystem::last_write_time(canonicalPath, error);
		if (error) return false;

		dependency.Path = canonicalPath;
		dependency.FileSize = static_cast<uint64>(fileSize);
		dependency.WriteTime = static_cast<int64>(writeTime.time_since_epoch().count());
		return true;
	}

	bool SerializeMaterial(FArchive& archive, FStaticMaterial& material)
	{
		uint8 hasDissolve = archive.IsSaving() && material.bHasDissolve ? 1 : 0;
		uint8 hasTransparency = archive.IsSaving() && material.bHasTransparency ? 1 : 0;
		if (!SerializeString(archive, material.MaterialLibraryPath)
			|| !SerializeString(archive, material.Name)
			|| !SerializeVector4(archive, material.AmbientColor)
			|| !SerializeVector4(archive, material.DiffuseColor)
			|| !SerializeVector4(archive, material.SpecularColor)
			|| !SerializeVector4(archive, material.EmissiveColor)
			|| !SerializeVector4(archive, material.TransmissionFilter)) return false;
		archive << material.SpecularExponent << material.OpticalDensity
			<< material.Dissolve << material.Transparency << material.IlluminationModel
			<< hasDissolve << hasTransparency;
		if (archive.IsError() || hasDissolve > 1 || hasTransparency > 1
			|| !SerializeString(archive, material.AmbientTexturePath)
			|| !SerializeString(archive, material.DiffuseTexturePath)
			|| !SerializeString(archive, material.SpecularTexturePath)
			|| !SerializeString(archive, material.SpecularExponentTexturePath)
			|| !SerializeString(archive, material.EmissiveTexturePath)
			|| !SerializeString(archive, material.OpacityTexturePath)
			|| !SerializeString(archive, material.NormalTexturePath)
			|| !SerializeString(archive, material.DisplacementTexturePath)
			|| !SerializeString(archive, material.DecalTexturePath)
			|| !SerializeString(archive, material.ReflectionTexturePath))
		{
			return false;
		}
		if (archive.IsLoading())
		{
			material.bHasDissolve = hasDissolve != 0;
			material.bHasTransparency = hasTransparency != 0;
		}
		return true;
	}

	bool ValidateMesh(const FStaticMesh& mesh)
	{
		const uint32 vertexCount = static_cast<uint32>(mesh.Vertices.Num());
		const uint32 indexCount = static_cast<uint32>(mesh.Indices.Num());
		const uint32 materialCount = static_cast<uint32>(mesh.Materials.Num());
		if (vertexCount == 0 || indexCount == 0 || indexCount % 3 != 0
			|| mesh.TriangleSmoothingGroups.Num() != static_cast<int32>(indexCount / 3)) return false;
		for (uint32 index : mesh.Indices) if (index >= vertexCount) return false;
		for (const FStaticMeshSection& section : mesh.Sections)
		{
			if (section.MaterialIndex >= materialCount || section.FirstIndex > indexCount
				|| section.NumIndices > indexCount - section.FirstIndex || section.NumIndices % 3 != 0) return false;
		}
		for (const FStaticMeshPart& part : mesh.Parts)
		{
			for (const FStaticMeshIndexRange& range : part.IndexRanges)
			{
				if (range.MaterialIndex >= materialCount || range.FirstIndex > indexCount
					|| range.NumIndices > indexCount - range.FirstIndex || range.NumIndices % 3 != 0) return false;
			}
		}
		return true;
	}

	bool SerializeMesh(FArchive& archive, FStaticMesh& mesh)
	{
		if (!SerializeString(archive, mesh.PathFileName)) return false;

		uint32 count = archive.IsSaving() ? static_cast<uint32>(mesh.Vertices.Num()) : 0;
		if (!SerializeCount(archive, count, MaxCacheElements, 48)) return false;
		if (archive.IsLoading()) mesh.Vertices.SetNum(static_cast<int32>(count), false);
		for (FVertexPNCT& vertex : mesh.Vertices)
		{
			if (!SerializeVector3(archive, vertex.Position) || !SerializeVector3(archive, vertex.Normal)
				|| !SerializeVector4(archive, vertex.Color) || !SerializeVector2(archive, vertex.UV)) return false;
		}

		count = archive.IsSaving() ? static_cast<uint32>(mesh.Indices.Num()) : 0;
		if (!SerializeCount(archive, count, MaxCacheElements, sizeof(uint32))) return false;
		if (archive.IsLoading()) mesh.Indices.SetNum(static_cast<int32>(count), false);
		for (uint32& index : mesh.Indices) archive << index;

		count = archive.IsSaving() ? static_cast<uint32>(mesh.Sections.Num()) : 0;
		if (!SerializeCount(archive, count, MaxCacheElements, 16)) return false;
		if (archive.IsLoading()) mesh.Sections.SetNum(static_cast<int32>(count), false);
		for (FStaticMeshSection& section : mesh.Sections)
		{
			if (!SerializeString(archive, section.MaterialName)) return false;
			archive << section.MaterialIndex << section.FirstIndex << section.NumIndices;
		}

		count = archive.IsSaving() ? static_cast<uint32>(mesh.Parts.Num()) : 0;
		if (!SerializeCount(archive, count, MaxCacheElements, 12)) return false;
		if (archive.IsLoading()) mesh.Parts.SetNum(static_cast<int32>(count), false);
		for (FStaticMeshPart& part : mesh.Parts)
		{
			if (!SerializeString(archive, part.ObjectName)) return false;
			uint32 groupCount = archive.IsSaving() ? static_cast<uint32>(part.GroupNames.Num()) : 0;
			if (!SerializeCount(archive, groupCount, MaxCacheElements, sizeof(uint32))) return false;
			if (archive.IsLoading()) part.GroupNames.SetNum(static_cast<int32>(groupCount), false);
			for (FString& groupName : part.GroupNames)
				if (!SerializeString(archive, groupName)) return false;

			uint32 rangeCount = archive.IsSaving() ? static_cast<uint32>(part.IndexRanges.Num()) : 0;
			if (!SerializeCount(archive, rangeCount, MaxCacheElements, 12)) return false;
			if (archive.IsLoading()) part.IndexRanges.SetNum(static_cast<int32>(rangeCount), false);
			for (FStaticMeshIndexRange& range : part.IndexRanges)
				archive << range.MaterialIndex << range.FirstIndex << range.NumIndices;
		}

		count = archive.IsSaving() ? static_cast<uint32>(mesh.TriangleSmoothingGroups.Num()) : 0;
		if (!SerializeCount(archive, count, MaxCacheElements, sizeof(int32))) return false;
		if (archive.IsLoading()) mesh.TriangleSmoothingGroups.SetNum(static_cast<int32>(count), false);
		for (int32& smoothingGroup : mesh.TriangleSmoothingGroups) archive << smoothingGroup;

		count = archive.IsSaving() ? static_cast<uint32>(mesh.Materials.Num()) : 0;
		if (!SerializeCount(archive, count, MaxCacheElements, 146)) return false;
		if (archive.IsLoading()) mesh.Materials.SetNum(static_cast<int32>(count), false);
		for (FStaticMaterial& material : mesh.Materials)
			if (!SerializeMaterial(archive, material)) return false;

		return !archive.IsError() && ValidateMesh(mesh);
	}

	bool TryLoadMeshCache(const std::filesystem::path& cachePath, FStaticMesh& mesh)
	{
		FWindowsBinReader archive(cachePath);
		std::array<char, MeshCacheMagic.size()> magic{};
		uint32 version = 0;
		archive.Serialize(magic.data(), magic.size());
		archive << version;
		if (archive.IsError() || magic != MeshCacheMagic || version != MeshCacheVersion) return false;

		uint32 dependencyCount = 0;
		if (!SerializeCount(archive, dependencyCount, MaxCacheDependencies, 20)
			|| dependencyCount == 0) return false;
		for (uint32 dependencyIndex = 0; dependencyIndex < dependencyCount; ++dependencyIndex)
		{
			FString path;
			uint64 cachedSize = 0;
			int64 cachedWriteTime = 0;
			if (!SerializeString(archive, path)) return false;
			archive << cachedSize << cachedWriteTime;
			if (archive.IsError()) return false;

			FMeshCacheDependency current;
			if (!QueryDependency(Utf8ToPath(path), current)
				|| current.FileSize != cachedSize || current.WriteTime != cachedWriteTime) return false;
		}

		FStaticMesh loadedMesh;
		if (!SerializeMesh(archive, loadedMesh) || !archive.IsAtEnd()) return false;
		mesh = std::move(loadedMesh);
		return true;
	}

}

bool FObjImporter::LoadBinaryFromFile(const std::filesystem::path& Path, const FFileManager& Files,
    FStaticMesh& OutMesh, FString& OutError)
{
    try
    {
        // A .pmesh is a source cache, so retain its original source size/time validation.
        if (!TryLoadMeshCache(Files.ResolvePath(Path), OutMesh))
            throw std::runtime_error("Invalid, unsupported or stale .pmesh cache (source OBJ/MTL must still match)");
        OutError = FString();
        return true;
    }
    catch (const std::exception& Error)
    {
        OutError = FString(Error.what());
        return false;
    }
}
