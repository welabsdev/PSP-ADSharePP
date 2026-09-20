#include "adshare.hpp"

namespace adshare {

/* =========================================================
   FILE BROWSER
   ========================================================= */

static void path_parent(char* path)
{
    int len = strlen(path);
    char* slash;

    if (len <= 5)
        return;

    while (len > 5 && path[len - 1] == '/')
        path[--len] = '\0';

    slash = strrchr(path, '/');
    if (!slash)
        return;

    if (slash == path + 4)
        slash[1] = '\0';
    else
        *slash = '\0';
}

static int load_directory(const char* path, FileEntry* entries, int max_entries)
{
    SceUID d;
    SceIoDirent ent;
    int count = 0;

    d = sceIoDopen(path);
    if (d < 0)
        return 0;

    while (count < max_entries) {
        memset(&ent, 0, sizeof(ent));
        if (sceIoDread(d, &ent) <= 0)
            break;

        if (!strcmp(ent.d_name, ".") || !strcmp(ent.d_name, ".."))
            continue;

        safe_copy(entries[count].name, ent.d_name, sizeof(entries[count].name));
        entries[count].is_dir = FIO_S_ISDIR(ent.d_stat.st_mode) ? 1 : 0;
        entries[count].size = ent.d_stat.st_size;
        count++;
    }

    sceIoDclose(d);
    return count;
}

/*
 * Browser de destino usado no PSP que RECEBE o arquivo.
 *
 * X        entra na pasta destacada
 * QUADRADO escolhe a pasta atual
 * O        volta; na raiz cancela
 * L        alterna ms0:/ <-> ef0:/ quando ef0:/ estiver disponivel
 *
 * Ele mostra somente diretorios. Nenhuma pasta ADShare e criada.
 */
int destination_folder_browser(char* selected_dir, int selected_size)
{
    FileEntry all_entries[MAX_FILES];
    FileEntry dirs[MAX_FILES];
    char current[512];
    int use_internal = 0;
    int sel = 0;

    if (!selected_dir || selected_size <= 0)
        return 0;

    selected_dir[0] = '\0';

    if (directory_exists("ms0:/")) {
        safe_copy(current, "ms0:/", sizeof(current));
        use_internal = 0;
    } else if (directory_exists("ef0:/")) {
        safe_copy(current, "ef0:/", sizeof(current));
        use_internal = 1;
    } else {
        set_status("Nenhum armazenamento disponivel",
                   "No storage available");
        return 0;
    }

    while (!osl_quit) {
        int all_count;
        int dir_count = 0;
        int i;
        int start;
        int visible = 6;
        char path_display[160];

        /*
         * Mantem a descoberta/controle vivos enquanto o usuario navega.
         * Retries da mesma oferta sao ignorados por poll_control_messages().
         */
        poll_control_messages();

        all_count = load_directory(
            current,
            all_entries,
            MAX_FILES
        );

        for (i = 0; i < all_count && dir_count < MAX_FILES; ++i) {
            if (!all_entries[i].is_dir)
                continue;

            dirs[dir_count++] = all_entries[i];
        }

        if (sel >= dir_count)
            sel = dir_count > 0 ? dir_count - 1 : 0;

        if (sel < 0)
            sel = 0;

        start = (sel / visible) * visible;

        oslStartDrawing();
        draw_background();

        draw_header(
            ui_text("Escolher onde salvar", "Choose save location"),
            "ADShare"
        );

        fit_text(
            current,
            path_display,
            sizeof(path_display),
            440
        );

        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(15, 34, path_display);

        /*
         * A pasta atual e sempre uma opcao valida, inclusive ms0:/ e ef0:/.
         */
        draw_panel(12, 48, 468, 76, C_PSN_BLUE_SOFT);

        oslSetTextColor(C_TEXT);
        oslDrawString(
            24,
            57,
            ui_text(
                "[QUADRADO] Salvar nesta pasta",
                "[SQUARE] Save in this folder"
            )
        );

        if (dir_count == 0) {
            const char* no_dirs =
                ui_text(
                    "Nenhuma subpasta. Voce pode salvar nesta pasta.",
                    "No subfolders. You can save in this folder."
                );

            oslSetTextColor(C_TEXT_DIM);
            oslDrawString(
                center_x(no_dirs),
                137,
                no_dirs
            );
        }

        for (i = start;
             i < start + visible && i < dir_count;
             ++i) {

            int row = i - start;
            int y = 91 + row * 25;
            int selected = (i == sel);
            char short_name[160];

            if (selected) {
                oslDrawFillRect(
                    10,
                    y - 3,
                    470,
                    y + 19,
                    C_HIGHLIGHT
                );

                oslDrawFillRect(
                    10,
                    y - 3,
                    13,
                    y + 19,
                    C_PSN_BLUE
                );
            }

            draw_folder_icon(
                24,
                y - 5,
                selected
            );

            fit_text(
                dirs[i].name,
                short_name,
                sizeof(short_name),
                360
            );

            oslSetTextColor(
                selected ? C_TEXT : C_TEXT_DIM
            );

            oslDrawString(
                60,
                y,
                short_name
            );
        }

        if (directory_exists("ef0:/")) {
            draw_footer(
                ui_text(
                    "[X] Abrir [QUADRADO] Salvar aqui [O] Voltar [L] ms0:/ef0:",
                    "[X] Open [SQUARE] Save here [O] Back [L] ms0:/ef0:"
                )
            );
        } else {
            draw_footer(
                ui_text(
                    "[X] Abrir [QUADRADO] Salvar aqui [O] Voltar",
                    "[X] Open [SQUARE] Save here [O] Back"
                )
            );
        }

        oslEndDrawing();
        oslSyncFrame();
        oslReadKeys();

        if (osl_keys->pressed.down && dir_count > 0) {
            sel++;
            if (sel >= dir_count)
                sel = 0;
        }

        if (osl_keys->pressed.up && dir_count > 0) {
            sel--;
            if (sel < 0)
                sel = dir_count - 1;
        }

        if (osl_keys->pressed.L &&
            directory_exists("ef0:/")) {

            use_internal = !use_internal;

            safe_copy(
                current,
                use_internal ? "ef0:/" : "ms0:/",
                sizeof(current)
            );

            sel = 0;
            continue;
        }

        if (osl_keys->pressed.square) {
            safe_copy(
                selected_dir,
                current,
                selected_size
            );

            return 1;
        }

        if (osl_keys->pressed.cross && dir_count > 0) {
            char next[512];

            join_path(
                current,
                dirs[sel].name,
                next,
                sizeof(next)
            );

            if (directory_exists(next)) {
                safe_copy(
                    current,
                    next,
                    sizeof(current)
                );
                sel = 0;
            }

            continue;
        }

        if (osl_keys->pressed.circle) {
            if (strlen(current) > 5) {
                path_parent(current);
                sel = 0;
            } else {
                selected_dir[0] = '\0';
                return 0;
            }
        }
    }

    selected_dir[0] = '\0';
    return 0;
}

int file_browser(char* selected_path, int selected_size)
{
    FileEntry entries[MAX_FILES];
    char current[512];
    int count;
    int sel = 0;
    int use_internal;

    if (directory_exists("ms0:/")) {
        safe_copy(current, "ms0:/", sizeof(current));
        use_internal = 0;
    } else {
        safe_copy(current, "ef0:/", sizeof(current));
        use_internal = 1;
    }

    while (!osl_quit) {
        int start;
        int i;
        int visible = 6;
        char path_display[160];

        poll_control_messages();

        if (g_has_pending_offer)
            return 0;

        count = load_directory(
            current,
            entries,
            MAX_FILES
        );

        if (sel >= count)
            sel = count > 0 ? count - 1 : 0;

        if (sel < 0)
            sel = 0;

        start = (sel / visible) * visible;

        oslStartDrawing();
        draw_background();

        draw_header(
            ui_text("Selecionar arquivo", "Select file"),
            "ADShare"
        );

        fit_text(
            current,
            path_display,
            sizeof(path_display),
            430
        );

        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(15, 34, path_display);

        if (count == 0) {
            const char* empty =
                ui_text(
                    "Esta pasta esta vazia.",
                    "This folder is empty."
                );

            oslSetTextColor(C_TEXT_DIM);
            oslDrawString(
                center_x(empty),
                120,
                empty
            );
        }

        for (i = start;
             i < start + visible && i < count;
             ++i) {

            int row = i - start;
            int y = 58 + row * 30;
            char name_short[160];
            int selected = (i == sel);

            if (selected) {
                oslDrawFillRect(
                    10,
                    y - 3,
                    470,
                    y + 24,
                    C_HIGHLIGHT
                );

                oslDrawFillRect(
                    10,
                    y - 3,
                    13,
                    y + 24,
                    C_PSN_BLUE
                );
            }

            fit_text(
                entries[i].name,
                name_short,
                sizeof(name_short),
                320
            );

            if (entries[i].is_dir) {
                draw_folder_icon(
                    22,
                    y - 3,
                    selected
                );
            } else {
                draw_file_icon(
                    22,
                    y - 3,
                    file_category(entries[i].name),
                    selected
                );
            }

            oslSetTextColor(C_TEXT);
            oslDrawString(
                60,
                y + 3,
                name_short
            );

            if (!entries[i].is_dir) {
                char size_text[48];

                format_size(
                    (uint64_t)entries[i].size,
                    size_text,
                    sizeof(size_text)
                );

                oslSetTextColor(C_TEXT_DARK);
                oslDrawString(
                    390,
                    y + 3,
                    size_text
                );
            }
        }

        if (count > visible) {
            float h = 174.0f / count;
            float y = 58.0f + sel * h;

            oslDrawFillRect(
                474,
                58,
                477,
                232,
                C_SCROLL_BG
            );

            oslDrawFillRect(
                474,
                y,
                477,
                y + h * visible,
                C_SCROLL_BAR
            );
        }

        if (directory_exists("ef0:/")) {
            draw_footer(
                ui_text(
                    "[X] Abrir/Enviar  [O] Voltar  [L] ms0:/ef0:",
                    "[X] Open/Send     [O] Back    [L] ms0:/ef0:"
                )
            );
        } else {
            draw_footer(
                ui_text(
                    "[X] Abrir/Enviar              [O] Voltar",
                    "[X] Open/Send                 [O] Back"
                )
            );
        }

        oslEndDrawing();
        oslSyncFrame();
        oslReadKeys();

        if (osl_keys->pressed.square) {
            toggle_ui_language();
            collect_device_info();
        }

        if (osl_keys->pressed.down && count > 0) {
            sel++;
            if (sel >= count)
                sel = 0;
        }

        if (osl_keys->pressed.up && count > 0) {
            sel--;
            if (sel < 0)
                sel = count - 1;
        }

        if (osl_keys->pressed.L &&
            directory_exists("ef0:/")) {

            use_internal = !use_internal;

            safe_copy(
                current,
                use_internal ? "ef0:/" : "ms0:/",
                sizeof(current)
            );

            sel = 0;
        }

        if (osl_keys->pressed.circle) {
            if (strlen(current) > 5) {
                path_parent(current);
                sel = 0;
            } else {
                return 0;
            }
        }

        if (osl_keys->pressed.cross &&
            count > 0) {

            char next[512];

            join_path(
                current,
                entries[sel].name,
                next,
                sizeof(next)
            );

            if (entries[sel].is_dir) {
                safe_copy(
                    current,
                    next,
                    sizeof(current)
                );

                sel = 0;
            } else {
                safe_copy(
                    selected_path,
                    next,
                    selected_size
                );

                return 1;
            }
        }
    }

    return 0;
}

} // namespace adshare
