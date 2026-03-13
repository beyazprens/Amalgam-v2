#pragma once
#include <d3d9.h>

namespace Hooks
{
    // D3D9 hook'u kurar (EndScene vtable hook).
    // pDevice geçerli bir IDirect3DDevice9* olmalı.
    bool Install(IDirect3DDevice9* pDevice);

    // Hook'u kaldırır, orijinal fonksiyonu geri yazar.
    void Remove();

    // Hook kurulu mu?
    bool IsInstalled();
}
