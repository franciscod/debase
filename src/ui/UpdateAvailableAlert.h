#pragma once
#include "Alert.h"

namespace UI {

class UpdateAvailableAlert : public Alert {
public:
    UpdateAvailableAlert(Version version) {
        width                           (26);
        title()->text                   ("debase v" + std::to_string(version) + " update");
        title()->align                  (UI::Align::Center);
        color                           (colors().menu);
        condensed                       (true);
        okButton()->label()->text       ("Download");
        dismissButton()->label()->text  ("Ignore");
//        okButton()->action              ([] (UI::Button&) {});
//        dismissButton()->action         ([] (UI::Button&) {});
        truncateEdges                   (UI::Edges{.l=0, .r=0, .t=0, .b=1});
    }
    
    // MARK: - Alert Overrides
    
    // focusable(): override Alert implementation because we don't want to affect cursor
    bool focusable() const override { return false; }
    
    bool handleEvent(const Event& ev) override {
        const bool hit = hitTest(ev.mouse.origin);
        const bool mouseDown = ev.mouseDown(Event::MouseButtons::Left|Event::MouseButtons::Right);
        const bool mouseUp = ev.mouseUp(Event::MouseButtons::Left|Event::MouseButtons::Right);
        if (hit && (mouseDown || mouseUp)) return true;
        return false;
    }
};

using UpdateAvailableAlertPtr = std::shared_ptr<UpdateAvailableAlert>;

} // namespace UI
