#pragma once

#include <lvgl.h>

#include "menu/MenuPage.h"
#include "ui/MenuHost.h"

namespace m5_redux {

// A list menu on a screen of its own, laid out like the old settings screen. App shows it like
// any other screen; the next screen App loads replaces it.
class MenuScreen : public MenuHost {
  public:
    void begin();
    void open(MenuPage& root);
    // Shows the open pages again, e.g. back from the diagnostics screen, on the same row.
    void resume();
    void close();

  private:
    lv_obj_t* screen_ = nullptr;

    void load();
};

}  // namespace m5_redux
