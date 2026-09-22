#pragma once

#include "Core.h"
#include <stdexcept>

enum class EAxis : int { X = 0, Y = 1, Z = 2 };

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
