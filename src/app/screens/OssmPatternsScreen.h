#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <lvgl.h>

#include "devices/ossm/OssmClient.h"
#include "devices/ossm/OssmControl.h"
#include "platform/RemoteInput.h"
#include "ui/StopButtonFeedback.h"

namespace m5_redux {

enum class OssmPatternsScreenAction : std::uint8_t {
    None,
    Cancel,
    Select,
};

class OssmPatternsScreen {
  public:
    explicit OssmPatternsScreen(OssmControl& control);

    void begin();
    void setCatalog(const ossm::OssmClient::PatternList& catalog);
    void invalidateCatalog();
    void enter();
    void leave();
    OssmPatternsScreenAction update(const RemoteInputEvents& events);
    void refresh();

    void requestCancel();
    void requestSelect();
    bool selectedPatternId(int& patternId) const;
    const char* patternName(int patternId) const;

  private:
    static constexpr std::size_t kNoSelection = static_cast<std::size_t>(-1);

    OssmControl& control_;
    ossm::OssmClient::PatternList catalog_{};
    std::vector<lv_obj_t*> rows_;
    std::size_t selectedIndex_ = kNoSelection;
    OssmPatternsScreenAction pendingAction_ = OssmPatternsScreenAction::None;
    StopButtonFeedback stopButtonFeedback_;

    void rebuildRows();
    void selectCurrentPattern();
    void selectRow(std::size_t index);
    void updateSelectButton();
    void handleRowClicked(lv_obj_t* row);

    static void handleRowEvent(lv_event_t* event);
};

}  // namespace m5_redux
