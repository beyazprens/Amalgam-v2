#pragma once
// ============================================================
//  SDK.h  –  Minimal TF2 x64 SDK for standalone ESP DLL
//  Tested against: TF2 (DirectX 9, 64-bit, Steam)
//
//  Dependencies the user must add to the VS project:
//    - d3d9.lib / d3d9.h
//    - psapi.lib / psapi.h
//    - imgui.h, imgui_impl_dx9.h, imgui_impl_win32.h  (Dear ImGui)
// ============================================================

#include <Windows.h>
#include <d3d9.h>
#include <psapi.h>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>

// ============================================================
//  MATH
// ============================================================

struct Vec2
{
    float x = 0.f, y = 0.f;
    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}
};

struct Vec3
{
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    float Length() const { return sqrtf(x * x + y * y + z * z); }
};

// Row-major 4×4 projection matrix returned by IVEngineClient::WorldToScreenMatrix()
struct VMatrix { float m[4][4]; };

// ============================================================
//  TF2 CONSTANTS
// ============================================================

constexpr int TF_TEAM_RED  = 2;
constexpr int TF_TEAM_BLUE = 3;
constexpr int MAX_ENTITIES = 2048;

// Class ID for CTFPlayer inside the network table
constexpr int CLASSID_CTFPLAYER = 247;

// Standing collision height for all TF2 player classes (Hammer units)
constexpr float TF_PLAYER_HEIGHT = 72.f;

// Maximum health per class (index 1–9: Scout … Engineer)
inline int GetMaxHealth(int cls)
{
    static const int tbl[] = { 0, 125, 125, 200, 175, 150, 300, 175, 125, 125 };
    return (cls >= 1 && cls <= 9) ? tbl[cls] : 125;
}

// ============================================================
//  SOURCE ENGINE STRUCTS  (for netvar walking)
// ============================================================

typedef void (*RecvVarProxyFn)(const void*, void*, void*);
typedef void (*DataTableRecvVarProxyFn)(const void*, void**, void*, int);
typedef void (*ArrayLengthRecvProxyFn)(void*, int, int);

enum SendPropType : int
{
    DPT_Int = 0, DPT_Float, DPT_Vector, DPT_VectorXY,
    DPT_String, DPT_Array, DPT_DataTable, DPT_Int64
};

struct RecvTable;

struct RecvProp
{
    const char*              m_pVarName;
    SendPropType             m_RecvType;
    int                      m_Flags;
    int                      m_StringBufferSize;
    bool                     m_bInsideArray;
    const void*              m_pExtraData;
    RecvProp*                m_pArrayProp;
    ArrayLengthRecvProxyFn   m_ArrayLengthProxy;
    RecvVarProxyFn           m_ProxyFn;
    DataTableRecvVarProxyFn  m_DataTableProxyFn;
    RecvTable*               m_pDataTable;
    int                      m_Offset;
    int                      m_ElementStride;
    int                      m_nElements;
    const char*              m_pParentArrayPropName;
};

struct RecvTable
{
    RecvProp*   m_pProps;
    int         m_nProps;
    void*       m_pDecoder;
    const char* m_pNetTableName;
    bool        m_bInitialized;
    bool        m_bInMainList;
};

struct ClientClass
{
    void*        m_pCreateFn;
    void*        m_pCreateEventFn;
    const char*  m_pNetworkName;
    RecvTable*   m_pRecvTable;
    ClientClass* m_pNext;
    int          m_ClassID;
};

// ============================================================
//  INTERFACE FACTORY
// ============================================================

using CreateInterfaceFn_t = void* (__cdecl*)(const char*, int*);

inline void* GetInterface(const char* dll, const char* name)
{
    HMODULE hMod = GetModuleHandleA(dll);
    if (!hMod) return nullptr;
    auto fn = reinterpret_cast<CreateInterfaceFn_t>(GetProcAddress(hMod, "CreateInterface"));
    if (!fn) return nullptr;
    return fn(name, nullptr);
}

// ============================================================
//  VIRTUAL FUNCTION HELPER
//  Calls vtable[idx] as  Ret __fastcall (thisptr, args…)
// ============================================================

template<typename Ret = void, typename... Args>
inline Ret Vfunc(void* thisptr, int idx, Args... args)
{
    void** vt = *reinterpret_cast<void***>(thisptr);
    using Fn  = Ret(__fastcall*)(void*, Args...);
    return reinterpret_cast<Fn>(vt[idx])(thisptr, args...);
}

// Read a field T at (base + offset)
template<typename T>
inline T& Field(void* base, uintptr_t offset)
{
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + offset);
}

// ============================================================
//  PATTERN SCANNER
// ============================================================

struct PatternByte { uint8_t val; bool wildcard; };

inline std::vector<PatternByte> ParsePattern(const char* pat)
{
    std::vector<PatternByte> out;
    while (*pat)
    {
        if (*pat == ' ') { ++pat; continue; }
        if (*pat == '?')
        {
            out.push_back({ 0, true });
            ++pat;
            if (*pat == '?') ++pat;
        }
        else
        {
            char* end = nullptr;
            long val = strtol(pat, &end, 16);
            // Skip exactly 2 hex digit characters; if fewer were consumed, skip only what was parsed
            int consumed = static_cast<int>(end - pat);
            if (consumed < 1) { ++pat; continue; }  // skip invalid byte
            out.push_back({ static_cast<uint8_t>(val), false });
            pat += consumed;
        }
    }
    return out;
}

// Returns address of first match, or 0 if not found
inline uintptr_t PatternScan(const char* module, const char* pattern)
{
    HMODULE hMod = GetModuleHandleA(module);
    if (!hMod) return 0;
    MODULEINFO mi{};
    if (!GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi))) return 0;

    auto bytes = ParsePattern(pattern);
    auto base  = reinterpret_cast<uint8_t*>(mi.lpBaseOfDll);
    size_t size = mi.SizeOfImage, count = bytes.size();

    for (size_t i = 0; i + count <= size; ++i)
    {
        bool ok = true;
        for (size_t j = 0; j < count; ++j)
            if (!bytes[j].wildcard && base[i + j] != bytes[j].val) { ok = false; break; }
        if (ok) return reinterpret_cast<uintptr_t>(base + i);
    }
    return 0;
}
