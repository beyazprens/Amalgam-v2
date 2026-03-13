#pragma once
// ============================================================
//  Menu.h  –  ImGui overlay menu (toggled with DEL key)
// ============================================================

#include "SDK.h"

class CMenu
{
public:
    bool Init();
    void Toggle();
    void Render();
    bool IsOpen() const { return m_bOpen; }

private:
    bool m_bOpen = false;
};

extern CMenu g_Menu;
