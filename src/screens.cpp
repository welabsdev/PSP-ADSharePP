#include "adshare.hpp"

namespace adshare {

/* =========================================================
   DEVICE INFO SCREEN
   ========================================================= */

void device_info_screen(void)
{
    int page = 0;

    collect_device_info();

    while (!osl_quit) {
        char free_mem[64];
        char header_right[96];
        char board_short[160];
        char model_short[96];
        char active_channel[48];
        int configured_channel =
            PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC;

        poll_control_messages();

        if (g_has_pending_offer)
            return;

        snprintf(
            free_mem,
            sizeof(free_mem),
            "%.1f MB",
            sceKernelTotalFreeMemSize() / 1048576.0
        );

        snprintf(
            header_right,
            sizeof(header_right),
            "%s %s | %d/2",
            free_mem,
            ui_text("livres", "free"),
            page + 1
        );

        fit_text(
            g_device.board_family,
            board_short,
            sizeof(board_short),
            270
        );

        fit_text(
            g_device.model,
            model_short,
            sizeof(model_short),
            330
        );

        sceUtilityGetSystemParamInt(
            PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL,
            &configured_channel
        );

        if (g_connected_channel > 0 &&
            configured_channel ==
                PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC) {

            snprintf(
                active_channel,
                sizeof(active_channel),
                "%s (%s %d)",
                ui_text("Automatico", "Automatic"),
                ui_text("ativo", "active"),
                g_connected_channel
            );

        } else if (g_connected_channel > 0) {
            snprintf(
                active_channel,
                sizeof(active_channel),
                "%s %d",
                ui_text("Canal", "Channel"),
                g_connected_channel
            );
        } else {
            safe_copy(
                active_channel,
                g_device.adhoc_channel,
                sizeof(active_channel)
            );
        }

        oslStartDrawing();
        draw_background();

        draw_header(
            ui_text(
                "Informacoes do dispositivo",
                "Device information"
            ),
            header_right
        );

        draw_panel(
            18,
            36,
            462,
            238,
            C_PSN_BLUE
        );

        draw_psp_icon(
            35,
            48,
            C_PSN_BLUE
        );

        oslSetTextColor(C_TEXT);
        oslDrawString(
            83,
            49,
            g_device.nickname
        );

        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(
            83,
            67,
            model_short
        );

#define INFO_ROW(y, label, value) \
        do { \
            oslSetTextColor(C_TEXT_DIM); \
            oslDrawString(36, (y), (label)); \
            oslSetTextColor(C_TEXT); \
            oslDrawString(170, (y), (value)); \
        } while (0)

        if (page == 0) {
            INFO_ROW(
                94,
                ui_text("Modelo real:", "Real model:"),
                g_device.model
            );

            INFO_ROW(
                112,
                ui_text("Geracao:", "Generation:"),
                g_device.generation
            );

            INFO_ROW(
                130,
                "Firmware:",
                g_device.firmware
            );

            INFO_ROW(
                148,
                ui_text("Regiao/SKU:", "Region/SKU:"),
                g_device.region
            );

            INFO_ROW(
                166,
                ui_text("Idioma:", "System language:"),
                g_device.language
            );

            INFO_ROW(
                184,
                "MAC WLAN:",
                g_device.mac
            );

            INFO_ROW(
                202,
                ui_text("Canal Ad Hoc:", "Ad Hoc channel:"),
                active_channel
            );

            INFO_ROW(
                220,
                ui_text("Grupo:", "Group:"),
                g_connected_group
            );
        } else {
            char kernel_model[32];

            if (g_device.kernel_model_result >= 0) {
                snprintf(
                    kernel_model,
                    sizeof(kernel_model),
                    "%d (%s)",
                    g_device.kernel_model_result,
                    g_device.generation
                );
            } else {
                snprintf(
                    kernel_model,
                    sizeof(kernel_model),
                    "%s 0x%08X",
                    ui_text("Erro", "Error"),
                    (unsigned int)
                        g_device.kernel_model_result
                );
            }

            INFO_ROW(
                94,
                ui_text("Placa:", "Board:"),
                board_short
            );

            INFO_ROW(
                112,
                ui_text(
                    "Regiao IDStorage:",
                    "IDStorage region:"
                ),
                g_device.region_id_text
            );

            INFO_ROW(
                130,
                "Tachyon:",
                g_device.tachyon_text
            );

            INFO_ROW(
                148,
                "Baryon:",
                g_device.baryon_text
            );

            INFO_ROW(
                166,
                "Pommel:",
                g_device.pommel_text
            );

            INFO_ROW(
                184,
                "Kernel model:",
                kernel_model
            );

            INFO_ROW(
                202,
                "Probe:",
                g_device.probe_status_text
            );

            INFO_ROW(
                220,
                ui_text("Creditos:", "Credits:"),
                "welabsdev"
            );
        }

#undef INFO_ROW

        {
            const ButtonHint hints[] = {
                { PspButtonIcon::LR,     ui_text("Pagina", "Page") },
                { PspButtonIcon::Select, ui_text("Canal", "Channel") },
                { PspButtonIcon::Square, ui_text("English", "PT-BR") },
                { PspButtonIcon::Circle, ui_text("Voltar", "Back") }
            };
            draw_button_hints_centered(hints, 4, 255, 7);
        }

        oslEndDrawing();
        oslSyncFrame();
        oslReadKeys();

        if (osl_keys->pressed.L ||
            osl_keys->pressed.R) {
            page = !page;
        }

        if (osl_keys->pressed.square) {
            toggle_ui_language();
            collect_device_info();
        }

        if (osl_keys->pressed.select) {
            channel_selector_screen();
            collect_device_info();
        }

        if (osl_keys->pressed.circle ||
            osl_keys->pressed.triangle) {
            return;
        }
    }
}


/* =========================================================
   INCOMING OFFER UI
   ========================================================= */

void draw_incoming_offer(void)
{
    char size_text[64];
    char short_name[160];
    char sender[96];

    format_size(
        g_pending_offer.file_size,
        size_text,
        sizeof(size_text)
    );

    fit_text(
        g_pending_offer.file_name,
        short_name,
        sizeof(short_name),
        350
    );

    snprintf(
        sender,
        sizeof(sender),
        "%s %s",
        ui_text("De:", "From:"),
        g_pending_offer.nickname
    );

    draw_panel(
        30,
        58,
        450,
        207,
        C_WARNING
    );

    oslSetTextColor(C_WARNING);
    oslDrawString(
        center_x(
            ui_text(
                "Solicitacao de transferencia",
                "Transfer request"
            )
        ),
        74,
        ui_text(
            "Solicitacao de transferencia",
            "Transfer request"
        )
    );

    oslSetTextColor(C_TEXT);
    oslDrawString(
        52,
        105,
        sender
    );

    oslDrawString(
        52,
        130,
        short_name
    );

    oslSetTextColor(C_TEXT_DIM);
    oslDrawString(
        52,
        153,
        file_category_display(
            g_pending_offer.file_type
        )
    );

    oslDrawString(
        130,
        153,
        size_text
    );

    {
        const ButtonHint hints[] = {
            { PspButtonIcon::Cross,  ui_text("Aceitar", "Accept") },
            { PspButtonIcon::Circle, ui_text("Recusar", "Reject") }
        };
        draw_button_hints_centered(hints, 2, 180, 42, C_TEXT);
    }
}


/* =========================================================
   MAIN UI
   ========================================================= */

void draw_connection_panel(int selected_peer_index)
{
    char local_mac[32];
    char channel_text[32];
    PeerInfo* peer = NULL;

    mac_to_string(
        g_local_mac,
        local_mac,
        sizeof(local_mac)
    );

    if (g_connected_channel > 0) {
        snprintf(
            channel_text,
            sizeof(channel_text),
            "%s %d",
            ui_text("Canal", "Channel"),
            g_connected_channel
        );
    } else {
        safe_copy(
            channel_text,
            ui_text(
                "Canal Automatico",
                "Automatic Channel"
            ),
            sizeof(channel_text)
        );
    }

    if (selected_peer_index >= 0 &&
        selected_peer_index < MAX_PEERS &&
        g_peers[selected_peer_index].used) {

        peer = &g_peers[selected_peer_index];
    }

    draw_panel(
        12,
        34,
        468,
        92,
        C_PSN_BLUE_SOFT
    );

    draw_psp_icon(
        24,
        49,
        C_PSN_BLUE
    );

    oslSetTextColor(C_TEXT);
    oslDrawString(
        65,
        46,
        g_local_nickname
    );

    oslSetTextColor(C_TEXT_DIM);
    oslDrawString(
        65,
        66,
        local_mac
    );

    oslSetTextColor(
        peer ? C_SUCCESS : C_TEXT_DARK
    );

    oslDrawString(
        223,
        53,
        peer ? "<====>" : "<  .  >"
    );

    if (peer) {
        char peer_mac[32];
        char name[72];

        mac_to_string(
            peer->mac,
            peer_mac,
            sizeof(peer_mac)
        );

        fit_text(
            peer->nickname,
            name,
            sizeof(name),
            130
        );

        draw_psp_icon(
            292,
            49,
            C_SUCCESS
        );

        oslSetTextColor(C_TEXT);
        oslDrawString(
            333,
            46,
            name
        );

        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(
            333,
            66,
            peer_mac
        );
    } else {
        oslSetTextColor(C_TEXT_DIM);

        oslDrawString(
            293,
            51,
            ui_text(
                "Procurando PSPs...",
                "Searching for PSPs..."
            )
        );

        oslDrawString(
            293,
            70,
            channel_text
        );
    }
}


void draw_main_screen(void)
{
    int i;
    int n = 0;
    int selected_real =
        peer_index_from_visible(g_selected_peer);
    int visible = 4;
    int start =
        (g_selected_peer / visible) * visible;

    oslStartDrawing();
    draw_background();

    {
        char header_right[64];

        snprintf(
            header_right,
            sizeof(header_right),
            "Ad Hoc  |  %s",
            ADHOC_GROUP
        );

        draw_header(
            APP_NAME,
            header_right
        );
    }

    draw_connection_panel(selected_real);

    oslSetTextColor(C_TEXT_DIM);

    {
        char found[64];

        snprintf(
            found,
            sizeof(found),
            "%s %d",
            ui_text(
                "Consoles encontrados:",
                "Consoles found:"
            ),
            g_peer_count
        );

        oslDrawString(
            16,
            101,
            found
        );
    }

    if (g_peer_count == 0) {
        const char* line1 =
            ui_text(
                "Abra o ADShare em outro PSP.",
                "Open ADShare on another PSP."
            );

        const char* line2 =
            ui_text(
                "Os dois devem estar no mesmo canal Ad Hoc.",
                "Both must use the same Ad Hoc channel."
            );

        oslSetTextColor(C_TEXT_DIM);

        oslDrawString(
            center_x(line1),
            143,
            line1
        );

        oslDrawString(
            center_x(line2),
            163,
            line2
        );
    } else {
        for (i = 0; i < MAX_PEERS; ++i) {
            char name[96];
            char detail[160];
            int y;
            int selected;

            if (!g_peers[i].used)
                continue;

            if (n < start ||
                n >= start + visible) {
                n++;
                continue;
            }

            y = 120 + (n - start) * 29;
            selected = (n == g_selected_peer);

            if (selected) {
                oslDrawFillRect(
                    10,
                    y - 3,
                    470,
                    y + 23,
                    C_HIGHLIGHT
                );

                oslDrawFillRect(
                    10,
                    y - 3,
                    13,
                    y + 23,
                    C_PSN_BLUE
                );
            }

            fit_text(
                g_peers[i].nickname,
                name,
                sizeof(name),
                160
            );

            snprintf(
                detail,
                sizeof(detail),
                "%s | FW %s",
                g_peers[i].device[0]
                    ? g_peers[i].device
                    : "PSP",
                g_peers[i].firmware[0]
                    ? g_peers[i].firmware
                    : "?"
            );

            draw_psp_icon(
                25,
                y - 5,
                selected
                    ? C_PSN_BLUE
                    : RGBA(65,65,72,255)
            );

            oslSetTextColor(C_TEXT);
            oslDrawString(
                68,
                y,
                name
            );

            oslSetTextColor(C_TEXT_DIM);
            oslDrawString(
                222,
                y,
                detail
            );

            n++;
        }
    }

    oslSetTextColor(C_TEXT_DARK);

    {
        char status[160];

        fit_text(
            g_status_line,
            status,
            sizeof(status),
            440
        );

        oslDrawString(
            16,
            238,
            status
        );
    }

    {
        const ButtonHint hints[] = {
            { PspButtonIcon::Cross,    ui_text("Enviar", "Send") },
            { PspButtonIcon::Select,   ui_text("Canal", "Channel") },
            { PspButtonIcon::Square,   ui_text("English", "PT-BR") },
            { PspButtonIcon::Triangle, "Info" },
            { PspButtonIcon::R,        ui_text("Buscar", "Search") }
        };
        draw_button_hints_centered(hints, 5, 255, 6);
    }

    if (g_has_pending_offer)
        draw_incoming_offer();

    oslEndDrawing();
    oslSyncFrame();
}


/* =========================================================
   START SCREEN
   ========================================================= */

int start_screen(void)
{
    while (!osl_quit) {
        char version_text[48];
        char channel_line[96];
        const char* subtitle;
        const char* offline;
        const char* credit;

        snprintf(
            version_text,
            sizeof(version_text),
            "%s %s",
            ui_text("Versao", "Version"),
            APP_VERSION
        );

        subtitle =
            ui_text(
                "Compartilhamento direto entre PSPs",
                "Direct sharing between PSPs"
            );

        offline =
            ui_text(
                "Sem servidor, sem internet, PSP para PSP",
                "No server, no internet, PSP to PSP"
            );

        credit =
            ui_text(
                "Desenvolvido por welabsdev",
                "Developed by welabsdev"
            );

        snprintf(
            channel_line,
            sizeof(channel_line),
            "%s: %s",
            ui_text("Canal atual", "Current channel"),
            g_device.adhoc_channel
        );

        oslStartDrawing();
        draw_background();
        draw_header(
            APP_NAME,
            version_text
        );

        oslSetTextColor(C_TEXT);
        oslDrawString(
            center_x("A D S H A R E + +"),
            76,
            "A D S H A R E + +"
        );

        oslSetTextColor(C_PSN_BLUE);
        oslDrawString(
            center_x(subtitle),
            102,
            subtitle
        );

        draw_panel(
            70,
            132,
            410,
            174,
            C_PSN_BLUE_SOFT
        );

        {
            const ButtonHint hints[] = {
                {
                    PspButtonIcon::Start,
                    ui_text("Ativar Ad Hoc", "Enable Ad Hoc")
                }
            };
            draw_button_hints_centered(hints, 1, 147, 0, C_TEXT);
        }

        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(
            center_x(offline),
            191,
            offline
        );

        oslDrawString(
            center_x(channel_line),
            210,
            channel_line
        );

        oslSetTextColor(C_TEXT_DARK);
        oslDrawString(
            center_x(credit),
            229,
            credit
        );

        {
            const ButtonHint hints[] = {
                { PspButtonIcon::Select, ui_text("Canal", "Channel") },
                { PspButtonIcon::Square, ui_text("English", "PT-BR") },
                { PspButtonIcon::Start,  ui_text("Iniciar", "Start") }
            };
            draw_button_hints_centered(hints, 3, 255, 8);
        }

        oslEndDrawing();
        oslSyncFrame();
        oslReadKeys();

        if (osl_keys->pressed.square) {
            toggle_ui_language();
            collect_device_info();
            set_status("Pronto", "Ready");
            continue;
        }

        if (osl_keys->pressed.select) {
            channel_selector_screen();
            collect_device_info();
            continue;
        }

        if (osl_keys->pressed.start)
            return 1;
    }

    return 0;
}

} // namespace adshare
