#include "OverlayStat.h"

#include "Core/FrameTimer.h"
#include "Core/Object/Object.h"
#include "Editor/EditorUIManager.h"
#include "Rendering/Renderer.h"
#include "Rendering/RenderingPipeline.h"
#include "ThirdParty/ImGui/imgui.h"

OverlayStatWindow&
OverlayStatWindow::GetInstance()
{
	static OverlayStatWindow Instance;
	return Instance;
}

OverlayStatWindow::OverlayStatWindow()
{
	StatArray.Add({ "Vertex shader bytecode", 0, 0 });
	StatArray.Add({ "Pixel shader bytecode", 0, 0 });
	StatArray.Add({ "Mesh uploads (VB + IB)", 0, 0 });
}

void OverlayStatWindow::SetStats(const FGuiReference& guiReference)
{
	fps = guiReference.FrameTimer.GetFPS();
	FrameTimeMs = static_cast<float>(guiReference.FrameTimer.GetFrameTimeMs());
	const FRenderStats& rdst = guiReference.RenderingPipeline.GetRenderer()->GetRenderStats();

	StatArray[0].CumulativeBytes = rdst.VSMemoryByte;
	StatArray[1].CumulativeBytes = rdst.PSmemoryByte;
	StatArray[2].CumulativeBytes = rdst.StaticmeshMemoryByte;

	StatArray[0].CreationCount = rdst.VSResourceCount;
	StatArray[1].CreationCount = rdst.PSResourceCount;
	StatArray[2].CreationCount = rdst.StaticmeshResourceCount;
}

void OverlayStatWindow::DrawStat(const FRect& SceneViewportRect)
{
	const float mPanelWidth = SceneViewportRect.X;
	if (bShowMemoryStat)
	{

		ImGui::SetNextWindowPos(ImVec2(mPanelWidth, 10.0f), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(540.0f, 290.0f), ImGuiCond_FirstUseEver);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
		if (ImGui::Begin("Stat Memory", &bShowMemoryStat, flags))
		{
			ImGui::SeparatorText("Current UObject allocations");
			ImGui::Text("Live UObject count: %llu", static_cast<unsigned long long>(UObject::GetTotalAllocationCount()));
			ImGui::Text("UObject memory: %llu bytes", static_cast<unsigned long long>(UObject::GetTotalAllocationBytes()));
			ImGui::TextWrapped("Object storage only; excludes separate member allocations and GPU resources.");
			ImGui::SeparatorText("Cumulative resource creation");
			if (ImGui::BeginTable("MemoryStats", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch, 2.0f);
				ImGui::TableSetupColumn("Total KiB");
				ImGui::TableSetupColumn("Events");
				ImGui::TableHeadersRow();

				for (int i = 0;i < StatArray.Num();i++)
				{
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(StatArray[i].MemoryCounter.CStr());

					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.2f", static_cast<double>(StatArray[i].CumulativeBytes) / 1024.0);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%llu bytes", static_cast<unsigned long long>(StatArray[i].CumulativeBytes));

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%llu", static_cast<unsigned long long>(StatArray[i].CreationCount));
				}
				ImGui::EndTable();
			}
			ImGui::TextWrapped("Renderer lifetime totals, including repeated creation. Not current memory or VRAM usage. One mesh event uploads its vertex/index buffers.");
		}
		ImGui::End();
	}

	if (bShowFpsStat && SceneViewportRect.Width > 0.0f && SceneViewportRect.Height > 0.0f)
	{
		ImDrawList* DrawList = ImGui::GetForegroundDrawList();

		char buffer[128];

		snprintf(buffer, sizeof(buffer), "%.1f fps\n%.1f ms", fps, FrameTimeMs);

		const ImVec2 TextSize = ImGui::CalcTextSize(buffer);
		constexpr float Margin = 12.0f;
		if (SceneViewportRect.Width >= TextSize.x + Margin * 2.0f &&
			SceneViewportRect.Height >= TextSize.y + Margin * 2.0f)
		{
			// Scene rect uses client coordinates; ImGui draw lists use screen coordinates.
			const ImVec2 Origin = ImGui::GetMainViewport()->Pos;
			const ImVec2 Position(Origin.x + SceneViewportRect.X + SceneViewportRect.Width - TextSize.x - Margin,
				Origin.y + SceneViewportRect.Y + Margin);
			DrawList->AddText(Position, IM_COL32(0, 255, 0, 255), buffer);
		}
	}
}
