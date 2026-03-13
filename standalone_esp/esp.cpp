#include "esp.h"
#include "sdk.h"

#include <imgui/imgui.h>
#include <Windows.h>

// ─── Statik arayüz pointer'ları ──────────────────────────────────────────────

static IClientEntityList* g_pEntityList = nullptr;
static IVEngineClient*    g_pEngine     = nullptr;
static IDirect3DDevice9*  g_pDevice     = nullptr;
static bool               g_bInited     = false;

// ─── Renkler ─────────────────────────────────────────────────────────────────

// TF2'de max HP sınıfa göre değişir (en yüksek Heavy = 300)
static constexpr float MAX_HP = 300.f;

// ─── Yardımcı: sağlık oranına göre renk ─────────────────────────────────────

static ImColor HealthColor(float ratio)
{
    if (ratio < 0.f) ratio = 0.f;
    if (ratio > 1.f) ratio = 1.f;

    if (ratio > 0.6f)
    {
        // yeşil → sarı
        float t = (ratio - 0.6f) / 0.4f;
        return ImColor(static_cast<int>((1.f - t) * 255), 255, 0, 255);
    }
    // sarı → kırmızı
    float t = ratio / 0.6f;
    return ImColor(255, static_cast<int>(t * 255), 0, 255);
}

// ─── Yardımcı: kenarlıklı kutu çiz ──────────────────────────────────────────

static void DrawBox(ImDrawList* dl,
                    float x, float y, float w, float h,
                    ImU32 colMain)
{
    // Siyah gölge (okunabilirlik)
    constexpr ImU32 shadow = IM_COL32(0, 0, 0, 200);
    dl->AddRect(ImVec2(x - 1.f, y - 1.f), ImVec2(x + w + 1.f, y + h + 1.f), shadow);
    dl->AddRect(ImVec2(x + 1.f, y + 1.f), ImVec2(x + w - 1.f, y + h - 1.f), shadow);
    // Ana kutu (cyan)
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), colMain, 0.f, 0, 1.5f);
}

// ─── Yardımcı: sağlık çubuğu ─────────────────────────────────────────────────

static void DrawHealthBar(ImDrawList* dl,
                          float bx, float by, float bh,
                          int hp, int maxHp)
{
    if (maxHp <= 0) return;

    constexpr float barW = 4.f;
    constexpr float gap  = 2.f;

    float ratio = static_cast<float>(hp) / static_cast<float>(maxHp);
    if (ratio < 0.f) ratio = 0.f;
    if (ratio > 1.f) ratio = 1.f;

    float barH = bh * ratio;
    float x    = bx - gap - barW;
    float yT   = by;
    float yB   = by + bh;

    dl->AddRectFilled(ImVec2(x, yT), ImVec2(x + barW, yB), IM_COL32(0, 0, 0, 160));
    dl->AddRectFilled(ImVec2(x, yB - barH), ImVec2(x + barW, yB), (ImU32)HealthColor(ratio));
    dl->AddRect(ImVec2(x, yT), ImVec2(x + barW, yB), IM_COL32(0, 0, 0, 200));
}

// ─── ESP::Initialize ─────────────────────────────────────────────────────────

bool ESP::Initialize()
{
    // Arayüzleri al:
    //   IClientEntityList → "VClientEntityList003" (client.dll)
    //   IVEngineClient    → "VEngineClient014"     (engine.dll)
    //   IBaseClientDLL    → "VClient017"           (client.dll)
    // Amalgam kaynak: Amalgam/src/SDK/Definitions/Interfaces/*.h

    for (int retry = 0; retry < 100; ++retry)
    {
        g_pEntityList = reinterpret_cast<IClientEntityList*>(
            GetInterface("client.dll", "VClientEntityList003"));
        g_pEngine = reinterpret_cast<IVEngineClient*>(
            GetInterface("engine.dll", "VEngineClient014"));

        if (g_pEntityList && g_pEngine)
        {
            // NetVar offsetlerini dinamik olarak çöz.
            // IBaseClientDLL (VClient017) üzerinden RecvTable ağacını yürü.
            // Amalgam: Amalgam/src/Utils/NetVars/NetVars.cpp
            void* pClientDLL = GetInterface("client.dll", "VClient017");
            if (pClientDLL)
            {
                g_Offsets.Init(pClientDLL);
                if (g_Offsets.ready)
                {
                    g_bInited = true;
                    return true;
                }
            }
        }
        Sleep(200);
    }
    return false;
}

// ─── ESP::GetDevice ──────────────────────────────────────────────────────────

IDirect3DDevice9* ESP::GetDevice()
{
    return g_pDevice;
}

// ─── ESP::Draw ───────────────────────────────────────────────────────────────

void ESP::Draw(IDirect3DDevice9* pDevice)
{
    if (!g_pDevice) g_pDevice = pDevice;

    if (!bEnabled || !g_bInited)
        return;

    if (!g_pEngine->IsInGame() || !g_pEngine->IsConnected())
        return;

    int localIndex = g_pEngine->GetLocalPlayer();

    int screenW, screenH;
    g_pEngine->GetScreenSize(screenW, screenH);

    const VMatrix& mtx = g_pEngine->WorldToScreenMatrix();

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    IClientEntity* pLocal     = g_pEntityList->GetClientEntity(localIndex);
    int            localTeam  = pLocal ? pLocal->GetTeam() : 0;

    int maxIdx = g_pEntityList->GetHighestEntityIndex();
    int limit  = (maxIdx < 64) ? maxIdx : 64; // TF2: en fazla 64 oyuncu

    for (int i = 1; i <= limit; ++i)
    {
        if (i == localIndex) continue;

        IClientEntity* pEnt = g_pEntityList->GetClientEntity(i);
        if (!pEnt)    continue;
        if (!pEnt->IsAlive()) continue;

        int team = pEnt->GetTeam();

        // Sadece TF2 takımları: BLU = 3, RED = 2
        if (team < 2 || team > 3) continue;

        // Düşman filtresi
        if (bEnemyOnly && team == localTeam) continue;

        Vec3  origin = pEnt->GetOrigin();
        Box2D box    = GetPlayerBox(mtx, origin, screenW, screenH);

        if (!box.valid) continue;

        float bx = box.left;
        float by = box.top;
        float bw = box.right  - box.left;
        float bh = box.bottom - box.top;

        if (bw < 4.f || bh < 4.f) continue; // çok küçük / çok uzak

        // ── Bounding box (cyan) ──
        DrawBox(dl, bx, by, bw, bh, IM_COL32(0, 255, 255, 255));

        // ── Sağlık çubuğu ──
        if (bShowHealth)
        {
            int hp    = pEnt->GetHealth();
            int maxHp = static_cast<int>(MAX_HP);
            if (hp > 0)
                DrawHealthBar(dl, bx, by, bh, hp, maxHp);
        }
    }
}

// ─── ESP::Cleanup ────────────────────────────────────────────────────────────

void ESP::Cleanup()
{
    g_pEntityList = nullptr;
    g_pEngine     = nullptr;
    g_pDevice     = nullptr;
    g_bInited     = false;
    g_Offsets     = {};
}

