#pragma once

#include "ui_types.hpp"

namespace darkui {

class LayoutEngine {
 public:
  void SetClientSize(float width, float height);
  void SetScale(float scale);
  void SetSidebarWidth(float width);

  [[nodiscard]] float SidebarWidth() const;
  [[nodiscard]] ScreenModel BuildScreen() const;

 private:
  [[nodiscard]] float topbar_height() const;
  [[nodiscard]] float statusbar_height() const;
  [[nodiscard]] float editor_header_height() const;
  [[nodiscard]] float toolbar_width() const;
  [[nodiscard]] float splitter_width() const;
  [[nodiscard]] float sidebar_min_width() const;
  [[nodiscard]] float sidebar_max_width() const;

  float client_width_ = 1440.0f;
  float client_height_ = 900.0f;
  float scale_ = 1.0f;
  float sidebar_width_ = 320.0f;
};

}  // namespace darkui
