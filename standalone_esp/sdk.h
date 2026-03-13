#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// TF2 x64 SDK — Amalgam-v2 kaynaklarından alınan vtable indeksleri ve yapılar
//
// TF2 Mayıs 2023 itibarıyla 64-bit'e geçti. Tüm pointer'lar 8 bayt,
// calling convention x64 standartı (thiscall yok; ilk argüman RCX = this).
// ─────────────────────────────────────────────────────────────────────────────

// ─── Temel tipler ─────────────────────────────────────────────────────────────

struct Vec3
{
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s)        const { return { x * s,   y * s,   z * s   }; }
    float Length()   const { return sqrtf(x*x + y*y + z*z); }
    float Length2D() const { return sqrtf(x*x + y*y); }
};

// Kaynak motorunun dünya→ekran dönüşümü için kullandığı 4×4 matris
struct VMatrix { float m[4][4]; };

// ─── Vtable yardımcı makrosu ─────────────────────────────────────────────────
// x64'te this pointer her zaman ilk argüman olarak geçer (RCX).
// __thiscall yoktur; aşağıdaki helper sanal fonksiyonları doğru şekilde çağırır.

template<typename Ret, int Index, typename... Args>
Ret CallVirtual(void* pThis, Args... args)
{
    using Fn = Ret(*)(void*, Args...);
    return (*reinterpret_cast<Fn**>(pThis))[Index](pThis, args...);
}

// ─── NetVar / RecvProp yapıları ───────────────────────────────────────────────
// Amalgam-v2 / Source SDK'dan birebir alınan bellek düzeni (x64).
// Amalgam kaynak: Amalgam/src/SDK/Definitions/Misc/dt_recv.h

class  RecvTable;

// RecvProp x64 bellek düzeni (Amalgam/src/SDK/Definitions/Misc/dt_recv.h)
struct RecvProp
{
    const char* m_pVarName;           // +0   (8)
    int         m_RecvType;           // +8   (4)
    int         m_Flags;              // +12  (4)
    int         m_StringBufferSize;   // +16  (4)
    bool        m_bInsideArray;       // +20  (1) + 3 pad
    uint8_t     _pad0[3];
    const void* m_pExtraData;         // +24  (8)
    RecvProp*   m_pArrayProp;         // +32  (8)
    void*       m_ArrayLengthProxy;   // +40  (8)
    void*       m_ProxyFn;            // +48  (8)
    void*       m_DataTableProxyFn;   // +56  (8)
    RecvTable*  m_pDataTable;         // +64  (8)
    int         m_Offset;             // +72  (4)
    int         m_ElementStride;      // +76  (4)
    int         m_nElements;          // +80  (4)
    // +84 padding / m_pParentArrayPropName pointer - kullanmıyoruz
};

// RecvTable x64 bellek düzeni
struct RecvTable
{
    RecvProp*   m_pProps;             // +0   (8)
    int         m_nProps;             // +8   (4)
    int         _pad0;                // +12  (4)
    void*       m_pDecoder;           // +16  (8)
    const char* m_pNetTableName;      // +24  (8)
    bool        m_bInitialized;       // +32  (1)
    bool        m_bInMainList;        // +33  (1)
};

// ClientClass bağlantılı listesi — IBaseClientDLL::GetAllClasses() döndürür.
// Amalgam: Amalgam/src/SDK/Definitions/Misc/ClientClass.h
struct ClientClass
{
    void*        m_pCreateFn;         // +0   (8)
    void*        m_pCreateEventFn;    // +8   (8)
    const char*  m_pNetworkName;      // +16  (8)
    RecvTable*   m_pRecvTable;        // +24  (8)
    ClientClass* m_pNext;             // +32  (8)
    int          m_ClassID;           // +40  (4)
};

// ─── Lightweight NetVar okuyucu ───────────────────────────────────────────────
// IBaseClientDLL::GetAllClasses() (vtable[8]) → RecvTable zincirini yürüyerek
// "CBasePlayer.m_iHealth" gibi netvar'ların dinamik offsetlerini bulur.
// Amalgam kaynak: Amalgam/src/Utils/NetVars/NetVars.cpp

namespace NetVars
{
    // Verilen RecvTable içinde szProp'u özyinelemeli arar; bulunursa offset döndürür.
    inline int GetOffset(RecvTable* pTable, const char* szProp)
    {
        if (!pTable) return 0;
        for (int i = 0; i < pTable->m_nProps; ++i)
        {
            RecvProp* p = &pTable->m_pProps[i];
            if (!p->m_pVarName) continue;
            if (strcmp(p->m_pVarName, szProp) == 0)
                return p->m_Offset;
            if (p->m_pDataTable)
            {
                int sub = GetOffset(p->m_pDataTable, szProp);
                if (sub) return p->m_Offset + sub;
            }
        }
        return 0;
    }

    // szClass sınıfının szProp netvar offsetini döndürür (pClientDLL üzerinden).
    // pClientDLL: "VClient017" interface pointer (IBaseClientDLL).
    inline int GetNetVar(void* pClientDLL, const char* szClass, const char* szProp)
    {
        // vtable[8] = GetAllClasses() → ClientClass* döndürür
        using GetAllClassesFn = ClientClass*(*)(void*);
        auto head = (*reinterpret_cast<GetAllClassesFn**>(pClientDLL))[8](pClientDLL);
        for (ClientClass* c = head; c; c = c->m_pNext)
        {
            if (c->m_pNetworkName && strcmp(c->m_pNetworkName, szClass) == 0)
                return GetOffset(c->m_pRecvTable, szProp);
        }
        return 0;
    }
} // namespace NetVars

// ─── IClientEntity — dinamik netvar erişimi ───────────────────────────────────
// Offsetler çalışma zamanında NetVars::GetNetVar() ile doldurulur.
// Ham bellek okuma yöntemi kullanılır (Amalgam NETVAR makrosu ile aynı mantık).

struct NetVarOffsets
{
    int m_iTeamNum  = 0;   // CBaseEntity.m_iTeamNum
    int m_iHealth   = 0;   // CBasePlayer.m_iHealth
    int m_lifeState = 0;   // CBasePlayer.m_lifeState
    int m_vecOrigin = 0;   // CBaseEntity.m_vecOrigin
    bool ready      = false;

    void Init(void* pClientDLL)
    {
        m_iTeamNum  = NetVars::GetNetVar(pClientDLL, "CBaseEntity", "m_iTeamNum");
        m_iHealth   = NetVars::GetNetVar(pClientDLL, "CBasePlayer",  "m_iHealth");
        m_lifeState = NetVars::GetNetVar(pClientDLL, "CBasePlayer",  "m_lifeState");
        m_vecOrigin = NetVars::GetNetVar(pClientDLL, "CBaseEntity",  "m_vecOrigin");
        ready = (m_iTeamNum && m_iHealth && m_lifeState && m_vecOrigin);
    }
};

// Global offset deposu (esp.cpp tarafından doldurulur, esp.h üzerinden paylaşılır)
inline NetVarOffsets g_Offsets;

class IClientEntity
{
public:
    template<typename T>
    const T& Read(int offset) const
    {
        return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(this) + offset);
    }

    int     GetTeam()      const { return Read<int>(g_Offsets.m_iTeamNum);    }
    int     GetHealth()    const { return Read<int>(g_Offsets.m_iHealth);     }
    uint8_t GetLifeState() const { return Read<uint8_t>(g_Offsets.m_lifeState); }
    Vec3    GetOrigin()    const { return Read<Vec3>(g_Offsets.m_vecOrigin);  }
    // LIFE_ALIVE = 0 (Amalgam/src/SDK/Definitions/Definitions.h satır 385)
    bool    IsAlive()      const { return GetLifeState() == 0; }
};

// ─── IClientEntityList ────────────────────────────────────────────────────────
// Interface: "VClientEntityList003" (client.dll)
// Amalgam kaynak: Amalgam/src/SDK/Definitions/Interfaces/IClientEntityList.h
//
// vtable sırası:
//   [0] GetClientNetworkable
//   [1] GetClientNetworkableFromHandle
//   [2] GetClientUnknownFromHandle
//   [3] GetClientEntity          ← kullanıyoruz
//   [4] GetClientEntityFromHandle
//   [5] NumberOfEntities
//   [6] GetHighestEntityIndex    ← kullanıyoruz
//   [7] SetMaxEntities
//   [8] GetMaxEntities

class IClientEntityList
{
public:
    IClientEntity* GetClientEntity(int index)
    {
        return CallVirtual<IClientEntity*, 3>(this, index);
    }
    int GetHighestEntityIndex()
    {
        return CallVirtual<int, 6>(this);
    }
};

// ─── IVEngineClient ───────────────────────────────────────────────────────────
// Interface: "VEngineClient014" (engine.dll)
// Amalgam kaynak: Amalgam/src/SDK/Definitions/Interfaces/IVEngineClient.h
//
// vtable indeksleri (Amalgam'ın IVEngineClient.h'ından sayarak):
//   [5]  GetScreenSize
//   [12] GetLocalPlayer
//   [26] IsInGame
//   [27] IsConnected
//   [36] WorldToScreenMatrix

class IVEngineClient
{
public:
    void GetScreenSize(int& w, int& h)
    {
        CallVirtual<void, 5>(this, std::ref(w), std::ref(h));
    }
    int GetLocalPlayer()
    {
        return CallVirtual<int, 12>(this);
    }
    bool IsInGame()
    {
        return CallVirtual<bool, 26>(this);
    }
    bool IsConnected()
    {
        return CallVirtual<bool, 27>(this);
    }
    const VMatrix& WorldToScreenMatrix()
    {
        return CallVirtual<const VMatrix&, 36>(this);
    }
};

// ─── Interface factory ────────────────────────────────────────────────────────

inline void* GetInterface(const char* moduleName, const char* interfaceName)
{
    HMODULE hMod = GetModuleHandleA(moduleName);
    if (!hMod) return nullptr;

    using CreateInterfaceFn = void*(*)(const char*, int*);
    auto pFn = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hMod, "CreateInterface"));
    if (!pFn) return nullptr;

    return pFn(interfaceName, nullptr);
}

// ─── World → Screen projeksiyon ───────────────────────────────────────────────
// Amalgam/src/SDK/SDK.cpp — W2S mantığı ile aynı matris formülü.

inline bool WorldToScreen(const VMatrix& mtx, const Vec3& world,
                          int screenW, int screenH,
                          float& sx, float& sy)
{
    // row 3 = W bileşeni
    float flW = mtx.m[3][0] * world.x
              + mtx.m[3][1] * world.y
              + mtx.m[3][2] * world.z
              + mtx.m[3][3];

    if (flW < 0.001f) return false; // kamera arkası

    float invW = 1.f / fabsf(flW);

    float x = mtx.m[0][0] * world.x
            + mtx.m[0][1] * world.y
            + mtx.m[0][2] * world.z
            + mtx.m[0][3];

    float y = mtx.m[1][0] * world.x
            + mtx.m[1][1] * world.y
            + mtx.m[1][2] * world.z
            + mtx.m[1][3];

    sx = (screenW / 2.f) + x * invW * (screenW / 2.f) + 0.5f;
    sy = (screenH / 2.f) - y * invW * (screenH / 2.f) + 0.5f;
    return true;
}

// ─── 2D bounding box ─────────────────────────────────────────────────────────
// Oyuncunun kök konumundan 8-köşe projeksiyon (Amalgam'ın IsOnScreen mantığı).

struct Box2D
{
    float left, top, right, bottom;
    bool  valid = false;
};

inline Box2D GetPlayerBox(const VMatrix& mtx, const Vec3& origin,
                          int screenW, int screenH)
{
    // TF2 varsayılan oyuncu boyutu: yükseklik ~72 HU, en ~32 HU
    constexpr float halfW  = 16.f;
    constexpr float height = 72.f;

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

    Box2D box { 1e9f, 1e9f, -1e9f, -1e9f, false };

    for (auto& c : corners)
    {
        float sx, sy;
        if (!WorldToScreen(mtx, c, screenW, screenH, sx, sy))
            return box; // kamera arkasındaki köşe varsa kutuyu atla

        if (sx < box.left)   box.left   = sx;
        if (sy < box.top)    box.top    = sy;
        if (sx > box.right)  box.right  = sx;
        if (sy > box.bottom) box.bottom = sy;
    }

    box.valid = true;
    return box;
}
