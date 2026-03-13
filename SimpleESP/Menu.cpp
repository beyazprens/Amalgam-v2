// ============================================================
//  Menu.cpp  –  Simple ImGui overlay menu
//
//  Open / close with the DEL key (handled in main.cpp Present hook).
//  Contains:
//    • ESP enable/disable checkbox
// ============================================================

#include "Menu.h"
#include "ESP.h"
#include "imgui.h"

bool CMenu::Init()
{
    return true; // nothing to pre-initialize
}

void CMenu::Toggle()
{
    m_bOpen = !m_bOpen;
}

void CMenu::Render()
{
    if (!m_bOpen) return;

    ImGui::SetNextWindowSize(ImVec2(260.f, 110.f), ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.88f);

    ImGui::Begin("TF2 ESP", &m_bOpen,
        ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoCollapse  |
        ImGuiWindowFlags_NoScrollbar);

    ImGui::Spacing();
    ImGui::Checkbox("Enemy ESP  (box + health bar)", &g_ESP.m_bEnabled);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Press  DEL  to show / hide this menu");

    ImGui::End();
}

// Global instance
CMenu g_Menu;
