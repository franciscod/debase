#include <iostream>
#include <unistd.h>
#include "ncurses/c++/cursesp.h"

int main(int argc, const char* argv[]) {
    // There doesn't seem to be a better way to do this
    setenv("TERM", "xterm-1003", true);
    ::endwin();
    
    NCursesWindow win(::stdscr);
    NCursesPanel panel(5, 10);
    panel.printw(1, 1, "hello");
    panel.box();
    
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    mouseinterval(0);
    
    for (;;) {
        NCursesPanel::redraw();
        int key = win.getch();
        if (key == KEY_MOUSE) {
            MEVENT mouseEvent = {};
            int ir = getmouse(&mouseEvent);
            if (ir != OK) continue;
            panel.mvwin(mouseEvent.y, mouseEvent.x);
        }
    }
    return 0;
    
    
    
    
//    bool bColors = b_Colors;
//    Soft_Label_Key_Set* S = 0;
//
//    int ts = titlesize();
//    if (ts>0) NCursesWindow::ripoffline(ts,rinit);
//    Soft_Label_Key_Set::Label_Layout fmt = useSLKs();
//    if (fmt!=Soft_Label_Key_Set::None) {
//        S = new Soft_Label_Key_Set(fmt);
//        assert(S != 0);
//        init_labels(*S);
//    }
//
//    Root_Window = new NCursesWindow(::stdscr);
//    init(bColors);
//
//    if (ts>0) title();
//    
//    if (fmt!=Soft_Label_Key_Set::None) {
//        push(*S);
//    }
//    
//    return run();
    
    
    
    return 0;
}
