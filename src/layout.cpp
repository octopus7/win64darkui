#include "layout.hpp"

#include <algorithm>
#include <utility>

namespace darkui {

namespace {

Rect make_rect(float left, float top, float right, float bottom)
{
  return Rect{left, top, right, bottom};
}

}  // namespace

void LayoutEngine::SetClientSize(const float width, const float height)
{
  client_width_ = std::max(width, 640.0f);
  client_height_ = std::max(height, 480.0f);
  SetSidebarWidth(sidebar_width_);
}

void LayoutEngine::SetScale(const float scale)
{
  scale_ = std::max(scale, 1.0f);
  SetSidebarWidth(sidebar_width_);
}

void LayoutEngine::SetSidebarWidth(const float width)
{
  const float available = std::max(220.0f * scale_, client_width_ - (toolbar_width() + 320.0f * scale_));
  sidebar_width_ = std::clamp(width, sidebar_min_width(), std::min(sidebar_max_width(), available));
}

float LayoutEngine::SidebarWidth() const
{
  return sidebar_width_;
}

ScreenModel LayoutEngine::BuildScreen() const
{
  ScreenModel screen;
  screen.bounds = make_rect(0.0f, 0.0f, client_width_, client_height_);
  screen.sidebar_width = sidebar_width_;

  const float topbar = topbar_height();
  const float statusbar = statusbar_height();
  const float editor_header = editor_header_height();
  const float toolbar = toolbar_width();
  const float splitter = splitter_width();

  const Rect topbar_area = make_rect(0.0f, 0.0f, client_width_, topbar);
  const Rect statusbar_area = make_rect(0.0f, client_height_ - statusbar, client_width_, client_height_);
  const Rect editor_area = make_rect(0.0f, topbar, client_width_, client_height_ - statusbar);

  const float content_top = editor_area.top + editor_header;
  const float sidebar_left = editor_area.right - sidebar_width_;
  const float splitter_left = sidebar_left - (splitter * 0.5f);
  const float splitter_right = sidebar_left + (splitter * 0.5f);

  const Rect editor_header_region = make_rect(editor_area.left, editor_area.top, editor_area.right, content_top);
  const Rect toolbar_region = make_rect(editor_area.left, content_top, editor_area.left + toolbar, editor_area.bottom);
  const Rect sidebar_region = make_rect(sidebar_left, content_top, editor_area.right, editor_area.bottom);
  const Rect viewport_region = make_rect(toolbar_region.right, content_top, splitter_left, editor_area.bottom);

  screen.sidebar_splitter = make_rect(splitter_left, content_top, splitter_right, editor_area.bottom);

  AreaModel topbar_model;
  topbar_model.id = L"global.topbar";
  topbar_model.title = L"Top Bar";
  topbar_model.role = AreaRole::Global;
  topbar_model.bounds = topbar_area;
  topbar_model.regions.push_back(RegionModel{
      .id = L"global.topbar.header",
      .title = L"Workspace Strip",
      .role = RegionRole::Header,
      .bounds = topbar_area,
  });

  AreaModel editor_model;
  editor_model.id = L"editor.main";
  editor_model.title = L"Primary Editor";
  editor_model.role = AreaRole::Editor;
  editor_model.bounds = editor_area;
  editor_model.regions.push_back(RegionModel{
      .id = L"editor.header",
      .title = L"Viewport Header",
      .role = RegionRole::Header,
      .bounds = editor_header_region,
  });
  editor_model.regions.push_back(RegionModel{
      .id = L"editor.toolbar",
      .title = L"Tool Shelf",
      .role = RegionRole::Toolbar,
      .bounds = toolbar_region,
  });
  editor_model.regions.push_back(RegionModel{
      .id = L"editor.window",
      .title = L"Viewport",
      .role = RegionRole::Window,
      .bounds = viewport_region,
  });
  editor_model.regions.push_back(RegionModel{
      .id = L"editor.sidebar",
      .title = L"Properties",
      .role = RegionRole::Sidebar,
      .bounds = sidebar_region,
  });

  AreaModel statusbar_model;
  statusbar_model.id = L"global.statusbar";
  statusbar_model.title = L"Status Bar";
  statusbar_model.role = AreaRole::Global;
  statusbar_model.bounds = statusbar_area;
  statusbar_model.regions.push_back(RegionModel{
      .id = L"global.statusbar.header",
      .title = L"Status",
      .role = RegionRole::Statusbar,
      .bounds = statusbar_area,
  });

  screen.areas.push_back(std::move(topbar_model));
  screen.areas.push_back(std::move(editor_model));
  screen.areas.push_back(std::move(statusbar_model));
  return screen;
}

float LayoutEngine::topbar_height() const
{
  return 34.0f * scale_;
}

float LayoutEngine::statusbar_height() const
{
  return 28.0f * scale_;
}

float LayoutEngine::editor_header_height() const
{
  return 30.0f * scale_;
}

float LayoutEngine::toolbar_width() const
{
  return 58.0f * scale_;
}

float LayoutEngine::splitter_width() const
{
  return 10.0f * scale_;
}

float LayoutEngine::sidebar_min_width() const
{
  return 260.0f * scale_;
}

float LayoutEngine::sidebar_max_width() const
{
  return 520.0f * scale_;
}

}  // namespace darkui
