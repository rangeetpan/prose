// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
<<<<<<< HEAD
#include "content/public/test/browser_test_utils.h"
=======
#include "content/public/test/browser_test.h"
>>>>>>> eaac5b5035fe189b6706e1637122e37134206059
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "chrome/browser/ui/views/accessibility/uia_accessibility_event_waiter.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"
#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"

#define EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(safearray, expected_property_values) \
  {                                                                         \
    EXPECT_EQ(sizeof(V_R8(LPVARIANT(NULL))),                                \
              ::SafeArrayGetElemsize(safearray));                           \
    ASSERT_EQ(1u, SafeArrayGetDim(safearray));                              \
    LONG array_lower_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetLBound(safearray, 1, &array_lower_bound));              \
    LONG array_upper_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetUBound(safearray, 1, &array_upper_bound));              \
    double* array_data;                                                     \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(                         \
        safearray, reinterpret_cast<void**>(&array_data)));                 \
    size_t count = array_upper_bound - array_lower_bound + 1;               \
    ASSERT_EQ(expected_property_values.size(), count);                      \
    for (size_t i = 0; i < count; ++i) {                                    \
      EXPECT_EQ(array_data[i], expected_property_values[i]);                \
    }                                                                       \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(safearray));           \
  }
using Microsoft::WRL::ComPtr;
#endif

namespace {

void SetClipboardText(ui::ClipboardBuffer buffer, const std::string& text) {
  ui::ScopedClipboardWriter(buffer).WriteText(base::ASCIIToUTF16(text));
}

}  // namespace

class FakeProvider : public AutocompleteProvider {
public:
  FakeProvider()
      : AutocompleteProvider(AutocompleteProvider::TYPE_HISTORY_URL) {}

  void Start(const AutocompleteInput& input, bool minimal_changes) override
  {}

 private:
  ~FakeProvider() override {}

  DISALLOW_COPY_AND_ASSIGN(FakeProvider);
};

class OmniboxViewViewsTest : public InProcessBrowserTest {
 protected:
  OmniboxViewViewsTest() {}

  friend class HighContrastOmniboxViewViewsTest;
  FRIEND_TEST_ALL_PREFIXES(HighContrastOmniboxViewViewsTest,
                           CloseOmniboxPopupOnTextDrag);

  static void GetOmniboxViewForBrowser(const Browser* browser,
                                       OmniboxView** omnibox_view) {
    BrowserWindow* window = browser->window();
    ASSERT_TRUE(window);
    LocationBar* location_bar = window->GetLocationBar();
    ASSERT_TRUE(location_bar);
    *omnibox_view = location_bar->GetOmniboxView();
    ASSERT_TRUE(*omnibox_view);
  }

  // Move the mouse to the center of the browser window and left-click.
  void ClickBrowserWindowCenter() {
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
        BrowserView::GetBrowserViewForBrowser(
            browser())->GetBoundsInScreen().CenterPoint()));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                                   ui_controls::DOWN));
    ASSERT_TRUE(
        ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
  }

  // Press and release the mouse at the specified locations.  If
  // |release_offset| differs from |press_offset|, the mouse will be moved
  // between the press and release.
  void Click(ui_controls::MouseButton button,
             const gfx::Point& press_location,
             const gfx::Point& release_location) {
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(press_location));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(button, ui_controls::DOWN));

    if (press_location != release_location)
      ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(release_location));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(button, ui_controls::UP));
  }

  // Touch down and release at the specified locations.
  void Tap(const gfx::Point& press_location,
           const gfx::Point& release_location) {
    gfx::NativeWindow window = GetRootWindow();
    ui::test::EventGenerator generator(window);
    if (press_location == release_location) {
      generator.GestureTapAt(press_location);
    } else {
      generator.GestureScrollSequence(press_location,
                                      release_location,
                                      base::TimeDelta::FromMilliseconds(10),
                                      1);
    }
  }

 private:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    chrome::FocusLocationBar(browser());
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  }

  gfx::NativeWindow GetRootWindow() const {
    gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
#if defined(USE_AURA)
    native_window = native_window->GetRootWindow();
#endif
    return native_window;
  }

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewViewsTest);
};

class HighContrastOmniboxViewViewsTest : public OmniboxViewViewsTest {
 public:
  HighContrastOmniboxViewViewsTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForceHighContrast);
  }

  DISALLOW_COPY_AND_ASSIGN(HighContrastOmniboxViewViewsTest);
};

IN_PROC_BROWSER_TEST_F(HighContrastOmniboxViewViewsTest,
                       PasteAndGoDoesNotLeavePopupOpen) {
  OmniboxView* view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  // Put an URL on the clipboard.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "http://www.example.com/");

  // Paste and go.
  omnibox_view_views->ExecuteCommand(IDC_PASTE_AND_GO, ui::EF_NONE);

  // The popup should not be open.
  EXPECT_FALSE(view->model()->popup_model()->IsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, PasteAndGoDoesNotLeavePopupOpen) {
  OmniboxView* view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  // Put an URL on the clipboard.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "http://www.example.com/");

  // Paste and go.
  omnibox_view_views->ExecuteCommand(IDC_PASTE_AND_GO, ui::EF_NONE);

  // The popup should not be open.
  EXPECT_FALSE(view->model()->popup_model()->IsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, PrematureObserverDeletion) {
  OmniboxView* view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  // Put an URL on the clipboard.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "https://www.bing.com/");

  // Set flag to prematurely delete observer.
  omnibox_view_views->model()->SetTestDeleteObserverFlag();

  // Paste and go.
  omnibox_view_views->ExecuteCommand(IDC_PASTE_AND_GO,
                                     ui::EF_NONE);

  // The popup should not be open.
  EXPECT_FALSE(view->model()->popup_model()->IsOpen());
}

#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, PasteAndGoAccelerator) {
  OmniboxView* view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);

  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "http://example.com");

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_L, true /* ctrl */,
        true /* shift */, false /* alt */, false /* command */));

  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);

  EXPECT_EQ(base::ASCIIToUTF16("example.com"), omnibox_view_views->GetText());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsLoading());
}
#endif // !defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, PasteAndGoCommandHandler) {
  BrowserWindow* window = browser()->window();
  ASSERT_TRUE(window);
  LocationBar* location_bar = window->GetLocationBar();
  ASSERT_TRUE(location_bar);
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  ASSERT_TRUE(omnibox_view);
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(omnibox_view);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);

  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "http://example.com");

  chrome::BrowserCommandController* controller = browser()->command_controller();
  controller->ExecuteCommand(IDC_PASTE_AND_GO);

  web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);

  EXPECT_EQ(base::ASCIIToUTF16("example.com"), omnibox_view_views->GetText());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsLoading());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, DoNotNavigateOnDrop) {
  OmniboxView* view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  OSExchangeData data;
  base::string16 input = base::ASCIIToUTF16("Foo bar baz");
  EXPECT_FALSE(data.HasString());
  data.SetString(input);
  EXPECT_TRUE(data.HasString());

  omnibox_view_views->OnDrop(data);
  EXPECT_EQ(input, omnibox_view_views->GetText());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view_views->IsSelectAll());
  EXPECT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsLoading());
}

// Flaky: https://crbug.com/915591.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, DISABLED_SelectAllOnClick) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  omnibox_view->SetUserText(base::ASCIIToUTF16("http://www.google.com/"));

  // Take the focus away from the omnibox.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Clicking in the omnibox should take focus and select all text.
  const gfx::Rect omnibox_bounds = BrowserView::GetBrowserViewForBrowser(
        browser())->GetViewByID(VIEW_ID_OMNIBOX)->GetBoundsInScreen();
  const gfx::Point click_location = omnibox_bounds.CenterPoint();
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click_location, click_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Clicking in another view should clear focus and the selection.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Clicking in the omnibox again should take focus and select all text again.
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click_location, click_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Clicking another omnibox spot should keep focus but clear the selection.
  omnibox_view->SelectAll(false);
  const gfx::Point click2_location = omnibox_bounds.origin() +
      gfx::Vector2d(omnibox_bounds.width() / 4, omnibox_bounds.height() / 4);
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click2_location, click2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Take the focus away and click in the omnibox again, but drag a bit before
  // releasing.  We should focus the omnibox but not select all of its text.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click_location, click2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Middle-click is only handled on Linux, by pasting the selection clipboard
  // and moving the cursor after the pasted text instead of selecting-all.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::MIDDLE,
                                click_location, click_location));
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
#else
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
#endif  // OS_LINUX && !OS_CHROMEOS
  EXPECT_FALSE(omnibox_view->IsSelectAll());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, SelectionClipboard) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  omnibox_view->SetUserText(base::ASCIIToUTF16("http://www.google.com/"));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);
  ASSERT_TRUE(omnibox_view_views);
  gfx::RenderText* render_text = omnibox_view_views->GetRenderText();

  // Take the focus away from the omnibox.
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  size_t cursor_position = 14;
  int cursor_x = render_text->GetCursorBounds(
      gfx::SelectionModel(cursor_position, gfx::CURSOR_FORWARD), false).x();
  gfx::Point click_location = omnibox_view_views->GetBoundsInScreen().origin();
  click_location.Offset(cursor_x + render_text->display_rect().x(),
                        omnibox_view_views->height() / 2);

  // Middle click focuses the omnibox, pastes, and sets a trailing cursor.
  // Select-all on focus shouldn't alter the selection clipboard or cursor.
  SetClipboardText(ui::ClipboardBuffer::kSelection, "123");
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::MIDDLE,
                                click_location, click_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  EXPECT_EQ(base::ASCIIToUTF16("http://www.goo123gle.com/"),
            omnibox_view->GetText());
  EXPECT_EQ(17U, omnibox_view_views->GetCursorPosition());

  // The omnibox can move when it gains focus, so refresh the click location.
  click_location.set_x(omnibox_view_views->GetBoundsInScreen().origin().x() +
                       cursor_x + render_text->display_rect().x());

  // Middle clicking again, with focus, pastes and updates the cursor.
  SetClipboardText(ui::ClipboardBuffer::kSelection, "4567");
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::MIDDLE,
                                click_location, click_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  EXPECT_EQ(base::ASCIIToUTF16("http://www.goo4567123gle.com/"),
            omnibox_view->GetText());
  EXPECT_EQ(18U, omnibox_view_views->GetCursorPosition());
}
#endif  // OS_LINUX && !OS_CHROMEOS

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !defined(OS_MACOSX) || defined(USE_AURA)

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, SelectAllOnTap) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  omnibox_view->SetUserText(base::ASCIIToUTF16("http://www.google.com/"));

  // Take the focus away from the omnibox.
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Tapping in the omnibox should take focus and select all text.
  const gfx::Rect omnibox_bounds = BrowserView::GetBrowserViewForBrowser(
      browser())->GetViewByID(VIEW_ID_OMNIBOX)->GetBoundsInScreen();
  const gfx::Point tap_location = omnibox_bounds.CenterPoint();
  ASSERT_NO_FATAL_FAILURE(Tap(tap_location, tap_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Tapping in another view should clear focus and the selection.
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Tapping in the omnibox again should take focus and select all text again.
  ASSERT_NO_FATAL_FAILURE(Tap(tap_location, tap_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Tapping another omnibox spot should keep focus and selection.
  omnibox_view->SelectAll(false);
  const gfx::Point tap2_location = omnibox_bounds.origin() +
      gfx::Vector2d(omnibox_bounds.width() / 4, omnibox_bounds.height() / 4);
  ASSERT_NO_FATAL_FAILURE(Tap(tap2_location, tap2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  // We don't test if the all text is selected because it depends on whether or
  // not there was text under the tap, which appears to be flaky.

  // Take the focus away and tap in the omnibox again, but drag a bit before
  // releasing.  We should focus the omnibox but not select all of its text.
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER));
  ASSERT_NO_FATAL_FAILURE(Tap(tap_location, tap2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
}

// Tests if executing a command hides touch editing handles.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest,
                       DeactivateTouchEditingOnExecuteCommand) {
  OmniboxView* view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);
  views::TextfieldTestApi textfield_test_api(omnibox_view_views);

  // Put a URL on the clipboard.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "http://www.example.com/");

  // Tap to activate touch editing.
  gfx::Point omnibox_center =
      omnibox_view_views->GetBoundsInScreen().CenterPoint();
  Tap(omnibox_center, omnibox_center);
  EXPECT_TRUE(textfield_test_api.touch_selection_controller());

  // Execute a command and check if it deactivate touch editing. Paste & Go is
  // chosen since it is specific to Omnibox and its execution wouldn't be
  // delegated to the base Textfield class.
  omnibox_view_views->ExecuteCommand(IDC_PASTE_AND_GO, ui::EF_NONE);
  EXPECT_FALSE(textfield_test_api.touch_selection_controller());
}

#endif  // !defined(OS_MACOSX) || defined(USE_AURA)

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, SelectAllOnTabToFocus) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  ui_test_utils::NavigateToURL(browser(), GURL("http://www.google.com/"));
  // RevertAll after navigation to invalidate the selection range saved on blur.
  omnibox_view->RevertAll();
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Pressing tab to focus the omnibox should select all text.
  while (!ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX)) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB,
                                                false, false, false, false));
  }
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(HighContrastOmniboxViewViewsTest,
                       CloseOmniboxPopupOnTextDrag) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->model()->popup_model()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->result_;
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = base::ASCIIToUTF16("http://autocomplete-result/");
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL("http://autocomplete-result/");
  match.allowed_to_be_default_match = true;
  matches.push_back(match);
  AutocompleteInput input(base::ASCIIToUTF16("a"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  results.AppendMatches(input, matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()));

  // The omnibox popup should open with suggestions displayed.
  omnibox_view->model()->popup_model()->OnResultChanged();
  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());

  // The omnibox text should be selected.
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Simulate a mouse click before dragging the mouse.
  gfx::Point point(omnibox_view_views->origin() + gfx::Vector2d(10, 10));
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  omnibox_view_views->OnMousePressed(pressed);
  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());

  // Simulate a mouse drag of the omnibox text, and the omnibox should close.
  ui::MouseEvent dragged(ui::ET_MOUSE_DRAGGED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  omnibox_view_views->OnMouseDragged(dragged);

  EXPECT_FALSE(omnibox_view->model()->popup_model()->IsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, CloseOmniboxPopupOnTextDrag) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->model()->popup_model()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->result_;
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = base::ASCIIToUTF16("http://autocomplete-result/");
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL("http://autocomplete-result/");
  match.allowed_to_be_default_match = true;
  matches.push_back(match);
  AutocompleteInput input(base::ASCIIToUTF16("a"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  results.AppendMatches(input, matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()));

  // The omnibox popup should open with suggestions displayed.
  omnibox_view->model()->popup_model()->OnResultChanged();
  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());

  // The omnibox text should be selected.
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Simulate a mouse click before dragging the mouse.
  gfx::Point point(omnibox_view_views->origin() + gfx::Vector2d(10, 10));
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  omnibox_view_views->OnMousePressed(pressed);
  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());

  // Simulate a mouse drag of the omnibox text, and the omnibox should close.
  ui::MouseEvent dragged(ui::ET_MOUSE_DRAGGED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  omnibox_view_views->OnMouseDragged(dragged);

  EXPECT_FALSE(omnibox_view->model()->popup_model()->IsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, MaintainCursorAfterFocusCycle) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->model()->popup_model()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->result_;
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = base::ASCIIToUTF16("http://autocomplete-result/");
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL("http://autocomplete-result/");
  match.allowed_to_be_default_match = true;
  matches.push_back(match);
  AutocompleteInput input(
      base::ASCIIToUTF16("autocomplete-result"), 19, "autocomplete-result",
      metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
  input.set_current_url(GURL("http://autocomplete-result/"));
  results.AppendMatches(input, matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()));

  // The omnibox popup should open with suggestions displayed.
  omnibox_view->model()->popup_model()->OnResultChanged();
  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());

  // TODO(krb): For some reason, we need to hit End twice to be registered.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_END, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_END, false,
                                              false, false, false));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Save cursor position, before blur.
  size_t prev_start, end;
  omnibox_view->GetSelectionBounds(&prev_start, &end);

  chrome::FocusAppMenu(browser());
  EXPECT_FALSE(omnibox_view->model()->popup_model()->IsOpen());

  // Re-focus.
  chrome::FocusLocationBar(browser());

  // Make sure cursor is restored.
  size_t start;
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(prev_start, start);
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, BackgroundIsOpaque) {
  // The omnibox text should be rendered on an opaque background. Otherwise, we
  // can't use subpixel rendering.
  OmniboxViewViews* view = BrowserView::GetBrowserViewForBrowser(browser())->
      toolbar()->location_bar()->omnibox_view();
  ASSERT_TRUE(view);
  EXPECT_FALSE(view->GetRenderText()->subpixel_rendering_suppressed());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, FocusedTextInputClient) {
  chrome::FocusLocationBar(browser());
  OmniboxView* view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);
  ui::InputMethod* input_method =
      omnibox_view_views->GetWidget()->GetInputMethod();
  EXPECT_EQ(static_cast<ui::TextInputClient*>(omnibox_view_views),
            input_method->GetTextInputClient());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, TextElideStatus) {
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  EXPECT_EQ(omnibox_view_views->GetRenderText()->elide_behavior(),
            gfx::ELIDE_TAIL);

  chrome::FocusLocationBar(browser());
  EXPECT_EQ(omnibox_view_views->GetRenderText()->elide_behavior(),
            gfx::NO_ELIDE);
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, FragmentUnescapedForDisplay) {
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  ui_test_utils::NavigateToURL(browser(),
                               GURL("http://example.com/#%E2%98%83"));

  EXPECT_EQ(view->GetText(), base::UTF8ToUTF16("example.com/#\u2603"));
}

// Ensure that when the user navigates between suggestions, that the accessible
// value of the Omnibox includes helpful information for human comprehension,
// such as the document title. When the user begins to arrow left/right
// within the label or edit it, the value is presented as the pure editable URL.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, FriendlyAccessibleLabel) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  base::string16 match_url = base::ASCIIToUTF16("https://google.com");
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = match_url;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL(match_url);
  match.description = base::ASCIIToUTF16("Google");
  match.allowed_to_be_default_match = true;

  // Enter user input mode to prevent spurious unelision.
  omnibox_view->model()->SetInputInProgress(true);

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->model()->popup_model()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->result_;
  ACMatches matches;
  matches.push_back(match);
  AutocompleteInput input(base::ASCIIToUTF16("g"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  results.AppendMatches(input, matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()));

  // The omnibox popup should open with suggestions displayed.
  chrome::FocusLocationBar(browser());
  omnibox_view->model()->popup_model()->OnResultChanged();
  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  // Ensure that the friendly accessibility label is updated.
  omnibox_view_views->OnTemporaryTextMaybeChanged(match_url, match, false,
                                                  false);
  omnibox_view->SelectAll(true);
  EXPECT_EQ(base::ASCIIToUTF16("https://google.com"),
            omnibox_view_views->GetText());

  // Test friendly label.
  const int kFriendlyPrefixLength = match.description.size() + 1;
  ui::AXNodeData node_data;
  omnibox_view_views->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(base::ASCIIToUTF16(
                "Google https://google.com location from history, 1 of 1"),
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kValue));
  // Selection offsets are moved over by length the inserted descriptive text
  // prefix ("Google") + 1 for the space.
  EXPECT_EQ(kFriendlyPrefixLength,
            node_data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd));
  EXPECT_EQ(kFriendlyPrefixLength + static_cast<int>(match_url.size()),
            node_data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart));
  EXPECT_EQ("both", node_data.GetStringAttribute(
                        ax::mojom::StringAttribute::kAutoComplete));
  EXPECT_EQ(ax::mojom::Role::kTextField, node_data.role);

  // Select second character -- even though the friendly "Google " prefix is
  // part of the exposed accessible text, setting the selection within select
  // the intended part of the editable text.
  ui::AXActionData set_selection_action_data;
  set_selection_action_data.action = ax::mojom::Action::kSetSelection;
  set_selection_action_data.anchor_node_id = node_data.id;
  set_selection_action_data.focus_offset = kFriendlyPrefixLength + 1;
  set_selection_action_data.anchor_offset = kFriendlyPrefixLength + 3;
  omnibox_view_views->HandleAccessibleAction(set_selection_action_data);

  EXPECT_EQ(base::ASCIIToUTF16("https://google.com"),
            omnibox_view_views->GetText());

  // Type "x" to replace the selected "tt" with that character.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_X, false,
                                              false, false, false));
  // Second character should be an "x" now.
  EXPECT_EQ(base::ASCIIToUTF16("hxps://google.com"),
            omnibox_view_views->GetText());

  // When editing starts, the accessible value becomes the same as the raw
  // edited text.
  omnibox_view_views->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(base::ASCIIToUTF16("hxps://google.com"),
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kValue));
}

// Ensure that the Omnibox popup exposes appropriate accessibility semantics,
// given a current state of open or closed.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, AccessiblePopup) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);
  chrome::FocusLocationBar(browser());

  base::string16 match_url = base::ASCIIToUTF16("https://google.com");
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = match_url;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL(match_url);
  match.description = base::ASCIIToUTF16("Google");
  match.allowed_to_be_default_match = true;

  OmniboxPopupContentsView* popup_view =
      omnibox_view_views->GetPopupContentsViewForTesting();
  ui::AXNodeData popup_node_data_1;
  popup_view->GetAccessibleNodeData(&popup_node_data_1);
  EXPECT_FALSE(popup_node_data_1.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(popup_node_data_1.HasState(ax::mojom::State::kCollapsed));
  EXPECT_TRUE(popup_node_data_1.HasState(ax::mojom::State::kInvisible));

  EXPECT_TRUE(
      popup_node_data_1.HasIntAttribute(ax::mojom::IntAttribute::kPopupForId));
  EXPECT_EQ(
      popup_node_data_1.GetIntAttribute(ax::mojom::IntAttribute::kPopupForId),
      omnibox_view_views->GetViewAccessibility().GetUniqueId().Get());

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->model()->popup_model()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->result_;
  ACMatches matches;
  matches.push_back(match);
  AutocompleteInput input(base::ASCIIToUTF16("g"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  results.AppendMatches(input, matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()));

  // The omnibox popup should open with suggestions displayed.
  omnibox_view->model()->popup_model()->OnResultChanged();
  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());
  ui::AXNodeData popup_node_data_2;
  popup_view->GetAccessibleNodeData(&popup_node_data_2);
  EXPECT_TRUE(popup_node_data_2.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(popup_node_data_2.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(popup_node_data_2.HasState(ax::mojom::State::kInvisible));
}

// Omnibox returns to clean state after chrome://kill and reload.
// https://crbug.com/993701 left the URL and icon as chrome://kill after reload.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, ReloadAfterKill) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  // Open new tab page.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Kill the tab with chrome://kill
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    ui_test_utils::NavigateToURL(browser(), GURL(content::kChromeUIKillURL));
    EXPECT_TRUE(tab->IsCrashed());
  }

  // Reload the tab.
  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  content::WaitForLoadStop(tab);

  // Verify the omnibox contents, URL and icon.
  EXPECT_EQ(base::ASCIIToUTF16(""), omnibox_view_views->GetText());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->location_bar_model()->GetURL());
}

// The following set of tests require UIA accessibility support, which only
// exists on Windows.
#if defined(OS_WIN)
class OmniboxViewViewsUIATest : public OmniboxViewViewsTest {
 public:
  OmniboxViewViewsUIATest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OmniboxViewViewsTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalUIAutomation);
  }
};

// Omnibox fires the right events when the popup opens/closes with UIA turned
// on.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsUIATest, AccessibleOmnibox) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  chrome::FocusLocationBar(browser());

  base::string16 match_url = base::ASCIIToUTF16("https://example.com");
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = match_url;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL(match_url);
  match.description = base::ASCIIToUTF16("Example");
  match.allowed_to_be_default_match = true;

  EXPECT_FALSE(omnibox_view->model()->popup_model()->IsOpen());

  HWND window_handle =
      browser()->window()->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  UiaAccessibilityWaiterInfo info = {
      window_handle, base::ASCIIToUTF16("textbox"),
      base::ASCIIToUTF16("Address and search bar"),
      ax::mojom::Event::kControlsChanged};
  UiaAccessibilityEventWaiter open_waiter(info);

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->model()->popup_model()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->result_;
  ACMatches matches;
  matches.push_back(match);
  AutocompleteInput input(base::ASCIIToUTF16("e"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  results.AppendMatches(input, matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()));

  // The omnibox popup should open with suggestions displayed.
  omnibox_view->model()->popup_model()->OnResultChanged();

  // Wait for ControllerFor property changed event.
  open_waiter.Wait();

  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());

  UiaAccessibilityEventWaiter close_waiter(info);
  // Close the popup. Another property change event is expected.
  ClickBrowserWindowCenter();
  close_waiter.Wait();
  EXPECT_FALSE(omnibox_view->model()->popup_model()->IsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsUIATest, EnsureAccNodeAddedAndSetup) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  ui::AXTreeManager* mgr = ui::AXTreeManagerMap::GetInstance().GetManager(
      omnibox_view_views->GetViewAccessibility().TreeManager()->GetTreeID());
  EXPECT_TRUE(mgr);
  views::ViewAXTreeManager* view_mgr =
      static_cast<views::ViewAXTreeManager*>(mgr);
  EXPECT_TRUE(view_mgr->GetRootAsAXNode());
  EXPECT_TRUE(omnibox_view_views->GetViewAccessibility()
                  .TreeManager()
                  ->GetRootAsAXNode());
  EXPECT_EQ(omnibox_view_views->GetViewAccessibility()
                .TreeManager()
                ->GetRootAsAXNode()
                ->data()
                .ToString(),
            view_mgr->GetRootAsAXNode()->data().ToString());
  EXPECT_TRUE(omnibox_view_views->GetViewAccessibility()
                  .TreeManager()
                  ->GetRootAsAXNode()
                  ->children()
                  .empty());
  EXPECT_TRUE(omnibox_view_views->GetViewAccessibility()
                  .TreeManager()
                  ->GetRootAsAXNode()
                  ->IsPlainTextField());
  gfx::Rect view_bounds = omnibox_view_views->GetBoundsInScreen();
  gfx::Rect delegate_bounds = view_mgr->delegate()->GetBoundsRect(
      ui::AXCoordinateSystem::kScreenDIPs, ui::AXClippingBehavior::kUnclipped);
  EXPECT_EQ(view_bounds, delegate_bounds);
  EXPECT_EQ(omnibox_view_views->GetText(), base::ASCIIToUTF16("about:blank"));
  int width =
      std::round(omnibox_view_views->GetRenderText()->GetStringSizeF().width());
  view_bounds = omnibox_view_views->GetContentsBoundsInScreen();
  view_bounds =
      gfx::Rect(view_bounds.x(), view_bounds.y(), width, view_bounds.height());
  ui::AXOffscreenResult offscreen_result = ui::AXOffscreenResult::kOnscreen;
  delegate_bounds = view_mgr->delegate()->GetInnerTextRangeBoundsRect(
      0, omnibox_view_views->GetText().size(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kUnclipped, &offscreen_result);
  EXPECT_EQ(view_bounds, delegate_bounds);
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsUIATest, EnsureAccNodeUpdate) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);
  ui::AXTreeManager* mgr = ui::AXTreeManagerMap::GetInstance().GetManager(
      omnibox_view_views->GetViewAccessibility().TreeManager()->GetTreeID());
  EXPECT_TRUE(mgr);
  int width =
      std::round(omnibox_view_views->GetRenderText()->GetStringSizeF().width());
  ui::AXOffscreenResult offscreen_result = ui::AXOffscreenResult::kOnscreen;
  views::ViewAXTreeManager* view_mgr =
      static_cast<views::ViewAXTreeManager*>(mgr);
  ui::AXNode::AXID init_id = omnibox_view_views->GetViewAccessibility()
                                 .TreeManager()
                                 ->GetRootAsAXNode()
                                 ->id();
  omnibox_view_views->SetUserText(base::ASCIIToUTF16("test"));
  width =
      std::round(omnibox_view_views->GetRenderText()->GetStringSizeF().width());
  gfx::Rect old_delegate_bounds =
      view_mgr->delegate()->GetInnerTextRangeBoundsRect(
          0, omnibox_view_views->GetText().size(),
          ui::AXCoordinateSystem::kScreenDIPs, ui::AXClippingBehavior::kUnclipped,
          &offscreen_result);
  base::string16 boxValue;
  EXPECT_TRUE(omnibox_view_views->GetViewAccessibility()
                  .TreeManager()
                  ->GetRootAsAXNode()
                  ->GetString16Attribute(ax::mojom::StringAttribute::kValue,
                                         &boxValue));
  EXPECT_EQ(base::ASCIIToUTF16("test"), boxValue);

  EXPECT_EQ(init_id, omnibox_view_views->GetViewAccessibility()
                         .TreeManager()
                         ->GetRootAsAXNode()
                         ->id());
  gfx::Rect view_bounds = omnibox_view_views->GetBoundsInScreen();
  view_bounds =
      gfx::Rect(view_bounds.x(), view_bounds.y(), width, view_bounds.height());
  offscreen_result = ui::AXOffscreenResult::kOnscreen;
  gfx::Rect delegate_bounds = view_mgr->delegate()->GetInnerTextRangeBoundsRect(
      0, omnibox_view_views->GetText().size(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kUnclipped, &offscreen_result);
  EXPECT_EQ(old_delegate_bounds, delegate_bounds);
  EXPECT_NE(view_bounds, delegate_bounds);
  old_delegate_bounds = delegate_bounds;

  omnibox_view->SelectAll(false);
  width =
      std::round(omnibox_view_views->GetRenderText()->GetStringSizeF().width());
  EXPECT_TRUE(omnibox_view_views->GetViewAccessibility()
                  .TreeManager()
                  ->GetRootAsAXNode()
                  ->GetString16Attribute(ax::mojom::StringAttribute::kValue,
                                         &boxValue));
  EXPECT_EQ(base::ASCIIToUTF16("test"), boxValue);
  view_bounds =
      gfx::Rect(view_bounds.x(), view_bounds.y(), width, view_bounds.height());
  offscreen_result = ui::AXOffscreenResult::kOnscreen;
  delegate_bounds = view_mgr->delegate()->GetInnerTextRangeBoundsRect(
      0, omnibox_view_views->GetText().size(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kUnclipped, &offscreen_result);
  EXPECT_EQ(old_delegate_bounds, delegate_bounds);
  EXPECT_NE(view_bounds, delegate_bounds);

  // Select substring of text and verify bounds rect.
  omnibox_view_views->SetUserText(base::ASCIIToUTF16("substring test"));
  omnibox_view_views->SetSelectedRange(gfx::Range(4, 8));
  ui::AXPlatformNode* ax_platform_node =
      static_cast<views::ViewAXPlatformNodeDelegate*>(view_mgr->delegate())
          ->ax_platform_node();
  ComPtr<IRawElementProviderSimple> root_node_raw;
  ax_platform_node->GetNativeViewAccessible()->QueryInterface(
      __uuidof(IRawElementProviderSimple), &root_node_raw);
  ComPtr<ITextProvider> document_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &document_provider));
  CComPtr<ITextRangeProvider> selected_text_range_provider;
  base::win::ScopedSafearray selection;
  LONG index = 0;
  LONG ubound;
  LONG lbound;

  // Verify the content of the selection.
  document_provider->GetSelection(selection.Receive());
  ASSERT_NE(nullptr, selection.Get());
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetUBound(selection.Get(), 1, &ubound));
  EXPECT_EQ(0, ubound);
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetLBound(selection.Get(), 1, &lbound));
  EXPECT_EQ(0, lbound);
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(selection.Get(), &index,
                                               &selected_text_range_provider));
  base::win::ScopedBstr provider_content;
  EXPECT_HRESULT_SUCCEEDED(
      selected_text_range_provider->GetText(-1, provider_content.Receive()));
  EXPECT_STREQ(L"trin", provider_content.Get());
  base::win::ScopedSafearray rectangles;
  EXPECT_HRESULT_SUCCEEDED(selected_text_range_provider->GetBoundingRectangles(
      rectangles.Receive()));

  // Manually construct left bounds value.
  int left_bound =
      display::win::ScreenWin::DIPToScreenRect(
          HWNDForView(omnibox_view_views),
          gfx::Rect(
              omnibox_view_views->GetViewAccessibility()
                      .TreeManager()
                      ->GetRootAsAXNode()
                      ->GetIntListAttribute(
                          ax::mojom::IntListAttribute::kCharacterOffsets)[4] +
                  omnibox_view_views->GetContentsBoundsInScreen().x(),
              0, 0, 0))
          .x();
  std::vector<double> expected_values = {left_bound, 60, 24, 20};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);
  delegate_bounds = view_mgr->delegate()->GetInnerTextRangeBoundsRect(
      4, 8, ui::AXCoordinateSystem::kScreenDIPs, ui::AXClippingBehavior::kUnclipped,
      &offscreen_result);
  EXPECT_EQ(gfx::Rect(left_bound, 60, 24, 20), delegate_bounds);
  rectangles.Reset();
  selected_text_range_provider.Release();
}

// Omnibox fires the right events when selection occurs and text change
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsUIATest, EnsureTextEvents) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(omnibox_view);

  HWND window_handle =
      browser()->window()->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  // Check text changed
  UiaAccessibilityWaiterInfo info = {
      window_handle, base::ASCIIToUTF16("textbox"),
      base::ASCIIToUTF16("Address and search bar"),
      ax::mojom::Event::kTextChanged};
  UiaAccessibilityEventWaiter text_change_waiter(info);

  omnibox_view_views->InsertText(base::ASCIIToUTF16("test"));
  text_change_waiter.Wait();

  // Check generic value changed
  info = {
      window_handle, base::ASCIIToUTF16("textbox"),
      base::ASCIIToUTF16("Address and search bar"),
      ax::mojom::Event::kValueChanged};
  UiaAccessibilityEventWaiter value_change_waiter(info);

  omnibox_view_views->AppendText(L"t");
  value_change_waiter.Wait();

  // Check selection changed
  info = {
      window_handle, base::ASCIIToUTF16("textbox"),
      base::ASCIIToUTF16("Address and search bar"),
      ax::mojom::Event::kTextSelectionChanged};
  UiaAccessibilityEventWaiter selection_change_waiter(info);

  chrome::FocusLocationBar(browser());
  omnibox_view_views->InsertText(L"t");
  selection_change_waiter.Wait();
}
#endif  // OS_WIN

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, OpenMatch) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->model()->popup_model()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->result_;
  ACMatches matches;
  AutocompleteMatch match(new FakeProvider(), 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = base::ASCIIToUTF16("http://autocomplete-result/");
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL("http://autocomplete-result/");
  match.allowed_to_be_default_match = true;
  matches.push_back(match);
  AutocompleteInput input(
      base::ASCIIToUTF16("autocomplete-result"), 19, "autocomplete-result",
      metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
  input.set_current_url(GURL("http://autocomplete-result/"));
  results.AppendMatches(input, matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()));

  // The omnibox popup should open with suggestions displayed.
  omnibox_view->model()->popup_model()->OnResultChanged();
  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());

  omnibox_view->OpenMatch(WindowOpenDisposition::CURRENT_TAB,
                          0,
                          base::TimeTicks::Now());
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);
  EXPECT_EQ(base::ASCIIToUTF16("autocomplete-result/"),
            omnibox_view_views->GetText());
}