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
		std::string texturePath;
		if (!std::getline(lineStream >> std::ws, texturePath) || texturePath.empty()) return false;
		const std::filesystem::path resolvedPath = (mtlPath.parent_path() / texturePath).lexically_normal();
		outTexturePath = std::string_view(resolvedPath.string());
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
