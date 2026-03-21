#ifndef TEXTMODE
#include "Render.h"

#include "../../Hooks/Direct3DDevice9.h"
#include "../../Features/Configs/Configs.h"
#include <ImGui/imgui_impl_win32.h>
#include "Fonts/MaterialDesign/MaterialIcons.h"
#include "Fonts/MaterialDesign/IconDefinitions.h"
#include "Fonts/CascadiaMono/CascadiaMono.h"
#include "Fonts/Roboto/RobotoMedium.h"
#include "Fonts/Roboto/RobotoBlack.h"
#include "Menu/Menu.h"
#include <dwmapi.h>
#pragma comment(lib, "d3d9.lib")

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

static constexpr const char* OVERLAY_CLASS = "AmalGamStreamproofOverlay";

void CRender::Render(IDirect3DDevice9* pDevice)
{
	using namespace ImGui;

	m_pDevice = pDevice;

	static std::once_flag initFlag;
	std::call_once(initFlag, [&]
		{
			Initialize(pDevice);
		});

	LoadColors();
	{
		static float flStaticScale = Vars::Menu::Scale.Value;
		float flOldScale = flStaticScale;
		float flNewScale = flStaticScale = Vars::Menu::Scale.Value;
		if (flNewScale != flOldScale)
		{
			LoadFonts();
			LoadStyle();
		}
	}

	if (m_bOverlayInit)
	{
		// Keep the overlay window in sync with the game window and apply streamproof affinity
		UpdateOverlay();

		// Render ImGui on the transparent overlay device
		m_pOverlayDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
		m_pOverlayDevice->BeginScene();
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		NewFrame();

		F::Menu.Render();

		EndFrame();
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(GetDrawData());
		m_pOverlayDevice->EndScene();
		m_pOverlayDevice->Present(nullptr, nullptr, nullptr, nullptr);
	}
	else
	{
		// Fallback: render directly on the game's D3D9 device (no streamproof)
		DWORD dwOldRGB; pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &dwOldRGB);
		pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, false);
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		NewFrame();

		F::Menu.Render();

		EndFrame();
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(GetDrawData());
		pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, dwOldRGB);
	}
}

void CRender::InitOverlay(IDirect3DDevice9* pGameDevice)
{
	RECT gameRect = {};
	if (!GetWindowRect(WndProc::hwWindow, &gameRect))
		return;

	int width  = gameRect.right  - gameRect.left;
	int height = gameRect.bottom - gameRect.top;
	if (width <= 0 || height <= 0)
		return;

	// Register a dedicated window class for the overlay
	WNDCLASSEXA wc   = {};
	wc.cbSize        = sizeof(wc);
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = DefWindowProcA;
	wc.hInstance     = GetModuleHandleA(nullptr);
	wc.lpszClassName = OVERLAY_CLASS;
	if (!RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		return;

	m_hOverlayWindow = CreateWindowExA(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
		OVERLAY_CLASS,
		nullptr,
		WS_POPUP,
		gameRect.left, gameRect.top,
		width, height,
		nullptr, nullptr,
		GetModuleHandleA(nullptr),
		nullptr);

	if (!m_hOverlayWindow)
		return;

	// Enable per-pixel alpha via Desktop Window Manager composition
	MARGINS margins = { -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(m_hOverlayWindow, &margins);

	ShowWindow(m_hOverlayWindow, SW_SHOW);
	UpdateWindow(m_hOverlayWindow);

	// Create a D3D9Ex instance (required for alpha-composited swap chains)
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3DEx)))
	{
		DestroyWindow(m_hOverlayWindow);
		m_hOverlayWindow = nullptr;
		return;
	}

	D3DPRESENT_PARAMETERS pp   = {};
	pp.Windowed                = TRUE;
	pp.SwapEffect              = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferFormat        = D3DFMT_A8R8G8B8;
	pp.BackBufferCount         = 1;
	pp.BackBufferWidth         = static_cast<UINT>(width);
	pp.BackBufferHeight        = static_cast<UINT>(height);
	pp.hDeviceWindow           = m_hOverlayWindow;
	pp.EnableAutoDepthStencil  = FALSE;
	pp.PresentationInterval    = D3DPRESENT_INTERVAL_IMMEDIATE;

	if (FAILED(m_pD3DEx->CreateDeviceEx(
		D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hOverlayWindow,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
		&pp, nullptr, &m_pOverlayDevice)))
	{
		m_pD3DEx->Release();
		m_pD3DEx = nullptr;
		DestroyWindow(m_hOverlayWindow);
		m_hOverlayWindow = nullptr;
		return;
	}

	// Point the ImGui DX9 backend at the overlay device instead of the game device
	ImGui_ImplDX9_Init(m_pOverlayDevice);

	// Apply the current streamproof setting immediately
	SetWindowDisplayAffinity(m_hOverlayWindow,
		Vars::Menu::Streamproof.Value ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);

	m_bOverlayInit = true;
}

void CRender::UpdateOverlay()
{
	if (!m_hOverlayWindow || !m_pOverlayDevice)
		return;

	// Toggle screen-capture exclusion whenever the setting changes.
	// Use a sentinel value so the affinity is always applied at least once on the first call,
	// ensuring consistency regardless of the value at initialisation time.
	static int iLastStreamproof = -1;
	const bool bCurrent = Vars::Menu::Streamproof.Value;
	if (static_cast<int>(bCurrent) != iLastStreamproof)
	{
		iLastStreamproof = static_cast<int>(bCurrent);
		SetWindowDisplayAffinity(m_hOverlayWindow,
			bCurrent ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
	}

	// Keep the overlay window in sync with the game window position / size
	RECT gameRect = {};
	if (!GetWindowRect(WndProc::hwWindow, &gameRect))
		return;

	const int width  = gameRect.right  - gameRect.left;
	const int height = gameRect.bottom - gameRect.top;
	if (width <= 0 || height <= 0)
		return;

	RECT overlayRect = {};
	GetWindowRect(m_hOverlayWindow, &overlayRect);
	const int overlayW = overlayRect.right  - overlayRect.left;
	const int overlayH = overlayRect.bottom - overlayRect.top;

	if (overlayRect.left != gameRect.left || overlayRect.top != gameRect.top
		|| overlayW != width || overlayH != height)
	{
		SetWindowPos(m_hOverlayWindow, HWND_TOPMOST,
			gameRect.left, gameRect.top, width, height,
			SWP_NOACTIVATE);

		if (overlayW != width || overlayH != height)
			ResetOverlayDevice(width, height);
	}
}

void CRender::ResetOverlayDevice(int width, int height)
{
	if (!m_pOverlayDevice)
		return;

	ImGui_ImplDX9_InvalidateDeviceObjects();

	D3DPRESENT_PARAMETERS pp   = {};
	pp.Windowed                = TRUE;
	pp.SwapEffect              = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferFormat        = D3DFMT_A8R8G8B8;
	pp.BackBufferCount         = 1;
	pp.BackBufferWidth         = static_cast<UINT>(width);
	pp.BackBufferHeight        = static_cast<UINT>(height);
	pp.hDeviceWindow           = m_hOverlayWindow;
	pp.EnableAutoDepthStencil  = FALSE;
	pp.PresentationInterval    = D3DPRESENT_INTERVAL_IMMEDIATE;

	if (SUCCEEDED(m_pOverlayDevice->ResetEx(&pp, nullptr)))
		ImGui_ImplDX9_CreateDeviceObjects();
}

void CRender::LoadColors()
{
	using namespace ImGui;

	auto ColorToVec = [](Color_t tColor) -> ImColor
		{
			return { tColor.r / 255.f, tColor.g / 255.f, tColor.b / 255.f, tColor.a / 255.f };
		};

	Accent = ColorToVec(Vars::Menu::Theme::Accent.Value);
	Background0 = ColorToVec(Vars::Menu::Theme::Background.Value);
	Background0p5 = ColorToVec(Vars::Menu::Theme::Background.Value.Lerp({ 127, 127, 127 }, 0.5f / 9, LerpEnum::NoAlpha));
	Background1 = ColorToVec(Vars::Menu::Theme::Background.Value.Lerp({ 127, 127, 127 }, 1.f / 9, LerpEnum::NoAlpha));
	Background1p5 = ColorToVec(Vars::Menu::Theme::Background.Value.Lerp({ 127, 127, 127 }, 1.5f / 9, LerpEnum::NoAlpha));
	Background1p5L = { Background1p5.Value.x * 1.1f, Background1p5.Value.y * 1.1f, Background1p5.Value.z * 1.1f, Background1p5.Value.w };
	Background2 = ColorToVec(Vars::Menu::Theme::Background.Value.Lerp({ 127, 127, 127 }, 2.f / 9, LerpEnum::NoAlpha));
	Inactive = ColorToVec(Vars::Menu::Theme::Inactive.Value);
	Active = ColorToVec(Vars::Menu::Theme::Active.Value);

	ImVec4* colors = GetStyle().Colors;
	colors[ImGuiCol_Border] = Background2;
	colors[ImGuiCol_Button] = {};
	colors[ImGuiCol_ButtonHovered] = {};
	colors[ImGuiCol_ButtonActive] = {};
	colors[ImGuiCol_FrameBg] = Background1p5;
	colors[ImGuiCol_FrameBgHovered] = Background1p5L;
	colors[ImGuiCol_FrameBgActive] = Background1p5;
	colors[ImGuiCol_Header] = {};
	colors[ImGuiCol_HeaderHovered] = { Background1p5L.Value.x * 1.1f, Background1p5L.Value.y * 1.1f, Background1p5L.Value.z * 1.1f, Background1p5.Value.w }; // divd by 1.1
	colors[ImGuiCol_HeaderActive] = Background1p5;
	colors[ImGuiCol_ModalWindowDimBg] = { Background0.Value.x, Background0.Value.y, Background0.Value.z, 0.4f };
	colors[ImGuiCol_PopupBg] = Background1p5L;
	colors[ImGuiCol_ResizeGrip] = {};
	colors[ImGuiCol_ResizeGripActive] = {};
	colors[ImGuiCol_ResizeGripHovered] = {};
	colors[ImGuiCol_ScrollbarBg] = {};
	colors[ImGuiCol_Text] = Active;
	colors[ImGuiCol_WindowBg] = {};
	colors[ImGuiCol_CheckMark] = Accent;
	colors[ImGuiCol_SliderGrab] = Accent;
	colors[ImGuiCol_SliderGrabActive] = { Accent.Value.x * 1.2f, Accent.Value.y * 1.2f, Accent.Value.z * 1.2f, Accent.Value.w };
}

void CRender::LoadFonts()
{
	static bool bHasLoaded = false;

	auto& io = ImGui::GetIO();
	if (bHasLoaded)
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
		io.Fonts->ClearFonts();
	}

	ImFontConfig fontConfig;
	fontConfig.OversampleH = 2;
	constexpr ImWchar fontRange[]{ 0x0020, 0x00FF, 0x0400, 0x044F, 0 }; // Basic Latin, Latin Supplement and Cyrillic
#ifndef AMALGAM_CUSTOM_FONTS
	FontSmall = io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\verdana.ttf)", H::Draw.Scale(11), &fontConfig, fontRange);
	FontRegular = io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\verdana.ttf)", H::Draw.Scale(13), &fontConfig, fontRange);
	FontBold = io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\verdanab.ttf)", H::Draw.Scale(13), &fontConfig, fontRange);
	FontLarge = io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\verdana.ttf)", H::Draw.Scale(14), &fontConfig, fontRange);
	FontMono = io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\cour.ttf)", H::Draw.Scale(16), &fontConfig, fontRange); // windows mono font installed by default
#else
	FontSmall = io.Fonts->AddFontFromMemoryCompressedTTF(RobotoMedium_compressed_data, RobotoMedium_compressed_size, H::Draw.Scale(12), &fontConfig, fontRange);
	FontRegular = io.Fonts->AddFontFromMemoryCompressedTTF(RobotoMedium_compressed_data, RobotoMedium_compressed_size, H::Draw.Scale(13), &fontConfig, fontRange);
	FontBold = io.Fonts->AddFontFromMemoryCompressedTTF(RobotoBlack_compressed_data, RobotoBlack_compressed_size, H::Draw.Scale(13), &fontConfig, fontRange);
	FontLarge = io.Fonts->AddFontFromMemoryCompressedTTF(RobotoMedium_compressed_data, RobotoMedium_compressed_size, H::Draw.Scale(15), &fontConfig, fontRange);
	FontMono = io.Fonts->AddFontFromMemoryCompressedTTF(CascadiaMono_compressed_data, CascadiaMono_compressed_size, H::Draw.Scale(15), &fontConfig, fontRange);
#endif

	ImFontConfig iconConfig;
	iconConfig.PixelSnapH = true;
	constexpr ImWchar iconRange[]{ short(ICON_MIN_MD), short(ICON_MAX_MD), 0 };
	IconFont = io.Fonts->AddFontFromMemoryCompressedTTF(MaterialIcons_compressed_data, MaterialIcons_compressed_size, H::Draw.Scale(16), &iconConfig, iconRange);

	io.Fonts->Build();
	io.ConfigDebugHighlightIdConflicts = false;

	bHasLoaded = true;
}

void CRender::LoadStyle()
{
	using namespace ImGui;

	auto& style = GetStyle();
	style.ButtonTextAlign = { 0.5f, 0.5f }; // Center button text
	style.CellPadding = { H::Draw.Scale(4), 0 };
	style.ChildBorderSize = 0.f;
	style.ChildRounding = H::Draw.Scale(4);
	style.FrameBorderSize = 0.f;
	style.FramePadding = { 0, 0 };
	style.FrameRounding = H::Draw.Scale(3);
	style.ItemInnerSpacing = { 0, 0 };
	style.ItemSpacing = { H::Draw.Scale(8), H::Draw.Scale(8) };
	style.PopupBorderSize = 0.f;
	style.PopupRounding = H::Draw.Scale(4);
	style.ScrollbarSize = 6.f + H::Draw.Scale(3);
	style.ScrollbarRounding = 0.f;
	style.WindowBorderSize = 0.f;
	style.WindowPadding = { 0, 0 };
	style.WindowRounding = H::Draw.Scale(4);
	style.GrabRounding = H::Draw.Scale(3);
}

void CRender::Initialize(IDirect3DDevice9* pDevice)
{
	// Initialize ImGui core and Win32 input backend
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(WndProc::hwWindow);

	// Try to create a transparent overlay window with its own D3D9Ex device.
	// If successful, ImGui will render there (enabling streamproof via WDA_EXCLUDEFROMCAPTURE).
	// On failure, fall back to rendering directly on the game's D3D9 device.
	InitOverlay(pDevice);
	if (!m_bOverlayInit)
		ImGui_ImplDX9_Init(pDevice);

	auto& io = ImGui::GetIO();
	static std::string sIniPath = F::Configs.m_sConfigPath + "imgui.ini";
	io.IniFilename = sIniPath.c_str();
	io.LogFilename = nullptr;

	LoadFonts();
	LoadStyle();
}
#endif 