#include "hooks.h"
#include "esp.h"
#include "menu.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx9.h>
#include <imgui/imgui_impl_win32.h>

// ─── Globals ─────────────────────────────────────────────────────────────────

static bool             g_bHookInstalled  = false;
static bool             g_bImGuiInited    = false;
static HWND             g_hWindow         = nullptr;
static WNDPROC          g_OrigWndProc     = nullptr;

// Orijinal EndScene fonksiyon işaretçisi (vtable'dan alınan)
using EndScene_t = HRESULT(__stdcall*)(IDirect3DDevice9*);
static EndScene_t       g_pOrigEndScene   = nullptr;

// vtable'daki EndScene slotu (index 42)
constexpr int ENDSCENE_VTABLE_INDEX = 42;

// ─── WndProc Hook ────────────────────────────────────────────────────────────

// ImGui'nin klavye/fare mesajlarını alabilmesi için WndProc hook'u
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // ImGui'ye mesajı ilet; eğer tüketildiyse oyuna gönderme
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
            return TRUE; // oyuna iletme
        default:
            break;
        }
    }

    return CallWindowProcA(g_OrigWndProc, hWnd, uMsg, wParam, lParam);
}

// ─── Hooked EndScene ─────────────────────────────────────────────────────────

static HRESULT __stdcall HookedEndScene(IDirect3DDevice9* pDevice)
{
    // ImGui'yi bir kez başlat
    if (!g_bImGuiInited)
    {
        // Pencere tanıtıcısını al
        D3DDEVICE_CREATION_PARAMETERS params{};
        pDevice->GetCreationParameters(&params);
        g_hWindow = params.hFocusWindow;

        // ImGui context ve backend
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        ImGui_ImplWin32_Init(g_hWindow);
        ImGui_ImplDX9_Init(pDevice);

        // Stil
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding    = 4.f;
        style.FrameRounding     = 3.f;
        style.ScrollbarRounding = 3.f;
        style.GrabRounding      = 3.f;
        style.WindowBorderSize  = 1.f;

        // WndProc'u kancala
        g_OrigWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(g_hWindow, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(HookedWndProc)));

        g_bImGuiInited = true;
    }

    // ── Frame başlat ──
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ── DEL tuşu → menüyü aç/kapat ──
    if (GetAsyncKeyState(VK_DELETE) & 1)
        Menu::Toggle();

    // ── ESP çiz ──
    ESP::Draw(pDevice);

    // ── Menü çiz ──
    Menu::Render();

    // ── Frame bitir ──
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return g_pOrigEndScene(pDevice);
}

// ─── Hook kurma / kaldırma ───────────────────────────────────────────────────

bool Hooks::Install(IDirect3DDevice9* pDevice)
{
    if (g_bHookInstalled || !pDevice)
        return false;

    // vtable'ı oku → slot 42 = EndScene
    void** vtable = *reinterpret_cast<void***>(pDevice);

    // Sayfayı yazılabilir yap
    DWORD oldProtect;
    if (!VirtualProtect(&vtable[ENDSCENE_VTABLE_INDEX],
                        sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    // Orijinali sakla, yerine bizimkini yaz
    g_pOrigEndScene = reinterpret_cast<EndScene_t>(vtable[ENDSCENE_VTABLE_INDEX]);
    vtable[ENDSCENE_VTABLE_INDEX] = reinterpret_cast<void*>(HookedEndScene);

    // Korumayı geri al
    VirtualProtect(&vtable[ENDSCENE_VTABLE_INDEX],
                   sizeof(void*), oldProtect, &oldProtect);

    g_bHookInstalled = true;
    return true;
}

void Hooks::Remove()
{
    if (!g_bHookInstalled || !g_pOrigEndScene)
        return;

    // NOT: Bu noktada pDevice pointer'ına ihtiyaç var.
    // Basit tutmak adına ESP::GetDevice() üzerinden alıyoruz.
    IDirect3DDevice9* pDevice = ESP::GetDevice();
    if (pDevice)
    {
        void** vtable = *reinterpret_cast<void***>(pDevice);

        DWORD oldProtect;
        VirtualProtect(&vtable[ENDSCENE_VTABLE_INDEX],
                       sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);

        vtable[ENDSCENE_VTABLE_INDEX] = reinterpret_cast<void*>(g_pOrigEndScene);

        VirtualProtect(&vtable[ENDSCENE_VTABLE_INDEX],
                       sizeof(void*), oldProtect, &oldProtect);
    }

    // ImGui temizle
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

    g_pOrigEndScene  = nullptr;
    g_bHookInstalled = false;
}

bool Hooks::IsInstalled()
{
    return g_bHookInstalled;
}
