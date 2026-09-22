#pragma once

#include "Core/Core.h"
#include "Core/Container/TArray.h"

struct FGuiReference;
struct FRect;

struct StatContainer
{
	FString MemoryCounter;

	uint64 CumulativeBytes;
	uint64 CreationCount;
};

class OverlayStatWindow
{
public:
	OverlayStatWindow();

	// Singleton pattern
	OverlayStatWindow(const  OverlayStatWindow&) = delete;
	OverlayStatWindow& operator=(const  OverlayStatWindow&) = delete;
	OverlayStatWindow(OverlayStatWindow&&) = delete;
	OverlayStatWindow& operator=(OverlayStatWindow&&) = delete;

	static  OverlayStatWindow& GetInstance();

	void DrawStat(const FRect& SceneViewportRect);
	void SetStats(const FGuiReference& guiReference);
	void ActivateMemoryStat(){ bShowMemoryStat = true; }
	void ActivateFpsStat() { bShowFpsStat = true; }
	void DeactivateAllStat() { bShowMemoryStat = false; bShowFpsStat = false; }

protected:
	float fps = 0.0f;
	float FrameTimeMs = 0.0f;

	TArray<StatContainer> StatArray;//vertexshadermemory / staticmeshtotalmemory / pixelshadermemory

	bool bShowMemoryStat = false;
	bool bShowFpsStat = false;
};
