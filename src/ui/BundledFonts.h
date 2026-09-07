#pragma once
// Private, embedded fonts: no machine-wide installation and no extra font files.
inline void LoadBundledFonts(HINSTANCE instance)
{
    for (int id : {301, 302}) {
        HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(id), RT_RCDATA);
        if (!resource) continue;
        HGLOBAL data = LoadResource(instance, resource);
        DWORD count = 0;
        AddFontMemResourceEx(LockResource(data), SizeofResource(instance, resource), nullptr, &count);
    }
}
