#pragma once
// ============================================================
//  ESP.h  –  Enemy bounding-box + health-bar ESP
// ============================================================

#include "SDK.h"

class CESP
{
public:
    // Call once after DLL inject to resolve interfaces and netvars
    bool Init();

    // Call every Present frame (passes device for future extensibility)
    void Render();

    // Toggled from the menu
    bool m_bEnabled = true;

private:
    // -- interfaces --
    void* m_pEntityList = nullptr;   // IClientEntityList
    void* m_pEngine     = nullptr;   // IVEngineClient

    // -- netvar offsets (resolved at Init time) --
    int m_offTeamNum   = 0;   // CBaseEntity::m_iTeamNum
    int m_offHealth    = 0;   // CBasePlayer::m_iHealth
    int m_offLifeState = 0;   // CBasePlayer::m_lifeState  (0 = alive)
    int m_offClass     = 0;   // CTFPlayer::m_iClass       (1–9)

    // -- screen size (refreshed each frame) --
    int m_screenW = 0;
    int m_screenH = 0;

    // -- helpers --
    bool WorldToScreen(const Vec3& world, Vec2& out);
    bool GetPlayerBounds(const Vec3& origin, float& bx, float& by, float& bw, float& bh);

    void DrawBox      (void* drawList, float x, float y, float w, float h, unsigned int color);
    void DrawHealthBar(void* drawList, float bx, float by, float bh, int hp, int maxHp);
};

extern CESP g_ESP;
