#include "layout.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include <d2d1.h>
#include <dwrite.h>
#include <shellapi.h>
#include <windowsx.h>
#include <wrl/client.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace darkui {

using Microsoft::WRL::ComPtr;

namespace {

struct Theme {
  D2D1_COLOR_F background_a = D2D1::ColorF(0x11161C);
  D2D1_COLOR_F background_b = D2D1::ColorF(0x1A212B);
  D2D1_COLOR_F chrome = D2D1::ColorF(0x212934);
  D2D1_COLOR_F panel = D2D1::ColorF(0x1B222B);
  D2D1_COLOR_F panel_alt = D2D1::ColorF(0x262F3A);
  D2D1_COLOR_F viewport = D2D1::ColorF(0x151B22);
  D2D1_COLOR_F accent = D2D1::ColorF(0xE7B05A);
  D2D1_COLOR_F accent_soft = D2D1::ColorF(0.91f, 0.69f, 0.35f, 0.18f);
  D2D1_COLOR_F line = D2D1::ColorF(0x3A4552);
  D2D1_COLOR_F line_soft = D2D1::ColorF(0.26f, 0.31f, 0.37f, 0.55f);
  D2D1_COLOR_F text = D2D1::ColorF(0xE7ECF2);
  D2D1_COLOR_F text_muted = D2D1::ColorF(0x92A0B0);
  D2D1_COLOR_F hover = D2D1::ColorF(0.20f, 0.25f, 0.31f, 0.95f);
  D2D1_COLOR_F splitter = D2D1::ColorF(0.54f, 0.60f, 0.67f, 0.30f);
};

enum class ThemeKind {
  Dark,
  Light,
  ColorWave,
};

Theme MakeDarkTheme()
{
  return Theme{};
}

Theme MakeLightTheme()
{
  Theme theme;
  theme.background_a = D2D1::ColorF(0xE9EDF3);
  theme.background_b = D2D1::ColorF(0xDDE5EE);
  theme.chrome = D2D1::ColorF(0xF7F9FC);
  theme.panel = D2D1::ColorF(0xEFF4F9);
  theme.panel_alt = D2D1::ColorF(0xE4EBF4);
  theme.viewport = D2D1::ColorF(0xF5F8FC);
  theme.accent = D2D1::ColorF(0x2F6BFF);
  theme.accent_soft = D2D1::ColorF(0.18f, 0.42f, 1.0f, 0.14f);
  theme.line = D2D1::ColorF(0xA8B6C7);
  theme.line_soft = D2D1::ColorF(0.60f, 0.68f, 0.77f, 0.55f);
  theme.text = D2D1::ColorF(0x182332);
  theme.text_muted = D2D1::ColorF(0x5B6878);
  theme.hover = D2D1::ColorF(0.85f, 0.90f, 0.96f, 0.96f);
  theme.splitter = D2D1::ColorF(0.40f, 0.48f, 0.58f, 0.24f);
  return theme;
}

Theme MakeColorWaveTheme()
{
  Theme theme;
  theme.background_a = D2D1::ColorF(0x141922);
  theme.background_b = D2D1::ColorF(0x111A1F);
  theme.chrome = D2D1::ColorF(0x1B2130);
  theme.panel = D2D1::ColorF(0x1A2230);
  theme.panel_alt = D2D1::ColorF(0x202B38);
  theme.viewport = D2D1::ColorF(0x121923);
  theme.accent = D2D1::ColorF(0x53D6C5);
  theme.accent_soft = D2D1::ColorF(0.27f, 0.84f, 0.77f, 0.17f);
  theme.line = D2D1::ColorF(0x375066);
  theme.line_soft = D2D1::ColorF(0.26f, 0.44f, 0.52f, 0.55f);
  theme.text = D2D1::ColorF(0xEEF5F8);
  theme.text_muted = D2D1::ColorF(0x8AA8B6);
  theme.hover = D2D1::ColorF(0.21f, 0.28f, 0.36f, 0.95f);
  theme.splitter = D2D1::ColorF(0.32f, 0.71f, 0.80f, 0.32f);
  return theme;
}

Theme MakeTheme(const ThemeKind kind)
{
  switch (kind) {
    case ThemeKind::Dark:
      return MakeDarkTheme();
    case ThemeKind::Light:
      return MakeLightTheme();
    case ThemeKind::ColorWave:
      return MakeColorWaveTheme();
  }
  return MakeDarkTheme();
}

const wchar_t *ThemeLabel(const ThemeKind kind)
{
  switch (kind) {
    case ThemeKind::Dark:
      return L"Dark";
    case ThemeKind::Light:
      return L"Light";
    case ThemeKind::ColorWave:
      return L"Color Wave";
  }
  return L"Dark";
}

const char *ThemeKey(const ThemeKind kind)
{
  switch (kind) {
    case ThemeKind::Dark:
      return "dark";
    case ThemeKind::Light:
      return "light";
    case ThemeKind::ColorWave:
      return "color_wave";
  }
  return "dark";
}

ThemeKind ThemeKindFromKey(const std::string &key)
{
  if (key == "light") {
    return ThemeKind::Light;
  }
  if (key == "color_wave") {
    return ThemeKind::ColorWave;
  }
  return ThemeKind::Dark;
}

Theme kTheme = MakeDarkTheme();

D2D1_RECT_F to_d2d_rect(const Rect &rect)
{
  return D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom);
}

Rect inset(const Rect &rect, const float margin)
{
  return Rect{rect.left + margin, rect.top + margin, rect.right - margin, rect.bottom - margin};
}

D2D1_COLOR_F with_alpha(const D2D1_COLOR_F color, const float alpha)
{
  return D2D1::ColorF(color.r, color.g, color.b, alpha);
}

}  // namespace

class AppWindow {
 public:
  int Run(HINSTANCE instance)
  {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &AppWindow::WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"Win64DarkUiShell";
    RegisterClassExW(&wc);

    RECT frame{0, 0, 1560, 940};
    AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW);

    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW,
                            wc.lpszClassName,
                            L"win64darkui - Blender-Inspired Shell",
                            WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            frame.right - frame.left,
                            frame.bottom - frame.top,
                            nullptr,
                            nullptr,
                            instance,
                            this);
    if (!hwnd_) {
      return 1;
    }

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
  }

 private:
  enum MenuCommand : UINT {
    kMenuFileNew = 1001,
    kMenuFileOpen,
    kMenuFileSaveAs,
    kMenuFileExit,
    kMenuEditUndo = 1101,
    kMenuEditRedo,
    kMenuEditPreferences,
    kMenuRenderImage = 1201,
    kMenuRenderAnimation,
    kMenuRenderView,
    kMenuWindowToggleFullscreen = 1301,
    kMenuWindowScreenshot,
    kMenuHelpManual = 1401,
    kMenuHelpAbout,
  };

  struct TopMenuLayout {
    std::array<Rect, 5> rects{};
  };

  struct DropdownMenuItem {
    std::wstring label;
    std::wstring secondary;
    UINT command = 0;
    bool separator = false;
    bool checkable = false;
    bool checked = false;
  };

  struct DropdownMenuLayout {
    Rect bounds;
    std::vector<DropdownMenuItem> items;
    std::vector<Rect> item_rects;
  };

  enum class SplashAction {
    None,
    Close,
    Manual,
    Continue,
  };

  struct SplashLayout {
    Rect card;
    Rect close_button;
    Rect manual_button;
    Rect continue_button;
  };

  enum class PreferencesAction {
    None,
    Close,
    ThemeField,
    ThemeOptionDark,
    ThemeOptionLight,
    ThemeOptionColorWave,
    ThemePreviewDark,
    ThemePreviewLight,
    ThemePreviewColorWave,
  };

  struct PreferencesLayout {
    Rect card;
    Rect close_button;
    Rect theme_field;
    Rect theme_dropdown;
    std::array<Rect, 3> option_rects{};
    std::array<Rect, 3> preview_rects{};
  };

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
  {
    AppWindow *self = nullptr;
    if (message == WM_NCCREATE) {
      auto *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
      self = static_cast<AppWindow *>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
      self->hwnd_ = hwnd;
    }
    else {
      self = reinterpret_cast<AppWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) {
      return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    return self->HandleMessage(message, wparam, lparam);
  }

  LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam)
  {
    switch (message) {
      case WM_CREATE:
        if (!CreateDeviceIndependentResources()) {
          return -1;
        }
        LoadSettings();
        UpdateScaleFromWindow();
        RebuildLayout();
        return 0;

      case WM_SIZE:
        ResizeRenderTarget();
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;

      case WM_DPICHANGED: {
        auto *suggested = reinterpret_cast<RECT *>(lparam);
        SetWindowPos(hwnd_,
                     nullptr,
                     suggested->left,
                     suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateScaleFromWindow();
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
      }

      case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT && UpdateCursor()) {
          return TRUE;
        }
        break;

      case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        return 0;

      case WM_LBUTTONDOWN:
        OnLeftButtonDown(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        return 0;

      case WM_LBUTTONUP:
        OnLeftButtonUp();
        return 0;

      case WM_CAPTURECHANGED:
        dragging_splitter_ = false;
        return 0;

      case WM_PAINT:
        OnPaint();
        return 0;

      case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
          if (splash_visible_) {
            CloseSplash();
            return 0;
          }
          if (preferences_visible_) {
            if (preferences_theme_dropdown_open_) {
              preferences_theme_dropdown_open_ = false;
              hovered_preferences_action_ = PreferencesAction::None;
              InvalidateRect(hwnd_, nullptr, FALSE);
            }
            else {
              ClosePreferences();
            }
            return 0;
          }
          if (IsDropdownOpen()) {
            CloseTopMenu();
            hovered_top_menu_ = -1;
            return 0;
          }
        }
        break;

      case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE && IsDropdownOpen()) {
          CloseTopMenu();
          hovered_top_menu_ = -1;
          return 0;
        }
        break;

      case WM_ERASEBKGND:
        return 1;

      case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
  }

  SplashLayout BuildSplashLayout() const
  {
    const float card_width = std::min(760.0f * scale_, screen_.bounds.width() - (100.0f * scale_));
    const float card_height = std::min(470.0f * scale_, screen_.bounds.height() - (100.0f * scale_));
    const float left = ((screen_.bounds.width() - card_width) * 0.5f);
    const float top = ((screen_.bounds.height() - card_height) * 0.5f);

    SplashLayout layout;
    layout.card = Rect{left, top, left + card_width, top + card_height};
    layout.close_button = Rect{
        layout.card.right - (42.0f * scale_),
        layout.card.top + (16.0f * scale_),
        layout.card.right - (16.0f * scale_),
        layout.card.top + (42.0f * scale_),
    };
    layout.manual_button = Rect{
        layout.card.left + (30.0f * scale_),
        layout.card.bottom - (68.0f * scale_),
        layout.card.left + (194.0f * scale_),
        layout.card.bottom - (28.0f * scale_),
    };
    layout.continue_button = Rect{
        layout.card.right - (170.0f * scale_),
        layout.card.bottom - (68.0f * scale_),
        layout.card.right - (30.0f * scale_),
        layout.card.bottom - (28.0f * scale_),
    };
    return layout;
  }

  PreferencesLayout BuildPreferencesLayout() const
  {
    const float card_width = std::min(560.0f * scale_, screen_.bounds.width() - (120.0f * scale_));
    const float card_height = 300.0f * scale_;
    const float left = (screen_.bounds.width() - card_width) * 0.5f;
    const float top = (screen_.bounds.height() - card_height) * 0.5f;

    PreferencesLayout layout;
    layout.card = Rect{left, top, left + card_width, top + card_height};
    layout.close_button = Rect{
        layout.card.right - (42.0f * scale_),
        layout.card.top + (16.0f * scale_),
        layout.card.right - (16.0f * scale_),
        layout.card.top + (42.0f * scale_),
    };
    layout.theme_field = Rect{
        layout.card.left + (170.0f * scale_),
        layout.card.top + (108.0f * scale_),
        layout.card.right - (34.0f * scale_),
        layout.card.top + (146.0f * scale_),
    };
    layout.theme_dropdown = Rect{
        layout.theme_field.left,
        layout.theme_field.bottom + (6.0f * scale_),
        layout.theme_field.right,
        layout.theme_field.bottom + (6.0f * scale_) + (3.0f * 34.0f * scale_) + (10.0f * scale_),
    };

    const float option_left = layout.theme_dropdown.left + (6.0f * scale_);
    const float option_right = layout.theme_dropdown.right - (6.0f * scale_);
    float option_top = layout.theme_dropdown.top + (5.0f * scale_);
    for (Rect &option : layout.option_rects) {
      option = Rect{option_left, option_top, option_right, option_top + (34.0f * scale_)};
      option_top += 34.0f * scale_;
    }

    float preview_x = layout.card.left + (34.0f * scale_);
    for (Rect &preview : layout.preview_rects) {
      preview = Rect{
          preview_x,
          layout.card.top + (176.0f * scale_),
          preview_x + (148.0f * scale_),
          layout.card.top + (232.0f * scale_),
      };
      preview_x += 160.0f * scale_;
    }
    return layout;
  }

  std::filesystem::path SettingsPath() const
  {
    wchar_t module_path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    std::filesystem::path path(module_path);
    return path.parent_path() / "win64darkui.settings";
  }

  void SaveSettings() const
  {
    std::ofstream output(settings_path_, std::ios::trunc);
    if (!output.is_open()) {
      return;
    }
    output << "theme=" << ThemeKey(theme_kind_) << "\n";
  }

  void ApplyTheme(const ThemeKind kind, const bool persist)
  {
    theme_kind_ = kind;
    kTheme = MakeTheme(kind);
    if (persist) {
      SaveSettings();
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
  }

  void LoadSettings()
  {
    settings_path_ = SettingsPath();

    std::ifstream input(settings_path_);
    if (!input.is_open()) {
      ApplyTheme(ThemeKind::Dark, false);
      return;
    }

    std::string line;
    while (std::getline(input, line)) {
      constexpr std::string_view prefix = "theme=";
      if (line.rfind(prefix.data(), 0) == 0) {
        ApplyTheme(ThemeKindFromKey(line.substr(prefix.size())), false);
      }
    }
  }

  PreferencesAction HitTestPreferences(const POINT point) const
  {
    if (!preferences_visible_) {
      return PreferencesAction::None;
    }

    const PreferencesLayout layout = BuildPreferencesLayout();
    if (layout.close_button.contains(point)) {
      return PreferencesAction::Close;
    }
    if (layout.theme_field.contains(point)) {
      return PreferencesAction::ThemeField;
    }

    if (preferences_theme_dropdown_open_ && layout.theme_dropdown.contains(point)) {
      if (layout.option_rects[0].contains(point)) {
        return PreferencesAction::ThemeOptionDark;
      }
      if (layout.option_rects[1].contains(point)) {
        return PreferencesAction::ThemeOptionLight;
      }
      if (layout.option_rects[2].contains(point)) {
        return PreferencesAction::ThemeOptionColorWave;
      }
    }

    if (layout.preview_rects[0].contains(point)) {
      return PreferencesAction::ThemePreviewDark;
    }
    if (layout.preview_rects[1].contains(point)) {
      return PreferencesAction::ThemePreviewLight;
    }
    if (layout.preview_rects[2].contains(point)) {
      return PreferencesAction::ThemePreviewColorWave;
    }

    return PreferencesAction::None;
  }

  void OpenPreferences()
  {
    CloseTopMenu();
    splash_visible_ = false;
    hovered_splash_action_ = SplashAction::None;
    preferences_visible_ = true;
    preferences_theme_dropdown_open_ = false;
    hovered_preferences_action_ = PreferencesAction::None;
    InvalidateRect(hwnd_, nullptr, FALSE);
  }

  void ClosePreferences()
  {
    preferences_visible_ = false;
    preferences_theme_dropdown_open_ = false;
    hovered_preferences_action_ = PreferencesAction::None;
    InvalidateRect(hwnd_, nullptr, FALSE);
  }

  const RegionModel *TopbarRegion() const
  {
    for (const AreaModel &area : screen_.areas) {
      if (area.id == L"global.topbar" && !area.regions.empty()) {
        return &area.regions.front();
      }
    }
    return nullptr;
  }

  TopMenuLayout BuildTopMenuLayout(const RegionModel &region) const
  {
    TopMenuLayout layout;
    const std::array<float, 5> menu_widths = {
        34.0f * scale_, 34.0f * scale_, 50.0f * scale_, 54.0f * scale_, 36.0f * scale_};

    float menu_x = 130.0f * scale_;
    for (size_t index = 0; index < layout.rects.size(); ++index) {
      layout.rects[index] = Rect{menu_x, region.bounds.top, menu_x + menu_widths[index], region.bounds.bottom};
      menu_x += menu_widths[index] + (8.0f * scale_);
    }
    return layout;
  }

  int HitTestTopMenu(const POINT point) const
  {
    const RegionModel *region = TopbarRegion();
    if (!region || !region->bounds.contains(point)) {
      return -1;
    }

    const TopMenuLayout layout = BuildTopMenuLayout(*region);
    for (int index = 0; index < static_cast<int>(layout.rects.size()); ++index) {
      if (layout.rects[index].contains(point)) {
        return index;
      }
    }
    return -1;
  }

  std::vector<DropdownMenuItem> BuildDropdownItems(const int top_menu) const
  {
    switch (top_menu) {
      case 0:
        return {
            {L"New", L"", kMenuFileNew},
            {L"Open...", L"", kMenuFileOpen},
            {L"Save As...", L"", kMenuFileSaveAs},
            {.separator = true},
            {L"Exit", L"", kMenuFileExit},
        };
      case 1:
        return {
            {L"Undo", L"", kMenuEditUndo},
            {L"Redo", L"", kMenuEditRedo},
            {.separator = true},
            {L"Preferences...", L"", kMenuEditPreferences},
        };
      case 2:
        return {
            {L"Render Image", L"", kMenuRenderImage},
            {L"Render Animation", L"", kMenuRenderAnimation},
            {.separator = true},
            {L"View Render", L"", kMenuRenderView},
        };
      case 3:
        return {
            {L"Toggle Window Fullscreen", L"Alt+Enter", kMenuWindowToggleFullscreen, false, true, is_fullscreen_},
            {.separator = true},
            {L"Save Screenshot...", L"", kMenuWindowScreenshot},
        };
      case 4:
        return {
            {L"Online Manual", L"", kMenuHelpManual},
            {.separator = true},
            {L"About", L"", kMenuHelpAbout},
        };
      default:
        return {};
    }
  }

  float DropdownWidthForMenu(const int top_menu) const
  {
    switch (top_menu) {
      case 0:
        return 214.0f * scale_;
      case 1:
        return 214.0f * scale_;
      case 2:
        return 224.0f * scale_;
      case 3:
        return 276.0f * scale_;
      case 4:
        return 210.0f * scale_;
      default:
        return 220.0f * scale_;
    }
  }

  bool IsDropdownOpen() const
  {
    return active_top_menu_ != -1;
  }

  DropdownMenuLayout BuildDropdownLayout() const
  {
    DropdownMenuLayout layout;
    const RegionModel *region = TopbarRegion();
    if (!region || !IsDropdownOpen()) {
      return layout;
    }

    const TopMenuLayout top_layout = BuildTopMenuLayout(*region);
    layout.items = BuildDropdownItems(active_top_menu_);
    if (layout.items.empty()) {
      return layout;
    }

    const float width = DropdownWidthForMenu(active_top_menu_);
    const float padding = 6.0f * scale_;
    const float item_height = 28.0f * scale_;
    const float separator_height = 9.0f * scale_;
    const float start_x = top_layout.rects[active_top_menu_].left - (8.0f * scale_);
    const float start_y = region->bounds.bottom - 1.0f;
    float y = start_y + padding;

    for (const DropdownMenuItem &item : layout.items) {
      const float height = item.separator ? separator_height : item_height;
      layout.item_rects.push_back(Rect{start_x + padding, y, start_x + width - padding, y + height});
      y += height;
    }

    layout.bounds = Rect{start_x, start_y, start_x + width, y + padding};
    return layout;
  }

  int HitTestDropdownItem(const POINT point) const
  {
    if (!IsDropdownOpen()) {
      return -1;
    }

    const DropdownMenuLayout layout = BuildDropdownLayout();
    if ((layout.bounds.width() <= 0.0f) || !layout.bounds.contains(point)) {
      return -1;
    }

    for (int index = 0; index < static_cast<int>(layout.item_rects.size()); ++index) {
      if (layout.item_rects[index].contains(point) && !layout.items[index].separator) {
        return index;
      }
    }
    return -1;
  }

  void OpenTopMenu(const int index)
  {
    if (index < 0) {
      return;
    }
    active_top_menu_ = index;
    hovered_top_menu_ = index;
    hovered_dropdown_item_ = -1;
    InvalidateRect(hwnd_, nullptr, FALSE);
  }

  void CloseTopMenu()
  {
    if (!IsDropdownOpen()) {
      return;
    }
    active_top_menu_ = -1;
    hovered_dropdown_item_ = -1;
    InvalidateRect(hwnd_, nullptr, FALSE);
  }

  void ShowPlaceholderPopup(const wchar_t *title, const wchar_t *message)
  {
    MessageBoxW(hwnd_, message, title, MB_OK | MB_ICONINFORMATION);
  }

  SplashAction HitTestSplash(const POINT point) const
  {
    if (!splash_visible_) {
      return SplashAction::None;
    }

    const SplashLayout layout = BuildSplashLayout();
    if (layout.close_button.contains(point)) {
      return SplashAction::Close;
    }
    if (layout.manual_button.contains(point)) {
      return SplashAction::Manual;
    }
    if (layout.continue_button.contains(point)) {
      return SplashAction::Continue;
    }
    return SplashAction::None;
  }

  void OpenSplash()
  {
    CloseTopMenu();
    preferences_visible_ = false;
    preferences_theme_dropdown_open_ = false;
    splash_visible_ = true;
    hovered_splash_action_ = SplashAction::None;
    InvalidateRect(hwnd_, nullptr, FALSE);
  }

  void CloseSplash()
  {
    splash_visible_ = false;
    hovered_splash_action_ = SplashAction::None;
    InvalidateRect(hwnd_, nullptr, FALSE);
  }

  void ToggleWindowFullscreen()
  {
    if (!is_fullscreen_) {
      windowed_style_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
      windowed_placement_.length = sizeof(windowed_placement_);
      GetWindowPlacement(hwnd_, &windowed_placement_);

      MONITORINFO monitor_info{};
      monitor_info.cbSize = sizeof(monitor_info);
      GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitor_info);

      SetWindowLongPtrW(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(windowed_style_ & ~WS_OVERLAPPEDWINDOW));
      SetWindowPos(hwnd_,
                   HWND_TOP,
                   monitor_info.rcMonitor.left,
                   monitor_info.rcMonitor.top,
                   monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                   monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
      is_fullscreen_ = true;
    }
    else {
      SetWindowLongPtrW(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(windowed_style_));
      SetWindowPlacement(hwnd_, &windowed_placement_);
      SetWindowPos(hwnd_,
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
      is_fullscreen_ = false;
    }

    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
  }

  void HandleMenuCommand(const UINT command)
  {
    switch (command) {
      case kMenuFileNew:
        ShowPlaceholderPopup(L"File > New", L"New file is not implemented yet.");
        break;
      case kMenuFileOpen:
        ShowPlaceholderPopup(L"File > Open", L"Open is not implemented yet.");
        break;
      case kMenuFileSaveAs:
        ShowPlaceholderPopup(L"File > Save As", L"Save As is not implemented yet.");
        break;
      case kMenuFileExit:
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        break;
      case kMenuEditUndo:
        ShowPlaceholderPopup(L"Edit > Undo", L"Undo stack is not implemented yet.");
        break;
      case kMenuEditRedo:
        ShowPlaceholderPopup(L"Edit > Redo", L"Redo stack is not implemented yet.");
        break;
      case kMenuEditPreferences:
        OpenPreferences();
        break;
      case kMenuRenderImage:
        ShowPlaceholderPopup(L"Render > Render Image", L"Render pipeline is not implemented yet.");
        break;
      case kMenuRenderAnimation:
        ShowPlaceholderPopup(L"Render > Render Animation", L"Animation render is not implemented yet.");
        break;
      case kMenuRenderView:
        ShowPlaceholderPopup(L"Render > View Render", L"Render view is not implemented yet.");
        break;
      case kMenuWindowToggleFullscreen:
        ToggleWindowFullscreen();
        break;
      case kMenuWindowScreenshot:
        ShowPlaceholderPopup(L"Window > Save Screenshot", L"Screenshot export is not implemented yet.");
        break;
      case kMenuHelpManual:
        ShellExecuteW(hwnd_, L"open", L"https://docs.blender.org/manual/en/latest/", nullptr, nullptr, SW_SHOWNORMAL);
        break;
      case kMenuHelpAbout:
        OpenSplash();
        break;
    }
  }

  bool CreateDeviceIndependentResources()
  {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.ReleaseAndGetAddressOf()))) {
      return false;
    }

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                   __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown **>(write_factory_.ReleaseAndGetAddressOf())))) {
      return false;
    }

    if (FAILED(write_factory_->CreateTextFormat(L"Segoe UI",
                                                nullptr,
                                                DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                15.5f,
                                                L"ko-KR",
                                                title_format_.ReleaseAndGetAddressOf()))) {
      return false;
    }

    if (FAILED(write_factory_->CreateTextFormat(L"Segoe UI",
                                                nullptr,
                                                DWRITE_FONT_WEIGHT_REGULAR,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                12.0f,
                                                L"ko-KR",
                                                ui_format_.ReleaseAndGetAddressOf()))) {
      return false;
    }

    if (FAILED(write_factory_->CreateTextFormat(L"Segoe UI",
                                                nullptr,
                                                DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                11.0f,
                                                L"ko-KR",
                                                small_bold_format_.ReleaseAndGetAddressOf()))) {
      return false;
    }

    if (FAILED(write_factory_->CreateTextFormat(L"Segoe UI",
                                                nullptr,
                                                DWRITE_FONT_WEIGHT_REGULAR,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                11.0f,
                                                L"ko-KR",
                                                menu_format_.ReleaseAndGetAddressOf()))) {
      return false;
    }

    if (FAILED(write_factory_->CreateTextFormat(L"Segoe UI",
                                                nullptr,
                                                DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                27.0f,
                                                L"ko-KR",
                                                splash_title_format_.ReleaseAndGetAddressOf()))) {
      return false;
    }

    if (FAILED(write_factory_->CreateTextFormat(L"Segoe UI",
                                                nullptr,
                                                DWRITE_FONT_WEIGHT_REGULAR,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                12.5f,
                                                L"ko-KR",
                                                splash_body_format_.ReleaseAndGetAddressOf()))) {
      return false;
    }

    title_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    ui_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    small_bold_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    menu_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    splash_title_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    splash_body_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    return true;
  }

  bool CreateDeviceResources()
  {
    if (render_target_) {
      return true;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);
    const auto size = D2D1::SizeU(client.right - client.left, client.bottom - client.top);

    if (FAILED(factory_->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_IGNORE)),
            D2D1::HwndRenderTargetProperties(hwnd_, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY),
            render_target_.ReleaseAndGetAddressOf()))) {
      return false;
    }

    if (FAILED(render_target_->CreateSolidColorBrush(kTheme.text, brush_.ReleaseAndGetAddressOf()))) {
      return false;
    }

    return true;
  }

  void ResizeRenderTarget()
  {
    if (!render_target_) {
      return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);
    render_target_->Resize(D2D1::SizeU(client.right - client.left, client.bottom - client.top));
  }

  void DiscardDeviceResources()
  {
    brush_.Reset();
    render_target_.Reset();
  }

  void UpdateScaleFromWindow()
  {
    scale_ = static_cast<float>(GetDpiForWindow(hwnd_)) / 96.0f;
    layout_.SetScale(scale_);
  }

  void RebuildLayout()
  {
    RECT client{};
    GetClientRect(hwnd_, &client);
    layout_.SetClientSize(static_cast<float>(client.right - client.left),
                          static_cast<float>(client.bottom - client.top));
    screen_ = layout_.BuildScreen();
    hover_ = HitTest(last_mouse_);
  }

  HitResult HitTest(const POINT point) const
  {
    if (screen_.sidebar_splitter.contains(point)) {
      return HitResult{.kind = HitResult::Kind::Splitter};
    }

    for (int area_index = 0; area_index < static_cast<int>(screen_.areas.size()); ++area_index) {
      const AreaModel &area = screen_.areas[area_index];
      for (int region_index = 0; region_index < static_cast<int>(area.regions.size()); ++region_index) {
        if (area.regions[region_index].bounds.contains(point)) {
          return HitResult{.kind = HitResult::Kind::Region, .area_index = area_index, .region_index = region_index};
        }
      }
    }
    return {};
  }

  bool UpdateCursor()
  {
    if (preferences_visible_ &&
        hovered_preferences_action_ != PreferencesAction::None &&
        hovered_preferences_action_ != PreferencesAction::ThemeField)
    {
      SetCursor(LoadCursorW(nullptr, IDC_HAND));
      return true;
    }
    if (preferences_visible_ && hovered_preferences_action_ == PreferencesAction::ThemeField) {
      SetCursor(LoadCursorW(nullptr, IDC_HAND));
      return true;
    }
    if (splash_visible_ && hovered_splash_action_ != SplashAction::None) {
      SetCursor(LoadCursorW(nullptr, IDC_HAND));
      return true;
    }
    SetCursor(LoadCursorW(nullptr, hover_.kind == HitResult::Kind::Splitter ? IDC_SIZEWE : IDC_ARROW));
    return true;
  }

  void OnMouseMove(const int x, const int y)
  {
    last_mouse_ = POINT{x, y};
    if (preferences_visible_) {
      const PreferencesAction next_action = HitTestPreferences(last_mouse_);
      if (next_action != hovered_preferences_action_) {
        hovered_preferences_action_ = next_action;
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      return;
    }

    if (splash_visible_) {
      const SplashAction next_action = HitTestSplash(last_mouse_);
      if (next_action != hovered_splash_action_) {
        hovered_splash_action_ = next_action;
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      return;
    }

    if (IsDropdownOpen()) {
      const int next_top_menu = HitTestTopMenu(last_mouse_);
      const int next_dropdown_item = HitTestDropdownItem(last_mouse_);
      bool changed = false;

      if (next_top_menu != -1 && next_top_menu != active_top_menu_) {
        active_top_menu_ = next_top_menu;
        hovered_dropdown_item_ = HitTestDropdownItem(last_mouse_);
        changed = true;
      }
      if (next_top_menu != hovered_top_menu_) {
        hovered_top_menu_ = next_top_menu;
        changed = true;
      }
      if (next_dropdown_item != hovered_dropdown_item_) {
        hovered_dropdown_item_ = next_dropdown_item;
        changed = true;
      }
      if (changed) {
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      return;
    }

    if (dragging_splitter_) {
      const float delta = static_cast<float>(x) - drag_start_x_;
      layout_.SetSidebarWidth(drag_start_sidebar_width_ - delta);
      screen_ = layout_.BuildScreen();
      hover_ = HitTest(last_mouse_);
      InvalidateRect(hwnd_, nullptr, FALSE);
      return;
    }

    const HitResult next = HitTest(last_mouse_);
    const int next_top_menu = HitTestTopMenu(last_mouse_);
    if (next.kind != hover_.kind || next.area_index != hover_.area_index || next.region_index != hover_.region_index ||
        next_top_menu != hovered_top_menu_)
    {
      hover_ = next;
      hovered_top_menu_ = next_top_menu;
      InvalidateRect(hwnd_, nullptr, FALSE);
    }
  }

  void OnLeftButtonDown(const int x, const int y)
  {
    last_mouse_ = POINT{x, y};
    if (preferences_visible_) {
      const PreferencesLayout layout = BuildPreferencesLayout();
      const PreferencesAction action = HitTestPreferences(last_mouse_);
      switch (action) {
        case PreferencesAction::Close:
          ClosePreferences();
          break;
        case PreferencesAction::ThemeField:
          preferences_theme_dropdown_open_ = !preferences_theme_dropdown_open_;
          hovered_preferences_action_ = action;
          InvalidateRect(hwnd_, nullptr, FALSE);
          break;
        case PreferencesAction::ThemeOptionDark:
          preferences_theme_dropdown_open_ = false;
          ApplyTheme(ThemeKind::Dark, true);
          break;
        case PreferencesAction::ThemeOptionLight:
          preferences_theme_dropdown_open_ = false;
          ApplyTheme(ThemeKind::Light, true);
          break;
        case PreferencesAction::ThemeOptionColorWave:
          preferences_theme_dropdown_open_ = false;
          ApplyTheme(ThemeKind::ColorWave, true);
          break;
        case PreferencesAction::ThemePreviewDark:
          preferences_theme_dropdown_open_ = false;
          ApplyTheme(ThemeKind::Dark, true);
          break;
        case PreferencesAction::ThemePreviewLight:
          preferences_theme_dropdown_open_ = false;
          ApplyTheme(ThemeKind::Light, true);
          break;
        case PreferencesAction::ThemePreviewColorWave:
          preferences_theme_dropdown_open_ = false;
          ApplyTheme(ThemeKind::ColorWave, true);
          break;
        case PreferencesAction::None:
          if (!layout.card.contains(last_mouse_)) {
            ClosePreferences();
          }
          else if (preferences_theme_dropdown_open_ && !layout.theme_dropdown.contains(last_mouse_) &&
                   !layout.theme_field.contains(last_mouse_))
          {
            preferences_theme_dropdown_open_ = false;
            hovered_preferences_action_ = PreferencesAction::None;
            InvalidateRect(hwnd_, nullptr, FALSE);
          }
          break;
      }
      return;
    }

    if (splash_visible_) {
      const SplashLayout layout = BuildSplashLayout();
      const SplashAction action = HitTestSplash(last_mouse_);
      switch (action) {
        case SplashAction::Close:
        case SplashAction::Continue:
          CloseSplash();
          break;
        case SplashAction::Manual:
          ShellExecuteW(hwnd_,
                        L"open",
                        L"https://docs.blender.org/manual/en/latest/",
                        nullptr,
                        nullptr,
                        SW_SHOWNORMAL);
          break;
        case SplashAction::None:
          if (!layout.card.contains(last_mouse_)) {
            CloseSplash();
          }
          break;
      }
      return;
    }

    const int top_menu = HitTestTopMenu(last_mouse_);
    if (IsDropdownOpen()) {
      const int dropdown_item = HitTestDropdownItem(last_mouse_);
      if (dropdown_item != -1) {
        const DropdownMenuLayout layout = BuildDropdownLayout();
        const UINT command = layout.items[dropdown_item].command;
        CloseTopMenu();
        HandleMenuCommand(command);
        return;
      }

      if (top_menu != -1) {
        OpenTopMenu(top_menu);
      }
      else {
        CloseTopMenu();
      }
      return;
    }

    hover_ = HitTest(last_mouse_);
    if (top_menu != -1) {
      hovered_top_menu_ = top_menu;
      OpenTopMenu(top_menu);
      return;
    }
    if (hover_.kind == HitResult::Kind::Splitter) {
      dragging_splitter_ = true;
      drag_start_x_ = static_cast<float>(x);
      drag_start_sidebar_width_ = layout_.SidebarWidth();
      SetCapture(hwnd_);
    }
  }

  void OnLeftButtonUp()
  {
    if (dragging_splitter_) {
      dragging_splitter_ = false;
      ReleaseCapture();
    }
  }

  void SetBrushColor(const D2D1_COLOR_F color)
  {
    brush_->SetColor(color);
  }

  void FillRect(const Rect &rect, const D2D1_COLOR_F color)
  {
    SetBrushColor(color);
    render_target_->FillRectangle(to_d2d_rect(rect), brush_.Get());
  }

  void DrawRect(const Rect &rect, const D2D1_COLOR_F color, const float stroke = 1.0f)
  {
    SetBrushColor(color);
    render_target_->DrawRectangle(to_d2d_rect(rect), brush_.Get(), stroke);
  }

  void FillRoundedRect(const Rect &rect, const D2D1_COLOR_F color, const float radius)
  {
    SetBrushColor(color);
    render_target_->FillRoundedRectangle(D2D1::RoundedRect(to_d2d_rect(rect), radius, radius), brush_.Get());
  }

  void DrawRoundedRect(const Rect &rect,
                       const D2D1_COLOR_F color,
                       const float radius,
                       const float stroke = 1.0f)
  {
    SetBrushColor(color);
    render_target_->DrawRoundedRectangle(
        D2D1::RoundedRect(to_d2d_rect(rect), radius, radius), brush_.Get(), stroke);
  }

  void DrawLine(const D2D1_POINT_2F from, const D2D1_POINT_2F to, const D2D1_COLOR_F color, const float stroke = 1.0f)
  {
    SetBrushColor(color);
    render_target_->DrawLine(from, to, brush_.Get(), stroke);
  }

  void DrawTextLine(std::wstring_view text,
                    const Rect &rect,
                    IDWriteTextFormat *format,
                    const D2D1_COLOR_F color,
                    const DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING)
  {
    format->SetTextAlignment(alignment);
    SetBrushColor(color);
    render_target_->DrawTextW(text.data(),
                              static_cast<UINT32>(text.size()),
                              format,
                              to_d2d_rect(rect),
                              brush_.Get());
  }

  bool IsHovered(const RegionModel &region) const
  {
    return hover_.kind == HitResult::Kind::Region && hover_.area_index >= 0 && hover_.region_index >= 0 &&
           screen_.areas[hover_.area_index].regions[hover_.region_index].id == region.id;
  }

  void DrawRegionChrome(const RegionModel &region, const bool hovered)
  {
    D2D1_COLOR_F fill = kTheme.panel;
    if (region.role == RegionRole::Header || region.role == RegionRole::Statusbar) {
      fill = kTheme.chrome;
    }
    else if (region.role == RegionRole::Window) {
      fill = kTheme.viewport;
    }
    else if (region.role == RegionRole::Sidebar) {
      fill = kTheme.panel_alt;
    }

    FillRect(region.bounds, fill);
    DrawRect(inset(region.bounds, 0.5f), kTheme.line_soft, 1.0f);
    if (hovered) {
      DrawRect(inset(region.bounds, 1.5f), with_alpha(kTheme.accent, 0.40f), 1.0f);
    }
  }

  void DrawTopBar(const RegionModel &region)
  {
    DrawRegionChrome(region, IsHovered(region));

    const float top = region.bounds.top;
    const float bottom = region.bounds.bottom;
    const float left = region.bounds.left;
    const float right = region.bounds.right;

    FillRect(Rect{left, bottom - 1.0f, right, bottom}, with_alpha(kTheme.line, 0.90f));

    DrawTextLine(L"win64darkui",
                 Rect{14.0f * scale_, top, 112.0f * scale_, bottom},
                 small_bold_format_.Get(),
                 kTheme.text);

    FillRect(Rect{118.0f * scale_, top + 7.0f * scale_, 119.0f * scale_, bottom - 7.0f * scale_},
             with_alpha(kTheme.line, 0.75f));

    const std::array<std::wstring_view, 5> menus = {L"File", L"Edit", L"Render", L"Window", L"Help"};
    const TopMenuLayout menu_layout = BuildTopMenuLayout(region);
    float menu_x = menu_layout.rects.back().right + (8.0f * scale_);
    for (size_t index = 0; index < menus.size(); ++index) {
      const Rect menu_rect = menu_layout.rects[index];
      if (static_cast<int>(index) == hovered_top_menu_ || static_cast<int>(index) == active_top_menu_) {
        FillRect(Rect{menu_rect.left - 4.0f * scale_, top + 4.0f * scale_, menu_rect.right + 4.0f * scale_, bottom - 4.0f * scale_},
                 with_alpha(kTheme.hover, static_cast<int>(index) == active_top_menu_ ? 0.72f : 0.50f));
      }
      DrawTextLine(menus[index], menu_rect, menu_format_.Get(), kTheme.text);
    }

    FillRect(Rect{menu_x + 6.0f * scale_, top + 7.0f * scale_, menu_x + 7.0f * scale_, bottom - 7.0f * scale_},
             with_alpha(kTheme.line, 0.75f));

    const float workspace_limit = right - (280.0f * scale_);
    const std::array<std::wstring_view, 6> tabs = {
        L"Layout", L"Modeling", L"Sculpting", L"UV Editing", L"Shading", L"Geometry Nodes"};
    float tab_x = menu_x + 22.0f * scale_;
    for (size_t index = 0; index < tabs.size(); ++index) {
      const float width = (index == 3 ? 86.0f : (index == 5 ? 108.0f : 72.0f)) * scale_;
      if ((tab_x + width) > workspace_limit) {
        break;
      }

      const Rect tab_rect{tab_x, top + 3.0f * scale_, tab_x + width, bottom - 2.0f * scale_};
      if (index == 0) {
        FillRect(tab_rect, with_alpha(kTheme.hover, 0.55f));
        FillRect(Rect{tab_rect.left + 8.0f * scale_, bottom - 3.0f * scale_, tab_rect.right - 8.0f * scale_, bottom - 1.0f * scale_},
                 kTheme.accent);
      }
      DrawTextLine(tabs[index],
                   tab_rect,
                   index == 0 ? small_bold_format_.Get() : menu_format_.Get(),
                   index == 0 ? kTheme.text : kTheme.text_muted,
                   DWRITE_TEXT_ALIGNMENT_CENTER);
      tab_x += width + (4.0f * scale_);
    }

    const Rect scene_rect{right - 254.0f * scale_, top + 5.0f * scale_, right - 186.0f * scale_, bottom - 5.0f * scale_};
    const Rect layer_rect{right - 180.0f * scale_, top + 5.0f * scale_, right - 92.0f * scale_, bottom - 5.0f * scale_};
    FillRect(scene_rect, with_alpha(kTheme.hover, 0.42f));
    FillRect(layer_rect, with_alpha(kTheme.hover, 0.42f));
    DrawTextLine(L"Scene", scene_rect, menu_format_.Get(), kTheme.text, DWRITE_TEXT_ALIGNMENT_CENTER);
    DrawTextLine(L"ViewLayer", layer_rect, menu_format_.Get(), kTheme.text, DWRITE_TEXT_ALIGNMENT_CENTER);
    DrawTextLine(L"16.4 ms",
                 Rect{right - 86.0f * scale_, top, right - 12.0f * scale_, bottom},
                 menu_format_.Get(),
                 kTheme.text_muted,
                 DWRITE_TEXT_ALIGNMENT_TRAILING);
  }

  void DrawDropdownMenu()
  {
    if (!IsDropdownOpen()) {
      return;
    }

    const DropdownMenuLayout layout = BuildDropdownLayout();
    if (layout.items.empty()) {
      return;
    }

    const Rect shadow = Rect{
        layout.bounds.left + (8.0f * scale_),
        layout.bounds.top + (10.0f * scale_),
        layout.bounds.right + (8.0f * scale_),
        layout.bounds.bottom + (10.0f * scale_),
    };
    FillRoundedRect(shadow, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.28f), 12.0f * scale_);
    FillRoundedRect(layout.bounds, kTheme.panel, 8.0f * scale_);
    DrawRoundedRect(layout.bounds, with_alpha(kTheme.line, 0.95f), 8.0f * scale_, 1.0f);

    for (int index = 0; index < static_cast<int>(layout.items.size()); ++index) {
      const DropdownMenuItem &item = layout.items[index];
      const Rect &item_rect = layout.item_rects[index];

      if (item.separator) {
        const float y = (item_rect.top + item_rect.bottom) * 0.5f;
        DrawLine(D2D1::Point2F(item_rect.left + (10.0f * scale_), y),
                 D2D1::Point2F(item_rect.right - (10.0f * scale_), y),
                 with_alpha(kTheme.line, 0.85f),
                 1.0f);
        continue;
      }

      if (index == hovered_dropdown_item_) {
        FillRoundedRect(item_rect, with_alpha(kTheme.hover, 0.92f), 6.0f * scale_);
      }

      const Rect indicator = Rect{
          item_rect.left + (8.0f * scale_),
          item_rect.top + (6.0f * scale_),
          item_rect.left + (24.0f * scale_),
          item_rect.bottom - (6.0f * scale_),
      };
      if (item.checkable) {
        if (item.checked) {
          FillRoundedRect(indicator, with_alpha(kTheme.accent, 0.20f), 4.0f * scale_);
          FillRect(Rect{
                       indicator.left + (4.0f * scale_),
                       indicator.top + (4.0f * scale_),
                       indicator.right - (4.0f * scale_),
                       indicator.bottom - (4.0f * scale_),
                   },
                   kTheme.accent);
        }
        else {
          DrawRect(indicator, with_alpha(kTheme.line, 0.60f), 1.0f);
        }
      }

      DrawTextLine(item.label,
                   Rect{item_rect.left + (34.0f * scale_), item_rect.top, item_rect.right - (72.0f * scale_), item_rect.bottom},
                   menu_format_.Get(),
                   kTheme.text);

      if (!item.secondary.empty()) {
        DrawTextLine(item.secondary,
                     Rect{item_rect.right - (92.0f * scale_), item_rect.top, item_rect.right - (10.0f * scale_), item_rect.bottom},
                     menu_format_.Get(),
                     kTheme.text_muted,
                     DWRITE_TEXT_ALIGNMENT_TRAILING);
      }
    }
  }

  void DrawEditorHeader(const RegionModel &region)
  {
    DrawRegionChrome(region, IsHovered(region));

    const float top = region.bounds.top;
    const float bottom = region.bounds.bottom;
    const Rect mode_rect{12.0f * scale_, top + 4.0f * scale_, 110.0f * scale_, bottom - 4.0f * scale_};
    FillRoundedRect(mode_rect, with_alpha(kTheme.hover, 0.50f), 4.0f * scale_);
    DrawTextLine(L"Object Mode", mode_rect, menu_format_.Get(), kTheme.text, DWRITE_TEXT_ALIGNMENT_CENTER);

    std::array<std::wstring_view, 4> modes = {L"Wire", L"Solid", L"Material", L"Render"};
    float x = 126.0f * scale_;
    for (size_t index = 0; index < modes.size(); ++index) {
      Rect button{x, top + 5.0f * scale_, x + 62.0f * scale_, bottom - 5.0f * scale_};
      if (index == 1) {
        FillRect(button, with_alpha(kTheme.hover, 0.55f));
        FillRect(Rect{button.left + 8.0f * scale_, bottom - 3.0f * scale_, button.right - 8.0f * scale_, bottom - 1.0f * scale_},
                 kTheme.accent);
      }
      DrawTextLine(modes[index],
                   button,
                   menu_format_.Get(),
                   index == 1 ? kTheme.text : kTheme.text_muted,
                   DWRITE_TEXT_ALIGNMENT_CENTER);
      x += 68.0f * scale_;
    }

    DrawTextLine(L"Camera Perspective     Collection | Cube | Light",
                 Rect{region.bounds.right - 420.0f * scale_, top, region.bounds.right - 18.0f * scale_, bottom},
                 menu_format_.Get(),
                 kTheme.text_muted,
                 DWRITE_TEXT_ALIGNMENT_TRAILING);
  }

  void DrawToolbar(const RegionModel &region)
  {
    DrawRegionChrome(region, IsHovered(region));

    std::array<std::wstring_view, 5> tools = {L"SEL", L"MOV", L"ROT", L"SCL", L"CRS"};
    float y = region.bounds.top + 18.0f * scale_;
    for (size_t index = 0; index < tools.size(); ++index) {
      Rect button{region.bounds.left + 12.0f * scale_, y, region.bounds.right - 12.0f * scale_, y + 34.0f * scale_};
      FillRoundedRect(button, index == 0 ? with_alpha(kTheme.accent, 0.20f) : with_alpha(kTheme.hover, 0.35f), 9.0f * scale_);
      DrawTextLine(tools[index],
                   button,
                   small_bold_format_.Get(),
                   index == 0 ? kTheme.accent : kTheme.text_muted,
                   DWRITE_TEXT_ALIGNMENT_CENTER);
      y += 46.0f * scale_;
    }
  }

  void DrawViewport(const RegionModel &region)
  {
    DrawRegionChrome(region, IsHovered(region));

    const Rect inner = inset(region.bounds, 22.0f * scale_);
    DrawTextLine(L"Viewport",
                 Rect{inner.left, inner.top, inner.left + 180.0f * scale_, inner.top + 28.0f * scale_},
                 title_format_.Get(),
                 kTheme.text);
    DrawTextLine(L"Blender-like region shell. Next step: real widget tree + event routing.",
                 Rect{inner.left, inner.top + 26.0f * scale_, inner.left + 520.0f * scale_, inner.top + 56.0f * scale_},
                 ui_format_.Get(),
                 kTheme.text_muted);

    for (float x = inner.left; x <= inner.right; x += 32.0f * scale_) {
      DrawLine(D2D1::Point2F(x, inner.top + 54.0f * scale_),
               D2D1::Point2F(x, inner.bottom - 18.0f * scale_),
               with_alpha(kTheme.line, 0.16f));
    }
    for (float y = inner.top + 54.0f * scale_; y <= inner.bottom; y += 32.0f * scale_) {
      DrawLine(D2D1::Point2F(inner.left, y), D2D1::Point2F(inner.right, y), with_alpha(kTheme.line, 0.16f));
    }

    const float center_x = (inner.left + inner.right) * 0.5f;
    const float center_y = (inner.top + inner.bottom) * 0.56f;
    Rect cube{center_x - 64.0f * scale_, center_y - 64.0f * scale_, center_x + 64.0f * scale_, center_y + 64.0f * scale_};
    FillRoundedRect(cube, with_alpha(kTheme.accent, 0.10f), 16.0f * scale_);
    DrawRect(cube, with_alpha(kTheme.accent, 0.65f), 2.0f);
    DrawLine(D2D1::Point2F(cube.left, cube.bottom), D2D1::Point2F(cube.right, cube.top), with_alpha(kTheme.accent, 0.35f), 2.0f);
    DrawLine(D2D1::Point2F(cube.left, cube.top), D2D1::Point2F(cube.right, cube.bottom), with_alpha(kTheme.accent, 0.20f), 2.0f);
  }

  void DrawSidebar(const RegionModel &region)
  {
    DrawRegionChrome(region, IsHovered(region));

    Rect panel = inset(region.bounds, 16.0f * scale_);
    panel.bottom = panel.top + 104.0f * scale_;
    FillRoundedRect(panel, with_alpha(kTheme.hover, 0.65f), 12.0f * scale_);
    DrawTextLine(L"Scene Collection",
                 Rect{panel.left + 12.0f * scale_, panel.top + 8.0f * scale_, panel.right, panel.top + 34.0f * scale_},
                 small_bold_format_.Get(),
                 kTheme.text);
    DrawTextLine(L"Camera\nCube\nLight",
                 Rect{panel.left + 12.0f * scale_, panel.top + 34.0f * scale_, panel.right - 12.0f * scale_, panel.bottom - 10.0f * scale_},
                 ui_format_.Get(),
                 kTheme.text_muted);

    panel.top += 122.0f * scale_;
    panel.bottom = panel.top + 120.0f * scale_;
    FillRoundedRect(panel, with_alpha(kTheme.hover, 0.48f), 12.0f * scale_);
    DrawTextLine(L"Transform",
                 Rect{panel.left + 12.0f * scale_, panel.top + 8.0f * scale_, panel.right, panel.top + 34.0f * scale_},
                 small_bold_format_.Get(),
                 kTheme.text);
    DrawTextLine(L"Location  0.00  0.00  0.00\nRotation  0.00  0.00  0.00\nScale     1.00  1.00  1.00",
                 Rect{panel.left + 12.0f * scale_, panel.top + 34.0f * scale_, panel.right - 16.0f * scale_, panel.bottom - 10.0f * scale_},
                 ui_format_.Get(),
                 kTheme.text_muted);

    panel.top += 138.0f * scale_;
    panel.bottom = panel.top + 96.0f * scale_;
    FillRoundedRect(panel, with_alpha(kTheme.hover, 0.40f), 12.0f * scale_);
    DrawTextLine(L"Modifiers",
                 Rect{panel.left + 12.0f * scale_, panel.top + 8.0f * scale_, panel.right, panel.top + 34.0f * scale_},
                 small_bold_format_.Get(),
                 kTheme.text);
    DrawTextLine(L"+ Add Modifier\nSubdivision Surface",
                 Rect{panel.left + 12.0f * scale_, panel.top + 34.0f * scale_, panel.right - 16.0f * scale_, panel.bottom - 10.0f * scale_},
                 ui_format_.Get(),
                 kTheme.text_muted);
  }

  void DrawStatusbar(const RegionModel &region)
  {
    DrawRegionChrome(region, IsHovered(region));
    DrawTextLine(L"LMB Select   MMB Orbit   Shift+MMB Pan   Ctrl+Space Maximize Area",
                 Rect{18.0f * scale_, region.bounds.top, region.bounds.right - 220.0f * scale_, region.bounds.bottom},
                 ui_format_.Get(),
                 kTheme.text_muted);
    DrawTextLine(L"v0.1 shell",
                 Rect{region.bounds.right - 180.0f * scale_, region.bounds.top, region.bounds.right - 18.0f * scale_, region.bounds.bottom},
                 small_bold_format_.Get(),
                 kTheme.accent,
                 DWRITE_TEXT_ALIGNMENT_TRAILING);
  }

  void DrawArea(const AreaModel &area)
  {
    for (const RegionModel &region : area.regions) {
      switch (region.role) {
        case RegionRole::Header:
          if (area.role == AreaRole::Global) {
            DrawTopBar(region);
          }
          else {
            DrawEditorHeader(region);
          }
          break;
        case RegionRole::Toolbar:
          DrawToolbar(region);
          break;
        case RegionRole::Window:
          DrawViewport(region);
          break;
        case RegionRole::Sidebar:
          DrawSidebar(region);
          break;
        case RegionRole::Statusbar:
          DrawStatusbar(region);
          break;
      }
    }
  }

  void DrawPreferencesOverlay()
  {
    if (!preferences_visible_) {
      return;
    }

    const PreferencesLayout layout = BuildPreferencesLayout();
    FillRect(screen_.bounds, D2D1::ColorF(0.04f, 0.05f, 0.07f, 0.54f));

    const Rect shadow = Rect{
        layout.card.left + (10.0f * scale_),
        layout.card.top + (14.0f * scale_),
        layout.card.right + (10.0f * scale_),
        layout.card.bottom + (14.0f * scale_),
    };
    FillRoundedRect(shadow, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.22f), 16.0f * scale_);
    FillRoundedRect(layout.card, kTheme.chrome, 16.0f * scale_);
    DrawRoundedRect(layout.card, with_alpha(kTheme.line, 0.90f), 16.0f * scale_, 1.0f);

    DrawTextLine(L"Preferences",
                 Rect{layout.card.left + (24.0f * scale_), layout.card.top + (18.0f * scale_), layout.card.right - (80.0f * scale_), layout.card.top + (56.0f * scale_)},
                 title_format_.Get(),
                 kTheme.text);
    DrawTextLine(L"Theme selection is applied immediately and saved for the next launch.",
                 Rect{layout.card.left + (24.0f * scale_), layout.card.top + (56.0f * scale_), layout.card.right - (32.0f * scale_), layout.card.top + (86.0f * scale_)},
                 ui_format_.Get(),
                 kTheme.text_muted);

    FillRoundedRect(layout.close_button,
                    hovered_preferences_action_ == PreferencesAction::Close ? with_alpha(kTheme.hover, 0.95f) :
                                                                             with_alpha(kTheme.hover, 0.60f),
                    8.0f * scale_);
    DrawTextLine(L"X", layout.close_button, small_bold_format_.Get(), kTheme.text, DWRITE_TEXT_ALIGNMENT_CENTER);

    DrawTextLine(L"Theme",
                 Rect{layout.card.left + (34.0f * scale_), layout.theme_field.top, layout.card.left + (134.0f * scale_), layout.theme_field.bottom},
                 small_bold_format_.Get(),
                 kTheme.text);

    FillRoundedRect(layout.theme_field,
                    hovered_preferences_action_ == PreferencesAction::ThemeField ? with_alpha(kTheme.hover, 0.92f) :
                                                                                  with_alpha(kTheme.hover, 0.62f),
                    8.0f * scale_);
    DrawRoundedRect(layout.theme_field, with_alpha(kTheme.line, 0.78f), 8.0f * scale_, 1.0f);
    DrawTextLine(ThemeLabel(theme_kind_),
                 Rect{layout.theme_field.left + (14.0f * scale_), layout.theme_field.top, layout.theme_field.right - (36.0f * scale_), layout.theme_field.bottom},
                 menu_format_.Get(),
                 kTheme.text);
    DrawTextLine(preferences_theme_dropdown_open_ ? L"^" : L"v",
                 Rect{layout.theme_field.right - (28.0f * scale_), layout.theme_field.top, layout.theme_field.right - (10.0f * scale_), layout.theme_field.bottom},
                 small_bold_format_.Get(),
                 kTheme.text_muted,
                 DWRITE_TEXT_ALIGNMENT_CENTER);

    const std::array<ThemeKind, 3> themes = {ThemeKind::Dark, ThemeKind::Light, ThemeKind::ColorWave};
    for (int index = 0; index < 3; ++index) {
      const ThemeKind kind = themes[index];
      Theme preview = MakeTheme(kind);
      const Rect &preview_rect = layout.preview_rects[index];
      FillRoundedRect(preview_rect, preview.chrome, 10.0f * scale_);
      FillRect(Rect{preview_rect.left + (10.0f * scale_), preview_rect.top + (10.0f * scale_), preview_rect.right - (10.0f * scale_), preview_rect.top + (20.0f * scale_)},
               preview.accent);
      FillRect(Rect{preview_rect.left + (10.0f * scale_), preview_rect.top + (26.0f * scale_), preview_rect.left + (46.0f * scale_), preview_rect.bottom - (10.0f * scale_)},
               preview.panel_alt);
      FillRect(Rect{preview_rect.left + (52.0f * scale_), preview_rect.top + (26.0f * scale_), preview_rect.right - (10.0f * scale_), preview_rect.bottom - (10.0f * scale_)},
               preview.viewport);
      const PreferencesAction preview_action = static_cast<PreferencesAction>(static_cast<int>(PreferencesAction::ThemePreviewDark) + index);
      if (hovered_preferences_action_ == preview_action) {
        DrawRoundedRect(preview_rect, with_alpha(kTheme.text, 0.45f), 10.0f * scale_, 2.0f);
      }
      if (kind == theme_kind_) {
        DrawRoundedRect(preview_rect, preview.accent, 10.0f * scale_, 2.0f);
      }
      DrawTextLine(ThemeLabel(kind),
                   Rect{preview_rect.left, preview_rect.bottom + (8.0f * scale_), preview_rect.right, preview_rect.bottom + (28.0f * scale_)},
                   menu_format_.Get(),
                   kTheme.text_muted,
                   DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    if (preferences_theme_dropdown_open_) {
      FillRoundedRect(layout.theme_dropdown, kTheme.panel, 8.0f * scale_);
      DrawRoundedRect(layout.theme_dropdown, with_alpha(kTheme.line, 0.90f), 8.0f * scale_, 1.0f);
      for (int index = 0; index < 3; ++index) {
        const PreferencesAction action = static_cast<PreferencesAction>(static_cast<int>(PreferencesAction::ThemeOptionDark) + index);
        const Rect &option_rect = layout.option_rects[index];
        if (hovered_preferences_action_ == action) {
          FillRoundedRect(option_rect, with_alpha(kTheme.hover, 0.92f), 6.0f * scale_);
        }

        const ThemeKind kind = themes[index];
        const Rect swatch{option_rect.left + (10.0f * scale_), option_rect.top + (7.0f * scale_), option_rect.left + (28.0f * scale_), option_rect.bottom - (7.0f * scale_)};
        FillRoundedRect(swatch, MakeTheme(kind).accent, 4.0f * scale_);
        DrawTextLine(ThemeLabel(kind),
                     Rect{option_rect.left + (38.0f * scale_), option_rect.top, option_rect.right - (34.0f * scale_), option_rect.bottom},
                     menu_format_.Get(),
                     kTheme.text);
        if (kind == theme_kind_) {
          DrawTextLine(L"OK",
                       Rect{option_rect.right - (30.0f * scale_), option_rect.top, option_rect.right - (10.0f * scale_), option_rect.bottom},
                       small_bold_format_.Get(),
                       kTheme.accent,
                       DWRITE_TEXT_ALIGNMENT_CENTER);
        }
      }
    }
  }

  void DrawSplashOverlay()
  {
    if (!splash_visible_) {
      return;
    }

    const SplashLayout layout = BuildSplashLayout();
    FillRect(screen_.bounds, D2D1::ColorF(0.04f, 0.05f, 0.07f, 0.62f));

    const Rect shadow = Rect{
        layout.card.left + (10.0f * scale_),
        layout.card.top + (14.0f * scale_),
        layout.card.right + (10.0f * scale_),
        layout.card.bottom + (14.0f * scale_),
    };
    FillRoundedRect(shadow, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.24f), 20.0f * scale_);
    FillRoundedRect(layout.card, kTheme.chrome, 18.0f * scale_);
    DrawRoundedRect(layout.card, with_alpha(kTheme.line, 0.90f), 18.0f * scale_, 1.0f);

    const Rect hero = Rect{
        layout.card.left + (20.0f * scale_),
        layout.card.top + (20.0f * scale_),
        layout.card.right - (20.0f * scale_),
        layout.card.top + (168.0f * scale_),
    };
    FillRoundedRect(hero, D2D1::ColorF(0.15f, 0.18f, 0.24f, 1.0f), 14.0f * scale_);
    FillRect(Rect{hero.left, hero.bottom - (4.0f * scale_), hero.right, hero.bottom}, kTheme.accent);

    const Rect accent_panel = Rect{
        hero.left + (18.0f * scale_),
        hero.top + (18.0f * scale_),
        hero.left + (166.0f * scale_),
        hero.bottom - (18.0f * scale_),
    };
    FillRoundedRect(accent_panel, with_alpha(kTheme.accent, 0.14f), 14.0f * scale_);
    DrawTextLine(L"4.5",
                 Rect{accent_panel.left, accent_panel.top + (6.0f * scale_), accent_panel.right, accent_panel.top + (56.0f * scale_)},
                 splash_title_format_.Get(),
                 kTheme.accent,
                 DWRITE_TEXT_ALIGNMENT_CENTER);
    DrawTextLine(L"UI Shell",
                 Rect{accent_panel.left, accent_panel.top + (58.0f * scale_), accent_panel.right, accent_panel.top + (92.0f * scale_)},
                 small_bold_format_.Get(),
                 kTheme.text_muted,
                 DWRITE_TEXT_ALIGNMENT_CENTER);

    DrawTextLine(L"win64darkui",
                 Rect{hero.left + (196.0f * scale_), hero.top + (24.0f * scale_), hero.right - (80.0f * scale_), hero.top + (72.0f * scale_)},
                 splash_title_format_.Get(),
                 kTheme.text);
    DrawTextLine(L"Blender-inspired Windows native UI shell",
                 Rect{hero.left + (196.0f * scale_), hero.top + (76.0f * scale_), hero.right - (60.0f * scale_), hero.top + (106.0f * scale_)},
                 splash_body_format_.Get(),
                 kTheme.text_muted);
    DrawTextLine(L"Custom regions, menu bar, fullscreen toggle, and the next layer for block-based widgets.",
                 Rect{hero.left + (196.0f * scale_), hero.top + (106.0f * scale_), hero.right - (44.0f * scale_), hero.bottom - (18.0f * scale_)},
                 splash_body_format_.Get(),
                 kTheme.text_muted);

    FillRoundedRect(layout.close_button,
                    hovered_splash_action_ == SplashAction::Close ? with_alpha(kTheme.hover, 0.95f) :
                                                                    with_alpha(kTheme.hover, 0.60f),
                    8.0f * scale_);
    DrawTextLine(L"X", layout.close_button, small_bold_format_.Get(), kTheme.text, DWRITE_TEXT_ALIGNMENT_CENTER);

    const float column_top = hero.bottom + (24.0f * scale_);
    const float column_mid = layout.card.left + (360.0f * scale_);
    DrawTextLine(L"Current Foundation",
                 Rect{layout.card.left + (30.0f * scale_), column_top, column_mid - (20.0f * scale_), column_top + (24.0f * scale_)},
                 small_bold_format_.Get(),
                 kTheme.text);
    DrawTextLine(L"- Window / Screen / Area / Region layering\n- Thin Blender-like top menu bar\n- Sidebar splitter interaction\n- Fullscreen menu command with toggle state",
                 Rect{layout.card.left + (30.0f * scale_), column_top + (30.0f * scale_), column_mid - (20.0f * scale_), layout.card.bottom - (106.0f * scale_)},
                 splash_body_format_.Get(),
                 kTheme.text_muted);

    DrawTextLine(L"Next Steps",
                 Rect{column_mid, column_top, layout.card.right - (30.0f * scale_), column_top + (24.0f * scale_)},
                 small_bold_format_.Get(),
                 kTheme.text);
    DrawTextLine(L"1. UI block/button system\n2. Panel collapse and tab state\n3. Blender-style area split/join\n4. Real viewport interaction",
                 Rect{column_mid, column_top + (30.0f * scale_), layout.card.right - (30.0f * scale_), layout.card.bottom - (106.0f * scale_)},
                 splash_body_format_.Get(),
                 kTheme.text_muted);

    FillRoundedRect(layout.manual_button,
                    hovered_splash_action_ == SplashAction::Manual ? with_alpha(kTheme.hover, 0.92f) :
                                                                     with_alpha(kTheme.hover, 0.62f),
                    10.0f * scale_);
    DrawTextLine(L"Online Manual",
                 layout.manual_button,
                 small_bold_format_.Get(),
                 kTheme.text,
                 DWRITE_TEXT_ALIGNMENT_CENTER);

    FillRoundedRect(layout.continue_button,
                    hovered_splash_action_ == SplashAction::Continue ? with_alpha(kTheme.accent, 0.32f) :
                                                                       with_alpha(kTheme.accent, 0.22f),
                    10.0f * scale_);
    DrawTextLine(L"Continue",
                 layout.continue_button,
                 small_bold_format_.Get(),
                 kTheme.accent,
                 DWRITE_TEXT_ALIGNMENT_CENTER);
  }

  void DrawBackground()
  {
    ComPtr<ID2D1GradientStopCollection> stops;
    const std::array<D2D1_GRADIENT_STOP, 2> gradient_stops = {{{0.0f, kTheme.background_a}, {1.0f, kTheme.background_b}}};
    render_target_->CreateGradientStopCollection(gradient_stops.data(),
                                                 static_cast<UINT32>(gradient_stops.size()),
                                                 D2D1_GAMMA_2_2,
                                                 D2D1_EXTEND_MODE_CLAMP,
                                                 stops.ReleaseAndGetAddressOf());

    ComPtr<ID2D1LinearGradientBrush> gradient;
    render_target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.0f),
                                                                                  D2D1::Point2F(0.0f, screen_.bounds.bottom)),
                                              stops.Get(),
                                              gradient.ReleaseAndGetAddressOf());

    render_target_->FillRectangle(to_d2d_rect(screen_.bounds), gradient.Get());
  }

  void DrawSplitter()
  {
    const D2D1_COLOR_F color = dragging_splitter_ ? with_alpha(kTheme.accent, 0.30f) :
                                                   (hover_.kind == HitResult::Kind::Splitter ? with_alpha(kTheme.accent, 0.18f) :
                                                                                               kTheme.splitter);
    FillRect(screen_.sidebar_splitter, color);
  }

  void OnPaint()
  {
    PAINTSTRUCT paint{};
    BeginPaint(hwnd_, &paint);

    if (!CreateDeviceResources()) {
      EndPaint(hwnd_, &paint);
      return;
    }

    render_target_->BeginDraw();
    DrawBackground();
    for (const AreaModel &area : screen_.areas) {
      DrawArea(area);
    }
    DrawSplitter();
    DrawDropdownMenu();
    DrawPreferencesOverlay();
    DrawSplashOverlay();

    if (render_target_->EndDraw() == D2DERR_RECREATE_TARGET) {
      DiscardDeviceResources();
    }

    EndPaint(hwnd_, &paint);
  }

  HWND hwnd_ = nullptr;
  ComPtr<ID2D1Factory> factory_;
  ComPtr<IDWriteFactory> write_factory_;
  ComPtr<ID2D1HwndRenderTarget> render_target_;
  ComPtr<ID2D1SolidColorBrush> brush_;
  ComPtr<IDWriteTextFormat> title_format_;
  ComPtr<IDWriteTextFormat> ui_format_;
  ComPtr<IDWriteTextFormat> small_bold_format_;
  ComPtr<IDWriteTextFormat> menu_format_;
  ComPtr<IDWriteTextFormat> splash_title_format_;
  ComPtr<IDWriteTextFormat> splash_body_format_;

  LayoutEngine layout_;
  ScreenModel screen_;
  HitResult hover_;
  POINT last_mouse_{};
  float scale_ = 1.0f;
  bool dragging_splitter_ = false;
  float drag_start_x_ = 0.0f;
  float drag_start_sidebar_width_ = 0.0f;
  int hovered_top_menu_ = -1;
  int active_top_menu_ = -1;
  int hovered_dropdown_item_ = -1;
  bool is_fullscreen_ = false;
  DWORD windowed_style_ = WS_OVERLAPPEDWINDOW;
  WINDOWPLACEMENT windowed_placement_{sizeof(WINDOWPLACEMENT)};
  bool splash_visible_ = false;
  SplashAction hovered_splash_action_ = SplashAction::None;
  bool preferences_visible_ = false;
  bool preferences_theme_dropdown_open_ = false;
  PreferencesAction hovered_preferences_action_ = PreferencesAction::None;
  ThemeKind theme_kind_ = ThemeKind::Dark;
  std::filesystem::path settings_path_;
};

}  // namespace darkui

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
  darkui::AppWindow app;
  return app.Run(instance);
}
