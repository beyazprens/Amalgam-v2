#pragma once
#include <d3d9.h>
#include "sdk.h"

namespace ESP
{
    // Arayüzleri ve D3D9 cihazını başlatır.
    // main thread'den çağrılır; oyunun belleğe yüklenmesi beklenir.
    bool Initialize();

    // Her frame çağrılır (HookedEndScene içinden).
    void Draw(IDirect3DDevice9* pDevice);

    // Temizlik.
    void Cleanup();

    // Geçerli D3D9 cihazını döndürür (hook kaldırırken gerekli).
    IDirect3DDevice9* GetDevice();

    // ── Ayarlar (menu tarafından değiştirilir) ──
    inline bool bEnabled    = false;  // ESP açık mı?
    inline bool bEnemyOnly  = true;   // Sadece düşmanlar mı?
    inline bool bShowHealth = true;   // Sağlık çubuğu
    inline bool bShowName   = false;  // İsim (gelecek)
}
