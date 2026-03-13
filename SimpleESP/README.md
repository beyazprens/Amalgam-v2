# TF2 Standalone ESP DLL – Kaynak Kodlar

Bu klasördeki dosyalar, **Amalgam-v2 projesinden bağımsız** olarak kendi DLL projenizde kullanmanız için hazırlanmıştır.

---

## Dosyalar

| Dosya | Açıklama |
|-------|----------|
| `SDK.h` | Minimal TF2 x64 SDK: math, pattern scanner, interface factory, vtable helper |
| `main.cpp` | DllMain + DX9 Present/Reset hook + ImGui init + main thread |
| `ESP.h` / `ESP.cpp` | Cyan bounding box + sol tarafta dikey health bar |
| `Menu.h` / `Menu.cpp` | ImGui overlay menüsü (DEL tuşu) |

---

## Visual Studio Proje Kurulumu

1. **Yeni proje oluştur:** `Dynamic-Link Library (DLL)` → **x64** Release
2. Bu klasördeki **tüm `.h` ve `.cpp` dosyalarını** projeye ekle
3. [Dear ImGui](https://github.com/ocornut/imgui) kaynak dosyalarını projeye ekle:
   - `imgui.cpp`
   - `imgui_draw.cpp`
   - `imgui_tables.cpp`
   - `imgui_widgets.cpp`
   - `imgui_impl_dx9.cpp`
   - `imgui_impl_win32.cpp`
4. **Linker → Additional Dependencies** bölümüne ekle:
   ```
   d3d9.lib
   psapi.lib
   ```
5. **Derle** → `Release x64` → çıkan `.dll` dosyası inject edilmeye hazır

---

## Kullanım

| Eylem | Kısayol |
|-------|---------|
| Menüyü aç / kapat | `DEL` tuşu |
| ESP'yi aktifleştir / kapat | Menüdeki checkbox |

---

## ESP Görünümü

```
  ┌────────────────────┐
  │                    │
█ │                    │   ← Cyan (#00FFFF) bounding box
█ │     DÜŞMAN         │
█ │                    │
  └────────────────────┘
↑
Dikey health bar (sol tarafta)
Yeşil = dolu, Kırmızı = az can
```

- **Bounding box rengi:** Cyan (`#00FFFF`)  
- **Health bar:** sol tarafta, 4 px genişlik, kutu yüksekliği kadar  
- **Health bar rengi:** Yeşil (tam can) → Kırmızı (düşük can)  
- **Siyah arka plan:** bar kenarlarında 1 px siyah kenarlık

---

## Teknik Notlar

- **Hedef:** TF2 x64 (Steam, DirectX 9)
- Netvar offsetleri (`m_iTeamNum`, `m_iHealth` vb.) inject anında otomatik çözümlenir → güncelleme sonrası tekrar compile gerekmez
- DX9 cihazı `shaderapidx9.dll` içinde pattern scan ile bulunur
- Hook yöntemi: **VMT patch** (Present slot 17, Reset slot 16)
