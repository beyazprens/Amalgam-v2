#pragma once

namespace Menu
{
    // ImGui overlay menüsünü çizer.
    // HookedEndScene içinden, ImGui::NewFrame() sonrasında çağrılır.
    void Render();

    // Menüyü aç / kapat.
    void Toggle();

    // Menü şu an açık mı?
    bool IsOpen();
}
