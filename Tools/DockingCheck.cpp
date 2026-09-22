#include "Editor/EditorUIManager.h"
#include "Editor/Console.h"
#include "Editor/FEditorViewportClient.h"
#include "Engine/SceneManager.h"
#include "Core/FrameTimer.h"
#include "Core/IO/FileManager.h"
#include "Rendering/RenderingPipeline.h"
#include "Rendering/Renderer.h"
#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_internal.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"
#include <DirectXTex.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <crtdbg.h>

static void Check(bool Value, const char* Message)
{
    if (!Value) throw std::runtime_error(Message);
}

int main()
{
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    try
    {
        Check(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), "COM initialization");
        // No visible window or changes to the user's editor layout.
        HWND Window = CreateWindowExW(0, L"STATIC", L"Docking check", WS_OVERLAPPEDWINDOW,
            0, 0, 1200, 800, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(Window != nullptr, "Hidden window");
        URenderer Renderer;
        Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &Renderer.Device, nullptr, &Renderer.DeviceContext)), "WARP device");
        FFileManager::Get().Initialize();
        {
            FRenderingPipeline Pipeline(Renderer);
            FEditorViewportClient Client;
            FSceneManager Scene(Client.GetCamera()); Scene.NewScene();
            FFrameTimer Timer(120);
            const FGuiReference References{Timer, Scene, Client, Pipeline, FFileManager::Get()};
            std::string SavedLayout;
            ImGuiID SavedPropertiesDock = 0;
            for (int Pass = 0; Pass < 2; ++Pass)
            {
                ImGui::CreateContext();
                auto& IO = ImGui::GetIO();
                IO.IniFilename = nullptr;
                IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
                Check(ImGui_ImplWin32_Init(Window), "Win32 backend");
                Check(ImGui_ImplDX11_Init(Renderer.Device, Renderer.DeviceContext), "DX11 backend");
                ConsoleWindow::Get().Init("Jungle Console Window", 1000);
                if (Pass) ImGui::LoadIniSettingsFromMemory(SavedLayout.c_str());
                FEditorUIManager UI;
                auto Frame = [&]
                {
                    FEditorCommands Commands;
                    UI.UpdateGui(References, Commands);
                    ImGui::Render();
                };
                Frame(); Frame();
                auto* Control = ImGui::FindWindowByName("PODO");
                auto* Properties = ImGui::FindWindowByName("Jungle Property Window");
                auto* Objects = ImGui::FindWindowByName("Object List Panel");
                auto* Console = ImGui::FindWindowByName("Jungle Console Window");
                Check(Control && Properties && Objects && Console, "Editor panels exist");
                Check(!(Properties->Flags & ImGuiWindowFlags_NoMove), "Properties can move");
                const auto Rect = UI.GetSceneViewportRect();
                Check(Rect.Width > 100 && Rect.Height > 100 && Rect.X > 0, "Central scene rectangle");
                const ImGuiID RootID = ImGui::DockNodeGetRootNode(Control->DockNode)->ID;
                const auto* Center = ImGui::DockBuilderGetCentralNode(RootID);
                Check(Center && Center->Windows.empty(), "Scene center stays empty");
                Check(Center->MergedFlags & ImGuiDockNodeFlags_PassthruCentralNode, "Scene center is transparent");
                if (!Pass)
                {
                    IO.AddMousePosEvent(Rect.X + Rect.Width * 0.5f, Rect.Y + Rect.Height * 0.5f);
                    Frame(); Frame();
                    Check(!IO.WantCaptureMouse, "Empty center passes mouse input to scene");
                    IO.AddMousePosEvent(Control->Pos.x + 20, Control->Pos.y + 50);
                    Frame(); Frame();
                    Check(IO.WantCaptureMouse, "Docked panel captures input instead of picking");
                    // Move Properties into the Object List tab group and resize the host.
                    ImGui::DockContextQueueUndockWindow(ImGui::GetCurrentContext(), Properties);
                    Frame(); Frame();
                    ImGui::DockContextQueueDock(ImGui::GetCurrentContext(), Objects, Objects->DockNode,
                        Properties, ImGuiDir_None, 0.5f, false);
                    SetWindowPos(Window, nullptr, 0, 0, 1000, 650, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                    Frame(); Frame();
                    Check(Properties->DockId == Objects->DockId, "Panels can share a tab group");
                    SavedPropertiesDock = Properties->DockId;
                    Check(UI.GetSceneViewportRect().Width < Rect.Width, "Host resize updates scene bounds");
                    // Undock the console without a platform window.
                    ImGui::DockContextQueueUndockWindow(ImGui::GetCurrentContext(), Console);
                    Frame(); Frame();
                    Check(Console->DockId == 0 && !Console->DockNode, "Console can float");
                    Check(!(Console->Flags & (ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)), "Floating console can move/resize");
                    size_t Size = 0;
                    const char* Ini = ImGui::SaveIniSettingsToMemory(&Size);
                    SavedLayout.assign(Ini, Size);
                }
                else
                {
                    Check(Properties->DockId == SavedPropertiesDock && Properties->DockId == Objects->DockId,
                        "Saved tab grouping restored");
                    Check(Console->DockId == 0, "Floating console restored");
                }
                // Render with the replacement backend to an offscreen target.
                auto Target = Renderer.CreateRenderTarget2D((uint32)IO.DisplaySize.x, (uint32)IO.DisplaySize.y,
                    DXGI_FORMAT_B8G8R8A8_UNORM);
                Check(Target != nullptr, "Offscreen target");
                Renderer.BindRenderTarget(Target, nullptr);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                if (Pass == 0)
                {
                    DirectX::ScratchImage Capture;
                    Check(SUCCEEDED(DirectX::CaptureTexture(Renderer.Device, Renderer.DeviceContext,
                        Target->Texture.Get(), Capture)), "Capture docking layout");
                    Check(SUCCEEDED(DirectX::SaveToWICFile(*Capture.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE,
                        DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), L"../Tools/bin/DockingCheck/layout.png")), "Save layout preview");
                }
                Renderer.DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
                ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
            }
        }
        Renderer.Release();
        DestroyWindow(Window);
        CoUninitialize();
        std::cout << "PASS: docking layout, panel tab/float, resize, ini round trip, DX11 offscreen render\n";
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: " << Error.what() << '\n';
        return 1;
    }
}
