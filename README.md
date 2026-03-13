<div align="center">

  ## <img src=".github/assets/amalgam_combo.png" alt="Amalgam" height="100">

  <sub>AVX2 may be faster than SSE2 though not all CPUs support it (`Steam > Help > System Information > Processor Information > AVX2`). Freetype uses freetype as the text rasterizer and includes some custom fonts, which results in better looking text but larger DLL sizes. PDBs are for developer use. </sub>
  ##
  
Read about the original Amalgam documentation and features [here](https://github.com/rei-2/Amalgam/wiki).  
Note: This repository is based on **TheGameEnhancer2004's fork** of Amalgam and includes additional changes and improvements.

  
## Changes I've Made

### Features
- Added **Smart Airblast**.
- Added **Sniper Triggerbot Detection** to CheaterDetection.

### Improvements
- Improved **Auto-switch Crossbow logic** to avoid interrupting Uber.
- Improved **Aimbot Accuracy** (doubletap sorting and projectile drag NaN handling).
- Optimized **SmoothVelocity** to prevent FPS drops.

### Fixes
- Fixed **Medic auto-arrow not aiming with SmoothVelocity aim type**.
- Fixed **Auto-arrow aimtype override and silent aimbot FOV bypass**.
- Fixed **Projectile aimbot calculation errors**.
- Fixed **Engineer melee tracking building center with SmoothVelocity**.
- Fixed **ACCESS VIOLATION crashes**.
- Fixed **Lag-compensation abuse false positives**.

### Changes
- Changed default value of **Auto Abandon if no navmesh** to `false`.

### Cleanup
- Cleaned up **README**.
</div>
