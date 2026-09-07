# FlashGuard — monospace and opacity

Close the preview/old instance, then run FlashGuard.exe from this folder.
F10 opens settings by default; Escape closes them.

Menu and loading banner now use Consolas monospace. Menu opacity defaults
to 92% (slightly transparent). The slider ranges from 60% to 100% and previews
changes immediately. Save changes remembers it; the loading banner uses the
same saved opacity on the next start. The slider is on Home for surface mode
and Advanced for legacy mode. Protection output opacity is unaffected.

Settings preview never saves changes. Close it and run this build normally
to save your preferred appearance. The status-label redraw issue is fixed.

Validation: surface executable compiled and linked; monospace, default 92%,
and slider transparency visually verified in preview before the status-label
paint correction. The running preview prevented replacement of the root exe;
this package contains the final executable directly from the build folder.
Protection shader sources match the previous 260-case passing build; those
tests were not repeated for this appearance change.
