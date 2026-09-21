#include "adshare.hpp"

namespace adshare {

/* =========================================================
   UI HELPERS
   ========================================================= */

int text_width(const char* text)
{
    if (!text)
        return 0;
    return g_font ? oslGetStringWidth(text) : (int)strlen(text) * 8;
}

int center_x(const char* text)
{
    return (480 - text_width(text)) / 2;
}

void fit_text(const char* src, char* dst, int dst_size, int max_width)
{
    int len;
    char test[320];

    safe_copy(dst, src, dst_size);
    if (text_width(dst) <= max_width)
        return;

    len = strlen(dst);
    while (len > 3) {
        dst[--len] = '\0';
        snprintf(test, sizeof(test), "%s...", dst);
        if (text_width(test) <= max_width) {
            safe_copy(dst, test, dst_size);
            return;
        }
    }
}

void draw_background(void)
{
    oslDrawFillRect(0, 0, 480, 272, C_BG);
    oslDrawFillRect(0, 0, 480, 24, C_BAR_BG);
    oslDrawFillRect(0, 24, 480, 25, C_PSN_BLUE);
    oslDrawFillRect(0, 248, 480, 249, RGBA(45, 45, 50, 255));
    oslDrawFillRect(0, 249, 480, 272, C_BAR_BG);
}

void draw_header(const char* title, const char* right)
{
    oslSetBkColor(RGBA(0, 0, 0, 0));
    oslSetTextColor(C_TEXT);
    oslDrawString(14, 6, title ? title : APP_NAME);

    if (right && right[0]) {
        int x = 466 - text_width(right);
        if (x < 250) x = 250;
        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(x, 6, right);
    }
}

void draw_footer(const char* text)
{
    oslSetTextColor(C_TEXT_DIM);
    oslDrawString(center_x(text), 255, text);
}

namespace {

void draw_pixel(int x, int y, unsigned int color)
{
    /*
     * OSLib is already the renderer used by ADShare++.
     * A 1x1-ish filled rectangle keeps this helper compatible with OSLib
     * packages that only expose the rectangle primitives used elsewhere
     * in the project.
     */
    oslDrawFillRect(x, y, x + 1, y + 1, color);
}

void draw_line_pixels(int x0, int y0, int x1, int y1, unsigned int color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        draw_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        const int e2 = err * 2;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_circle_pixels(int cx, int cy, int radius, unsigned int color)
{
    int x = radius;
    int y = 0;
    int err = 1 - radius;

    while (x >= y) {
        draw_pixel(cx + x, cy + y, color);
        draw_pixel(cx + y, cy + x, color);
        draw_pixel(cx - y, cy + x, color);
        draw_pixel(cx - x, cy + y, color);
        draw_pixel(cx - x, cy - y, color);
        draw_pixel(cx - y, cy - x, color);
        draw_pixel(cx + y, cy - x, color);
        draw_pixel(cx + x, cy - y, color);

        ++y;

        if (err < 0) {
            err += 2 * y + 1;
        } else {
            --x;
            err += 2 * (y - x + 1);
        }
    }
}

constexpr int BUTTON_ICON_HEIGHT = 16;
constexpr int BUTTON_TEXT_Y_OFFSET = 5;
constexpr int BUTTON_LABEL_GAP = 4;
constexpr int BUTTON_PAIR_GAP = 3;
constexpr int BUTTON_SCREEN_MARGIN = 6;

int compact_button_width(const char* label)
{
    /*
     * Two pixels of horizontal padding on each side plus the text.
     * The returned width is the exact visual width used by the renderer.
     */
    return text_width(label) + 8;
}

void draw_compact_button(const char* label, int x, int y, unsigned int border)
{
    const int w = compact_button_width(label);

    /*
     * All button glyphs now share the same 14 px high visual box.
     * This keeps L/R/START/SELECT vertically aligned with the
     * Cross/Circle/Square/Triangle icons and their labels.
     */
    oslDrawFillRect(x, y, x + w - 1, y + BUTTON_ICON_HEIGHT - 1, C_PANEL_2);
    oslDrawRect(x, y, x + w - 1, y + BUTTON_ICON_HEIGHT - 1, border);

    oslSetTextColor(C_TEXT);
    oslDrawString(
        x + (w - text_width(label)) / 2,
        y + BUTTON_TEXT_Y_OFFSET,
        label
    );
}

} // namespace

int button_icon_width(PspButtonIcon button)
{
    switch (button) {
        case PspButtonIcon::L:
            return compact_button_width("L");

        case PspButtonIcon::R:
            return compact_button_width("R");

        case PspButtonIcon::LR:
            return compact_button_width("L")
                 + BUTTON_PAIR_GAP
                 + compact_button_width("R");

        case PspButtonIcon::Start:
            return compact_button_width("START");

        case PspButtonIcon::Select:
            return compact_button_width("SELECT");

        case PspButtonIcon::Cross:
        case PspButtonIcon::Circle:
        case PspButtonIcon::Square:
        case PspButtonIcon::Triangle:
        default:
            return 14;
    }
}

void draw_button_icon(PspButtonIcon button, int x, int y)
{
    switch (button) {
        case PspButtonIcon::Cross:
            draw_line_pixels(x + 2, y + 3, x + 11, y + 12, C_BTN_CROSS);
            draw_line_pixels(x + 11, y + 3, x + 2, y + 12, C_BTN_CROSS);
            break;

        case PspButtonIcon::Circle:
            draw_circle_pixels(x + 7, y + 8, 5, C_BTN_CIRCLE);
            break;

        case PspButtonIcon::Square:
            oslDrawRect(x + 2, y + 3, x + 12, y + 13, C_BTN_SQUARE);
            break;

        case PspButtonIcon::Triangle:
            draw_line_pixels(x + 7, y + 2, x + 1, y + 13, C_BTN_TRIANGLE);
            draw_line_pixels(x + 1, y + 13, x + 13, y + 13, C_BTN_TRIANGLE);
            draw_line_pixels(x + 13, y + 13, x + 7, y + 2, C_BTN_TRIANGLE);
            break;

        case PspButtonIcon::L:
            draw_compact_button("L", x, y, C_BTN_NEUTRAL);
            break;

        case PspButtonIcon::R:
            draw_compact_button("R", x, y, C_BTN_NEUTRAL);
            break;

        case PspButtonIcon::LR: {
            const int lw = compact_button_width("L");
            draw_compact_button("L", x, y, C_BTN_NEUTRAL);
            draw_compact_button(
                "R",
                x + lw + BUTTON_PAIR_GAP,
                y,
                C_BTN_NEUTRAL
            );
            break;
        }

        case PspButtonIcon::Start:
            draw_compact_button("START", x, y, C_BTN_NEUTRAL);
            break;

        case PspButtonIcon::Select:
            draw_compact_button("SELECT", x, y, C_BTN_NEUTRAL);
            break;
    }
}

int button_hint_width(PspButtonIcon button, const char* label)
{
    const int icon_w = button_icon_width(button);
    const int label_w = (label && label[0]) ? text_width(label) : 0;

    return icon_w + (label_w > 0 ? BUTTON_LABEL_GAP + label_w : 0);
}

void draw_button_hint(PspButtonIcon button, const char* label,
                      int x, int y, unsigned int text_color)
{
    const int icon_w = button_icon_width(button);

    /*
     * y is now the TOP of the entire hint row.
     * The glyph and the label therefore share one coordinate system instead
     * of mixing y-1 for the icon with y for the text.
     */
    draw_button_icon(button, x, y);

    if (label && label[0]) {
        oslSetTextColor(text_color);
        oslDrawString(
            x + icon_w + BUTTON_LABEL_GAP,
            y + BUTTON_TEXT_Y_OFFSET,
            label
        );
    }
}

void draw_button_hints_centered(const ButtonHint* hints, int count,
                                int y, int gap, unsigned int text_color)
{
    int content_width = 0;
    int actual_gap = gap;
    int total;
    int x;
    const int available_width = 480 - (BUTTON_SCREEN_MARGIN * 2);

    if (!hints || count <= 0)
        return;

    for (int i = 0; i < count; ++i) {
        content_width += button_hint_width(
            hints[i].button,
            hints[i].label
        );
    }

    /*
     * Reduce only the inter-item spacing if a translated footer would exceed
     * the PSP screen. This keeps the whole row truly centered and prevents
     * the final hint from drifting off-screen.
     */
    if (count > 1) {
        const int max_gap =
            (available_width - content_width) / (count - 1);

        if (max_gap < actual_gap)
            actual_gap = max_gap;

        if (actual_gap < 2)
            actual_gap = 2;
    } else {
        actual_gap = 0;
    }

    total = content_width + actual_gap * (count - 1);
    x = (480 - total) / 2;

    if (x < BUTTON_SCREEN_MARGIN)
        x = BUTTON_SCREEN_MARGIN;

    for (int i = 0; i < count; ++i) {
        draw_button_hint(
            hints[i].button,
            hints[i].label,
            x,
            y,
            text_color
        );

        x += button_hint_width(
            hints[i].button,
            hints[i].label
        );

        if (i + 1 < count)
            x += actual_gap;
    }
}

void draw_panel(int x1, int y1, int x2, int y2, unsigned int border)
{
    oslDrawFillRect(x1, y1, x2, y2, C_PANEL);
    oslDrawRect(x1, y1, x2, y2, border);
}

void draw_psp_icon(int x, int y, unsigned int color)
{
    oslDrawFillRect(x + 2, y + 5, x + 28, y + 21, color);
    oslDrawRect(x + 5, y + 8, x + 25, y + 18, C_TEXT_DIM);
    oslDrawFillRect(x + 0, y + 10, x + 4, y + 16, color);
    oslDrawFillRect(x + 28, y + 10, x + 32, y + 16, color);
    oslDrawFillRect(x + 8, y + 24, x + 22, y + 26, C_TEXT_DARK);
}

void draw_file_icon(int x, int y, const char* category, int selected)
{
    unsigned int c = selected ? C_PSN_BLUE : RGBA(58, 58, 66, 255);

    if (!strcmp(category, "PHOTO") || !strcmp(category, "Foto")) {
        oslDrawFillRect(x + 3, y + 4, x + 27, y + 24, c);
        oslDrawRect(x + 6, y + 7, x + 24, y + 21, C_TEXT_DIM);
        oslDrawFillRect(x + 17, y + 10, x + 20, y + 13, C_TEXT);
    } else if (!strcmp(category, "MUSIC") || !strcmp(category, "Musica")) {
        oslDrawFillRect(x + 18, y + 4, x + 21, y + 20, c);
        oslDrawFillRect(x + 21, y + 4, x + 27, y + 7, c);
        oslDrawFillRect(x + 10, y + 17, x + 19, y + 25, c);
    } else if (!strcmp(category, "GAME") || !strcmp(category, "Jogo")) {
        oslDrawFillRect(x + 3, y + 8, x + 29, y + 22, c);
        oslDrawFillRect(x + 7, y + 4, x + 25, y + 10, c);
        oslDrawFillRect(x + 8, y + 13, x + 15, y + 15, C_TEXT);
        oslDrawFillRect(x + 10, y + 11, x + 12, y + 17, C_TEXT);
    } else {
        oslDrawFillRect(x + 6, y + 3, x + 25, y + 25, c);
        oslDrawRect(x + 9, y + 8, x + 22, y + 9, C_TEXT_DIM);
        oslDrawRect(x + 9, y + 13, x + 22, y + 14, C_TEXT_DIM);
    }
}

void draw_folder_icon(int x, int y, int selected)
{
    unsigned int mainc = selected ? C_PSN_BLUE : C_FOLDER;
    unsigned int tabc = selected ? C_PSN_BLUE : C_FOLDER_DARK;
    oslDrawFillRect(x + 2, y + 9, x + 29, y + 25, mainc);
    oslDrawFillRect(x + 2, y + 5, x + 15, y + 10, tabc);
}

void draw_loading(const char* title, const char* message)
{
    oslStartDrawing();
    draw_background();
    draw_header(APP_NAME, title);
    draw_panel(35, 92, 445, 164, C_PSN_BLUE);
    oslSetTextColor(C_TEXT);
    oslDrawString(center_x(message), 116, message);
    oslSetTextColor(C_TEXT_DIM);
    oslDrawString(center_x(ui_text("Nao desligue o PSP", "Do not turn off the PSP")), 141,
                  ui_text("Nao desligue o PSP", "Do not turn off the PSP"));
    oslEndDrawing();
    oslSyncFrame();
}

void show_message(const char* right, const char* title, const char* line1, const char* line2, unsigned int border)
{
    while (!osl_quit) {
        oslStartDrawing();
        draw_background();
        draw_header(APP_NAME, right);
        draw_panel(28, 72, 452, 196, border);

        oslSetTextColor(C_TEXT);
        oslDrawString(center_x(title), 92, title);
        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(center_x(line1 ? line1 : ""), 126, line1 ? line1 : "");
        if (line2 && line2[0])
            oslDrawString(center_x(line2), 147, line2);

        {
            const ButtonHint hints[] = {
                { PspButtonIcon::Circle, ui_text("Voltar", "Back") }
            };
            draw_button_hints_centered(hints, 1, 255);
        }
        oslEndDrawing();
        oslSyncFrame();

        oslReadKeys();
        if (osl_keys->pressed.circle || osl_keys->pressed.cross)
            break;
    }
}

} // namespace adshare
