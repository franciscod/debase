#pragma once
#include <algorithm>
#include "Panel.h"
#include "Button.h"
#include "UTF8.h"

namespace UI {

class _SnapshotMenu : public _Menu {
public:
    using _Menu::_Menu;
    
    void draw() {
        const MenuOptions& opts = options();
        _Menu::draw();
        const int width = bounds().size.x;
        
        // Draw separator
        if (opts.buttons.size() > 1) {
            UI::Button button0 = opts.buttons[0];
            UI::_Window::Attr color = attr(opts.colors.menu);
            Point p = {1, button0->options().frame.ymax()+1};
            int len = width-2;
            cchar_t c = { .chars = L"╍" };
            mvwhline_set(*this, p.y, p.x, &c, len);
        }
    }
};

using SnapshotMenu = std::shared_ptr<_SnapshotMenu>;

} // namespace UI