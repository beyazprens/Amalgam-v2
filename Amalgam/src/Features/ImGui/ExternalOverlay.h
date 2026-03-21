#pragma once
#ifndef TEXTMODE
#include "../../SDK/SDK.h"

// WDA_EXCLUDEFROMCAPTURE: Hides the window from screen capture tools (OBS, Discord, etc.)
// Available on Windows 10 20H2 and later (build 19041+)
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

class CExternalOverlay
{
public:
	// Initialize the overlay window and D3D9 device; called once on first Present
	void Initialize(HWND hGameWindow, IDirect3DDevice9* pGameDevice);

	// Update overlay position to match the game window and render ImGui
	void Render(HWND hGameWindow);

	// Release overlay resources
	void Unload();

	// Called when the game device is reset so we can resize our overlay device
	void OnGameReset(HWND hGameWindow);

	HWND              m_hWindow   = nullptr;
	IDirect3DDevice9* m_pDevice   = nullptr;
	IDirect3D9*       m_pD3D      = nullptr;
	bool              m_bReady    = false;

private:
	bool CreateOverlayWindow(HWND hGameWindow);
	bool CreateDevice(int nWidth, int nHeight);
	void DestroyDevice();
	void ResetDevice(int nWidth, int nHeight);

	int  m_nWidth  = 0;
	int  m_nHeight = 0;
};

ADD_FEATURE(CExternalOverlay, ExternalOverlay);
#endif
