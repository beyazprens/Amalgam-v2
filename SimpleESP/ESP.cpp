// ============================================================
//  ESP.cpp  –  Bounding box (cyan) + vertical health bar (left)
//
//  Visual style (matches reference image):
//    • Cyan (#00FFFF) outlined rectangle around the player
//    • Thin vertical bar on the LEFT of the box
//      - Width : 4 px
//      - Height: same as bounding box
//      - Color : green (full HP) → red (0 HP)
//      - Black 1-px background border
// ============================================================

#include "ESP.h"
#include "imgui.h"

// ============================================================
//  Netvar walker
// ============================================================

static int FindNetVarOffset(RecvTable* table, const char* name)
{
    for (int i = 0; i < table->m_nProps; i++)
    {
        RecvProp& p = table->m_pProps[i];
        if (p.m_pVarName && !strcmp(p.m_pVarName, name))
            return p.m_Offset;

        if (p.m_RecvType == DPT_DataTable && p.m_pDataTable)
        {
            int sub = FindNetVarOffset(p.m_pDataTable, name);
            if (sub) return p.m_Offset + sub;
        }
    }
    return 0;
}

// Walks IBaseClientDLL client-classes to find a named netvar offset.
// pBaseClient  : pointer to IBaseClientDLL (interface "VClient017" from client.dll)
// GetAllClasses() is at vtable index 8.
static int GetNetVar(void* pBaseClient, const char* className, const char* varName)
{
    for (auto cc = Vfunc<ClientClass*>(pBaseClient, 8); cc; cc = cc->m_pNext)
    {
        if (!cc->m_pNetworkName) continue;
        if (!strcmp(cc->m_pNetworkName, className))
            return FindNetVarOffset(cc->m_pRecvTable, varName);
    }
    return 0;
}

// ============================================================
//  CESP::Init
// ============================================================

bool CESP::Init()
{
    m_pEntityList = GetInterface("client.dll", "VClientEntityList003");
    m_pEngine     = GetInterface("engine.dll", "VEngineClient014");
    if (!m_pEntityList || !m_pEngine) return false;

    void* pBaseClient = GetInterface("client.dll", "VClient017");
    if (!pBaseClient) return false;

    // Resolve netvars at startup so there is zero overhead per frame
    m_offTeamNum   = GetNetVar(pBaseClient, "CBaseEntity", "m_iTeamNum");
    m_offHealth    = GetNetVar(pBaseClient, "CBasePlayer",  "m_iHealth");
    m_offLifeState = GetNetVar(pBaseClient, "CBasePlayer",  "m_lifeState");
    m_offClass     = GetNetVar(pBaseClient, "CTFPlayer",    "m_iClass");

    return (m_offTeamNum != 0 && m_offHealth != 0);
}

// ============================================================
//  WorldToScreen
//  Uses IVEngineClient::WorldToScreenMatrix() (vtable index 36)
//  and IVEngineClient::GetScreenSize()        (vtable index 5)
// ============================================================

bool CESP::WorldToScreen(const Vec3& world, Vec2& out)
{
    // Refresh screen dimensions (cheap call, returns two ints)
    using GetScrFn = void(__fastcall*)(void*, int*, int*);
    auto vt = *reinterpret_cast<void***>(m_pEngine);
    reinterpret_cast<GetScrFn>(vt[5])(m_pEngine, &m_screenW, &m_screenH);

    // Get the world-to-screen matrix
    using W2SMatFn = const VMatrix*(__fastcall*)(void*);
    const VMatrix* mat = reinterpret_cast<W2SMatFn>(vt[36])(m_pEngine);
    if (!mat) return false;

    float w = mat->m[3][0] * world.x + mat->m[3][1] * world.y
            + mat->m[3][2] * world.z + mat->m[3][3];
    if (w < 0.001f) return false;

    float x = mat->m[0][0] * world.x + mat->m[0][1] * world.y
            + mat->m[0][2] * world.z + mat->m[0][3];
    float y = mat->m[1][0] * world.x + mat->m[1][1] * world.y
            + mat->m[1][2] * world.z + mat->m[1][3];

    float invW = 1.f / w;
    out.x = (m_screenW * 0.5f) + (m_screenW * 0.5f) * x * invW;
    out.y = (m_screenH * 0.5f) - (m_screenH * 0.5f) * y * invW;
    return true;
}

// ============================================================
//  GetPlayerBounds
//  Projects feet (origin) and head (origin + 72 HU) to screen.
//  Outputs the top-left corner (bx, by), width (bw), height (bh).
// ============================================================

bool CESP::GetPlayerBounds(const Vec3& origin,
                            float& bx, float& by, float& bw, float& bh)
{
    Vec3 headPos = { origin.x, origin.y, origin.z + TF_PLAYER_HEIGHT };

    Vec2 feet, head;
    if (!WorldToScreen(origin,  feet)) return false;
    if (!WorldToScreen(headPos, head)) return false;

    bh = feet.y - head.y;
    if (bh < 8.f) return false;  // too small or partially off-screen

    // Approximate player width at this distance
    bw = bh * 0.45f;
    bx = (head.x + feet.x) * 0.5f - bw * 0.5f;
    by = head.y;
    return true;
}

// ============================================================
//  DrawBox  –  cyan outlined rectangle with black shadow
// ============================================================

void CESP::DrawBox(void* drawListRaw,
                   float x, float y, float w, float h, unsigned int color)
{
    auto* dl = static_cast<ImDrawList*>(drawListRaw);

    // 1-px black outline for contrast
    dl->AddRect(ImVec2(x - 1.f, y - 1.f),
                ImVec2(x + w + 1.f, y + h + 1.f),
                IM_COL32(0, 0, 0, 200), 0.f, 0, 2.5f);

    // Colored box
    dl->AddRect(ImVec2(x, y),
                ImVec2(x + w, y + h),
                color, 0.f, 0, 1.5f);
}

// ============================================================
//  DrawHealthBar
//  Vertical bar on the LEFT of the bounding box.
//  Fills from bottom → top proportional to current health.
//  Colour: green (full) ──► red (empty).
// ============================================================

void CESP::DrawHealthBar(void* drawListRaw,
                          float bx, float by, float bh,
                          int hp, int maxHp)
{
    if (maxHp <= 0) maxHp = 125;
    float ratio = static_cast<float>(hp) / static_cast<float>(maxHp);
    if (ratio < 0.f) ratio = 0.f;
    if (ratio > 1.f) ratio = 1.f;

    constexpr float BAR_W = 4.f;   // bar width in pixels
    constexpr float GAP   = 3.f;   // gap between bar and box edge

    const float x    = bx - GAP - BAR_W;
    const float y    = by;
    const float h    = bh;
    const float fill = h * ratio;

    auto* dl = static_cast<ImDrawList*>(drawListRaw);

    // Dark background (slightly larger than the bar itself)
    dl->AddRectFilled(ImVec2(x - 1.f, y - 1.f),
                      ImVec2(x + BAR_W + 1.f, y + h + 1.f),
                      IM_COL32(0, 0, 0, 180));

    // Coloured fill – grows upward from the bottom
    if (fill > 0.f)
    {
        float fillTop = y + h - fill;
        // Green → Red colour interpolation
        auto r = static_cast<uint8_t>(255.f * (1.f - ratio));
        auto g = static_cast<uint8_t>(255.f * ratio);
        dl->AddRectFilled(ImVec2(x, fillTop),
                          ImVec2(x + BAR_W, y + h),
                          IM_COL32(r, g, 0, 255));
    }
}

// ============================================================
//  CESP::Render  –  called every Present frame
// ============================================================

void CESP::Render()
{
    if (!m_bEnabled)        return;
    if (!m_pEntityList)     return;
    if (!m_pEngine)         return;

    // IVEngineClient::IsInGame() – vtable index 26
    if (!Vfunc<bool>(m_pEngine, 26)) return;

    // IVEngineClient::GetLocalPlayer() – vtable index 12
    int localIndex = Vfunc<int>(m_pEngine, 12);

    // IClientEntityList::GetHighestEntityIndex() – vtable index 6
    int maxEnt = Vfunc<int>(m_pEntityList, 6);

    // Get local player's team so we only draw enemies
    int localTeam = TF_TEAM_RED; // fallback
    {
        // IClientEntityList::GetClientEntity(i) – vtable index 3
        void* pLocal = Vfunc<void*>(m_pEntityList, 3, localIndex);
        if (pLocal && m_offTeamNum)
            localTeam = Field<int>(pLocal, static_cast<uintptr_t>(m_offTeamNum));
    }

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    for (int i = 1; i <= maxEnt; i++)
    {
        if (i == localIndex) continue;

        // --- Get the IClientNetworkable pointer ---
        // IClientEntityList::GetClientNetworkable(i) – vtable index 0
        // Returns IClientNetworkable* which points to the networkable sub-object.
        void* pNet = Vfunc<void*>(m_pEntityList, 0, i);
        if (!pNet) continue;

        // IClientNetworkable::IsDormant() – vtable index 8
        if (Vfunc<bool>(pNet, 8)) continue;

        // IClientNetworkable::GetClientClass() – vtable index 2
        auto* pCC = Vfunc<ClientClass*>(pNet, 2);
        if (!pCC || pCC->m_ClassID != CLASSID_CTFPLAYER) continue;

        // --- Get the entity pointer for field access ---
        // IClientEntityList::GetClientEntity(i) – vtable index 3
        // Returns IClientEntity* == start of the entity object.
        void* pEnt = Vfunc<void*>(m_pEntityList, 3, i);
        if (!pEnt) continue;

        // Team check  –  skip teammates, draw enemies only
        int team = Field<int>(pEnt, static_cast<uintptr_t>(m_offTeamNum));
        if (team == localTeam) continue;
        if (team != TF_TEAM_RED && team != TF_TEAM_BLUE) continue;

        // Life state (0 = alive)
        if (m_offLifeState)
        {
            uint8_t lifeState = Field<uint8_t>(pEnt, static_cast<uintptr_t>(m_offLifeState));
            if (lifeState != 0) continue;
        }

        // Health
        int health = Field<int>(pEnt, static_cast<uintptr_t>(m_offHealth));
        if (health <= 0) continue;

        // Class → max health
        int cls   = m_offClass ? Field<int>(pEnt, static_cast<uintptr_t>(m_offClass)) : 0;
        int maxHp = GetMaxHealth(cls);

        // --- Origin via GetAbsOrigin() ---
        // IClientEntity primary vtable (IClientUnknown chain) index layout:
        //   0  ~IHandleEntity (dtor)
        //   1  SetRefEHandle
        //   2  GetRefEHandle
        //   3  GetCollideable        (IClientUnknown)
        //   4  GetClientNetworkable
        //   5  GetClientRenderable
        //   6  GetIClientEntity
        //   7  GetBaseEntity
        //   8  GetClientThinkable
        //   9  Release               (IClientEntity own)
        //   10 GetAbsOrigin          <──
        //   11 GetAbsAngles
        using GetAbsOriginFn = const Vec3*(__fastcall*)(void*);
        void** vt = *reinterpret_cast<void***>(pEnt);
        const Vec3* pOrigin = reinterpret_cast<GetAbsOriginFn>(vt[10])(pEnt);
        if (!pOrigin) continue;

        // --- Project and draw ---
        float bx, by, bw, bh;
        if (!GetPlayerBounds(*pOrigin, bx, by, bw, bh)) continue;

        // Cyan bounding box
        DrawBox(dl, bx, by, bw, bh, IM_COL32(0, 255, 255, 255));

        // Green → red health bar (left side)
        DrawHealthBar(dl, bx, by, bh, health, maxHp);
    }
}

// Global instance
CESP g_ESP;
