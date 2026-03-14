# Blender UI Architecture Notes

These notes capture the Blender 4.5 LTS source references we are mirroring in `win64darkui`.

## Source Anchors

1. Hierarchy overview
   - `source/blender/windowmanager/WM_types.hh`
   - Blender documents the main chain as `wmWindow -> bScreen -> ScrArea -> ARegion`.

2. Saved/runtime screen data
   - `source/blender/makesdna/DNA_screen_types.h`
   - `ScrArea` holds editor identity, region list, handlers, and action zones.
   - `ARegion` holds bounds, alignment, flags, panels, and runtime draw data.

3. Region layout and draw hooks
   - `source/blender/editors/screen/area.cc`
   - `ED_region_do_layout()` computes dynamic layout.
   - `ED_region_do_draw()` performs per-region drawing and post-draw callbacks.

4. Window composition
   - `source/blender/windowmanager/intern/wm_draw.cc`
   - Blender draws regions off-screen, then composites them onto the window.
   - This is the practical model to preserve when we later add caching and partial redraw.

5. Global bars
   - `source/blender/editors/screen/screen_edit.cc`
   - Top bar and status bar are refreshed as global areas, not hard-coded special widgets.

6. Immediate-mode UI blocks
   - `source/blender/editors/interface/interface.cc`
   - `UI_block_begin()` creates a transient block for a region draw pass.
   - `UI_block_end()` finalizes layout before painting.

## Mapping In This Repository

Blender concept to local concept:

- `wmWindow` -> `AppWindow`
- `bScreen` -> `ScreenModel`
- `ScrArea` -> `AreaModel`
- `ARegion` -> `RegionModel`
- `global area refresh` -> `LayoutEngine::BuildScreen()`
- `UI block` -> later `UiBlock` layer on top of the current drawing helpers

## Current Scope

This repository intentionally starts one level below a full widget system.

Stage 1:

- native window creation
- DPI-aware client layout
- area and region modeling
- custom dark theme rendering
- splitter interaction

Stage 2:

- retained scene/state model
- command routing
- hit-testing per widget
- panel collapse and tab switching

Stage 3:

- per-region invalidation
- off-screen caching
- dock split/join behavior closer to Blender

## Why This Order

Blender's UI is not "just widgets". The difficult part is the screen model and the redraw pipeline.
If we get the structural layers right first, the later widget work stays local instead of leaking
through the entire application.
