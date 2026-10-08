#pragma once

#include <lvgl.h>

#include <cstdint>

#include "menu/MenuPage.h"
#include "ui/MenuHost.h"

namespace m5_redux {

// The quick actions: tiles in a see-through panel that slides in from the left over the control
// screen. It lives on LVGL's top layer, so the control screen stays loaded below it.
class QuickMenuPanel : public MenuHost {
  public:
    void begin();
    void open(MenuPage& root);
    // Slides the panel out, or hides it at once when another screen replaces the control screen.
    void close(bool animated);

  protected:
    void pagesClosed() override;

  private:
    lv_obj_t* root_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    // In place or sliding in.
    bool shown_ = false;

    void slideIn();
    void slideOut();
    void hideNow();

    static void setPanelX(void* panel, std::int32_t x);
    static void handleSlideOutCompleted(lv_anim_t* animation);
    static void handleCloseEvent(lv_event_t* event);
};

}  // namespace m5_redux
