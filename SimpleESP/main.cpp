// ============================================================
//  main.cpp  –  DLL entry point, DirectX 9 VMT hook, ImGui init
//
//  HOW TO USE
//  ----------
//  1. Create a new Visual Studio "Dynamic-Link Library (DLL)" project (x64).
//  2. Add all files from this folder to the project.
//  3. Add Dear ImGui source files (imgui.cpp, imgui_draw.cpp,
//     imgui_tables.cpp, imgui_widgets.cpp, imgui_impl_dx9.cpp,
//     imgui_impl_win32.cpp) to the project.
//  4. Set Configuration Properties → Linker → Additional Dependencies:
//       d3d9.lib; psapi.lib
//  5. Build in Release x64.
//  6. Inject the resulting DLL into hl2.exe (TF2) with any injector.
//
//  CONTROLS
//  --------
//    DEL key  – toggle the ImGui overlay menu
//    Menu checkbox "Enemy ESP" – enable / disable the ESP
//
//  NOTES
//  -----
//  • Targets TF2 x64 (DirectX 9 mode, current Steam build).
//  • If the game is updated the netvar offsets are re-resolved
//    automatically at injection time via the live RecvTables.
//  • The DX9 device is found via a pattern scan in shaderapidx9.dll;
//    no dummy-window creation is required.
// ============================================================

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "psapi.lib")

#include "SDK.h"
#include "ESP.h"
#include "Menu.h"

// Dear ImGui headers (user adds source files to the project)
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

// Forward-declared by imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ============================================================
//  Globals
// ============================================================

static HMODULE  g_hModule  = nullptr;
static bool     g_bRunning = false;
static bool     g_bImgui   = false;

static HWND     g_hWnd     = nullptr;
static WNDPROC  g_oWndProc = nullptr;

// DX9 hook originals
using PresentFn = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using ResetFn   = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
static PresentFn g_oPresent = nullptr;
static ResetFn   g_oReset   = nullptr;

// ============================================================
//  WndProc hook  –  forward input to ImGui when menu is open
// ============================================================

static LRESULT WINAPI HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_bImgui && g_Menu.IsOpen())
    {
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

        // Block mouse messages from reaching the game while menu is open
        if (WM_MOUSEFIRST <= msg && msg <= WM_MOUSELAST)
            return 1;

        // Block all keyboard messages while menu is open so the game
        // does not process movement / action keys through the menu
        if (WM_KEYFIRST <= msg && msg <= WM_KEYLAST)
            return 1;
    }
    return CallWindowProcA(g_oWndProc, hWnd, msg, wParam, lParam);
}

// ============================================================
//  DX9 Reset hook  –  keep ImGui DX9 objects valid
// ============================================================

static HRESULT WINAPI HookedReset(IDirect3DDevice9* pDevice,
                                   D3DPRESENT_PARAMETERS* pParams)
{
    if (g_bImgui)
        ImGui_ImplDX9_InvalidateDeviceObjects();

    HRESULT hr = g_oReset(pDevice, pParams);

    if (g_bImgui && SUCCEEDED(hr))
        ImGui_ImplDX9_CreateDeviceObjects();

    return hr;
}

// ============================================================
//  DX9 Present hook  –  ImGui frame + ESP drawing
// ============================================================

static HRESULT WINAPI HookedPresent(IDirect3DDevice9* pDevice,
                                     const RECT* pSrc,
                                     const RECT* pDst,
                                     HWND        hWnd,
                                     const RGNDATA* pDirty)
{
    if (!g_bRunning)
        return g_oPresent(pDevice, pSrc, pDst, hWnd, pDirty);

    // ---- One-time ImGui initialisation ----
    if (!g_bImgui)
    {
        ImGui::CreateContext();

        ImGuiIO& io    = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.IniFilename  = nullptr;  // do not write imgui.ini

        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(g_hWnd);
        ImGui_ImplDX9_Init(pDevice);
        g_bImgui = true;
    }

    // ---- Begin frame ----
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ---- DEL key toggle ----
    static bool bDelPrev = false;
    bool bDelNow = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;
    if (bDelNow && !bDelPrev) g_Menu.Toggle();
    bDelPrev = bDelNow;

    // ---- Render ----
    g_ESP.Render();   // draws into ImGui background draw-list
    g_Menu.Render();  // renders menu window on top

    // ---- End frame ----
    ImGui::EndFrame();
    ImGui::Render();

    // Disable SRGB write so colours appear correctly
    DWORD dwOldSRGB = 0;
    pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &dwOldSRGB);
    pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, dwOldSRGB);

    return g_oPresent(pDevice, pSrc, pDst, hWnd, pDirty);
}

// ============================================================
//  VMT hook helper  –  patches one vtable slot
// ============================================================

static void VmtHook(void** pVtable, int idx, void* newFn, void** ppOriginal)
{
    DWORD oldProt = 0;
    VirtualProtect(&pVtable[idx], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
    if (ppOriginal) *ppOriginal = pVtable[idx];
    pVtable[idx] = newFn;
    VirtualProtect(&pVtable[idx], sizeof(void*), oldProt, &oldProt);
}

// ============================================================
//  FindDX9Device  –  locates the game's IDirect3DDevice9
//
//  Signature in shaderapidx9.dll:
//    48 8B 0D ? ? ? ? 48 8B 01 FF 50 ? 8B F8
//  Decodes as: mov rcx, [rip + disp32]
//  After the 7-byte instruction, the RIP-relative address
//  holds the pointer to the device.
// ============================================================

static IDirect3DDevice9* FindDX9Device()
{
    const char* kSig = "48 8B 0D ? ? ? ? 48 8B 01 FF 50 ? 8B F8";

    for (auto* dll : { "shaderapidx9.dll", "shaderapivk.dll" })
    {
        uintptr_t sig = PatternScan(dll, kSig);
        if (!sig) continue;

        int32_t disp = *reinterpret_cast<int32_t*>(sig + 3);
        auto ppDevice = reinterpret_cast<IDirect3DDevice9**>(sig + 7 + disp);
        if (ppDevice && *ppDevice) return *ppDevice;
    }
    return nullptr;
}

// ============================================================
//  Main thread  –  wait for modules, init, hook, loop
// ============================================================

static DWORD WINAPI MainThread(LPVOID)
{
    // Wait until the required modules are loaded
    while (!GetModuleHandleA("client.dll")  ||
           !GetModuleHandleA("engine.dll")  ||
           !GetModuleHandleA("shaderapidx9.dll"))
        Sleep(100);

    Sleep(500); // let modules finish self-initialisation

    // Find the TF2 main window ("Valve001" is the Source window class)
    g_hWnd = FindWindowA("Valve001", nullptr);
    if (!g_hWnd)
    {
        FreeLibraryAndExitThread(g_hModule, 0);
        return 0;
    }

    // Initialise ESP (resolves interfaces + netvars)
    if (!g_ESP.Init() || !g_Menu.Init())
    {
        FreeLibraryAndExitThread(g_hModule, 0);
        return 0;
    }

    // Wait for the DX9 device to be created
    IDirect3DDevice9* pDevice = nullptr;
    while (!(pDevice = FindDX9Device()))
        Sleep(100);

    // Hook WndProc so ImGui can receive input when the menu is open
    g_oWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(g_hWnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(HookedWndProc)));

    // Hook IDirect3DDevice9::Present (slot 17) and Reset (slot 16)
    void** vtable = *reinterpret_cast<void***>(pDevice);
    VmtHook(vtable, 17, HookedPresent, reinterpret_cast<void**>(&g_oPresent));
    VmtHook(vtable, 16, HookedReset,   reinterpret_cast<void**>(&g_oReset));

    g_bRunning = true;

    // Keep the thread alive; g_bRunning can be cleared for manual unload
    while (g_bRunning)
        Sleep(50);

    // ---- Cleanup ----

    // Restore WndProc
    SetWindowLongPtrA(g_hWnd, GWLP_WNDPROC,
                      reinterpret_cast<LONG_PTR>(g_oWndProc));

    // Restore DX9 vtable
    VmtHook(vtable, 17, reinterpret_cast<void*>(g_oPresent), nullptr);
    VmtHook(vtable, 16, reinterpret_cast<void*>(g_oReset),   nullptr);

    // Shutdown ImGui
    if (g_bImgui)
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_bImgui = false;
    }

    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}

// ============================================================
//  DllMain
// ============================================================

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hInstance);
        g_hModule = hInstance;

        HANDLE hThread = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
