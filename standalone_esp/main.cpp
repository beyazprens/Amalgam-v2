#include <Windows.h>
#include "esp.h"
#include "hooks.h"

// ─── D3D9 cihazını yakalamak için sahte pencere ───────────────────────────────
//
// Hook kurmadan önce geçerli bir IDirect3DDevice9*'e ihtiyacımız var.
// Bunu elde etmenin en güvenli yolu: küçük bir sahte pencere açıp
// orada bir D3D9 cihazı oluşturmak, vtable'ı kopyalamak, sonra cihazı
// silmek.  Kopyaladığımız vtable pointer'ı oyunun gerçek cihazı ile
// aynı olacağı için hook oyunun cihazına da uygulanır.

static IDirect3DDevice9* CreateDummyDevice(HWND& hDummyWnd)
{
    hDummyWnd = CreateWindowExA(0, "STATIC", "dummy",
                                WS_OVERLAPPED, 0, 0, 1, 1,
                                nullptr, nullptr, nullptr, nullptr);
    if (!hDummyWnd)
        return nullptr;

    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D)
    {
        DestroyWindow(hDummyWnd);
        hDummyWnd = nullptr;
        return nullptr;
    }

    D3DPRESENT_PARAMETERS pp{};
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow    = hDummyWnd;
    pp.Windowed         = TRUE;
    pp.BackBufferWidth  = 1;
    pp.BackBufferHeight = 1;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;

    IDirect3DDevice9* pDevice = nullptr;
    pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                       hDummyWnd,
                       D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                       &pp, &pDevice);
    pD3D->Release();
    return pDevice;
}

// ─── Ana thread ─────────────────────────────────────────────────────────────

DWORD WINAPI MainThread(LPVOID lpParam)
{
    // 1) Oyunun tam yüklenmesini bekle
    while (!GetModuleHandleA("client.dll") ||
           !GetModuleHandleA("engine.dll"))
        Sleep(500);

    Sleep(1000); // biraz daha bekle

    // 2) ESP arayüzlerini başlat
    if (!ESP::Initialize())
    {
        FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 1);
        return 1;
    }

    // 3) Sahte D3D9 cihazı oluştur ve hook'u kur
    HWND hDummyWnd = nullptr;
    IDirect3DDevice9* pDummy = CreateDummyDevice(hDummyWnd);
    if (!pDummy)
    {
        FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 1);
        return 1;
    }

    Hooks::Install(pDummy);

    // Sahte cihazı serbest bırak (vtable hook zaten kuruldu)
    pDummy->Release();
    if (hDummyWnd)
        DestroyWindow(hDummyWnd);

    // 4) END tuşuna basılana kadar döngüde bekle
    while (!(GetAsyncKeyState(VK_END) & 1))
        Sleep(100);

    // 5) Temizle ve çık
    Hooks::Remove();
    ESP::Cleanup();

    FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 0);
    return 0;
}

// ─── DLL giriş noktası ───────────────────────────────────────────────────────

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpvReserved*/)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);

        HANDLE hThread = CreateThread(nullptr, 0, MainThread,
                                      hinstDLL, 0, nullptr);
        if (hThread)
            CloseHandle(hThread);
    }
    return TRUE;
}
