#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cmath>
#include <string>

// ─── Temel tipler ───────────────────────────────────────────────────────────

struct Vec3
{
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s)        const { return { x * s,   y * s,   z * s   }; }
    float Length() const { return sqrtf(x * x + y * y + z * z); }
};

// Kaynak motorunun dünya→ekran için kullandığı 4×4 matris
struct VMatrix
{
    float m[4][4];
};

// ─── TF2 / Source Engine sabit offsetler (x64 Linux build) ─────────────────
// Not: Bu offsetler güncelleme ile değişebilir.
// client.dll bazlı, IClientEntity* vtable erişimi ile okunur.

namespace Offsets
{
    // CBaseEntity
    constexpr std::ptrdiff_t m_iTeamNum    = 0x0088;   // int
    constexpr std::ptrdiff_t m_iHealth     = 0x0096;   // int  (CBasePlayer)
    constexpr std::ptrdiff_t m_lifeState   = 0x025B;   // byte (0 = alive)
    constexpr std::ptrdiff_t m_vecOrigin   = 0x0138;   // Vec3 (m_vecAbsOrigin)
    // Oyuncu sınıfı için kullanılan ayrı bir yapı
    constexpr std::ptrdiff_t m_iClass      = 0x07A4;   // int (CTFPlayer::m_PlayerClass.m_iClass)
}

// ─── Temel Source arayüzleri ────────────────────────────────────────────────
// Sadece ihtiyacımız olan metodları tanımlıyoruz.

class IClientEntity
{
public:
    // Bunlar sanal tablo üzerinden çağrılır; offset numaraları TF2 x86'ya göredir.
    // vtable[0] = destructor, vtable[10] = GetIndex(), vb.
    // Doğrudan bellek okuması daha güvenilir olduğu için helper fonksiyonlar kullanıyoruz.

    template<typename T>
    T Read(std::ptrdiff_t offset) const
    {
        return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(this) + offset);
    }

    int   GetTeam()       const { return Read<int>(Offsets::m_iTeamNum); }
    int   GetHealth()     const { return Read<int>(Offsets::m_iHealth);  }
    uint8_t GetLifeState()const { return Read<uint8_t>(Offsets::m_lifeState); }
    Vec3  GetOrigin()     const { return Read<Vec3>(Offsets::m_vecOrigin); }
    bool  IsAlive()       const { return GetLifeState() == 0; }
};

// ─── IClientEntityList arayüzü ──────────────────────────────────────────────

class IClientEntityList
{
public:
    // vtable[3] = GetClientEntity(int index)
    IClientEntity* GetClientEntity(int index)
    {
        using fn = IClientEntity * (__thiscall*)(void*, int);
        return (*reinterpret_cast<fn**>(this))[3](this, index);
    }
    // vtable[8] = GetHighestEntityIndex()
    int GetHighestEntityIndex()
    {
        using fn = int(__thiscall*)(void*);
        return (*reinterpret_cast<fn**>(this))[8](this);
    }
};

// ─── IEngineClient arayüzü ──────────────────────────────────────────────────

class IEngineClient
{
public:
    // vtable[12] = GetLocalPlayer()
    int GetLocalPlayer()
    {
        using fn = int(__thiscall*)(void*);
        return (*reinterpret_cast<fn**>(this))[12](this);
    }
    // vtable[37] = WorldToScreenMatrix()  → VMatrix const&
    const VMatrix& WorldToScreenMatrix()
    {
        using fn = const VMatrix & (__thiscall*)(void*);
        return (*reinterpret_cast<fn**>(this))[37](this);
    }
    // vtable[5] = GetScreenSize(int& w, int& h)
    void GetScreenSize(int& w, int& h)
    {
        using fn = void(__thiscall*)(void*, int&, int&);
        (*reinterpret_cast<fn**>(this))[5](this, w, h);
    }
    // vtable[26] = IsInGame()
    bool IsInGame()
    {
        using fn = bool(__thiscall*)(void*);
        return (*reinterpret_cast<fn**>(this))[26](this);
    }
    // vtable[27] = IsConnected()
    bool IsConnected()
    {
        using fn = bool(__thiscall*)(void*);
        return (*reinterpret_cast<fn**>(this))[27](this);
    }
};

// ─── Interface factory yardımcısı ───────────────────────────────────────────

inline void* GetInterface(const char* moduleName, const char* interfaceName)
{
    HMODULE hMod = GetModuleHandleA(moduleName);
    if (!hMod)
        return nullptr;

    using CreateInterfaceFn = void* (*)(const char*, int*);
    auto CreateInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hMod, "CreateInterface"));
    if (!CreateInterface)
        return nullptr;

    return CreateInterface(interfaceName, nullptr);
}

// ─── World → Screen projeksiyon ─────────────────────────────────────────────

inline bool WorldToScreen(const VMatrix& matrix, const Vec3& worldPos,
                           int screenW, int screenH,
                           float& screenX, float& screenY)
{
    // Homogen koordinat dönüşümü
    float w = matrix.m[3][0] * worldPos.x
            + matrix.m[3][1] * worldPos.y
            + matrix.m[3][2] * worldPos.z
            + matrix.m[3][3];

    if (w < 0.001f)
        return false; // kamera arkasında

    float x = matrix.m[0][0] * worldPos.x
            + matrix.m[0][1] * worldPos.y
            + matrix.m[0][2] * worldPos.z
            + matrix.m[0][3];

    float y = matrix.m[1][0] * worldPos.x
            + matrix.m[1][1] * worldPos.y
            + matrix.m[1][2] * worldPos.z
            + matrix.m[1][3];

    float inv = 1.f / w;
    screenX = (screenW  / 2.f) + (x * inv) * (screenW  / 2.f);
    screenY = (screenH / 2.f)  - (y * inv) * (screenH / 2.f);
    return true;
}

// ─── Oyuncu etrafında 2D kutu hesaplama ─────────────────────────────────────
// Oyuncunun dünya konumundan yaklaşık bir bounding box üretir.

struct Box2D
{
    float left, top, right, bottom;
    bool valid = false;
};

inline Box2D GetPlayerBox(const VMatrix& mtx, const Vec3& origin,
                          int screenW, int screenH)
{
    // TF2 oyuncu yaklaşık yüksekliği ~72 HU (Hammer Unit), eni ~32 HU
    const float halfW = 16.f;
    const float height = 72.f;

    // 8 köşe noktası
    Vec3 corners[8] = {
        { origin.x - halfW, origin.y - halfW, origin.z          },
        { origin.x + halfW, origin.y - halfW, origin.z          },
        { origin.x + halfW, origin.y + halfW, origin.z          },
        { origin.x - halfW, origin.y + halfW, origin.z          },
        { origin.x - halfW, origin.y - halfW, origin.z + height },
        { origin.x + halfW, origin.y - halfW, origin.z + height },
        { origin.x + halfW, origin.y + halfW, origin.z + height },
        { origin.x - halfW, origin.y + halfW, origin.z + height },
    };

    Box2D box;
    box.left   =  1e9f;
    box.top    =  1e9f;
    box.right  = -1e9f;
    box.bottom = -1e9f;

    for (auto& c : corners)
    {
        float sx, sy;
        if (!WorldToScreen(mtx, c, screenW, screenH, sx, sy))
            return box; // ekran dışı / kamera arkası

        if (sx < box.left)   box.left   = sx;
        if (sy < box.top)    box.top    = sy;
        if (sx > box.right)  box.right  = sx;
        if (sy > box.bottom) box.bottom = sy;
    }

    box.valid = true;
    return box;
}
