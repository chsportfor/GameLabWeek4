#include "ObjImporter.h"

#include "Platform/WindowsBinArchive.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace
{
	constexpr std::array<char, 8> MeshCacheMagic{ 'P', 'O', 'D', 'O', 'M', 'S', 'H', '\0' };
	constexpr uint32 MeshCacheVersion = 1;
	constexpr uint32 MaxCacheDependencies = 1024;
	constexpr uint32 MaxCacheStrings = 1024 * 1024;
	constexpr uint32 MaxCacheElements = 100 * 1024 * 1024;

	FString PathToUtf8(const std::filesystem::path& path)
	{
		return Wide2Utf(path.wstring());
	}

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

	std::filesystem::path GetMeshCachePath(const std::filesystem::path& objPath)
	{
		std::filesystem::path cachePath = objPath;
		cachePath.replace_extension(".pmesh");
		return cachePath;
	}

	bool SerializeMaterial(FArchive& archive, FStaticMaterial& material)
	{
		uint8 hasDissolve = archive.IsSaving() && material.bHasDissolve ? 1 : 0;
		uint8 hasTransparency = archive.IsSaving() && material.bHasTransparency ? 1 : 0;
		if (!SerializeString(archive, material.Name)
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

	bool SaveMeshCache(const std::filesystem::path& cachePath,
		const TArray<FMeshCacheDependency>& dependencies, FStaticMesh& mesh)
	{
		std::filesystem::path temporaryPath = cachePath;
		temporaryPath += ".tmp";
		FWindowsBinWriter archive(temporaryPath);
		std::array<char, MeshCacheMagic.size()> magic = MeshCacheMagic;
		uint32 version = MeshCacheVersion;
		uint32 dependencyCount = static_cast<uint32>(dependencies.Num());
		archive.Serialize(magic.data(), magic.size());
		archive << version;
		bool succeeded = SerializeCount(archive, dependencyCount, MaxCacheDependencies, 20);
		for (const FMeshCacheDependency& dependency : dependencies)
		{
			FString path = PathToUtf8(dependency.Path);
			uint64 fileSize = dependency.FileSize;
			int64 writeTime = dependency.WriteTime;
			succeeded = succeeded && SerializeString(archive, path);
			archive << fileSize << writeTime;
			succeeded = succeeded && !archive.IsError();
		}
		if (succeeded) succeeded = SerializeMesh(archive, mesh);
		const bool closed = archive.Close();
		succeeded = succeeded && closed;
		if (!succeeded)
		{
			std::error_code ignored;
			std::filesystem::remove(temporaryPath, ignored);
			return false;
		}

		if (!MoveFileExW(temporaryPath.c_str(), cachePath.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			std::error_code ignored;
			std::filesystem::remove(temporaryPath, ignored);
			return false;
		}
		return true;
	}

	struct FVertexKey
	{
		int32 PositionIndex = -1;
		int32 UVIndex = -1;
		int32 NormalIndex = -1;
		int32 GeneratedNormalID = -1;

		bool operator==(const FVertexKey& other) const
		{
			return PositionIndex == other.PositionIndex
				&& UVIndex == other.UVIndex
				&& NormalIndex == other.NormalIndex
				&& GeneratedNormalID == other.GeneratedNormalID;
		}
	};

	struct FVertexKeyHasher
	{
		std::size_t operator()(const FVertexKey& key) const
		{
			return (static_cast<std::size_t>(key.PositionIndex) * 73856093u)
				^ (static_cast<std::size_t>(key.UVIndex + 1) * 19349663u)
				^ (static_cast<std::size_t>(key.NormalIndex + 1) * 83492791u)
				^ (static_cast<std::size_t>(key.GeneratedNormalID + 1) * 2654435761u);
		}
	};

	bool Fail(FString& outError, uint32 lineNumber, std::string_view message)
	{
		std::string error = "OBJ parse error on line " + std::to_string(lineNumber) + ": ";
		error += message;
		outError = std::string_view(error);
		return false;
	}

	// OBJ is conventionally Y-up. The engine is X-forward, Y-right, Z-up.
	FVector ConvertObjVector(const FVector& value)
	{
		return FVector(value.x, value.z, value.y);
	}

	bool ParseIndex(std::string_view token, int32 count, int32& outIndex)
	{
		if (token.empty()) return false;

		int32 value = 0;
		const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
		if (result.ec != std::errc() || result.ptr != token.data() + token.size() || value == 0)
		{
			return false;
		}

		outIndex = value > 0 ? value - 1 : count + value;
		return true;
	}

	bool ParseFaceVertex(std::string_view token, int32 positionCount, int32 uvCount, int32 normalCount,
		FObjVertexIndex& outIndex)
	{
		const size_t firstSlash = token.find('/');
		if (firstSlash == std::string_view::npos)
		{
			return ParseIndex(token, positionCount, outIndex.PositionIndex);
		}

		const size_t secondSlash = token.find('/', firstSlash + 1);
		if (secondSlash != std::string_view::npos && token.find('/', secondSlash + 1) != std::string_view::npos)
		{
			return false;
		}

		if (!ParseIndex(token.substr(0, firstSlash), positionCount, outIndex.PositionIndex))
		{
			return false;
		}

		if (secondSlash == std::string_view::npos)
		{
			return ParseIndex(token.substr(firstSlash + 1), uvCount, outIndex.UVIndex);
		}

		const std::string_view uvToken = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
		if (!uvToken.empty() && !ParseIndex(uvToken, uvCount, outIndex.UVIndex))
		{
			return false;
		}

		const std::string_view normalToken = token.substr(secondSlash + 1);
		return !normalToken.empty() && ParseIndex(normalToken, normalCount, outIndex.NormalIndex);
	}

	bool IsValidIndex(int32 index, int32 count)
	{
		return index >= 0 && index < count;
	}

	float Cross2D(const FVector2& first, const FVector2& second, const FVector2& third)
	{
		return (second.x - first.x) * (third.y - first.y)
			- (second.y - first.y) * (third.x - first.x);
	}

	bool IsPointOnSegment(const FVector2& point, const FVector2& start,
		const FVector2& end, float epsilon)
	{
		if (std::abs(Cross2D(start, end, point)) > epsilon) return false;
		return point.x >= (std::min)(start.x, end.x) - epsilon
			&& point.x <= (std::max)(start.x, end.x) + epsilon
			&& point.y >= (std::min)(start.y, end.y) - epsilon
			&& point.y <= (std::max)(start.y, end.y) + epsilon;
	}

	bool SegmentsIntersect(const FVector2& firstStart, const FVector2& firstEnd,
		const FVector2& secondStart, const FVector2& secondEnd, float epsilon)
	{
		const float firstSide = Cross2D(firstStart, firstEnd, secondStart);
		const float secondSide = Cross2D(firstStart, firstEnd, secondEnd);
		const float thirdSide = Cross2D(secondStart, secondEnd, firstStart);
		const float fourthSide = Cross2D(secondStart, secondEnd, firstEnd);

		if (((firstSide > epsilon && secondSide < -epsilon)
				|| (firstSide < -epsilon && secondSide > epsilon))
			&& ((thirdSide > epsilon && fourthSide < -epsilon)
				|| (thirdSide < -epsilon && fourthSide > epsilon))) return true;
		return IsPointOnSegment(secondStart, firstStart, firstEnd, epsilon)
			|| IsPointOnSegment(secondEnd, firstStart, firstEnd, epsilon)
			|| IsPointOnSegment(firstStart, secondStart, secondEnd, epsilon)
			|| IsPointOnSegment(firstEnd, secondStart, secondEnd, epsilon);
	}

	bool PointInTriangle(const FVector2& point, const FVector2& first,
		const FVector2& second, const FVector2& third, float winding, float epsilon)
	{
		return Cross2D(first, second, point) * winding >= -epsilon
			&& Cross2D(second, third, point) * winding >= -epsilon
			&& Cross2D(third, first, point) * winding >= -epsilon;
	}

	bool TriangulateFace(const FObjFace& face, const FObjInfo& rawMesh,
		std::vector<std::array<int32, 3>>& outTriangles, FString& outError)
	{
		outTriangles.clear();
		const int32 vertexCount = face.Vertices.Num();
		if (vertexCount < 3) return Fail(outError, face.LineNumber, "face requires at least three vertices.");

		FVector normal(0.0f);
		for (int32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
		{
			const int32 currentPositionIndex = face.Vertices[vertexIndex].PositionIndex;
			const int32 nextPositionIndex = face.Vertices[(vertexIndex + 1) % vertexCount].PositionIndex;
			if (!IsValidIndex(currentPositionIndex, rawMesh.Positions.Num())
				|| !IsValidIndex(nextPositionIndex, rawMesh.Positions.Num()))
			{
				return Fail(outError, face.LineNumber,
					"face position index is outside the available position list.");
			}
			const FVector& current = rawMesh.Positions[currentPositionIndex];
			const FVector& next = rawMesh.Positions[nextPositionIndex];
			normal.x += (current.y - next.y) * (current.z + next.z);
			normal.y += (current.z - next.z) * (current.x + next.x);
			normal.z += (current.x - next.x) * (current.y + next.y);
		}
		if (normal.IsNearlyZero())
			return Fail(outError, face.LineNumber, "face is degenerate and cannot be triangulated.");

		const float absX = std::abs(normal.x);
		const float absY = std::abs(normal.y);
		const float absZ = std::abs(normal.z);
		std::vector<FVector2> projectedVertices;
		projectedVertices.reserve(static_cast<size_t>(vertexCount));
		for (const FObjVertexIndex& vertex : face.Vertices)
		{
			const FVector& position = rawMesh.Positions[vertex.PositionIndex];
			if (absX >= absY && absX >= absZ) projectedVertices.emplace_back(position.y, position.z);
			else if (absY >= absZ) projectedVertices.emplace_back(position.x, position.z);
			else projectedVertices.emplace_back(position.x, position.y);
		}

		float minX = projectedVertices[0].x;
		float maxX = projectedVertices[0].x;
		float minY = projectedVertices[0].y;
		float maxY = projectedVertices[0].y;
		double signedArea = 0.0;
		for (int32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
		{
			const FVector2& current = projectedVertices[vertexIndex];
			const FVector2& next = projectedVertices[(vertexIndex + 1) % vertexCount];
			minX = (std::min)(minX, current.x);
			maxX = (std::max)(maxX, current.x);
			minY = (std::min)(minY, current.y);
			maxY = (std::max)(maxY, current.y);
			signedArea += static_cast<double>(current.x) * next.y
				- static_cast<double>(next.x) * current.y;
		}
		const float extent = (std::max)(maxX - minX, maxY - minY);
		const float epsilon = (std::max)(1.0e-7f, extent * extent * 1.0e-6f);
		if (std::abs(signedArea) <= static_cast<double>(epsilon))
			return Fail(outError, face.LineNumber, "face has zero projected area and cannot be triangulated.");

		for (int32 firstEdge = 0; firstEdge < vertexCount; ++firstEdge)
		{
			const int32 firstNext = (firstEdge + 1) % vertexCount;
			for (int32 secondEdge = firstEdge + 1; secondEdge < vertexCount; ++secondEdge)
			{
				const int32 secondNext = (secondEdge + 1) % vertexCount;
				if (firstEdge == secondEdge || firstNext == secondEdge
					|| secondNext == firstEdge) continue;
				if (SegmentsIntersect(projectedVertices[firstEdge], projectedVertices[firstNext],
					projectedVertices[secondEdge], projectedVertices[secondNext], epsilon))
				{
					return Fail(outError, face.LineNumber,
						"face is self-intersecting and cannot be triangulated.");
				}
			}
		}

		const float winding = signedArea > 0.0 ? 1.0f : -1.0f;
		std::vector<int32> remaining;
		remaining.reserve(static_cast<size_t>(vertexCount));
		for (int32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
			remaining.push_back(vertexIndex);

		while (remaining.size() > 3)
		{
			bool clippedEar = false;
			for (size_t remainingIndex = 0; remainingIndex < remaining.size(); ++remainingIndex)
			{
				const int32 previous = remaining[(remainingIndex + remaining.size() - 1) % remaining.size()];
				const int32 current = remaining[remainingIndex];
				const int32 next = remaining[(remainingIndex + 1) % remaining.size()];
				if (Cross2D(projectedVertices[previous], projectedVertices[current],
					projectedVertices[next]) * winding <= epsilon) continue;

				bool containsVertex = false;
				for (int32 candidate : remaining)
				{
					if (candidate == previous || candidate == current || candidate == next) continue;
					if (PointInTriangle(projectedVertices[candidate], projectedVertices[previous],
						projectedVertices[current], projectedVertices[next], winding, epsilon))
					{
						containsVertex = true;
						break;
					}
				}
				if (containsVertex) continue;

				outTriangles.push_back({ previous, current, next });
				remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(remainingIndex));
				clippedEar = true;
				break;
			}
			if (!clippedEar)
				return Fail(outError, face.LineNumber, "face cannot be triangulated by ear clipping.");
		}

		if (Cross2D(projectedVertices[remaining[0]], projectedVertices[remaining[1]],
			projectedVertices[remaining[2]]) * winding <= epsilon)
		{
			return Fail(outError, face.LineNumber, "face is degenerate and cannot be triangulated.");
		}
		outTriangles.push_back({ remaining[0], remaining[1], remaining[2] });
		return true;
	}

	bool FailMtl(FString& outError, const std::filesystem::path& path, uint32 lineNumber, std::string_view message)
	{
		std::string error = "MTL parse error in " + std::string(PathToUtf8(path))
			+ " on line " + std::to_string(lineNumber) + ": ";
		error += message;
		outError = std::string_view(error);
		return false;
	}

	FObjMaterial* FindObjMaterial(FObjInfo& rawMesh, std::string_view name)
	{
		for (FObjMaterial& material : rawMesh.Materials)
		{
			if (material.Name.Equals(name)) return &material;
		}
		return nullptr;
	}

	bool ParseMaterialColor(std::istringstream& lineStream, FVector4& outColor)
	{
		float red, green, blue;
		if (!(lineStream >> red >> green >> blue)) return false;
		outColor = FVector4(red, green, blue, 1.0f);
		return true;
	}

	bool ParseMaterialTexturePath(std::istringstream& lineStream, const std::filesystem::path& mtlPath,
		FString& outTexturePath)
	{
		std::vector<std::string> tokens;
		std::string token;
		while (lineStream >> std::quoted(token)) tokens.push_back(token);
		if (tokens.empty()) return false;

		auto IsNumber = [](std::string_view value)
		{
			float number = 0.0f;
			const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
			return result.ec == std::errc() && result.ptr == value.data() + value.size();
		};

		size_t pathIndex = 0;
		while (pathIndex < tokens.size() && tokens[pathIndex].starts_with('-'))
		{
			const std::string& option = tokens[pathIndex++];
			if (option == "-o" || option == "-s" || option == "-t")
			{
				int32 valueCount = 0;
				while (pathIndex < tokens.size() && valueCount < 3 && IsNumber(tokens[pathIndex]))
				{
					++pathIndex;
					++valueCount;
				}
				if (valueCount == 0) return false;
			}
			else
			{
				const size_t argumentCount = option == "-mm" ? 2 : 1;
				if (option != "-mm" && option != "-blendu" && option != "-blendv"
					&& option != "-boost" && option != "-texres" && option != "-clamp"
					&& option != "-bm" && option != "-imfchan" && option != "-type"
					&& option != "-cc" && option != "-colorspace")
				{
					return false;
				}
				if (pathIndex + argumentCount > tokens.size()) return false;
				pathIndex += argumentCount;
			}
		}
		if (pathIndex >= tokens.size()) return false;

		std::string texturePath = tokens[pathIndex++];
		while (pathIndex < tokens.size())
		{
			texturePath += ' ';
			texturePath += tokens[pathIndex++];
		}
		const std::filesystem::path resolvedPath =
			(mtlPath.parent_path() / Utf8ToPath(FString(texturePath))).lexically_normal();
		outTexturePath = PathToUtf8(resolvedPath);
		return true;
	}

	bool ParseMtl(std::string_view mtlText, const std::filesystem::path& mtlPath, FObjInfo& rawMesh, FString& outError)
	{
		std::istringstream input{ std::string(mtlText) };
		std::string line;
		uint32 lineNumber = 0;
		FObjMaterial* currentMaterial = nullptr;

		while (std::getline(input, line))
		{
			++lineNumber;
			if (const size_t commentStart = line.find('#'); commentStart != std::string::npos)
			{
				line.erase(commentStart);
			}

			std::istringstream lineStream(line);
			std::string keyword;
			if (!(lineStream >> keyword)) continue;

			if (keyword == "newmtl")
			{
				std::string materialName;
				if (!(lineStream >> materialName)) return FailMtl(outError, mtlPath, lineNumber, "newmtl requires a material name.");
				if (FindObjMaterial(rawMesh, materialName)) return FailMtl(outError, mtlPath, lineNumber, "material names must be unique.");

				FObjMaterial material;
				material.Name = std::string_view(materialName);
				material.MaterialLibraryPath = FString(mtlPath.generic_string());
				rawMesh.Materials.Add(material);
				currentMaterial = &rawMesh.Materials[rawMesh.Materials.Num() - 1];
			}
			else if (keyword == "Kd")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "Kd must follow newmtl.");
				if (!ParseMaterialColor(lineStream, currentMaterial->DiffuseColor))
					return FailMtl(outError, mtlPath, lineNumber, "Kd requires three numbers.");
			}
			else if (keyword == "Ka")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "Ka must follow newmtl.");
				if (!ParseMaterialColor(lineStream, currentMaterial->AmbientColor))
					return FailMtl(outError, mtlPath, lineNumber, "Ka requires three numbers.");
			}
			else if (keyword == "Ks")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "Ks must follow newmtl.");
				if (!ParseMaterialColor(lineStream, currentMaterial->SpecularColor))
					return FailMtl(outError, mtlPath, lineNumber, "Ks requires three numbers.");
			}
			else if (keyword == "Ke")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "Ke must follow newmtl.");
				if (!ParseMaterialColor(lineStream, currentMaterial->EmissiveColor))
					return FailMtl(outError, mtlPath, lineNumber, "Ke requires three numbers.");
			}
			else if (keyword == "Tf")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "Tf must follow newmtl.");
				if (!ParseMaterialColor(lineStream, currentMaterial->TransmissionFilter))
					return FailMtl(outError, mtlPath, lineNumber, "Tf requires three numbers.");
			}
			else if (keyword == "Ns")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "Ns must follow newmtl.");
				if (!(lineStream >> currentMaterial->SpecularExponent))
					return FailMtl(outError, mtlPath, lineNumber, "Ns requires a number.");
			}
			else if (keyword == "Ni")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "Ni must follow newmtl.");
				if (!(lineStream >> currentMaterial->OpticalDensity))
					return FailMtl(outError, mtlPath, lineNumber, "Ni requires a number.");
			}
			else if (keyword == "d")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "d must follow newmtl.");
				std::string value;
				if (!(lineStream >> value)) return FailMtl(outError, mtlPath, lineNumber, "d requires a number.");
				if (value == "-halo" && !(lineStream >> value))
					return FailMtl(outError, mtlPath, lineNumber, "d -halo requires a number.");
				const auto result = std::from_chars(value.data(), value.data() + value.size(), currentMaterial->Dissolve);
				if (result.ec != std::errc() || result.ptr != value.data() + value.size())
					return FailMtl(outError, mtlPath, lineNumber, "d requires a number.");
				currentMaterial->bHasDissolve = true;
			}
			else if (keyword == "Tr")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "Tr must follow newmtl.");
				if (!(lineStream >> currentMaterial->Transparency))
					return FailMtl(outError, mtlPath, lineNumber, "Tr requires a number.");
				currentMaterial->bHasTransparency = true;
			}
			else if (keyword == "illum")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "illum must follow newmtl.");
				if (!(lineStream >> currentMaterial->IlluminationModel))
					return FailMtl(outError, mtlPath, lineNumber, "illum requires an integer.");
			}
			else if (keyword == "map_Ka" || keyword == "map_Kd" || keyword == "map_Ks"
				|| keyword == "map_Ns" || keyword == "map_Ke" || keyword == "map_d"
				|| keyword == "map_Bump" || keyword == "map_bump" || keyword == "bump"
				|| keyword == "disp" || keyword == "decal" || keyword == "refl")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "texture map must follow newmtl.");
				FString* texturePath = nullptr;
				if (keyword == "map_Ka") texturePath = &currentMaterial->AmbientTexturePath;
				else if (keyword == "map_Kd") texturePath = &currentMaterial->DiffuseTexturePath;
				else if (keyword == "map_Ks") texturePath = &currentMaterial->SpecularTexturePath;
				else if (keyword == "map_Ns") texturePath = &currentMaterial->SpecularExponentTexturePath;
				else if (keyword == "map_Ke") texturePath = &currentMaterial->EmissiveTexturePath;
				else if (keyword == "map_d") texturePath = &currentMaterial->OpacityTexturePath;
				else if (keyword == "map_Bump" || keyword == "map_bump" || keyword == "bump") texturePath = &currentMaterial->NormalTexturePath;
				else if (keyword == "disp") texturePath = &currentMaterial->DisplacementTexturePath;
				else if (keyword == "decal") texturePath = &currentMaterial->DecalTexturePath;
				else texturePath = &currentMaterial->ReflectionTexturePath;
				if (!ParseMaterialTexturePath(lineStream, mtlPath, *texturePath))
					return FailMtl(outError, mtlPath, lineNumber, "texture map requires a path.");
			}
		}

		return true;
	}

	bool LoadMaterialLibraries(FObjInfo& rawMesh, const std::filesystem::path& objPath,
		const FFileManager& fileManager, FString& outError)
	{
		for (const FString& libraryPath : rawMesh.MaterialLibraryPaths)
		{
			const std::filesystem::path resolvedPath =
				(objPath.parent_path() / Utf8ToPath(libraryPath)).lexically_normal();
			try
			{
				const FString mtlText = fileManager.ReadFileToString(resolvedPath);
				if (!ParseMtl(static_cast<std::string_view>(mtlText), resolvedPath, rawMesh, outError)) return false;
			}
			catch (const std::exception& exception)
			{
				outError = std::string_view(exception.what());
				return false;
			}
		}
		return true;
	}

	bool CollectMeshDependencies(const std::filesystem::path& objPath,
		const FObjInfo& rawMesh, TArray<FMeshCacheDependency>& dependencies)
	{
		dependencies.Reset();
		std::unordered_map<std::string, bool> visitedPaths;
		auto AddDependency = [&dependencies, &visitedPaths](const std::filesystem::path& path)
		{
			FMeshCacheDependency dependency;
			if (!QueryDependency(path, dependency)) return false;
			const std::string key = PathToUtf8(dependency.Path);
			if (!visitedPaths.emplace(key, true).second) return true;
			dependencies.Add(dependency);
			return true;
		};

		if (!AddDependency(objPath)) return false;
		for (const FString& libraryPath : rawMesh.MaterialLibraryPaths)
		{
			const std::filesystem::path resolvedPath =
				(objPath.parent_path() / Utf8ToPath(libraryPath)).lexically_normal();
			if (!AddDependency(resolvedPath)) return false;
		}
		return true;
	}

	bool ParseObjRaw(std::string_view objText, FObjInfo& rawMesh, FString& outError)
	{
		FString currentObjectName;
		TArray<FString> currentGroupNames;
		FString currentMaterialName;
		int32 currentSmoothingGroup = 0;
		std::istringstream input{ std::string(objText) };
		std::string line;
		uint32 lineNumber = 0;

		while (std::getline(input, line))
		{
			++lineNumber;
			if (const size_t commentStart = line.find('#'); commentStart != std::string::npos)
			{
				line.erase(commentStart);
			}

			std::istringstream lineStream(line);
			std::string keyword;
			if (!(lineStream >> keyword)) continue;

			if (keyword == "v")
			{
				float x, y, z;
				if (!(lineStream >> x >> y >> z)) return Fail(outError, lineNumber, "vertex position requires three numbers.");
				rawMesh.Positions.Add(ConvertObjVector(FVector(x, y, z)));
			}
			else if (keyword == "vt")
			{
				float u, v;
				if (!(lineStream >> u >> v)) return Fail(outError, lineNumber, "vertex UV requires two numbers.");
				rawMesh.UVs.Add(FVector2(u, v));
			}
			else if (keyword == "vn")
			{
				float x, y, z;
				if (!(lineStream >> x >> y >> z)) return Fail(outError, lineNumber, "vertex normal requires three numbers.");
				FVector normal = ConvertObjVector(FVector(x, y, z));
				if (normal.IsNearlyZero()) return Fail(outError, lineNumber, "vertex normal cannot be zero.");
				normal.Normalize();
				rawMesh.Normals.Add(normal);
			}
			else if (keyword == "usemtl")
			{
				std::string materialName;
				if (!(lineStream >> materialName)) return Fail(outError, lineNumber, "usemtl requires a material name.");
				currentMaterialName = std::string_view(materialName);
			}
			else if (keyword == "o")
			{
				std::string objectName;
				if (!(lineStream >> objectName)) return Fail(outError, lineNumber, "o requires an object name.");
				currentObjectName = std::string_view(objectName);
			}
			else if (keyword == "g")
			{
				currentGroupNames.Reset();
				std::string groupName;
				while (lineStream >> groupName)
				{
					if (groupName != "off") currentGroupNames.Add(FString(std::string_view(groupName)));
				}
			}
			else if (keyword == "s")
			{
				std::string smoothingGroup;
				if (!(lineStream >> smoothingGroup)) return Fail(outError, lineNumber, "s requires off, on, or a group number.");
				if (smoothingGroup == "off" || smoothingGroup == "0") currentSmoothingGroup = 0;
				else if (smoothingGroup == "on") currentSmoothingGroup = 1;
				else
				{
					const auto result = std::from_chars(smoothingGroup.data(), smoothingGroup.data() + smoothingGroup.size(), currentSmoothingGroup);
					if (result.ec != std::errc() || result.ptr != smoothingGroup.data() + smoothingGroup.size()
						|| currentSmoothingGroup <= 0)
					{
						return Fail(outError, lineNumber, "s requires off, on, or a positive group number.");
					}
				}
			}
			else if (keyword == "mtllib")
			{
				std::string materialLibraryPath;
				while (lineStream >> materialLibraryPath)
				{
					rawMesh.MaterialLibraryPaths.Add(FString(std::string_view(materialLibraryPath)));
				}
				if (materialLibraryPath.empty()) return Fail(outError, lineNumber, "mtllib requires a material library path.");
			}
			else if (keyword == "f")
			{
				FObjFace face;
				face.LineNumber = lineNumber;
				face.ObjectName = currentObjectName;
				face.GroupNames = currentGroupNames;
				face.MaterialName = currentMaterialName;
				face.SmoothingGroup = currentSmoothingGroup;
				std::string vertexToken;
				while (lineStream >> vertexToken)
				{
					FObjVertexIndex index;
					if (!ParseFaceVertex(vertexToken, rawMesh.Positions.Num(), rawMesh.UVs.Num(), rawMesh.Normals.Num(), index))
					{
						return Fail(outError, lineNumber, "face vertices must use valid v, v/vt, v//vn, or v/vt/vn indices.");
					}
					face.Vertices.Add(index);
				}
				if (face.Vertices.Num() < 3) return Fail(outError, lineNumber, "face requires at least three vertices.");
				rawMesh.Faces.Add(face);
			}
		}

		return true;
	}

	bool BuildStaticMesh(const FObjInfo& rawMesh, FStaticMesh& outMesh, FString& outError)
	{
		if (rawMesh.Faces.IsEmpty())
		{
			outError = std::string_view("OBJ contains no faces.");
			return false;
		}

		FStaticMesh cookedMesh;
		std::unordered_map<FVertexKey, uint32, FVertexKeyHasher> vertexCache;
		std::unordered_map<std::string, uint32> materialIndices;
		struct FPartSectionBuildData
		{
			uint32 PartIndex = 0;
			TArray<uint32> Indices;
			TArray<int32> SmoothingGroups;
		};
		struct FSectionBuildData
		{
			FString MaterialName;
			uint32 MaterialIndex = 0;
			TArray<FPartSectionBuildData> Parts;
		};
		TArray<FSectionBuildData> sectionBuildData;
		std::unordered_map<uint32, uint32> sectionByMaterial;
		int32 generatedNormalID = 0;

		for (const FObjMaterial& rawMaterial : rawMesh.Materials)
		{
			FStaticMaterial material;
			material.MaterialLibraryPath = rawMaterial.MaterialLibraryPath;
			material.Name = rawMaterial.Name;
			material.AmbientColor = rawMaterial.AmbientColor;
			material.DiffuseColor = rawMaterial.DiffuseColor;
			material.SpecularColor = rawMaterial.SpecularColor;
			material.EmissiveColor = rawMaterial.EmissiveColor;
			material.TransmissionFilter = rawMaterial.TransmissionFilter;
			material.SpecularExponent = rawMaterial.SpecularExponent;
			material.OpticalDensity = rawMaterial.OpticalDensity;
			material.Dissolve = rawMaterial.Dissolve;
			material.Transparency = rawMaterial.Transparency;
			material.IlluminationModel = rawMaterial.IlluminationModel;
			material.bHasDissolve = rawMaterial.bHasDissolve;
			material.bHasTransparency = rawMaterial.bHasTransparency;
			material.AmbientTexturePath = rawMaterial.AmbientTexturePath;
			material.DiffuseTexturePath = rawMaterial.DiffuseTexturePath;
			material.SpecularTexturePath = rawMaterial.SpecularTexturePath;
			material.SpecularExponentTexturePath = rawMaterial.SpecularExponentTexturePath;
			material.EmissiveTexturePath = rawMaterial.EmissiveTexturePath;
			material.OpacityTexturePath = rawMaterial.OpacityTexturePath;
			material.NormalTexturePath = rawMaterial.NormalTexturePath;
			material.DisplacementTexturePath = rawMaterial.DisplacementTexturePath;
			material.DecalTexturePath = rawMaterial.DecalTexturePath;
			material.ReflectionTexturePath = rawMaterial.ReflectionTexturePath;
			const uint32 materialIndex = cookedMesh.Materials.Add(material);
			materialIndices.emplace(std::string(static_cast<std::string_view>(material.Name)), materialIndex);
		}

		auto getMaterialIndex = [&cookedMesh, &materialIndices](const FString& materialName)
		{
			const std::string name = materialName.Len() > 0
				? std::string(static_cast<std::string_view>(materialName)) : "Default";
			if (const auto found = materialIndices.find(name); found != materialIndices.end()) return found->second;

			FStaticMaterial material;
			material.Name = std::string_view(name);
			const uint32 materialIndex = cookedMesh.Materials.Add(material);
			materialIndices.emplace(name, materialIndex);
			return materialIndex;
		};

		auto getPartIndex = [&cookedMesh](const FObjFace& face)
		{
			for (int32 partIndex = 0; partIndex < cookedMesh.Parts.Num(); ++partIndex)
			{
				const FStaticMeshPart& part = cookedMesh.Parts[partIndex];
				if (!part.ObjectName.Equals(static_cast<std::string_view>(face.ObjectName))
					|| part.GroupNames.Num() != face.GroupNames.Num()) continue;

				bool groupsMatch = true;
				for (int32 groupIndex = 0; groupIndex < part.GroupNames.Num(); ++groupIndex)
				{
					if (!part.GroupNames[groupIndex].Equals(static_cast<std::string_view>(face.GroupNames[groupIndex])))
					{
						groupsMatch = false;
						break;
					}
				}
				if (groupsMatch) return static_cast<uint32>(partIndex);
			}

			FStaticMeshPart part;
			part.ObjectName = face.ObjectName;
			part.GroupNames = face.GroupNames;
			return static_cast<uint32>(cookedMesh.Parts.Add(part));
		};

		auto addVertex = [&rawMesh, &cookedMesh, &vertexCache, &outError](const FObjVertexIndex& index,
			const FVector& generatedNormal, int32 normalID, uint32 lineNumber, TArray<uint32>& outIndices) -> bool
		{
			if (!IsValidIndex(index.PositionIndex, rawMesh.Positions.Num())
				|| (index.UVIndex != -1 && !IsValidIndex(index.UVIndex, rawMesh.UVs.Num()))
				|| (index.NormalIndex != -1 && !IsValidIndex(index.NormalIndex, rawMesh.Normals.Num())))
			{
				return Fail(outError, lineNumber, "face index is outside the available position, UV, or normal list.");
			}

			const FVertexKey key{
				index.PositionIndex,
				index.UVIndex,
				index.NormalIndex,
				index.NormalIndex == -1 ? normalID : -1
			};
			if (const auto found = vertexCache.find(key); found != vertexCache.end())
			{
				outIndices.Add(found->second);
				return true;
			}

			FVertexPNCT vertex{};
			vertex.Position = rawMesh.Positions[index.PositionIndex];
			vertex.Normal = index.NormalIndex == -1 ? generatedNormal : rawMesh.Normals[index.NormalIndex];
			vertex.UV = index.UVIndex == -1 ? FVector2(0.0f, 0.0f) : rawMesh.UVs[index.UVIndex];
			vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

			const uint32 vertexIndex = cookedMesh.Vertices.Add(vertex);
			vertexCache.emplace(key, vertexIndex);
			outIndices.Add(vertexIndex);
			return true;
		};

		for (const FObjFace& face : rawMesh.Faces)
		{
			const uint32 materialIndex = getMaterialIndex(face.MaterialName);
			const uint32 partIndex = getPartIndex(face);
			uint32 sectionIndex;
			if (const auto found = sectionByMaterial.find(materialIndex); found != sectionByMaterial.end())
			{
				sectionIndex = found->second;
			}
			else
			{
				FSectionBuildData section;
				section.MaterialName = face.MaterialName;
				section.MaterialIndex = materialIndex;
				sectionIndex = sectionBuildData.Add(section);
				sectionByMaterial.emplace(materialIndex, sectionIndex);
			}

			FPartSectionBuildData* partBuildData = nullptr;
			for (FPartSectionBuildData& existingPart : sectionBuildData[sectionIndex].Parts)
			{
				if (existingPart.PartIndex == partIndex)
				{
					partBuildData = &existingPart;
					break;
				}
			}
			if (!partBuildData)
			{
				FPartSectionBuildData newPart;
				newPart.PartIndex = partIndex;
				const uint32 partBuildIndex = sectionBuildData[sectionIndex].Parts.Add(newPart);
				partBuildData = &sectionBuildData[sectionIndex].Parts[partBuildIndex];
			}

			TArray<uint32>& sectionIndices = partBuildData->Indices;
			std::vector<std::array<int32, 3>> triangles;
			if (!TriangulateFace(face, rawMesh, triangles, outError)) return false;
			for (const std::array<int32, 3>& triangle : triangles)
			{
				// The Y-up to Z-up conversion mirrors handedness, so preserve front faces by reversing winding.
				const FObjVertexIndex& first = face.Vertices[triangle[0]];
				const FObjVertexIndex& second = face.Vertices[triangle[2]];
				const FObjVertexIndex& third = face.Vertices[triangle[1]];

				FVector generatedNormal = FVector::cross(
					rawMesh.Positions[second.PositionIndex] - rawMesh.Positions[first.PositionIndex],
					rawMesh.Positions[third.PositionIndex] - rawMesh.Positions[first.PositionIndex]);
				if (generatedNormal.IsNearlyZero())
				{
					return Fail(outError, face.LineNumber, "face is degenerate and cannot generate a normal.");
				}
				generatedNormal.Normalize();

				const int32 triangleNormalID = generatedNormalID++;
				if (!addVertex(first, generatedNormal, triangleNormalID, face.LineNumber, sectionIndices)
					|| !addVertex(second, generatedNormal, triangleNormalID, face.LineNumber, sectionIndices)
					|| !addVertex(third, generatedNormal, triangleNormalID, face.LineNumber, sectionIndices))
				{
					return false;
				}
				partBuildData->SmoothingGroups.Add(face.SmoothingGroup);
			}
		}

		uint32 totalIndexCount = 0;
		for (const FSectionBuildData& section : sectionBuildData)
		{
			for (const FPartSectionBuildData& part : section.Parts)
			{
				totalIndexCount += static_cast<uint32>(part.Indices.Num());
			}
		}
		cookedMesh.Indices.Reserve(totalIndexCount);
		cookedMesh.TriangleSmoothingGroups.Reserve(totalIndexCount / 3);

		for (const FSectionBuildData& sectionData : sectionBuildData)
		{
			FStaticMeshSection section;
			section.MaterialName = sectionData.MaterialName;
			section.MaterialIndex = sectionData.MaterialIndex;
			section.FirstIndex = static_cast<uint32>(cookedMesh.Indices.Num());
			for (const FPartSectionBuildData& partData : sectionData.Parts)
			{
				FStaticMeshIndexRange range;
				range.MaterialIndex = sectionData.MaterialIndex;
				range.FirstIndex = static_cast<uint32>(cookedMesh.Indices.Num());
				range.NumIndices = static_cast<uint32>(partData.Indices.Num());
				for (const uint32 index : partData.Indices)
				{
					cookedMesh.Indices.Add(index);
				}
				for (const int32 smoothingGroup : partData.SmoothingGroups)
				{
					cookedMesh.TriangleSmoothingGroups.Add(smoothingGroup);
				}
				cookedMesh.Parts[partData.PartIndex].IndexRanges.Add(range);
			}
			section.NumIndices = static_cast<uint32>(cookedMesh.Indices.Num()) - section.FirstIndex;
			cookedMesh.Sections.Add(section);
		}

		outMesh = std::move(cookedMesh);
		return true;
	}
}

bool FObjImporter::Parse(std::string_view objText, FStaticMesh& outMesh, FString& outError)
{
	outError.Reset();
	FObjInfo rawMesh;
	if (!ParseObjRaw(objText, rawMesh, outError)) return false;
	return BuildStaticMesh(rawMesh, outMesh, outError);
}

bool FObjImporter::LoadFromFile(const std::filesystem::path& path, const FFileManager& fileManager,
	FStaticMesh& outMesh, FString& outError)
{
	try
	{
		outError.Reset();
		const std::filesystem::path objPath = path.is_absolute()
			? std::filesystem::weakly_canonical(path)
			: std::filesystem::weakly_canonical(fileManager.GetFileDirectoryPath() / path);
		const std::filesystem::path cachePath = GetMeshCachePath(objPath);

		FStaticMesh cachedMesh;
		if (TryLoadMeshCache(cachePath, cachedMesh))
		{
			cachedMesh.PathFileName = PathToUtf8(path);
			outMesh = std::move(cachedMesh);
			const std::string message = "Loaded OBJ mesh cache: " + std::string(PathToUtf8(cachePath)) + "\n";
			OutputDebugStringA(message.c_str());
			return true;
		}

		const FString objText = fileManager.ReadFileToString(objPath);
		FObjInfo rawMesh;
		if (!ParseObjRaw(static_cast<std::string_view>(objText), rawMesh, outError)) return false;

		if (!LoadMaterialLibraries(rawMesh, objPath, fileManager, outError)) return false;

		FStaticMesh parsedMesh;
		if (!BuildStaticMesh(rawMesh, parsedMesh, outError)) return false;

		parsedMesh.PathFileName = PathToUtf8(path);
		TArray<FMeshCacheDependency> dependencies;
		if (CollectMeshDependencies(objPath, rawMesh, dependencies)
			&& !SaveMeshCache(cachePath, dependencies, parsedMesh))
		{
			const std::string message = "Failed to save OBJ mesh cache: " + std::string(PathToUtf8(cachePath)) + "\n";
			OutputDebugStringA(message.c_str());
		}
		outMesh = std::move(parsedMesh);
		return true;
	}
	catch (const std::exception& exception)
	{
		outError = std::string_view(exception.what());
		return false;
	}
}


bool FObjImporter::LoadMaterialsFromFile(const std::filesystem::path& Path, const FFileManager& Files,
    TArray<FObjMaterial>& OutMaterials, FString& OutError)
{
    try
    {
        OutError.Reset();
        const auto resolved = Files.ResolvePath(Path);
        const FString text = Files.ReadFileToString(resolved);
        FObjInfo raw;
        if (!ParseMtl(static_cast<std::string_view>(text), resolved, raw, OutError)) return false;
        OutMaterials = std::move(raw.Materials);
        return true;
    }
    catch (const std::exception& error) { OutError = std::string_view(error.what()); return false; }
}
