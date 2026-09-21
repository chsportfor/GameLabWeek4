#pragma once

#include "Core.h"
#include <stdexcept>

enum class EAxis : int { X = 0, Y = 1, Z = 2 };

enum class EPrimitive
{
	EP_Sphere,
	EP_Cube,
	EP_Triangle,
	EP_GizmoArrow,
	EP_Circle,
	EP_BillboardQuad,
};

enum EGIZMO_AXIS //어떤축이 선택되었는지
{
	NONE,
	X,
	Y,
	Z
};

enum EGIZMO_TYPE {
	TRANSLATE,
	ROTATE,
	SCALE,
};

enum class EViewModeIndex : uint32
{
	VMI_Lit,
	VMI_Unlit,
	VMI_Wireframe,
	VMI_Max,
};

enum class EEngineShowFlags : uint32
{
	SF_Primitives = 1 << 0,
	SF_BillboardText = 1 << 1,
	SF_WorldAxis = 1 << 2,
	SF_BoundingBox = 1 << 3,
	SF_Grid = 1 << 4,
};

constexpr EEngineShowFlags operator|(EEngineShowFlags lhs, EEngineShowFlags rhs)
{
	return static_cast<EEngineShowFlags>(static_cast<uint32>(lhs) | static_cast<uint32>(rhs));
}

constexpr EEngineShowFlags operator&(EEngineShowFlags lhs, EEngineShowFlags rhs)
{
	return static_cast<EEngineShowFlags>(static_cast<uint32>(lhs) & static_cast<uint32>(rhs));
}

inline EPrimitive StringToEPrimitive(const char* str)
{
	if (strcmp(str, "Sphere") == 0)
	{
		return EPrimitive::EP_Sphere;
	}
	else if (strcmp(str, "Cube") == 0)
	{
		return EPrimitive::EP_Cube;
	}
	else if (strcmp(str, "Triangle") == 0)
	{
		return EPrimitive::EP_Triangle;
	}
	else if (strcmp(str, "GizmoArrow") == 0)
	{
		return EPrimitive::EP_GizmoArrow;
	}
	else if (strcmp(str, "Circle") == 0)
	{
		return EPrimitive::EP_Circle;
	}
	else if (strcmp(str, "BillboardQuad") == 0)
	{
		return EPrimitive::EP_BillboardQuad;
	}
	else
	{
		throw std::runtime_error("Unknown EPrimitive value");
	}
}

inline const char* PrimitiveToString(EPrimitive primitiveType)
{
	switch (primitiveType)
	{
	case EPrimitive::EP_Cube:
		return "Cube";

	case EPrimitive::EP_Sphere:
		return "Sphere";

	case EPrimitive::EP_Triangle:
		return "Triangle";

	case EPrimitive::EP_GizmoArrow:
		return "GizmoArrow";

	case EPrimitive::EP_Circle:
		return "Circle";

	case EPrimitive::EP_BillboardQuad:
		return "BillboardQuad";

	default:
		return "Unknown";
	}
}

enum EBlendStateType
{
	BST_Default,
	BST_AlphaBlend,
	BST_Additive,
	BST_NoColorWrite,
	BST_Count,
};


enum class ELevelViewportType : uint8 {
	Perspective,
	Top,
	Bottom,
	Left,
	Right,
	Front,
	Back
};
