#include "adshare.hpp"

namespace adshare {

/* =========================================================
   AD HOC INIT / SHUTDOWN
   ========================================================= */


static int wait_adhoc_connected(int timeout_ms)
{
    int elapsed = 0;
    int state = 0;

    while (elapsed < timeout_ms) {
        int ret = sceNetAdhocctlGetState(&state);
        if (ret >= 0 && state == 1)
            return 1;

        draw_loading("Ad Hoc", ui_text("Entrando no grupo ADShare...", "Joining ADShare group..."));
        sceKernelDelayThread(100000);
        elapsed += 100;
    }

    return 0;
}

int start_adhoc(void)
{
    int ret;
    struct productStruct product;
    struct SceNetAdhocctlParams params;

    if (g_adhoc_connected && g_pdp_id >= 0)
        return 1;

    if (!sceWlanGetSwitchState()) {
        show_message(ui_text("Erro", "Error"),
                     ui_text("WLAN desligada", "WLAN is off"),
                     ui_text("Ative a chave WLAN do PSP.", "Turn on the PSP WLAN switch."),
                     ui_text("Depois tente novamente.", "Then try again."), C_ERROR);
        return 0;
    }

    /* Mantem o clock padrao seguro para WLAN e reduz consumo desnecessario. */
    scePowerSetClockFrequency(222, 222, 111);

    draw_loading("Ad Hoc", ui_text("Carregando modulos de rede...", "Loading network modules..."));

    ret = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    if (ret < 0 && ret != (int)0x80111102) {
        char msg[96];
        snprintf(msg, sizeof(msg), "COMMON: 0x%08X", (unsigned int)ret);
        show_message(ui_text("Erro", "Error"), ui_text("Falha na rede", "Network failure"), msg, "", C_ERROR);
        return 0;
    }

    ret = sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);
    if (ret < 0 && ret != (int)0x80111102) {
        char msg[96];
        snprintf(msg, sizeof(msg), "ADHOC: 0x%08X", (unsigned int)ret);
        show_message(ui_text("Erro", "Error"), ui_text("Falha no modulo Ad Hoc", "Ad Hoc module failure"), msg, "", C_ERROR);
        return 0;
    }

    ret = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
    if (ret < 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "sceNetInit: 0x%08X", (unsigned int)ret);
        show_message(ui_text("Erro", "Error"), ui_text("Falha ao iniciar rede", "Failed to initialize network"), msg, "", C_ERROR);
        return 0;
    }
    g_net_initialized = 1;

    ret = sceNetAdhocInit();
    if (ret < 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "sceNetAdhocInit: 0x%08X", (unsigned int)ret);
        stop_adhoc();
        show_message(ui_text("Erro", "Error"), ui_text("Falha no Ad Hoc", "Ad Hoc failure"), msg, "", C_ERROR);
        return 0;
    }
    g_adhoc_initialized = 1;

    /*
     * productStruct.product possui EXATAMENTE 9 bytes.
     * Em PSP real, um Product ID curto (ex.: 8 chars + '\0') faz
     * sceNetAdhocctlInit() retornar 0x80410B04 (INVALID_ARG).
     *
     * Portanto NÃO use strcpy/safe_copy aqui. Copiamos os 9 bytes
     * diretamente, sem terminador NUL dentro de product.product.
     */
    memset(&product, 0, sizeof(product));
    product.unknown = 0;
    memcpy(product.product, ADHOC_PRODUCT, sizeof(product.product));

    ret = sceNetAdhocctlInit(0x2000, 0x30, &product);
    if (ret < 0) {
        char msg[128];

        if ((unsigned int)ret == 0x80410B04u) {
            snprintf(msg, sizeof(msg),
                     "adhocctlInit: 0x%08X (Product ID invalid)",
                     (unsigned int)ret);
        } else if ((unsigned int)ret == 0x80410B03u) {
            snprintf(msg, sizeof(msg),
                     "adhocctlInit: 0x%08X (WLAN off)",
                     (unsigned int)ret);
        } else if ((unsigned int)ret == 0x80410B07u) {
            snprintf(msg, sizeof(msg),
                     "adhocctlInit: 0x%08X (already initialized)",
                     (unsigned int)ret);
        } else if ((unsigned int)ret == 0x80410B13u) {
            snprintf(msg, sizeof(msg),
                     "adhocctlInit: 0x%08X (stack too small)",
                     (unsigned int)ret);
        } else {
            snprintf(msg, sizeof(msg),
                     "adhocctlInit: 0x%08X",
                     (unsigned int)ret);
        }

        stop_adhoc();
        show_message(ui_text("Erro", "Error"),
                     ui_text("Falha no controle Ad Hoc", "Ad Hoc control failure"),
                     msg,
                     ui_text("Verifique WLAN e reinicie o ADShare.",
                             "Check WLAN and restart ADShare."), C_ERROR);
        return 0;
    }
    g_adhocctl_initialized = 1;

    collect_device_info();

    ret = sceNetAdhocctlConnect(ADHOC_GROUP);
    if (ret < 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "adhocctlConnect: 0x%08X", (unsigned int)ret);
        stop_adhoc();
        show_message(ui_text("Erro", "Error"), ui_text("Nao foi possivel entrar no grupo", "Could not join group"), msg, "", C_ERROR);
        return 0;
    }

    if (!wait_adhoc_connected(12000)) {
        stop_adhoc();
        show_message(ui_text("Erro", "Error"),
                     ui_text("Timeout no Ad Hoc", "Ad Hoc timeout"),
                     ui_text("Os PSPs devem usar o mesmo canal.",
                             "Both PSPs must use the same channel."),
                     ui_text("Escolha o mesmo canal nos dois consoles.",
                             "Choose the same channel on both consoles."), C_ERROR);
        return 0;
    }

    g_adhoc_connected = 1;

    memset(&params, 0, sizeof(params));
    if (sceNetAdhocctlGetParameter(&params) >= 0) {
        g_connected_channel = params.channel;
        safe_copy(g_connected_group, params.name, sizeof(g_connected_group));
        if (params.nickname[0]) {
            safe_copy(g_local_nickname, params.nickname, sizeof(g_local_nickname));
            safe_copy(g_device.nickname, params.nickname, sizeof(g_device.nickname));
        }
    }

    if (sceWlanGetEtherAddr(g_local_mac) < 0) {
        stop_adhoc();
        show_message(ui_text("Erro", "Error"),
                     ui_text("Nao foi possivel obter o MAC WLAN",
                             "Could not read WLAN MAC"),
                     ui_text("Verifique o hardware de rede.",
                             "Check the network hardware."), "", C_ERROR);
        return 0;
    }

    /* Atualiza o MAC exibido depois que a WLAN estiver efetivamente ativa. */
    {
        char mac_text[32];
        mac_to_string(g_local_mac, mac_text, sizeof(mac_text));
        safe_copy(g_device.mac, mac_text, sizeof(g_device.mac));
    }

    g_pdp_id = sceNetAdhocPdpCreate(g_local_mac, CTRL_PORT, PDP_BUFFER_SIZE, 0);
    if (g_pdp_id < 0) {
        char msg[96];
        int pdp_error = g_pdp_id;
        g_pdp_id = -1;
        stop_adhoc();
        snprintf(msg, sizeof(msg), "PDP: 0x%08X", (unsigned int)pdp_error);
        show_message(ui_text("Erro", "Error"), ui_text("Falha no canal de descoberta", "Discovery channel failure"), msg, "", C_ERROR);
        return 0;
    }

    g_last_hello = 0;
    g_outgoing_waiting = 0;
    g_transfer_busy = 0;
    g_has_pending_offer = 0;
    memset(g_peers, 0, sizeof(g_peers));
    g_peer_count = 0;
    g_selected_peer = 0;
    set_status("Ad Hoc ativo - procurando consoles...", "Ad Hoc active - searching for consoles...");
    return 1;
}

void stop_adhoc(void)
{
    if (g_pdp_id >= 0) {
        sceNetAdhocPdpDelete(g_pdp_id, 0);
        g_pdp_id = -1;
    }

    if (g_adhocctl_initialized) {
        if (g_adhoc_connected)
            sceNetAdhocctlDisconnect();

        sceNetAdhocctlTerm();
        g_adhocctl_initialized = 0;
    }

    if (g_adhoc_initialized) {
        sceNetAdhocTerm();
        g_adhoc_initialized = 0;
    }

    if (g_net_initialized) {
        sceNetTerm();
        g_net_initialized = 0;
    }

    g_adhoc_connected = 0;
    g_connected_channel = 0;
    g_outgoing_waiting = 0;
    g_transfer_busy = 0;
    g_has_pending_offer = 0;
}

/* =========================================================
   PEERS / CONTROLE PDP
   ========================================================= */

static int find_peer(const unsigned char* mac)
{
    int i;
    for (i = 0; i < MAX_PEERS; ++i)
        if (g_peers[i].used && mac_equal(g_peers[i].mac, mac))
            return i;
    return -1;
}

static int alloc_peer(void)
{
    int i;
    for (i = 0; i < MAX_PEERS; ++i)
        if (!g_peers[i].used)
            return i;
    return -1;
}

static void recount_peers(void)
{
    int i;
    g_peer_count = 0;
    for (i = 0; i < MAX_PEERS; ++i)
        if (g_peers[i].used)
            g_peer_count++;
}

static void touch_peer(const unsigned char* mac, const AdPacket* p)
{
    int i = find_peer(mac);
    if (i < 0)
        i = alloc_peer();
    if (i < 0)
        return;

    g_peers[i].used = 1;
    memcpy(g_peers[i].mac, mac, 6);
    g_peers[i].last_seen = now_us();

    if (p) {
        safe_copy(g_peers[i].nickname, p->nickname, sizeof(g_peers[i].nickname));
        safe_copy(g_peers[i].device, p->device, sizeof(g_peers[i].device));
        safe_copy(g_peers[i].firmware, p->firmware, sizeof(g_peers[i].firmware));
    }

    if (!g_peers[i].nickname[0]) {
        char full_nickname[128];
        memset(full_nickname, 0, sizeof(full_nickname));
        if (sceNetAdhocctlGetNameByAddr(g_peers[i].mac, full_nickname) >= 0)
            safe_copy(g_peers[i].nickname, full_nickname, sizeof(g_peers[i].nickname));
    }

    if (!g_peers[i].nickname[0])
        safe_copy(g_peers[i].nickname, "PSP", sizeof(g_peers[i].nickname));

    recount_peers();
}

static void prune_peers(void)
{
    int i;
    uint32_t now = now_us();

    for (i = 0; i < MAX_PEERS; ++i) {
        if (g_peers[i].used && (uint32_t)(now - g_peers[i].last_seen) > PEER_TIMEOUT_US)
            memset(&g_peers[i], 0, sizeof(g_peers[i]));
    }

    recount_peers();
}

void fill_common_packet(AdPacket* p, int type)
{
    memset(p, 0, sizeof(*p));
    memcpy(p->magic, PROTOCOL_MAGIC, 4);
    p->version = PROTOCOL_VERSION;
    p->type = type;
    memcpy(p->sender_mac, g_local_mac, 6);
    safe_copy(p->nickname, g_local_nickname, sizeof(p->nickname));
    safe_copy(p->device, g_device.model, sizeof(p->device));
    safe_copy(p->firmware, g_device.firmware, sizeof(p->firmware));
}

int send_packet_to(const unsigned char* mac, AdPacket* p)
{
    if (g_pdp_id < 0)
        return -1;

    return sceNetAdhocPdpSend(
        g_pdp_id,
        (unsigned char*)mac,
        CTRL_PORT,
        p,
        sizeof(*p),
        500000,
        0
    );
}

void send_hello(void)
{
    static unsigned char broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    AdPacket p;
    fill_common_packet(&p, PKT_HELLO);
    send_packet_to(broadcast, &p);
    g_last_hello = now_us();
}

void send_simple_response(const unsigned char* mac, int type, uint32_t token)
{
    AdPacket p;
    fill_common_packet(&p, type);
    p.token = token;
    send_packet_to(mac, &p);
}

void poll_control_messages(void)
{
    while (g_pdp_id >= 0) {
        AdPacket p;
        unsigned char srcmac[6];
        unsigned short srcport = 0;
        int len = sizeof(p);
        int ret;

        memset(&p, 0, sizeof(p));
        ret = sceNetAdhocPdpRecv(
            g_pdp_id,
            srcmac,
            &srcport,
            &p,
            &len,
            0,
            1
        );

        if (ret < 0)
            break;

        if (len < 12 || memcmp(p.magic, PROTOCOL_MAGIC, 4) != 0 ||
            p.version != PROTOCOL_VERSION)
            continue;

        if (mac_equal(srcmac, g_local_mac))
            continue;

        touch_peer(srcmac, &p);

        switch (p.type) {
            case PKT_HELLO:
                break;

            case PKT_OFFER:
                if (g_has_pending_offer) {
                    /*
                     * O remetente repete a oferta ate receber resposta.
                     * Se for a mesma oferta ja exibida, apenas ignore o retry.
                     */
                    if (p.token != g_pending_offer.token ||
                        !mac_equal(srcmac, g_pending_offer_mac))
                        send_simple_response(srcmac, PKT_REJECT, p.token);
                } else if (!g_outgoing_waiting && !g_transfer_busy) {
                    g_pending_offer = p;
                    memcpy(g_pending_offer_mac, srcmac, 6);
                    g_has_pending_offer = 1;
                    set_status("Solicitacao de arquivo recebida", "File request received");
                } else {
                    /* Evita duas transferencias simultaneas no mesmo PSP. */
                    send_simple_response(srcmac, PKT_REJECT, p.token);
                }
                break;

            case PKT_PREPARE:
                if (p.token == g_outgoing_token &&
                    mac_equal(srcmac, g_outgoing_peer_mac))
                    g_outgoing_response = 2;
                break;

            case PKT_ACCEPT:
                if (p.token == g_outgoing_token &&
                    mac_equal(srcmac, g_outgoing_peer_mac))
                    g_outgoing_response = 1;
                break;

            case PKT_REJECT:
            case PKT_CANCEL:
                if (p.token == g_outgoing_token &&
                    mac_equal(srcmac, g_outgoing_peer_mac))
                    g_outgoing_response = -1;
                break;

            case PKT_DONE:
                set_status("Transferencia concluida pelo outro PSP", "Transfer completed by the other PSP");
                break;
        }
    }

    if (elapsed_us(g_last_hello, HELLO_INTERVAL_US))
        send_hello();

    prune_peers();
}


/* =========================================================
   SELETOR DE CANAL AD HOC
   ========================================================= */

static const int g_channel_values[] = {
    PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC,
    PSP_SYSTEMPARAM_ADHOC_CHANNEL_1,
    PSP_SYSTEMPARAM_ADHOC_CHANNEL_6,
    PSP_SYSTEMPARAM_ADHOC_CHANNEL_11
};

static const char* channel_value_name(int value)
{
    switch (value) {
        case PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC:
            return ui_text("Automatico", "Automatic");
        case PSP_SYSTEMPARAM_ADHOC_CHANNEL_1:
            return ui_text("Canal 1", "Channel 1");
        case PSP_SYSTEMPARAM_ADHOC_CHANNEL_6:
            return ui_text("Canal 6", "Channel 6");
        case PSP_SYSTEMPARAM_ADHOC_CHANNEL_11:
            return ui_text("Canal 11", "Channel 11");
        default:
            return ui_text("Desconhecido", "Unknown");
    }
}

static int channel_value_index(int value)
{
    int i;
    int count = (int)(sizeof(g_channel_values) / sizeof(g_channel_values[0]));

    for (i = 0; i < count; ++i)
        if (g_channel_values[i] == value)
            return i;

    return 0;
}

static int apply_adhoc_channel(int value)
{
    int ret;
    int was_active =
        g_net_initialized || g_adhoc_initialized ||
        g_adhocctl_initialized || g_adhoc_connected ||
        g_pdp_id >= 0;

    if (g_transfer_busy || g_outgoing_waiting || g_has_pending_offer) {
        show_message(ui_text("Canal", "Channel"),
                     ui_text("Nao e possivel mudar agora", "Cannot change it now"),
                     ui_text("Finalize ou recuse a transferencia atual.",
                             "Finish or reject the current transfer."),
                     "", C_WARNING);
        return 0;
    }

    if (value != PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC &&
        value != PSP_SYSTEMPARAM_ADHOC_CHANNEL_1 &&
        value != PSP_SYSTEMPARAM_ADHOC_CHANNEL_6 &&
        value != PSP_SYSTEMPARAM_ADHOC_CHANNEL_11) {
        show_message(ui_text("Canal", "Channel"),
                     ui_text("Canal invalido", "Invalid channel"),
                     ui_text("Use Automatico, 1, 6 ou 11.",
                             "Use Automatic, 1, 6 or 11."),
                     "", C_ERROR);
        return 0;
    }

    if (was_active) {
        set_status("Reiniciando Ad Hoc para trocar canal...",
                   "Restarting Ad Hoc to change channel...");
        stop_adhoc();
        sceKernelDelayThread(150000);
    }

    ret = sceUtilitySetSystemParamInt(
        PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL,
        value
    );

    if (ret < 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "sceUtilitySetSystemParamInt: 0x%08X",
                 (unsigned int)ret);
        collect_device_info();
        show_message(ui_text("Canal", "Channel"),
                     ui_text("Falha ao salvar canal", "Failed to save channel"),
                     msg, "", C_ERROR);

        if (was_active) {
            if (start_adhoc())
                send_hello();
        }
        return 0;
    }

    collect_device_info();

    if (was_active) {
        draw_loading("Ad Hoc", ui_text("Aplicando novo canal...", "Applying new channel..."));
        if (!start_adhoc()) {
            show_message(ui_text("Canal", "Channel"),
                         ui_text("Canal salvo, mas a rede falhou",
                                 "Channel saved, but network failed"),
                         ui_text("Reinicie a conexao Ad Hoc.",
                                 "Restart the Ad Hoc connection."),
                         ui_text("Verifique se a chave WLAN esta ligada.",
                                 "Make sure the WLAN switch is on."), C_WARNING);
            return 0;
        }
        send_hello();
    }

    snprintf(g_status_line, sizeof(g_status_line),
             "%s: %s",
             ui_text("Canal Ad Hoc", "Ad Hoc channel"),
             channel_value_name(value));
    return 1;
}

int channel_selector_screen(void)
{
    int current = PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC;
    int selected;
    int count =
        (int)(sizeof(g_channel_values) / sizeof(g_channel_values[0]));

    if (sceUtilityGetSystemParamInt(
            PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL,
            &current) < 0) {
        current = PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC;
    }

    selected = channel_value_index(current);

    while (!osl_quit) {
        int i;

        oslStartDrawing();
        draw_background();

        draw_header(
            ui_text("Canal Ad Hoc", "Ad Hoc Channel"),
            "ADShare"
        );

        draw_panel(52, 48, 428, 218, C_PSN_BLUE);

        oslSetTextColor(C_TEXT);
        oslDrawString(
            center_x(
                ui_text(
                    "Selecione o canal dos dois PSPs",
                    "Select the channel for both PSPs"
                )
            ),
            64,
            ui_text(
                "Selecione o canal dos dois PSPs",
                "Select the channel for both PSPs"
            )
        );

        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(
            center_x(
                ui_text(
                    "Ambos precisam usar o mesmo canal.",
                    "Both consoles must use the same channel."
                )
            ),
            84,
            ui_text(
                "Ambos precisam usar o mesmo canal.",
                "Both consoles must use the same channel."
            )
        );

        for (i = 0; i < count; ++i) {
            int y = 111 + i * 25;
            int selected_row = (i == selected);

            if (selected_row) {
                oslDrawFillRect(
                    75, y - 4, 405, y + 18,
                    C_HIGHLIGHT
                );
                oslDrawFillRect(
                    75, y - 4, 79, y + 18,
                    C_PSN_BLUE
                );
            }

            oslSetTextColor(
                selected_row ? C_TEXT : C_TEXT_DIM
            );

            oslDrawString(
                98,
                y,
                channel_value_name(g_channel_values[i])
            );

            if (g_channel_values[i] == current) {
                oslSetTextColor(C_SUCCESS);
                oslDrawString(
                    305,
                    y,
                    ui_text("Atual", "Current")
                );
            }
        }

        {
            const ButtonHint hints[] = {
                { PspButtonIcon::Cross,  ui_text("Aplicar", "Apply") },
                { PspButtonIcon::Square, ui_text("English", "PT-BR") },
                { PspButtonIcon::Circle, ui_text("Cancelar", "Cancel") }
            };
            draw_button_hints_centered(hints, 3, 255, 12);
        }

        oslEndDrawing();
        oslSyncFrame();
        oslReadKeys();

        if (osl_keys->pressed.down) {
            selected++;
            if (selected >= count)
                selected = 0;
        }

        if (osl_keys->pressed.up) {
            selected--;
            if (selected < 0)
                selected = count - 1;
        }

        if (osl_keys->pressed.square) {
            toggle_ui_language();
            collect_device_info();
        }

        if (osl_keys->pressed.circle)
            return 0;

        if (osl_keys->pressed.cross) {
            int value = g_channel_values[selected];

            if (value == current) {
                collect_device_info();
                return 1;
            }

            if (apply_adhoc_channel(value))
                return 1;

            if (sceUtilityGetSystemParamInt(
                    PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL,
                    &current) < 0) {
                current =
                    PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC;
            }

            selected = channel_value_index(current);
        }
    }

    return 0;
}

int peer_index_from_visible(int visible_index)
{
    int i, n = 0;
    for (i = 0; i < MAX_PEERS; ++i) {
        if (g_peers[i].used) {
            if (n == visible_index)
                return i;
            n++;
        }
    }
    return -1;
}

} // namespace adshare
