#define WIN32_LEAN_AND_MEAN
#include "../StartupScreen.h"
#include <imgui.h>
#include <filesystem>
#include <windows.h>
#include <shobjidl.h>
#include <objbase.h>
#include "Projects/RecentProjects.h"

#pragma comment(lib, "Ole32.lib")


static std::filesystem::path OpenFolderDialog()
{
    std::filesystem::path result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool shouldUninit = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    hr = CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog)
    );

    if (SUCCEEDED(hr))
    {
        DWORD options;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

        dialog->SetTitle(L"Select Project Location");

        if (SUCCEEDED(dialog->Show(nullptr)))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)))
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    result = std::filesystem::path(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (shouldUninit)
        CoUninitialize();

    return result;
}

static std::filesystem::path OpenProjectFileDialog()
{
    std::filesystem::path result;

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog))))
    {
        COMDLG_FILTERSPEC filters[] =
        {
            { L"MiniEngine Project (*.meproj)", L"*.meproj" }
        };

        dialog->SetFileTypes(1, filters);
        dialog->SetTitle(L"Open MiniEngine Project");

        if (SUCCEEDED(dialog->Show(nullptr)))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)))
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    result = std::filesystem::path(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    return result;
}


StartupResult StartupScreen::Draw()
{
    StartupResult result{};

    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(),
        ImGuiCond_Always,
        { 0.5f, 0.5f }
    );

    ImGui::SetNextWindowSize({ 520, 320 });

    ImGui::Begin("Welcome to MiniEngine", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking);

    // ---------------- Buttons ----------------
    if (ImGui::Button(" New Project", { -1, 40 }))
    {
        ImGui::OpenPopup("New Project");
    }

    if (ImGui::Button(" Open Project", { -1, 40 }))
    {
        auto folder = OpenProjectFileDialog();
        if (!folder.empty())
        {
            result.projectChosen = true;
            result.projectPath = folder;
        }
    }

    // ---------------- New Project Modal ----------------
    static char projectName[128] = "";
    static std::filesystem::path projectLocation;

    if (ImGui::BeginPopupModal("New Project", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Create a new project");
        ImGui::Separator();

        ImGui::InputText("Project Name", projectName, IM_ARRAYSIZE(projectName));

        ImGui::Text("Location:");
        ImGui::SameLine();
        ImGui::TextDisabled("%s",
            projectLocation.empty()
            ? "<not selected>"
            : projectLocation.string().c_str());

        if (ImGui::Button("Browse..."))
        {
            auto folder = OpenFolderDialog();
            if (!folder.empty())
                projectLocation = folder;
        }

        ImGui::Spacing();
        ImGui::Separator();

        bool canCreate =
            strlen(projectName) > 0 &&
            !projectLocation.empty();

        if (!canCreate)
            ImGui::BeginDisabled();

        if (ImGui::Button("Create", { 120, 0 }))
        {
            result.projectChosen = true;
            result.projectPath = projectLocation / projectName;

            // Reset state
            projectName[0] = '\0';
            projectLocation.clear();

            ImGui::CloseCurrentPopup();
        }

        if (!canCreate)
            ImGui::EndDisabled();

        ImGui::SameLine();

        if (ImGui::Button("Cancel", { 120, 0 }))
        {
            projectName[0] = '\0';
            projectLocation.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (bShowLoadError)
    {
        ImGui::OpenPopup("Failed to Load Project");
        bShowLoadError = false;
    }

    if (ImGui::BeginPopupModal("Failed to Load Project", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            "This project could not be loaded.\n\n"
            "The project file may be invalid, missing, or created with a newer version of MiniEngine."
        );

        ImGui::Separator();

        if (ImGui::Button("OK", { 120, 0 }))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Recent Projects");

    for (const auto& p : RecentProjects::Get())
    {
        if (ImGui::Selectable(p.filename().string().c_str()))
        {
            result.projectChosen = true;
            result.projectPath = p;
        }
    }

    ImGui::End();
    return result;
}

void StartupScreen::NotifyLoadError()
{
	bShowLoadError = true;
}
