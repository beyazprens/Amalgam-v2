# TF2 ESP — Standalone DLL

Bağımsız, inject edilebilir bir Team Fortress 2 ESP DLL projesi.

## Dosya Yapısı

```
standalone_esp/
├── main.cpp      ← DllMain + MainThread (başlangıç noktası)
├── sdk.h         ← TF2 / Source Engine interface tanımları
├── hooks.h
├── hooks.cpp     ← DirectX 9 EndScene vtable hook
├── esp.h
├── esp.cpp       ← ESP kutu çizim mantığı
├── menu.h
└── menu.cpp      ← ImGui overlay menüsü
```

## Visual Studio Proje Kurulumu

### 1. Yeni Proje Oluştur

- **Proje tipi:** Dynamic-Link Library (DLL)
- **Platform:** x86 (32-bit) — TF2 hâlâ 32-bit çalışır
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
> Buraya ImGui klasörünü ekle (örn. `$(ProjectDir)\imgui`)

### 4. Gerekli Kütüphaneler

Proje Özellikleri → Linker → Input → Additional Dependencies'e ekle:

```
d3d9.lib
```

### 5. Diğer Ayarlar

| Ayar | Değer |
|------|-------|
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
- **Sadece Düşman** — Yalnızca karşı takımı göster
- **Sağlık Çubuğu** — Oyuncunun yanında HP çubuğu

---

## ESP Görünümü

- **Kutu rengi:** Cyan (0, 255, 255) — isteğe göre `esp.cpp` içinden değiştirilebilir
- **Sağlık çubuğu:** Solda, yeşil→sarı→kırmızı gradyan
- **Outline:** Siyah gölge (okunabilirlik için)

---

## Önemli Notlar

- **TF2 güncellemelerinde** vtable indeksleri veya `sdk.h` içindeki offsetler değişebilir.  
  Oyun güncellenirse `Offsets` namespace'ini ve vtable indekslerini kontrol et.
- Inject için Xenos, dll-injector vb. araçlar kullanabilirsin.
- Antivirüs yazılımları DLL inject araçlarını işaretleyebilir; bunlar false-positive'dir.

---

## Renk Değiştirme

`esp.cpp` içindeki satırı düzenle:

```cpp
// Cyan yerine istediğin rengi yaz: IM_COL32(R, G, B, A)
DrawBox(dl, bx, by, bw, bh,
        IM_COL32(0, 255, 255, 255));  // cyan
```

Örnek renkler:
- Yeşil: `IM_COL32(0, 255, 0, 255)`
- Kırmızı: `IM_COL32(255, 0, 0, 255)`
- Sarı: `IM_COL32(255, 255, 0, 255)`
