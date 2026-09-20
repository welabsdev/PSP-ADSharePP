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

        draw_footer(ui_text("[O] Voltar", "[O] Back"));
        oslEndDrawing();
        oslSyncFrame();

        oslReadKeys();
        if (osl_keys->pressed.circle || osl_keys->pressed.cross)
            break;
    }
}

} // namespace adshare
