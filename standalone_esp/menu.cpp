#include "menu.h"
#include "esp.h"

#include <imgui/imgui.h>

// ─── Durum ───────────────────────────────────────────────────────────────────

static bool g_bOpen = false;

// ─── Menu::Toggle ────────────────────────────────────────────────────────────

void Menu::Toggle()
{
    g_bOpen = !g_bOpen;
}

bool Menu::IsOpen()
{
    return g_bOpen;
}

// ─── Menu::Render ────────────────────────────────────────────────────────────

void Menu::Render()
{
    if (!g_bOpen)
        return;

    // ImGui'nin fareyi yakalasın
    ImGui::GetIO().MouseDrawCursor = true;

    // Pencere ayarları: yeniden boyutlandırılabilir, kapatma butonu yok
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoScrollbar;

    ImGui::SetNextWindowSize(ImVec2(300.f, 220.f), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(80.f, 80.f),    ImGuiCond_Once);

    // Başlık çubuğu rengi (koyu mavi)
    ImGui::PushStyleColor(ImGuiCol_TitleBg,       ImVec4(0.08f, 0.08f, 0.20f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,  ImVec4(0.10f, 0.10f, 0.30f, 1.f));

    if (ImGui::Begin("TF2 ESP  |  [DEL] kapat", nullptr, flags))
    {
        // ── ESP bölümü ──────────────────────────────────────────────────────
        ImGui::SeparatorText("ESP");

        ImGui::Checkbox("ESP Aktif",     &ESP::bEnabled);
        ImGui::Checkbox("Sadece Dusman", &ESP::bEnemyOnly);
        ImGui::Checkbox("Saglik Cubugu", &ESP::bShowHealth);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Bilgi ──────────────────────────────────────────────────────────
        ImGui::TextDisabled("DEL   - Menu ac / kapat");
        ImGui::TextDisabled("END   - DLL'i kaldir (unload)");

        ImGui::Spacing();

        // ── Kapat butonu ───────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.60f, 0.10f, 0.10f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.15f, 0.15f, 1.f));
        if (ImGui::Button("Kapat##closeBtn", ImVec2(-1.f, 0.f)))
            g_bOpen = false;
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);

    // Menü kapatıldığında fareyi gizle
    if (!g_bOpen)
        ImGui::GetIO().MouseDrawCursor = false;
}
