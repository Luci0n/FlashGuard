# FlashGuard — overlay UI update

Close the old instance, extract this folder, and run FlashGuard.exe.
F10 opens settings by default; Escape closes them. Existing custom hotkeys
are preserved. Choose Save changes to apply and save your edits.

- Compact, dark Home / Shortcuts / Advanced menu with a draggable header.
- Top-left loading banner with elapsed time and a smooth progress bar.
- A separate banner UI thread keeps repainting during shader compilation.
- Current settings are shown; unused legacy detector controls are hidden
  from the surface backend. Settings and the live banner remain excluded
  from capture.

The HSV protection shaders are unchanged from the 260-case passing build.
No extra capture frames or filtering delay were introduced by this UI change.

Validation: surface build passed; Home, Shortcuts and Advanced visually
checked in settings preview; Escape close passed. Loading preview completed
successfully while its calling thread was deliberately blocked. Its banner
was not exposed by the screenshot tool, so no banner screenshot was verified.
Live protection was not enabled during UI verification. Previous algorithm
results are included for reference, not claimed as rerun on this executable.

Developer preview commands: --settings-preview and --startup-preview.
These never start capture or protection. Settings preview does not save edits.
