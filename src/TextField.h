#pragma once
#include <algorithm>
#include "Window.h"
#include "UTF8.h"
#include "CursorState.h"
#include "lib/tinyutf8/tinyutf8.h"

namespace UI {

struct TextFieldOptions {
    const ColorPalette& colors;
//    bool enabled = false;
//    bool center = false;
//    bool drawBorder = false;
//    int insetX = 0;
//    bool highlight = false;
//    bool mouseActive = false;
    Rect frame;
//    bool active = false;
};

class _TextField : public std::enable_shared_from_this<_TextField> {
public:
    _TextField(const TextFieldOptions& opts) : _opts(opts) {}
    
    bool hitTest(const UI::Point& p, UI::Size expand={0,0}) {
        return UI::HitTest(_opts.frame, p, expand);
    }
    
    void draw(Window win) {
        UI::Attr color;
        if (!_active) color = UI::Attr(win, _opts.colors.dimmed);
        win->drawLineHoriz(_opts.frame.point, _opts.frame.size.x);
        
        win->drawText(_opts.frame.point, "%s", _value.c_str());
        
        if (_active) {
            Point p = win->frame().point + _opts.frame.point;
            
//            std::string_view value = _value;
//            CursorState a = CursorState::Push(true, {win->frame().point.x, win->frame().point.y});
//            size_t lenBytes = std::distance(_value.begin(), _valuePosCursor);
//            size_t lenRunes = UTF8::Strlen(value.substr(0, lenBytes));
            
//            _cursorState = CursorState(true, {p.x+(int)lenRunes, p.y});
        }
        
//        if (_active) {
//            curs_set(1);
//            wmove(*win, win->frame().point.y, win->frame().point.x);
//        }
        
//        if (_opts.active) {
//            if (!_cursorVis) {
//                _cursorVis = CursorVisibility(true);
//            }
//        
//        } else {
//            _cursorVis = std::nullopt;
//        }
//        
//        win->drawRect(_opts.frame);
    }
    
    UI::Event handleEvent(Window win, const UI::Event& ev) {
        if (ev.type == UI::Event::Type::Mouse) {
            
        
        } else if (ev.type == UI::Event::Type::WindowResize) {
            
        
        } else if (ev.type == UI::Event::Type::KeyDelete) {
            if (_valuePosCursor != _value.begin()) {
//                _valuePosCursor = _value.erase(std::prev(_valuePosCursor));
            }
        
        } else if (ev.type == UI::Event::Type::KeyFnDelete) {
            if (_valuePosCursor != _value.end()) {
//                _valuePosCursor = _value.erase(_valuePosCursor);
            }
        
        } else {
            if (!iscntrl((int)ev.type)) {
                uint8_t c = (uint8_t)ev.type;
                _value.insert(_value.end(), &c);
                _value.insert(_value.end(), "hello");
//                _valuePosCursor = _value.insert(_valuePosCursor, (int)ev.type);
//                _valuePosCursor++;
            }
        }
        
        return {};
    }
    
    const tiny_utf8::string& value() const {
        return _value;
    }
    
    TextFieldOptions& options() { return _opts; }
    const TextFieldOptions& options() const { return _opts; }
    
    bool active() const { return _active; }
    void active(bool x) {
        _active = x;
        if (_cursorState) _cursorState.restore();
//        if (_active && !_cursorState) _cursorState.set(true);
//        else if (!_active && _cursorState) _cursorVis.restore();
    }
    
private:
    static constexpr int KeySpacing = 2;
    TextFieldOptions _opts;
    tiny_utf8::string _value;
    tiny_utf8::string::iterator _valuePosStart = _value.begin();
    tiny_utf8::string::iterator _valuePosCursor = _value.end();
    bool _active = false;
    CursorState _cursorState;
//    CursorVisibility _cursorVis;
};

using TextField = std::shared_ptr<_TextField>;

} // namespace UI
