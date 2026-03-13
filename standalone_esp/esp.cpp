#include "esp.h"
#include "sdk.h"

#include <imgui/imgui.h>
#include <Windows.h>

// ─── Statik arayüz pointer'ları ──────────────────────────────────────────────

static IClientEntityList* g_pEntityList = nullptr;
static IEngineClient*     g_pEngine     = nullptr;
static IDirect3DDevice9*  g_pDevice     = nullptr;
static bool               g_bInited     = false;

// ─── Renkler ─────────────────────────────────────────────────────────────────

// Düşman kutu rengi: cyan (0, 255, 255, 255)
static constexpr ImVec4 COLOR_ENEMY_BOX    { 0.f,  1.f,  1.f, 1.f };
// Sağlık çubuğu renkleri
static constexpr ImVec4 COLOR_HP_HIGH      { 0.f,  1.f,  0.f, 1.f }; // yeşil
static constexpr ImVec4 COLOR_HP_MED       { 1.f,  1.f,  0.f, 1.f }; // sarı
static constexpr ImVec4 COLOR_HP_LOW       { 1.f,  0.f,  0.f, 1.f }; // kırmızı
static constexpr ImVec4 COLOR_HP_BG        { 0.f,  0.f,  0.f, 0.6f};
static constexpr ImVec4 COLOR_OUTLINE      { 0.f,  0.f,  0.f, 1.f }; // siyah gölge

// TF2'de max HP sınıfına göre değişir; en yüksek = Heavy (300)
static constexpr float MAX_HP = 300.f;

// ─── Yardımcı: sağlık oranına göre renk ─────────────────────────────────────

static ImColor HealthColor(float ratio)
{
    if (ratio > 0.6f)
    {
        // yeşil → sarı
        float t = (ratio - 0.6f) / 0.4f;
        return ImColor(
            static_cast<int>((1.f - t) * 255),
            255,
            0,
            255
        );
    }
    // sarı → kırmızı
    float t = ratio / 0.6f;
    return ImColor(
        255,
        static_cast<int>(t * 255),
        0,
        255
    );
}

// ─── Yardımcı: tek piksellik kenarlıklı kutu çiz ─────────────────────────────

static void DrawBox(ImDrawList* dl,
                    float x, float y, float w, float h,
                    ImU32 colMain)
{
    // Gölge (1px offset, siyah)
    constexpr ImU32 shadow = IM_COL32(0, 0, 0, 200);
    dl->AddRect(ImVec2(x - 1, y - 1), ImVec2(x + w + 1, y + h + 1), shadow);
    dl->AddRect(ImVec2(x + 1, y + 1), ImVec2(x + w - 1, y + h - 1), shadow);

    // Ana kutu
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), colMain, 0.f, 0, 1.5f);
}

// ─── Yardımcı: sağlık çubuğu ─────────────────────────────────────────────────

static void DrawHealthBar(ImDrawList* dl,
                          float bx, float by, float bh,
                          int hp, int maxHp)
{
    constexpr float barW = 4.f;
    constexpr float gap  = 2.f;

    float ratio  = (float)hp / (float)maxHp;
    if (ratio < 0.f) ratio = 0.f;
    if (ratio > 1.f) ratio = 1.f;

    float barH = bh * ratio;

    float x  = bx - gap - barW;
    float yT = by;
    float yB = by + bh;

    // Arka plan
    dl->AddRectFilled(ImVec2(x, yT), ImVec2(x + barW, yB),
                      IM_COL32(0, 0, 0, 160));
    // Dolu kısım (aşağıdan yukarıya)
    ImColor hpCol = HealthColor(ratio);
    dl->AddRectFilled(ImVec2(x, yB - barH), ImVec2(x + barW, yB),
                      (ImU32)hpCol);
    // Kenarlık
    dl->AddRect(ImVec2(x, yT), ImVec2(x + barW, yB),
                IM_COL32(0, 0, 0, 200));
}

// ─── ESP::Initialize ─────────────────────────────────────────────────────────

bool ESP::Initialize()
{
    // Oyunun tam olarak yüklenmesini bekle
    for (int retry = 0; retry < 100; ++retry)
    {
        g_pEntityList = reinterpret_cast<IClientEntityList*>(
            GetInterface("client.dll", "VClientEntityList003"));
        g_pEngine = reinterpret_cast<IEngineClient*>(
            GetInterface("engine.dll", "VEngineClient014"));

        if (g_pEntityList && g_pEngine)
        {
            g_bInited = true;
            return true;
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
    // Cihazı sakla (hook kaldırmada lazım)
    if (!g_pDevice)
        g_pDevice = pDevice;

    if (!bEnabled || !g_bInited)
        return;

    if (!g_pEngine->IsInGame() || !g_pEngine->IsConnected())
        return;

    int localIndex = g_pEngine->GetLocalPlayer();

    int screenW, screenH;
    g_pEngine->GetScreenSize(screenW, screenH);

    const VMatrix& mtx = g_pEngine->WorldToScreenMatrix();

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl)
        return;

    // Düşman takım numarasını bulmak için local oyuncuyu oku
    IClientEntity* pLocal = g_pEntityList->GetClientEntity(localIndex);
    int localTeam = pLocal ? pLocal->GetTeam() : 0;

    int maxIdx = g_pEntityList->GetHighestEntityIndex();

    // TF2'de oyuncu indeksleri 1..64 arasında
    constexpr int MAX_PLAYERS = 64;
    int limit = (maxIdx < MAX_PLAYERS) ? maxIdx : MAX_PLAYERS;

    for (int i = 1; i <= limit; ++i)
    {
        if (i == localIndex)
            continue;

        IClientEntity* pEnt = g_pEntityList->GetClientEntity(i);
        if (!pEnt)
            continue;

        if (!pEnt->IsAlive())
            continue;

        int team = pEnt->GetTeam();

        // Takım filtresi
        if (bEnemyOnly && team == localTeam)
            continue;

        // Sadece geçerli takımları göster (BLU=3, RED=2)
        if (team < 2 || team > 3)
            continue;

        Vec3 origin = pEnt->GetOrigin();
        Box2D box   = GetPlayerBox(mtx, origin, screenW, screenH);

        if (!box.valid)
            continue;

        float bx = box.left;
        float by = box.top;
        float bw = box.right  - box.left;
        float bh = box.bottom - box.top;

        // Çok küçük kutuları atla (çok uzak oyuncular)
        if (bw < 4.f || bh < 4.f)
            continue;

        // ── Kutu ──
        DrawBox(dl, bx, by, bw, bh,
                IM_COL32(0, 255, 255, 255));  // cyan

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
}
