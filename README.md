# win64darkui

Windows native UI shell project inspired by Blender 4.5 LTS.

The goal is not to clone Blender wholesale. The goal is to port the core UI architecture in stages:

- `Window`
- `Screen`
- `Area`
- `Region`
- `UI Block`

The first milestone in this repository is a custom Win32 + Direct2D shell with:

- global top bar
- editor header
- left toolbar
- central viewport
- right properties sidebar
- bottom status bar
- draggable sidebar splitter

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

## Notes

- Blender analysis notes: `docs/blender_ui_architecture.md`
- Native entry point: `src/main.cpp`
- Layout model: `src/layout.cpp`, `src/layout.hpp`, `src/ui_types.hpp`
