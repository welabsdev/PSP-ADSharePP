#include "adshare.hpp"
#include "app.hpp"

namespace adshare {

Application::~Application() noexcept
{
    shutdown();
}

void Application::initialize()
{
    setup_callbacks();

    oslInit(0);
    oslInitGfx(OSL_PF_8888, 1);
    oslIntraFontInit(INTRAFONT_CACHE_ALL);

    g_font = oslLoadIntraFontFile(
        "flash0:/font/ltn8.pgf",
        INTRAFONT_CACHE_ALL
    );

    if (g_font)
        oslSetFont(g_font);

    oslSetKeyAutorepeatInit(40);
    oslSetKeyAutorepeatInterval(5);

    init_ui_language();
    collect_device_info();
    set_status("Pronto", "Ready");

    initialized_ = true;
}

void Application::shutdown() noexcept
{
    if (shutdown_done_)
        return;

    stop_adhoc();

    if (g_font) {
        oslDeleteFont(g_font);
        g_font = nullptr;
    }

    if (initialized_) {
        oslEndGfx();
        oslQuit();
    }

    shutdown_done_ = true;
}

void Application::main_loop()
{
    send_hello();

    while (!osl_quit) {
        int selected_real = -1;

        poll_control_messages();

        if (g_peer_count <= 0) {
            g_selected_peer = 0;
        } else {
            if (g_selected_peer >= g_peer_count)
                g_selected_peer = g_peer_count - 1;
            if (g_selected_peer < 0)
                g_selected_peer = 0;
        }

        draw_main_screen();
        oslReadKeys();

        // An incoming offer always has priority over normal menu actions.
        if (g_has_pending_offer) {
            if (osl_keys->pressed.cross) {
                const AdPacket offer = g_pending_offer;
                unsigned char sender[6];
                char destination_dir[512];

                memcpy(sender, g_pending_offer_mac, sizeof(sender));

                // Tell the sender to keep waiting while the receiver browses.
                send_simple_response(sender, PKT_PREPARE, offer.token);

                set_status(
                    "Escolha a pasta onde deseja salvar",
                    "Choose the folder where you want to save"
                );

                if (destination_folder_browser(
                        destination_dir,
                        sizeof(destination_dir))) {

                    g_has_pending_offer = 0;
                    receive_file_offer(&offer, sender, destination_dir);
                } else {
                    send_simple_response(sender, PKT_REJECT, offer.token);
                    g_has_pending_offer = 0;
                    set_status("Recebimento cancelado", "Receive cancelled");
                }

                continue;
            }

            if (osl_keys->pressed.circle) {
                send_simple_response(
                    g_pending_offer_mac,
                    PKT_REJECT,
                    g_pending_offer.token
                );

                g_has_pending_offer = 0;
                set_status("Arquivo recusado", "File rejected");
                continue;
            }

            continue;
        }

        if (osl_keys->pressed.down && g_peer_count > 0) {
            ++g_selected_peer;
            if (g_selected_peer >= g_peer_count)
                g_selected_peer = 0;
        }

        if (osl_keys->pressed.up && g_peer_count > 0) {
            --g_selected_peer;
            if (g_selected_peer < 0)
                g_selected_peer = g_peer_count - 1;
        }

        if (osl_keys->pressed.R) {
            send_hello();
            set_status("Descoberta atualizada", "Discovery refreshed");
        }

        if (osl_keys->pressed.square) {
            toggle_ui_language();
            collect_device_info();
            set_status(
                "Idioma alterado para Portugues (Brasil)",
                "Language changed to English"
            );
            continue;
        }

        if (osl_keys->pressed.select) {
            channel_selector_screen();
            collect_device_info();
            continue;
        }

        if (osl_keys->pressed.triangle) {
            device_info_screen();
            continue;
        }

        if (osl_keys->pressed.cross && g_peer_count > 0) {
            char selected_path[512];
            selected_real = peer_index_from_visible(g_selected_peer);

            if (selected_real >= 0 && g_peers[selected_real].used) {
                if (file_browser(selected_path, sizeof(selected_path))) {
                    // Copy because discovery can refresh the peer list while waiting.
                    const PeerInfo target = g_peers[selected_real];
                    send_offer_and_wait(&target, selected_path);
                }
            }
        }

        sceKernelDelayThread(16000);
    }
}

int Application::run()
{
    initialize();

    if (!start_screen())
        return 0;

    if (!start_adhoc())
        return 0;

    main_loop();
    return 0;
}

} // namespace adshare
