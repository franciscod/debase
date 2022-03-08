#include <iostream>
#include <unistd.h>
#include <vector>
#include <cassert>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <deque>
#include <set>
#include "Git.h"
#include "Window.h"
#include "Panel.h"
#include "RuntimeError.h"
#include "xterm-256color.h"
#include "RuntimeError.h"
#include "CommitPanel.h"
#include "BorderedPanel.h"
#include "BranchColumn.h"

struct Selection {
    BranchColumn* column = nullptr;
    CommitPanelSet panels;
};

static Window _RootWindow;

static std::vector<BranchColumn> _BranchColumns;

static struct {
    std::optional<CommitPanel> titlePanel;
    std::vector<BorderedPanel> shadowPanels;
    bool copy = false;
} _Drag;

static Selection _Selection;

static struct {
    std::optional<Panel> panel;
    Rect rect;
} _SelectionRect;

static std::optional<Rect> _InsertionMarker;

static void _Draw() {
    const Color selectionColor = (_Drag.copy ? Colors::SelectionCopy : Colors::SelectionMove);
    
    // Update selection colors
    {
        if (_Drag.titlePanel) {
            _Drag.titlePanel->setBorderColor(selectionColor);
            
            for (BorderedPanel& panel : _Drag.shadowPanels) {
                panel.setBorderColor(selectionColor);
            }
        }
        
        for (BranchColumn& col : _BranchColumns) {
            for (CommitPanel& panel : col.panels()) {
                bool selected = Contains(_Selection.panels, &panel);
                if (selected) {
                    panel.setBorderColor(selectionColor);
                } else {
                    panel.setBorderColor(std::nullopt);
                }
            }
        }
    }
    
    // Draw everything
    {
        _RootWindow.erase();
        
        for (BranchColumn& col : _BranchColumns) {
            col.draw();
        }
        
        if (_Drag.titlePanel) {
            _Drag.titlePanel->drawIfNeeded();
        }
        
        for (BorderedPanel& panel : _Drag.shadowPanels) {
            panel.drawIfNeeded();
        }
        
        if (_SelectionRect.panel) {
            _SelectionRect.panel->erase();
            Window::Attr attr = _SelectionRect.panel->setAttr(COLOR_PAIR(selectionColor));
            _SelectionRect.panel->drawRect(_SelectionRect.rect);
        }
        
        if (_InsertionMarker) {
            Window::Attr attr = _RootWindow.setAttr(COLOR_PAIR(selectionColor));
            _RootWindow.drawLineHoriz(_InsertionMarker->point, _InsertionMarker->size.x);
            
            const char* text = (_Drag.copy ? " Copy " : " Move ");
            const Point point = {
                _InsertionMarker->point.x + (_InsertionMarker->size.x - (int)strlen(text))/2,
                _InsertionMarker->point.y
            };
            _RootWindow.drawText(point, "%s", text);
        }
        
        Window::Redraw();
    }
}

struct _HitTestResult {
    BranchColumn& column;
    CommitPanel& panel;
};

static std::optional<_HitTestResult> _HitTest(const Point& p) {
    for (BranchColumn& col : _BranchColumns) {
        if (CommitPanel* panel = col.hitTest(p)) {
            return _HitTestResult{col, *panel};
        }
    }
    return std::nullopt;
}

static std::tuple<BranchColumn*,CommitPanelVecIter> _FindInsertionPoint(const Point& p) {
    BranchColumn* insertionCol = nullptr;
    CommitPanelVec::iterator insertionIter;
    std::optional<int> insertLeastDistance;
    for (BranchColumn& col : _BranchColumns) {
        CommitPanelVec& panels = col.panels();
        const Rect lastRect = panels.back().rect();
        const int midX = lastRect.point.x + lastRect.size.x/2;
        const int endY = lastRect.point.y + lastRect.size.y;
        
        for (auto it=panels.begin();; it++) {
            const int x = (it!=panels.end() ? it->rect().point.x+it->rect().size.x/2 : midX);
            const int y = (it!=panels.end() ? it->rect().point.y : endY);
            int dist = (p.x-x)*(p.x-x)+(p.y-y)*(p.y-y);
            
            if (!insertLeastDistance || dist<insertLeastDistance) {
                insertionCol = &col;
                insertionIter = it;
                insertLeastDistance = dist;
            }
            if (it==panels.end()) break;
        }
    }
    
    // Adjust the insert point so that it doesn't occur within a selection
    assert(insertionCol);
    CommitPanelVec& insertionColPanels = insertionCol->panels();
    while (insertionIter!=insertionColPanels.begin() && Contains(_Selection.panels, &*std::prev(insertionIter))) {
        insertionIter--;
    }
    
    return std::make_tuple(insertionCol, insertionIter);
}

static bool _WaitForMouseEvent(MEVENT& mouse) {
    const bool mouseUp = mouse.bstate & BUTTON1_RELEASED;
    if (mouseUp) return false;
    
    // Wait for another mouse event
    for (;;) {
        int key = _RootWindow.getChar();
        if (key != KEY_MOUSE) continue;
        int ir = ::getmouse(&mouse);
        if (ir != OK) continue;
        return true;
    }
}

// _TrackMouseInsideCommitPanel
// Handles dragging a set of CommitPanels
static void _TrackMouseInsideCommitPanel(MEVENT mouseDownEvent, BranchColumn& mouseDownColumn, CommitPanel& mouseDownPanel) {
    const Rect mouseDownPanelRect = mouseDownPanel.rect();
    const Size delta = {
        mouseDownPanelRect.point.x-mouseDownEvent.x,
        mouseDownPanelRect.point.y-mouseDownEvent.y,
    };
    const bool wasSelected = Contains(_Selection.panels, &mouseDownPanel);
    
    // Reset the selection to solely contain the mouse-down CommitPanel if:
    //   - there's no selection; or
    //   - the mouse-down CommitPanel is in a different column than the current selection; or
    //   - an unselected CommitPanel was clicked
    if (_Selection.panels.empty() || (_Selection.column != &mouseDownColumn) || !wasSelected) {
        _Selection = {
            .column = &mouseDownColumn,
            .panels = {&mouseDownPanel},
        };
    
    } else {
        assert(!_Selection.panels.empty() && (_Selection.column == &mouseDownColumn));
        _Selection.panels.insert(&mouseDownPanel);
    }
    
    MEVENT mouse = mouseDownEvent;
    BranchColumn* insertionCol = nullptr;
    CommitPanelVecIter insertionIter;
    for (;;) {
        assert(!_Selection.panels.empty());
        
        const int w = std::abs(mouseDownEvent.x - mouse.x);
        const int h = std::abs(mouseDownEvent.y - mouse.y);
        const bool dragStart = w>1 || h>1;
        
        if (!_Drag.titlePanel && dragStart) {
            const CommitPanel& titlePanel = *(*_Selection.panels.begin());
            Git::Commit titleCommit = titlePanel.commit();
            _Drag.titlePanel.emplace(titlePanel.commit(), 0, titlePanel.rect().size.x);
            
            // Create shadow panels
            Size shadowSize = (*_Selection.panels.begin())->rect().size;
            for (size_t i=0; i<_Selection.panels.size()-1; i++) {
                _Drag.shadowPanels.emplace_back(shadowSize);
            }
            
            // Hide the original CommitPanels while we're dragging
            for (auto it=_Selection.panels.begin(); it!=_Selection.panels.end(); it++) {
                (*it)->setVisible(false);
            }
            
            // Order all the title panel and shadow panels
            for (auto it=_Drag.shadowPanels.rbegin(); it!=_Drag.shadowPanels.rend(); it++) {
                it->orderFront();
            }
            _Drag.titlePanel->orderFront();
        }
        
        if (_Drag.titlePanel) {
            // Update _Drag.copy depending on whether the option key is held
            {
                const bool copy = (mouse.bstate & BUTTON_ALT);
                _Drag.copy = copy;
            }
            
            // Position title panel / shadow panels
            {
                const Point pos0 = {
                    mouse.x+delta.x,
                    mouse.y+delta.y,
                };
                
                _Drag.titlePanel->setPosition(pos0);
                
                // Position shadowPanels
                int off = 1;
                for (Panel& p : _Drag.shadowPanels) {
                    const Point pos = {pos0.x+off, pos0.y+off};
                    p.setPosition(pos);
                    off++;
                }
            }
            
            // Find insertion position
            std::tie(insertionCol, insertionIter) = _FindInsertionPoint({mouse.x, mouse.y});
            
            // Update insertion marker
            {
                constexpr int InsertionExtraWidth = 6;
                CommitPanelVec& insertionColPanels = insertionCol->panels();
                const Rect lastRect = insertionColPanels.back().rect();
                const int endY = lastRect.point.y + lastRect.size.y;
                const int insertY = (insertionIter!=insertionColPanels.end() ? insertionIter->rect().point.y : endY+1);
                
                _InsertionMarker = {
                    .point = {lastRect.point.x-InsertionExtraWidth/2, insertY-1},
                    .size = {lastRect.size.x+InsertionExtraWidth, 0},
                };
            }
        }
        
        _Draw();
        if (!_WaitForMouseEvent(mouse)) break;
    }
    
//    BranchColumn* insertionCol = nullptr;
//    CommitPanelVecIter insertionIter;
//    
//    // Move or copy _Selection to `insertionIter` within `insertionCol`
//    if () {
//        
//    }
    
    // Reset state
    {
        _Drag = {};
        _InsertionMarker = std::nullopt;
        
        // Make every commit visible again
        for (BranchColumn& col : _BranchColumns) {
            for (CommitPanel& panel : col.panels()) {
                panel.setVisible(true);
            }
        }
    }
}

// _TrackMouseOutsideCommitPanel
// Handles updating the selection rectangle / selection state
static void _TrackMouseOutsideCommitPanel(MEVENT mouseDownEvent) {
    auto selectionOld = _Selection;
    MEVENT mouse = mouseDownEvent;
    for (;;) {
        const int x = std::min(mouseDownEvent.x, mouse.x);
        const int y = std::min(mouseDownEvent.y, mouse.y);
        const int w = std::abs(mouseDownEvent.x - mouse.x);
        const int h = std::abs(mouseDownEvent.y - mouse.y);
        const bool dragStart = w>1 || h>1;
        
        // Mouse-down outside of a commit:
        // Handle selection rect drawing / selecting commits
        const Rect selectionRect = {{x,y}, {std::max(1,w),std::max(1,h)}};
        
        if (!_SelectionRect.panel && dragStart) {
            _SelectionRect.panel.emplace();
            _SelectionRect.panel->setSize(_RootWindow.rect().size);
            _SelectionRect.panel->orderFront();
        }
        
        // Update selection
        {
            _SelectionRect.rect = selectionRect;
            
            Selection selectionNew;
            for (BranchColumn& col : _BranchColumns) {
                for (CommitPanel& panel : col.panels()) {
                    if (!Empty(Intersection(selectionRect, panel.rect()))) {
                        selectionNew.column = &col;
                        selectionNew.panels.insert(&panel);
                    }
                }
                if (!selectionNew.panels.empty()) break;
            }
            
            const bool shift = (mouseDownEvent.bstate & BUTTON_SHIFT);
            if (shift && (selectionNew.panels.empty() || selectionOld.column==selectionNew.column)) {
                Selection selection = {
                    .column = selectionOld.column,
                };
                
                // selection = _Selection XOR selectionNew
                std::set_symmetric_difference(
                    selectionOld.panels.begin(), selectionOld.panels.end(),
                    selectionNew.panels.begin(), selectionNew.panels.end(),
                    std::inserter(selection.panels, selection.panels.begin())
                );
                
                _Selection = selection;
            
            } else {
                _Selection = selectionNew;
            }
        }
        
        _Draw();
        if (!_WaitForMouseEvent(mouse)) break;
    }
    
    // Reset state
    {
        _SelectionRect = {};
    }
}

int main(int argc, const char* argv[]) {
//    volatile bool a = false;
//    while (!a);
    
    try {
        // Handle args
        std::vector<std::string> branches;
        {
            for (int i=1; i<argc; i++) {
                branches.push_back(argv[i]);
            }
        }
        
        // Init ncurses
        {
            // Default linux installs may not contain the /usr/share/terminfo database,
            // so provide a fallback terminfo that usually works.
            nc_set_default_terminfo(xterm_256color, sizeof(xterm_256color));
            
            // Override the terminfo 'kmous' and 'XM' properties
            //   kmous = the prefix used to detect/parse mouse events
            //   XM    = the escape string used to enable mouse events (1006=SGR 1006 mouse
            //           event mode; 1003=report mouse-moved events in addition to clicks)
            setenv("TERM_KMOUS", "\x1b[<", true);
            setenv("TERM_XM", "\x1b[?1006;1003%?%p1%{1}%=%th%el%;", true);
            
            ::initscr();
            ::noecho();
            ::cbreak();
            
            ::use_default_colors();
            ::start_color();
            
            #warning TODO: cleanup color logic
            #warning TODO: fix: colors aren't restored when exiting
            // Redefine colors to our custom palette
            {
                int c = 1;
                
                ::init_color(c, 0, 0, 1000);
                ::init_pair(Colors::SelectionMove, c, -1);
                c++;
                
                ::init_color(c, 0, 1000, 0);
                ::init_pair(Colors::SelectionCopy, c, -1);
                c++;
                
                ::init_color(c, 300, 300, 300);
                ::init_pair(Colors::SubtitleText, c, -1);
                c++;
            }
            
            // Hide cursor
            ::curs_set(0);
        }
        
        // Init libgit2
        {
            git_libgit2_init();
        }
        
//        volatile bool a = false;
//        while (!a);
        
        _RootWindow = Window(::stdscr);
        
        // Create a BranchColumn for each specified branch
        {
            constexpr int InsetX = 3;
            constexpr int ColumnWidth = 32;
            constexpr int ColumnSpacing = 6;
            Git::Repo repo = Git::RepoOpen(".");
            
            int OffsetX = InsetX;
            branches.insert(branches.begin(), "HEAD");
            for (const std::string& branch : branches) {
                std::string displayName = branch;
                if (branch == "HEAD") {
                    std::string currentBranchName = Git::CurrentBranchName(repo);
                    if (!currentBranchName.empty()) {
                        displayName = currentBranchName + " (HEAD)";
                    }
                }
                _BranchColumns.emplace_back(_RootWindow, repo, branch, displayName, OffsetX, ColumnWidth);
                OffsetX += ColumnWidth+ColumnSpacing;
            }
        }
        
        mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
        mouseinterval(0);
        for (int i=0;; i++) {
            _Draw();
            int key = _RootWindow.getChar();
            if (key == KEY_MOUSE) {
                MEVENT mouse = {};
                int ir = ::getmouse(&mouse);
                if (ir != OK) continue;
                if (mouse.bstate & BUTTON1_PRESSED) {
                    const bool shift = (mouse.bstate & BUTTON_SHIFT);
                    const auto hitTest = _HitTest({mouse.x, mouse.y});
                    if (hitTest && !shift) {
                        // Mouse down inside of a CommitPanel, without shift key
                        _TrackMouseInsideCommitPanel(mouse, hitTest->column, hitTest->panel);
                    } else {
                        // Mouse down outside of a CommitPanel, or mouse down anywhere with shift key
                        _TrackMouseOutsideCommitPanel(mouse);
                    }
                }
            
            } else if (key == KEY_RESIZE) {
                throw std::runtime_error("hello");
            }
        }
    
    } catch (const std::exception& e) {
        ::endwin();
        fprintf(stderr, "Error: %s\n", e.what());
    }
    
    return 0;
}
