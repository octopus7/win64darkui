#pragma once

#include <string>
#include <vector>

#include <windows.h>

namespace darkui {

struct Rect {
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  [[nodiscard]] float width() const
  {
    return right - left;
  }

  [[nodiscard]] float height() const
  {
    return bottom - top;
  }

  [[nodiscard]] bool contains(const POINT point) const
  {
    const float x = static_cast<float>(point.x);
    const float y = static_cast<float>(point.y);
    return (x >= left) && (x <= right) && (y >= top) && (y <= bottom);
  }
};

enum class AreaRole {
  Global,
  Editor,
};

enum class RegionRole {
  Header,
  Toolbar,
  Window,
  Sidebar,
  Statusbar,
};

struct RegionModel {
  std::wstring id;
  std::wstring title;
  RegionRole role = RegionRole::Window;
  Rect bounds;
};

struct AreaModel {
  std::wstring id;
  std::wstring title;
  AreaRole role = AreaRole::Editor;
  Rect bounds;
  std::vector<RegionModel> regions;
};

struct ScreenModel {
  Rect bounds;
  Rect sidebar_splitter;
  float sidebar_width = 320.0f;
  std::vector<AreaModel> areas;
};

struct HitResult {
  enum class Kind {
    None,
    Splitter,
    Region,
  };

  Kind kind = Kind::None;
  int area_index = -1;
  int region_index = -1;
};

}  // namespace darkui
