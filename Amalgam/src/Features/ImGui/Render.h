#pragma once
#include "../../SDK/SDK.h"
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui.h>

class CRender
{
public:
	void Render(IDirect3DDevice9* pDevice);
	void Initialize(IDirect3DDevice9* pDevice);

	void LoadColors();
	void LoadFonts();
	void LoadStyle();

	int Cursor = 2;

	// Colors
	ImColor Accent = {};
	ImColor Background0 = {};
	ImColor Background0p5 = {};
	ImColor Background1 = {};
	ImColor Background1p5 = {};
	ImColor Background1p5L = {};
	ImColor Background2 = {};
	ImColor Inactive = {};
	ImColor Active = {};

	// Fonts
	ImFont* FontSmall = nullptr;
	ImFont* FontRegular = nullptr;
	ImFont* FontBold = nullptr;
	ImFont* FontLarge = nullptr;
	ImFont* FontMono = nullptr;

	ImFont* IconFont = nullptr;

	IDirect3DDevice9* GetDevice() const { return m_pDevice; }

	// Streamproof overlay
	bool m_bOverlayInit = false;
	HWND m_hOverlayWindow = nullptr;
	IDirect3D9Ex* m_pD3DEx = nullptr;
	IDirect3DDevice9Ex* m_pOverlayDevice = nullptr;

private:
	IDirect3DDevice9* m_pDevice = nullptr;

	void InitOverlay(IDirect3DDevice9* pGameDevice);
	void UpdateOverlay();
	void ResetOverlayDevice(int width, int height);
};

ADD_FEATURE(CRender, Render);