# TF2 ESP — Standalone DLL (x64)

Bağımsız, inject edilebilir bir Team Fortress 2 ESP DLL projesi.

> **ÖNEMLİ:** TF2, Mayıs 2023 güncellemesiyle **64-bit** mimarisine geçti.
> Bu proje **x64** olarak derlenmelidir. x86 derlemesi çalışmaz.

## Dosya Yapısı

```
standalone_esp/
├── main.cpp      ← DllMain + MainThread (başlangıç noktası)
├── sdk.h         ← TF2 / Source Engine arayüzleri + dinamik NetVar okuyucu
├── hooks.h
├── hooks.cpp     ← IDirect3DDevice9 vtable[17] Present hook + ImGui init
├── esp.h
├── esp.cpp       ← ESP kutu çizim mantığı (cyan box + HP bar)
├── menu.h
└── menu.cpp      ← ImGui overlay menüsü
```

## Amalgam-v2'den Alınan Bilgiler

| Bileşen | Kaynak |
|---------|--------|
| `Present` vtable index = **17** | `Amalgam/src/Hooks/Direct3DDevice9.cpp` |
| `IClientEntityList::GetClientEntity` index = **3** | `Amalgam/src/SDK/Definitions/Interfaces/IClientEntityList.h` |
| `IClientEntityList::GetHighestEntityIndex` index = **6** | aynı |
| `IVEngineClient::WorldToScreenMatrix` index = **36** | `Amalgam/src/SDK/Definitions/Interfaces/IVEngineClient.h` |
| `IVEngineClient::GetLocalPlayer` index = **12** | aynı |
| Interface isimleri: `VClient017`, `VClientEntityList003`, `VEngineClient014` | `Amalgam/src/SDK/Definitions/Interfaces/*.h` |
| NetVar offsetleri (m_iTeamNum, m_iHealth, m_lifeState, m_vecOrigin) | **Dinamik** — RecvTable zinciri yürünerek çözülür |
| RecvProp / RecvTable bellek düzeni | `Amalgam/src/SDK/Definitions/Misc/dt_recv.h` |

## Visual Studio Proje Kurulumu

### 1. Yeni Proje Oluştur

- **Proje tipi:** Dynamic-Link Library (DLL)
- **Platform: x64** — TF2 artık 64-bit
- **Yapılandırma:** Release

### 2. Dosyaları Ekle

Yukarıdaki tüm `.cpp` ve `.h` dosyalarını projeye ekle.

### 3. ImGui Ekle

Projeye şu ImGui dosyalarını ekle (kendi indirdiğin ImGui'den):

```
imgui.h             imgui.cpp
imgui_internal.h    imgui_draw.cpp
imconfig.h          imgui_tables.cpp
                    imgui_widgets.cpp
imgui_impl_dx9.h    imgui_impl_dx9.cpp
imgui_impl_win32.h  imgui_impl_win32.cpp
```

> **Include yolu ayarı:**
> Proje Özellikleri → C/C++ → General → Additional Include Directories
> Buraya ImGui klasörünün üst dizinini ekle; dosyalar `imgui/imgui.h` şeklinde dahil edilir.

### 4. Gerekli Kütüphaneler

Proje Özellikleri → Linker → Input → Additional Dependencies'e ekle:

```
d3d9.lib
```

### 5. Diğer Ayarlar

| Ayar | Değer |
|------|-------|
| **Platform** | **x64** |
| Runtime Library | Multi-threaded (/MT) |
| Character Set | Multi-Byte |
| Subsystem | Windows |

---

## Kullanım

| Tuş | Açıklama |
|-----|----------|
| **DEL** | Menüyü aç / kapat |
| **END** | DLL'i oyundan kaldır (unload) |

### Menü Seçenekleri

- **ESP Aktif** — ESP'yi aç/kapat
- **Sadece Düşman** — Yalnızca karşı takımı göster (BLU=3, RED=2)
- **Sağlık Çubuğu** — Oyuncunun solunda HP çubuğu

---

## ESP Görünümü

- **Kutu rengi:** Cyan `IM_COL32(0, 255, 255, 255)` — isteğe göre `esp.cpp` içinden değiştirilebilir
- **Sağlık çubuğu:** Solda dikey, yeşil→sarı→kırmızı gradyan
- **Outline:** Siyah gölge (okunabilirlik)

### Renk Değiştirme

`esp.cpp` içindeki satırı düzenle:

```cpp
DrawBox(dl, bx, by, bw, bh, IM_COL32(0,   255, 255, 255)); // cyan
DrawBox(dl, bx, by, bw, bh, IM_COL32(0,   255, 0,   255)); // yeşil
DrawBox(dl, bx, by, bw, bh, IM_COL32(255, 255, 0,   255)); // sarı
```

---

## Önemli Notlar

- **NetVar offsetleri dinamiktir** — `IBaseClientDLL::GetAllClasses()` üzerinden
  RecvTable ağacı yürünerek her başlatmada hesaplanır. Oyun güncellendiğinde
  hardcoded offset değiştirmek gerekmez.
- **vtable indeksleri** güncelleme ile nadiren değişir. Değişirse `sdk.h` içindeki
  `CallVirtual<..., INDEX>(...)` çağrılarını güncelle.
- Inject için Xenos, dll-injector vb. araçlar kullanabilirsin.
- Antivirüs yazılımları DLL inject araçlarını işaretleyebilir (false-positive).
