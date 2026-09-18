#include "ObjImporter.h"

#include <charconv>
#include <exception>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>

namespace
{
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

	bool FailMtl(FString& outError, const std::filesystem::path& path, uint32 lineNumber, std::string_view message)
	{
		std::string error = "MTL parse error in " + path.string() + " on line " + std::to_string(lineNumber) + ": ";
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
				rawMesh.Materials.Add(material);
				currentMaterial = &rawMesh.Materials[rawMesh.Materials.Num() - 1];
			}
			else if (keyword == "Kd")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "Kd must follow newmtl.");
				float red, green, blue;
				if (!(lineStream >> red >> green >> blue)) return FailMtl(outError, mtlPath, lineNumber, "Kd requires three numbers.");
				currentMaterial->DiffuseColor = FVector4(red, green, blue, 1.0f);
			}
			else if (keyword == "map_Kd")
			{
				if (!currentMaterial) return FailMtl(outError, mtlPath, lineNumber, "map_Kd must follow newmtl.");
				std::string texturePath;
				if (!(lineStream >> texturePath)) return FailMtl(outError, mtlPath, lineNumber, "map_Kd requires a texture path.");
				const std::filesystem::path resolvedPath = (mtlPath.parent_path() / texturePath).lexically_normal();
				currentMaterial->DiffuseTexturePath = std::string_view(resolvedPath.string());
			}
		}

		return true;
	}

	bool LoadMaterialLibraries(FObjInfo& rawMesh, const std::filesystem::path& objPath,
		const FFileManager& fileManager, FString& outError)
	{
		for (const FString& libraryPath : rawMesh.MaterialLibraryPaths)
		{
			const std::filesystem::path resolvedPath = (objPath.parent_path()
				/ std::string(static_cast<std::string_view>(libraryPath))).lexically_normal();
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

	bool ParseObjRaw(std::string_view objText, FObjInfo& rawMesh, FString& outError)
	{
		FString currentMaterialName;
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
				face.MaterialName = currentMaterialName;
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
		int32 generatedNormalID = 0;

		for (const FObjMaterial& rawMaterial : rawMesh.Materials)
		{
			FStaticMaterial material;
			material.Name = rawMaterial.Name;
			material.DiffuseColor = rawMaterial.DiffuseColor;
			material.DiffuseTexturePath = rawMaterial.DiffuseTexturePath;
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

		auto addVertex = [&rawMesh, &cookedMesh, &vertexCache, &outError](const FObjVertexIndex& index,
			const FVector& generatedNormal, int32 normalID, uint32 lineNumber) -> bool
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
				cookedMesh.Indices.Add(found->second);
				return true;
			}

			FVertexPNCT vertex{};
			vertex.Position = rawMesh.Positions[index.PositionIndex];
			vertex.Normal = index.NormalIndex == -1 ? generatedNormal : rawMesh.Normals[index.NormalIndex];
			vertex.UV = index.UVIndex == -1 ? FVector2(0.0f, 0.0f) : rawMesh.UVs[index.UVIndex];
			vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

			const uint32 vertexIndex = cookedMesh.Vertices.Add(vertex);
			vertexCache.emplace(key, vertexIndex);
			cookedMesh.Indices.Add(vertexIndex);
			return true;
		};

		for (const FObjFace& face : rawMesh.Faces)
		{
			if (cookedMesh.Sections.IsEmpty()
				|| !cookedMesh.Sections[cookedMesh.Sections.Num() - 1].MaterialName.Equals(face.MaterialName))
			{
				FStaticMeshSection section;
				section.MaterialName = face.MaterialName;
				section.MaterialIndex = getMaterialIndex(face.MaterialName);
				section.FirstIndex = static_cast<uint32>(cookedMesh.Indices.Num());
				cookedMesh.Sections.Add(section);
			}

			FStaticMeshSection& activeSection = cookedMesh.Sections[cookedMesh.Sections.Num() - 1];
			for (int32 corner = 1; corner < face.Vertices.Num() - 1; ++corner)
			{
				// The Y-up to Z-up conversion mirrors handedness, so preserve front faces by reversing winding.
				const FObjVertexIndex& first = face.Vertices[0];
				const FObjVertexIndex& second = face.Vertices[corner + 1];
				const FObjVertexIndex& third = face.Vertices[corner];
				if (!IsValidIndex(first.PositionIndex, rawMesh.Positions.Num())
					|| !IsValidIndex(second.PositionIndex, rawMesh.Positions.Num())
					|| !IsValidIndex(third.PositionIndex, rawMesh.Positions.Num()))
				{
					return Fail(outError, face.LineNumber, "face position index is outside the available position list.");
				}

				FVector generatedNormal = FVector::cross(
					rawMesh.Positions[second.PositionIndex] - rawMesh.Positions[first.PositionIndex],
					rawMesh.Positions[third.PositionIndex] - rawMesh.Positions[first.PositionIndex]);
				if (generatedNormal.IsNearlyZero())
				{
					return Fail(outError, face.LineNumber, "face is degenerate and cannot generate a normal.");
				}
				generatedNormal.Normalize();

				const int32 triangleNormalID = generatedNormalID++;
				if (!addVertex(first, generatedNormal, triangleNormalID, face.LineNumber)
					|| !addVertex(second, generatedNormal, triangleNormalID, face.LineNumber)
					|| !addVertex(third, generatedNormal, triangleNormalID, face.LineNumber))
				{
					return false;
				}
				activeSection.NumIndices += 3;
			}
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

bool FObjImporter::LoadFromFile(std::string_view path, const FFileManager& fileManager,
	FStaticMesh& outMesh, FString& outError)
{
	try
	{
		const FString objText = fileManager.ReadFileToString(std::filesystem::path(path));
		outError.Reset();
		FObjInfo rawMesh;
		if (!ParseObjRaw(static_cast<std::string_view>(objText), rawMesh, outError)) return false;

		const std::filesystem::path objPath{ std::string(path) };
		if (!LoadMaterialLibraries(rawMesh, objPath, fileManager, outError)) return false;

		FStaticMesh parsedMesh;
		if (!BuildStaticMesh(rawMesh, parsedMesh, outError)) return false;

		parsedMesh.PathFileName = path;
		outMesh = std::move(parsedMesh);
		return true;
	}
	catch (const std::exception& exception)
	{
		outError = std::string_view(exception.what());
		return false;
	}
}
