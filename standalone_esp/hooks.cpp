#include "hooks.h"
#include "esp.h"
#include "menu.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx9.h>
#include <imgui/imgui_impl_win32.h>

// ─── Globals ─────────────────────────────────────────────────────────────────

static bool    g_bHookInstalled = false;
static bool    g_bImGuiInited   = false;
static HWND    g_hWindow        = nullptr;
static WNDPROC g_OrigWndProc    = nullptr;

// D3D9 Present imzası — Amalgam kaynak: Amalgam/src/Hooks/Direct3DDevice9.cpp
//   MAKE_HOOK(Direct3DDevice9_Present, U::Memory.GetVirtual(I::DirectXDevice, 17), ...)
// IDirect3DDevice9 vtable düzeni (standart D3D9):
//   [0]  QueryInterface     [1]  AddRef       [2]  Release
//   [3]  TestCooperativeLevel [4] GetAvailableTextureMem ...
//   [13] CreateAdditionalSwapChain [14] GetSwapChain [15] GetNumberOfSwapChains
//   [16] Reset              [17] Present      ← burayı kullanıyoruz
//   [18] GetBackBuffer ...

using Present_t = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
static Present_t g_pOrigPresent = nullptr;

constexpr int PRESENT_VTABLE_INDEX = 17;

// ─── WndProc Hook ────────────────────────────────────────────────────────────

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return TRUE;

    // Menü açıkken fare ve klavye inputunu oyundan gizle
    if (Menu::IsOpen())
    {
        switch (uMsg)
        {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR:
            return TRUE;
        default:
            break;
        }
    }

    return CallWindowProcA(g_OrigWndProc, hWnd, uMsg, wParam, lParam);
}

// ─── Hooked Present ──────────────────────────────────────────────────────────

static HRESULT __stdcall HookedPresent(IDirect3DDevice9* pDevice,
                                        const RECT* pSrc, const RECT* pDst,
                                        HWND hDestWnd, const RGNDATA* pDirty)
{
    if (!g_bImGuiInited)
    {
        D3DDEVICE_CREATION_PARAMETERS params{};
        pDevice->GetCreationParameters(&params);
        g_hWindow = params.hFocusWindow;

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        ImGui_ImplWin32_Init(g_hWindow);
        ImGui_ImplDX9_Init(pDevice);

        ImGui::StyleColorsDark();
        ImGuiStyle& style  = ImGui::GetStyle();
        style.WindowRounding   = 4.f;
        style.FrameRounding    = 3.f;
        style.GrabRounding     = 3.f;
        style.WindowBorderSize = 1.f;

        g_OrigWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(g_hWindow, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(HookedWndProc)));

        g_bImGuiInited = true;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // DEL tuşu → menüyü aç/kapat
    if (GetAsyncKeyState(VK_DELETE) & 1)
        Menu::Toggle();

    ESP::Draw(pDevice);
    Menu::Render();

    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return g_pOrigPresent(pDevice, pSrc, pDst, hDestWnd, pDirty);
}

// ─── Hooks::Install ──────────────────────────────────────────────────────────

bool Hooks::Install(IDirect3DDevice9* pDevice)
{
    if (g_bHookInstalled || !pDevice)
        return false;

    void** vtable = *reinterpret_cast<void***>(pDevice);

    DWORD oldProtect;
    if (!VirtualProtect(&vtable[PRESENT_VTABLE_INDEX],
                        sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    g_pOrigPresent = reinterpret_cast<Present_t>(vtable[PRESENT_VTABLE_INDEX]);
    vtable[PRESENT_VTABLE_INDEX] = reinterpret_cast<void*>(HookedPresent);

    VirtualProtect(&vtable[PRESENT_VTABLE_INDEX],
                   sizeof(void*), oldProtect, &oldProtect);

    g_bHookInstalled = true;
    return true;
}

// ─── Hooks::Remove ───────────────────────────────────────────────────────────

void Hooks::Remove()
{
    if (!g_bHookInstalled || !g_pOrigPresent)
        return;

    IDirect3DDevice9* pDevice = ESP::GetDevice();
    if (pDevice)
    {
        void** vtable = *reinterpret_cast<void***>(pDevice);

        DWORD oldProtect;
        VirtualProtect(&vtable[PRESENT_VTABLE_INDEX],
                       sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        vtable[PRESENT_VTABLE_INDEX] = reinterpret_cast<void*>(g_pOrigPresent);
        VirtualProtect(&vtable[PRESENT_VTABLE_INDEX],
                       sizeof(void*), oldProtect, &oldProtect);
    }

    if (g_bImGuiInited)
    {
        if (g_hWindow && g_OrigWndProc)
            SetWindowLongPtrA(g_hWindow, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(g_OrigWndProc));

        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_bImGuiInited = false;
    }

    g_pOrigPresent   = nullptr;
    g_bHookInstalled = false;
}

bool Hooks::IsInstalled() { return g_bHookInstalled; }

