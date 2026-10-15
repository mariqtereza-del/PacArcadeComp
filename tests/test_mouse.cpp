// X04: the mouse in the menus. Part 1 (this file): pixel -> item mapping, hover, click, right click, at several window sizes and scalings. No ROM, no window.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/video/gfx.h"
#include <SDL.h>
#include "../src/app/controls.cpp"
#include "../src/app/overlay.cpp"
#include "../src/app/menu.cpp"
#include "../src/app/display.h"
using namespace menu;
static int failures = 0, checks = 0;
static void expect(bool ok, const std::string &what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}
int main() {
    Model m;
    Menu mn;
    mn.open();
    // the rows of the main page (11 items): row i spans picture y from 46 + 14 i to 59 + 14 i, x from 12 to 211
    for (int i = 0; i < 11; ++i) {
        int y0 = 20 + 28 - 2 + 14 * i;
        expect(mn.item_at(100, y0) == i && mn.item_at(100, y0 + 13) == i, "row " + std::to_string(i) + " first and last pixel");
    }
    expect(mn.item_at(100, 45) == -1 && mn.item_at(100, 46 + 14 * 11) == -1, "above the first and below the last row: nothing");
    expect(mn.item_at(11, 60) == -1 && mn.item_at(12, 60) >= 0 && mn.item_at(211, 60) >= 0 && mn.item_at(212, 60) == -1, "left and right edge of the rows");
    bool rh = true;
    mn.item_at(20, 60, &rh);
    expect(!rh, "left half");
    mn.item_at(200, 60, &rh);
    expect(rh, "right half");
    // hover
    expect(mn.hover(100, 46 + 14 * 4 + 5) && mn.cursor() == 4, "hover over row 4 selects it");
    expect(!mn.hover(100, 46 + 14 * 4 + 6), "hover again on the same row changes nothing");
    expect(!mn.hover(300, 100) && mn.cursor() == 4, "hover outside the box keeps the selection");
    // left click on a plain action: RESUME = Enter
    {
        Menu a;
        a.open();
        Result r = a.click(100, 50, m);
        expect(r.cmd == Cmd::Resume && r.delta == 1 && a.cursor() == 0, "click on RESUME resumes");
    }
    // click on CONTROLS (item 4) opens the page; its first line is BINDINGS; clicking BACK (last item) returns; right click returns / resumes
    {
        Menu a;
        a.open();
        Result r = a.click(100, 46 + 14 * 4 + 3, m);
        expect(r.cmd == Cmd::None && a.page() == 2, "click on CONTROLS opens the controls page");
        expect(a.visible_lines(m)[0].find("BINDINGS") == 0, "controls page starts with BINDINGS");
        int n = (int)a.visible_lines(m).size();
        r = a.click(100, 46 + 14 * (n - 1) + 3, m);
        expect(a.page() == 0 && r.cmd == Cmd::None, "click on BACK returns to the main page");
        a.click(100, 46 + 14 * 4 + 3, m);
        r = a.right_click(m);
        expect(a.page() == 0 && r.cmd == Cmd::None, "right click on a page goes back");
        r = a.right_click(m);
        expect(r.cmd == Cmd::Resume, "right click on the main page resumes");
    }
    // choice items: left half steps down, right half up (GAME OPTIONS page: LIVES is the first item)
    {
        Menu a;
        a.open();
        a.click(100, 46 + 14 * 3 + 3, m);
        expect(a.page() == 1, "click on GAME OPTIONS opens page 1");
        Result r = a.click(200, 46 + 3, m);
        expect(r.cmd == Cmd::Lives && r.delta == 1, "right half of LIVES: up");
        r = a.click(30, 46 + 3, m);
        expect(r.cmd == Cmd::Lives && r.delta == -1, "left half of LIVES: down");
        Model pm;
        pm.purist = true;
        a.right_click(pm);
        a.click(100, 46 + 14 * 5 + 3, pm); /* DISPLAY */
        expect(a.page() == 3, "DISPLAY page");
        r = a.click(200, 46 + 14 * 0 + 3, pm);
        expect(r.cmd == Cmd::Locked, "a locked item (Purist) answers Locked to the mouse too");
    }
    // a click outside any row does nothing
    {
        Menu a;
        a.open();
        Result r = a.click(300, 100, m);
        expect(r.cmd == Cmd::None && a.page() == 0, "click outside the box does nothing");
    }
    // window -> picture, several window sizes (the content is 352 x 288 while the menu is open)
    struct W {
        int ww, wh;
        display::Fit fit;
    };
    const W ws[] = {{704, 576, display::Fit::Integer}, {1056, 864, display::Fit::Integer}, {1280, 800, display::Fit::Integer}, {1280, 800, display::Fit::Fill},
                    {1920, 1080, display::Fit::Fill},  {500, 400, display::Fit::Fill},     {352, 288, display::Fit::Integer}};
    for (const W &w : ws) {
        display::Layout l = display::compute_layout(w.ww, w.wh, overlay::kOutW, overlay::kOutH, w.fit);
        // the centre of every menu row (picture coordinates) must map back to that row when the pointer is at the matching window pixel
        for (int i = 0; i < 9; ++i) {
            double py = 46 + 14 * i + 6.5, px = 100;
            int wx = l.dest.x + (int)(px * l.dest.w / overlay::kOutW), wy = l.dest.y + (int)(py * l.dest.h / overlay::kOutH), cx, cy;
            bool in = window_to_picture(l.dest.x, l.dest.y, l.dest.w, l.dest.h, overlay::kOutW, overlay::kOutH, wx, wy, &cx, &cy);
            Menu a;
            a.open();
            expect(in && a.item_at(cx, cy) == i,
                   "window " + std::to_string(w.ww) + "x" + std::to_string(w.wh) + (w.fit == display::Fit::Fill ? " fill" : " whole") + ": row " + std::to_string(i));
        }
        int cx, cy;
        expect(!window_to_picture(l.dest.x, l.dest.y, l.dest.w, l.dest.h, overlay::kOutW, overlay::kOutH, l.dest.x - 1, l.dest.y + 5, &cx, &cy),
               "left of the picture (black bar) is outside");
        expect(!window_to_picture(l.dest.x, l.dest.y, l.dest.w, l.dest.h, overlay::kOutW, overlay::kOutH, l.dest.x + l.dest.w, l.dest.y + 5, &cx, &cy),
               "right of the picture is outside");
        expect(window_to_picture(l.dest.x, l.dest.y, l.dest.w, l.dest.h, overlay::kOutW, overlay::kOutH, l.dest.x, l.dest.y, &cx, &cy) && cx == 0 && cy == 0,
               "top-left pixel is picture (0, 0)");
        expect(window_to_picture(l.dest.x, l.dest.y, l.dest.w, l.dest.h, overlay::kOutW, overlay::kOutH, l.dest.x + l.dest.w - 1, l.dest.y + l.dest.h - 1, &cx, &cy) &&
                   cx == overlay::kOutW - 1 && cy == overlay::kOutH - 1,
               "bottom-right pixel is the last picture pixel");
    }
    // ---- X05: the Bindings screen. Text row r is at picture y = 46 + 15 r (6 + 40 + 15 r); its highlight spans y - 3 .. y + 11. Slots: KEY 1 x 85..164, KEY 2 x 177..256 (see draw_bindings), PAD x 253..
    {
        int r, c;
        expect(bindings_cell_at(100, 46 - 3, &r, &c) && r == 0 && c == 0, "first pixel of row 0, KEY 1");
        expect(bindings_cell_at(100, 46 - 3 + 15 * 12 - 1, &r, &c) && r == 11 && c == 0, "last pixel of the last row (REWIND)");
        expect(!bindings_cell_at(100, 46 - 4, &r, &c) && !bindings_cell_at(100, 46 - 3 + 15 * 12, &r, &c), "above the first and below the last row: nothing");
        expect(!bindings_cell_at(11, 60, &r, &c) && !bindings_cell_at(340, 60, &r, &c) && bindings_cell_at(12, 60, &r, &c) && bindings_cell_at(339, 60, &r, &c),
               "left and right edge of the box");
        for (int row = 0; row < 12; ++row) {
            int y = 46 + 15 * row + 4;
            expect(bindings_cell_at(100, y, &r, &c) && r == row && c == 0, "row " + std::to_string(row) + " KEY 1");
            expect(bindings_cell_at(200, y, &r, &c) && r == row && c == 1, "row " + std::to_string(row) + " KEY 2");
            expect(bindings_cell_at(300, y, &r, &c) && r == row && c == 2, "row " + std::to_string(row) + " PAD BUTTON");
            expect(bindings_cell_at(30, y, &r, &c) && r == row && c == (row >= controls::kActions ? 2 : 0),
                   "row " + std::to_string(row) + " name goes to the first slot that can change");
        }
        controls::Bindings b;
        controls::BindEditor e;
        e.begin(b);
        e.point(4, 1);
        expect(e.row() == 4 && e.col() == 1 && !e.waiting(), "point moves the selection");
        e.click_cell(4, 0);
        expect(e.row() == 4 && e.col() == 0 && e.waiting(), "a click on COIN KEY 1 waits for a key");
        e.point(2, 2);
        expect(e.row() == 4 && e.col() == 0, "while waiting, hovering moves nothing");
        e.click_cell(5, 0);
        expect(e.row() == 5 && e.waiting(), "a click on another slot while waiting moves the wait there");
        expect(!e.back() && !e.waiting(), "right click (back) cancels the wait first");
        e.click_cell(0, 2);
        expect(!e.waiting() && !e.message().empty(), "a click on a fixed slot (UP, pad column) does not wait and says why: " + e.message());
        e.click_cell(9, 0);
        expect(!e.waiting() && !e.message().empty(), "a click on a fixed system key (SAVE, KEY 1) says why");
        e.click_cell(9, 2);
        expect(e.waiting(), "a click on the SAVE pad button waits for a button");
        expect(e.on_key(30) == false, "(a key while waiting for a pad button changes nothing)");
    }
    // the key panel header (pin)
    expect(help_pin_at(230, 8) && help_pin_at(351, 19) && help_pin_at(224, 4), "the panel header is the pin hit area");
    expect(!help_pin_at(223, 8) && !help_pin_at(230, 3) && !help_pin_at(230, 20) && !help_pin_at(352, 8), "just outside the pin hit area");
    std::printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
