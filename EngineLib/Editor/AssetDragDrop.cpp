#include "AssetDragDrop.h"

#include "Core/AssetSystem/AssetManager.h"
#include "ThirdParty/ImGui/imgui.h"
#include <cfloat>
#include <cstring>
#include <string>

namespace AssetDragDrop
{
    namespace { constexpr const char* PayloadType = "CONTENT_BROWSER_ASSET"; }

    void Source(const char* AssetPath, const char* DisplayName)
    {
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(PayloadType, AssetPath, std::strlen(AssetPath) + 1);
            ImGui::TextUnformatted(DisplayName);
            ImGui::EndDragDropSource();
        }
    }

    bool Slot(const char* Id, const char* TypeLabel, const char* CurrentPath,
        const FClassInfo* ExpectedClass, const UAssetManager& Assets, FName& DroppedName)
    {
        ImGui::PushID(Id);
        std::string Filename = CurrentPath;
        const auto Slash = Filename.find_last_of("/\\");
        if (Slash != std::string::npos) Filename.erase(0, Slash + 1);
        const std::string Label = std::string(TypeLabel) + "\n" +
            (Filename.empty() ? "Drop asset here" : Filename) + "###AssetSlot";
        ImGui::Button(Label.c_str(), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 2 + ImGui::GetStyle().FramePadding.y * 2));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Drop a %s asset here.\n%s", TypeLabel, CurrentPath);

        bool Delivered = false;
        if (ImGui::BeginDragDropTarget())
        {
            const auto* Payload = ImGui::GetDragDropPayload();
            if (Payload && Payload->IsDataType(PayloadType) && Payload->DataSize > 1)
            {
                const auto* Path = static_cast<const char*>(Payload->Data);
                if (Path[Payload->DataSize - 1] == '\0')
                {
                    const FName Name(Path);
                    const auto* Meta = Assets.FindMetaInfo(Name);
                    // Check before accepting so incompatible slots aren't highlighted.
                    if (Meta && Meta->AssetClass == ExpectedClass && ImGui::AcceptDragDropPayload(PayloadType))
                    {
                        DroppedName = Meta->AssetName;
                        Delivered = true;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();
        return Delivered;
    }
}
