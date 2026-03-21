#ifndef TEXTMODE
#include "ExternalOverlay.h"
#include "Render.h"

#include <dwmapi.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui.h>
#pragma comment(lib, "dwmapi.lib")

// Window class name for the overlay
static constexpr const char* OVERLAY_CLASS_NAME = "AmalgamExternalOverlay";

// ─── Window creation ────────────────────────────────────────────────────────

bool CExternalOverlay::CreateOverlayWindow(HWND hGameWindow)
{
	WNDCLASSEX wc     = {};
	wc.cbSize         = sizeof(WNDCLASSEX);
	wc.style          = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc    = DefWindowProc;
	wc.hInstance      = GetModuleHandle(nullptr);
	wc.lpszClassName  = OVERLAY_CLASS_NAME;

	if (!RegisterClassEx(&wc))
	{
		// ERROR_CLASS_ALREADY_EXISTS is benign (e.g. DLL reload); all other
		// errors are unexpected and should prevent window creation.
		if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			return false;
	}

	// Obtain the initial game window rect
	RECT rc = {};
	GetWindowRect(hGameWindow, &rc);
	m_nWidth  = rc.right  - rc.left;
	m_nHeight = rc.bottom - rc.top;

	// Create a borderless, transparent, always-on-top window
	m_hWindow = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
		OVERLAY_CLASS_NAME,
		nullptr,
		WS_POPUP,
		rc.left, rc.top, m_nWidth, m_nHeight,
		nullptr, nullptr, GetModuleHandle(nullptr), nullptr
	);

	if (!m_hWindow)
		return false;

	// LWA_ALPHA=255 keeps the window fully opaque at the OS level; real per-pixel
	// transparency is handled by DWM below.
	SetLayeredWindowAttributes(m_hWindow, 0, 255, LWA_ALPHA);

	// Extend DWM glass into the entire client area so that D3D alpha values are
	// composited properly against the desktop (true ARGB transparency).
	MARGINS margins = { -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(m_hWindow, &margins);

	// ── Streamproofing ──────────────────────────────────────────────────────
	// WDA_EXCLUDEFROMCAPTURE prevents OBS, Discord and any other screen-capture
	// tool from picking up this window even during monitor-wide capture.
	// Requires Windows 10 20H2 (build 19041) or later.
	SetWindowDisplayAffinity(m_hWindow, WDA_EXCLUDEFROMCAPTURE);

	ShowWindow(m_hWindow, SW_SHOWNOACTIVATE);
	UpdateWindow(m_hWindow);

	return true;
}

// ─── D3D9 device ────────────────────────────────────────────────────────────

bool CExternalOverlay::CreateDevice(IDirect3D9* pD3D, HWND hFocusWindow, int nWidth, int nHeight)
{
	// Re-use the IDirect3D9 object the game device was created from so we
	// never need to call Direct3DCreate9 (avoids linking against d3d9.lib).
	m_pD3D = pD3D;
	m_pD3D->AddRef(); // we own a reference; released in DestroyDevice()

	D3DPRESENT_PARAMETERS pp  = {};
	pp.Windowed               = TRUE;
	pp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferFormat       = D3DFMT_A8R8G8B8; // ARGB for true alpha transparency
	pp.BackBufferWidth        = static_cast<UINT>(nWidth  > 0 ? nWidth  : 1);
	pp.BackBufferHeight       = static_cast<UINT>(nHeight > 0 ? nHeight : 1);
	pp.EnableAutoDepthStencil = FALSE;
	pp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;
	pp.hDeviceWindow          = m_hWindow;

	// Use the game window as the D3D9 focus window.  The overlay window has
	// WS_EX_NOACTIVATE | WS_EX_TRANSPARENT which makes it unsuitable as a
	// focus window on some drivers.
	//
	// D3DCREATE_NOWINDOWCHANGES: prevents D3D9 from changing the window's
	//   style or monitor assignment — important when CreateDevice is called
	//   from inside a Present hook.
	//
	// Try hardware VP first (performance); fall back to software VP so the
	// device creation succeeds even on adapters that refuse a second
	// hardware-VP device.
	static const DWORD s_aBehaviors[] = {
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_NOWINDOWCHANGES,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_NOWINDOWCHANGES,
	};

	HRESULT hr = E_FAIL;
	for (DWORD dwBehavior : s_aBehaviors)
	{
		hr = m_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
		                          hFocusWindow, dwBehavior, &pp, &m_pDevice);
		if (SUCCEEDED(hr))
			break;
	}

	if (FAILED(hr))
	{
		m_pD3D->Release();
		m_pD3D = nullptr;
		return false;
	}

	return true;
}

void CExternalOverlay::DestroyDevice()
{
	if (m_pDevice)
	{
		m_pDevice->Release();
		m_pDevice = nullptr;
	}
	if (m_pD3D)
	{
		m_pD3D->Release();
		m_pD3D = nullptr;
	}
}

void CExternalOverlay::ResetDevice(int nWidth, int nHeight)
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	D3DPRESENT_PARAMETERS pp  = {};
	pp.Windowed               = TRUE;
	pp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferFormat       = D3DFMT_A8R8G8B8;
	pp.BackBufferWidth        = static_cast<UINT>(nWidth);
	pp.BackBufferHeight       = static_cast<UINT>(nHeight);
	pp.EnableAutoDepthStencil = FALSE;
	pp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;
	pp.hDeviceWindow          = m_hWindow;

	if (SUCCEEDED(m_pDevice->Reset(&pp)))
		ImGui_ImplDX9_CreateDeviceObjects();
}

// ─── Public API ─────────────────────────────────────────────────────────────

void CExternalOverlay::Initialize(HWND hGameWindow, IDirect3DDevice9* pGameDevice)
{
	if (m_bReady)
		return;

	if (!CreateOverlayWindow(hGameWindow))
		return;

	// Derive IDirect3D9* from the existing game device – no Direct3DCreate9 call.
	IDirect3D9* pD3D = nullptr;
	if (FAILED(pGameDevice->GetDirect3D(&pD3D)) || !pD3D)
	{
		::DestroyWindow(m_hWindow);
		m_hWindow = nullptr;
		return;
	}

	if (!CreateDevice(pD3D, hGameWindow, m_nWidth, m_nHeight))
	{
		pD3D->Release(); // Release our temporary reference; CreateDevice owns its own reference via AddRef
		::DestroyWindow(m_hWindow);
		m_hWindow = nullptr;
		return;
	}

	// pD3D reference is now owned by m_pD3D (AddRef'd inside CreateDevice).
	pD3D->Release();

	// ImGui Win32 backend: use the *game* window so that GetForegroundWindow()
	// focus checks and ScreenToClient coordinate transforms are relative to the
	// actual game surface.  The DX9 backend uses the overlay device so all
	// draw calls target the streamproof overlay window instead.
	F::Render.Initialize(hGameWindow, m_pDevice);

	m_bReady = true;
}

void CExternalOverlay::Render(HWND hGameWindow)
{
	if (!m_bReady || !m_pDevice || !m_hWindow)
		return;

	// Keep the overlay window snapped to the game window every frame
	RECT rc = {};
	GetWindowRect(hGameWindow, &rc);
	int nNewWidth  = rc.right  - rc.left;
	int nNewHeight = rc.bottom - rc.top;

	UINT uFlags = SWP_NOACTIVATE | SWP_NOSENDCHANGING;
	if (nNewWidth == m_nWidth && nNewHeight == m_nHeight)
		uFlags |= SWP_NOSIZE; // avoid a costly resize when nothing changed

	SetWindowPos(m_hWindow, HWND_TOPMOST,
		rc.left, rc.top, nNewWidth, nNewHeight, uFlags);

	if (nNewWidth != m_nWidth || nNewHeight != m_nHeight)
	{
		m_nWidth  = nNewWidth;
		m_nHeight = nNewHeight;
		ResetDevice(m_nWidth, m_nHeight);
		// Fall through -- still present a valid cleared frame this tick
	}

	// Clear to fully transparent so only ImGui widgets are visible
	m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET,
		D3DCOLOR_ARGB(0, 0, 0, 0), 1.f, 0);

	if (SUCCEEDED(m_pDevice->BeginScene()))
	{
		F::Render.Render(m_pDevice);
		m_pDevice->EndScene();
	}

	m_pDevice->Present(nullptr, nullptr, nullptr, nullptr);
}

void CExternalOverlay::OnGameReset(HWND hGameWindow)
{
	// The game device was reset (resolution change / alt-tab).
	// Re-snap our overlay window; if the size changed our Render() call above
	// will handle the overlay device reset on the next frame.
	if (!m_bReady || !m_hWindow)
		return;

	RECT rc = {};
	GetWindowRect(hGameWindow, &rc);
	SetWindowPos(m_hWindow, HWND_TOPMOST,
		rc.left, rc.top,
		rc.right - rc.left, rc.bottom - rc.top,
		SWP_NOACTIVATE | SWP_NOSENDCHANGING);
}

void CExternalOverlay::Unload()
{
	if (!m_bReady)
		return;

	// Tear down ImGui backends before releasing the D3D9 device
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	DestroyDevice();

	if (m_hWindow)
	{
		::DestroyWindow(m_hWindow);
		m_hWindow = nullptr;
	}

	UnregisterClass(OVERLAY_CLASS_NAME, GetModuleHandle(nullptr));

	m_bReady  = false;
	m_nWidth  = 0;
	m_nHeight = 0;
}
#endif
