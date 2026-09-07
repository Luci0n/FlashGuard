# FlashGuard — compact menu and fast shader startup

Close old instances, extract the entire folder, and run FlashGuard.exe.
Keep the shaders folder beside the executable.

F10 (or your saved menu shortcut) opens and closes settings. Escape also closes.
The menu shortcut is temporarily released while recording a custom shortcut.
Profiles have been removed. The menu is now 440 x 360 instead of 560 x 520.
JetBrains Mono Regular/Bold are embedded and loaded privately; no installation
or separate font download is needed. Font license and provenance are in licenses.
The opacity slider and saved 92% default remain available.

Compiled shaders are included for first launch and cached in
%LOCALAPPDATA%/OutlastFlashGuard/shader-cache when a compilation is needed.
Cache keys include source, entry point, target, flags and compiler generation;
files are size/checksum checked. Missing/invalid caches compile normally.
UI-only builds reuse identical shader sources. A cache write failure does not
prevent normal compilation and startup.

Focused validation: surface build and embedded font resources verified;
compact Home layout and absent profiles checked in preview. The live menu
hotkey registration/close/reopen paths were reviewed. Automated F10 input was
blocked by concurrent user input, so a completed automated toggle cycle is not
claimed. The two shader-startup checks compiled 11 shaders in 143.420 seconds,
then loaded all 11 with zero recompiles in 0.324 seconds. This measures GPU and
shader setup, not complete capture/presentation startup. Bundled shader files
were verified and copied from that successful run. The flashing suite was not
rerun for these UI/cache changes; the embedded protection HLSL is unchanged.
