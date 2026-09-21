#include "OverlayStat.h"

#include "Core/FrameTimer.h"
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
	StatArray.Add({ "Vertex Shader Memory", 0,0,0.0f });
	StatArray.Add({ "Pixel Shader Memory", 0,0,0.0f });
	StatArray.Add({ "Static Mesh Memory", 0,0,0.0f });
}

void OverlayStatWindow::SetStats(const FGuiReference& guiReference)
{
	fps = guiReference.FrameTimer.GetFPS();
	FrameTimeMs = guiReference.FrameTimer.GetDeltaTime() * 1000;
	FRenderStats rdst = guiReference.RenderingPipeline.GetRenderer()->GetRenderStats();

	StatArray[0].UsedMemoryByte = rdst.VSMemoryByte;
	StatArray[1].UsedMemoryByte = rdst.PSmemoryByte;
	StatArray[2].UsedMemoryByte = rdst.StaticmeshMemoryByte;

	float totalByte = StatArray[0].UsedMemoryByte + StatArray[1].UsedMemoryByte + StatArray[2].UsedMemoryByte;

	StatArray[0].ResourceCount = rdst.VSResourceCount;
	StatArray[1].ResourceCount = rdst.PSResourceCount;
	StatArray[2].ResourceCount = rdst.StaticmeshResourceCount;

	if (totalByte != 0)
	{
		StatArray[0].Mempercent = StatArray[0].UsedMemoryByte / totalByte * 100;
		StatArray[1].Mempercent = StatArray[1].UsedMemoryByte / totalByte * 100;
		StatArray[2].Mempercent = StatArray[2].UsedMemoryByte / totalByte * 100;
	}
}

void OverlayStatWindow::DrawStat(float mPanelWidth)
{
	ImGuiIO& io = ImGui::GetIO();
	if (bShowMemoryStat)
	{
		
		ImGui::SetNextWindowPos(ImVec2(mPanelWidth, 10.0f), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - mPanelWidth*2, 200.0f),ImGuiCond_Once);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
		ImGui::Begin("Stat Memory", nullptr, flags);
		if (ImGui::BeginTable("MemoryStats", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Memory Counter");
			ImGui::TableSetupColumn("Used Memory Byte");
			ImGui::TableSetupColumn("Resource Count");
			ImGui::TableSetupColumn("Mempercent");
			ImGui::TableHeadersRow();

			for (int i = 0;i < StatArray.Num();i++)
			{
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(StatArray[i].MemoryCounter.CStr());

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.2f KB", StatArray[i].UsedMemoryByte/1024);

				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%d", StatArray[i].ResourceCount);

				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%.1f%%", StatArray[i].Mempercent);
			}
			ImGui::EndTable();
		}
		ImGui::End();
	}

	if (bShowFpsStat)
	{
		ImDrawList* DrawList = ImGui::GetForegroundDrawList();

		char buffer[128];

		snprintf(buffer, sizeof(buffer), "%.1f fps\n%.1f ms", fps, FrameTimeMs);

		DrawList->AddText(ImVec2(io.DisplaySize.x - 100.0f,100.0f ),IM_COL32(0, 255, 0, 255), buffer);
	}
}

