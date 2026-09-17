#include "ObjImporter.h"

#include <charconv>
#include <exception>
#include <sstream>
#include <string>
#include <unordered_map>

namespace
{
	struct FVertexKey
	{
		uint32 PositionIndex;
		uint32 UVIndex;
		uint32 NormalIndex;

		bool operator==(const FVertexKey& other) const
		{
			return PositionIndex == other.PositionIndex
				&& UVIndex == other.UVIndex
				&& NormalIndex == other.NormalIndex;
		}
	};

	struct FVertexKeyHasher
	{
		std::size_t operator()(const FVertexKey& key) const
		{
			return (static_cast<std::size_t>(key.PositionIndex) * 73856093u)
				^ (static_cast<std::size_t>(key.UVIndex) * 19349663u)
				^ (static_cast<std::size_t>(key.NormalIndex) * 83492791u);
		}
	};

	bool Fail(FString& outError, uint32 lineNumber, std::string_view message)
	{
		std::string error = "OBJ parse error on line " + std::to_string(lineNumber) + ": ";
		error += message;
		outError = std::string_view(error);
		return false;
	}

	bool ParsePositiveIndex(std::string_view token, int32& outIndex)
	{
		if (token.empty())
		{
			return false;
		}

		int32 value = 0;
		const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
		if (result.ec != std::errc() || result.ptr != token.data() + token.size() || value <= 0)
		{
			return false;
		}

		outIndex = value - 1;
		return true;
	}

	bool ParseFaceVertex(std::string_view token, FObjVertexIndex& outIndex)
	{
		const size_t firstSlash = token.find('/');
		const size_t secondSlash = firstSlash == std::string_view::npos ? std::string_view::npos : token.find('/', firstSlash + 1);
		if (firstSlash == std::string_view::npos || secondSlash == std::string_view::npos
			|| token.find('/', secondSlash + 1) != std::string_view::npos)
		{
			return false;
		}

		return ParsePositiveIndex(token.substr(0, firstSlash), outIndex.PositionIndex)
			&& ParsePositiveIndex(token.substr(firstSlash + 1, secondSlash - firstSlash - 1), outIndex.UVIndex)
			&& ParsePositiveIndex(token.substr(secondSlash + 1), outIndex.NormalIndex);
	}

	bool IsValidIndex(int32 index, int32 count)
	{
		return index >= 0 && index < count;
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

		auto addVertex = [&rawMesh, &cookedMesh, &vertexCache, &outError](const FObjVertexIndex& index, uint32 lineNumber) -> bool
		{
			if (!IsValidIndex(index.PositionIndex, rawMesh.Positions.Num())
				|| !IsValidIndex(index.UVIndex, rawMesh.UVs.Num())
				|| !IsValidIndex(index.NormalIndex, rawMesh.Normals.Num()))
			{
				return Fail(outError, lineNumber, "face index is outside the available position, UV, or normal list.");
			}

			const FVertexKey key{
				static_cast<uint32>(index.PositionIndex),
				static_cast<uint32>(index.UVIndex),
				static_cast<uint32>(index.NormalIndex)
			};

			const auto found = vertexCache.find(key);
			if (found != vertexCache.end())
			{
				cookedMesh.Indices.Add(found->second);
				return true;
			}

			FVertexPNCT vertex{};
			vertex.Position = rawMesh.Positions[index.PositionIndex];
			vertex.Normal = rawMesh.Normals[index.NormalIndex];
			vertex.UV = rawMesh.UVs[index.UVIndex];
			vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

			const uint32 vertexIndex = cookedMesh.Vertices.Add(vertex);
			vertexCache.emplace(key, vertexIndex);
			cookedMesh.Indices.Add(vertexIndex);
			return true;
		};

		for (const FObjFace& face : rawMesh.Faces)
		{
			for (int32 corner = 1; corner < face.Vertices.Num() - 1; ++corner)
			{
				if (!addVertex(face.Vertices[0], face.LineNumber)
					|| !addVertex(face.Vertices[corner], face.LineNumber)
					|| !addVertex(face.Vertices[corner + 1], face.LineNumber))
				{
					return false;
				}
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
	std::istringstream input{ std::string(objText) };
	std::string line;
	uint32 lineNumber = 0;

	while (std::getline(input, line))
	{
		++lineNumber;
		const size_t commentStart = line.find('#');
		if (commentStart != std::string::npos)
		{
			line.erase(commentStart);
		}

		std::istringstream lineStream(line);
		std::string keyword;
		if (!(lineStream >> keyword))
		{
			continue;
		}

		if (keyword == "v")
		{
			float x, y, z;
			if (!(lineStream >> x >> y >> z))
			{
				return Fail(outError, lineNumber, "vertex position requires three numbers.");
			}
			rawMesh.Positions.Add(FVector(x, y, z));
		}
		else if (keyword == "vt")
		{
			float u, v;
			if (!(lineStream >> u >> v))
			{
				return Fail(outError, lineNumber, "vertex UV requires two numbers.");
			}
			rawMesh.UVs.Add(FVector2(u, v));
		}
		else if (keyword == "vn")
		{
			float x, y, z;
			if (!(lineStream >> x >> y >> z))
			{
				return Fail(outError, lineNumber, "vertex normal requires three numbers.");
			}
			rawMesh.Normals.Add(FVector(x, y, z));
		}
		else if (keyword == "f")
		{
			FObjFace face;
			face.LineNumber = lineNumber;
			std::string vertexToken;
			while (lineStream >> vertexToken)
			{
				FObjVertexIndex index;
				if (!ParseFaceVertex(vertexToken, index))
				{
					return Fail(outError, lineNumber, "face vertices must use positive v/vt/vn indices.");
				}
				face.Vertices.Add(index);
			}

			if (face.Vertices.Num() < 3)
			{
				return Fail(outError, lineNumber, "face requires at least three vertices.");
			}
			rawMesh.Faces.Add(face);
		}
	}

	return BuildStaticMesh(rawMesh, outMesh, outError);
}

bool FObjImporter::LoadFromFile(std::string_view path, const FFileManager& fileManager,
	FStaticMesh& outMesh, FString& outError)
{
	try
	{
		const FString objText = fileManager.ReadFileToString(path);
		FStaticMesh parsedMesh;
		if (!Parse(static_cast<std::string_view>(objText), parsedMesh, outError))
		{
			return false;
		}

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
