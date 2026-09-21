#include "adshare.hpp"

namespace adshare {

namespace {

class TransferSessionGuard final {
public:
    TransferSessionGuard() noexcept
    {
        g_transfer_busy = 1;
        scePowerSetClockFrequency(333, 333, 166);
    }

    ~TransferSessionGuard() noexcept
    {
        scePowerSetClockFrequency(222, 222, 111);
        g_transfer_busy = 0;
    }

    TransferSessionGuard(const TransferSessionGuard&) = delete;
    TransferSessionGuard& operator=(const TransferSessionGuard&) = delete;
};

} // namespace

/* =========================================================
   TELA DE TRANSFERENCIA
   ========================================================= */

static void draw_transfer_screen(
    const char* mode,
    const char* peer_name,
    const char* filename,
    uint64_t done,
    uint64_t total,
    uint32_t start_time)
{
    char size_text[96];
    char pct_text[32];
    char speed_text[64];
    char short_file[160];
    int pct = total ? (int)((done * 100ULL) / total) : 0;
    int bar = (pct * 360) / 100;
    uint32_t elapsed = now_us() - start_time;
    double seconds = elapsed / 1000000.0;
    double speed = seconds > 0.05 ? (done / 1048576.0) / seconds : 0.0;

    if (pct > 100)
        pct = 100;

    fit_text(filename, short_file, sizeof(short_file), 340);

    snprintf(
        size_text,
        sizeof(size_text),
        "%.2f MB / %.2f MB",
        done / 1048576.0,
        total / 1048576.0
    );

    snprintf(pct_text, sizeof(pct_text), "%d%%", pct);

    snprintf(
        speed_text,
        sizeof(speed_text),
        "%s %.2f MB/s",
        ui_text("Velocidade:", "Speed:"),
        speed
    );

    oslStartDrawing();
    draw_background();
    draw_header(APP_NAME, mode);

    draw_panel(24, 42, 456, 216, C_PSN_BLUE);
    draw_file_icon(45, 61, file_category(filename), 1);

    oslSetTextColor(C_TEXT);
    oslDrawString(88, 62, short_file);

    oslSetTextColor(C_TEXT_DIM);
    oslDrawString(88, 82, peer_name);

    oslDrawFillRect(58, 128, 418, 139, C_SCROLL_BG);
    oslDrawRect(58, 128, 418, 139, RGBA(80,80,90,255));

    if (bar > 0)
        oslDrawFillRect(58, 128, 58 + bar, 139, C_PSN_BLUE);

    oslSetTextColor(C_TEXT_DIM);
    oslDrawString(58, 103, size_text);
    oslDrawString(58, 154, speed_text);

    oslSetTextColor(C_TEXT);
    oslDrawString(390, 103, pct_text);

    {
        const ButtonHint hints[] = {
            {
                PspButtonIcon::Circle,
                ui_text("Cancelar transferencia", "Cancel transfer")
            }
        };
        draw_button_hints_centered(hints, 1, 190, 0, C_TEXT_DARK);
    }

    oslEndDrawing();
    oslSyncFrame();
}

/* =========================================================
   TRANSFERENCIA - ENVIO PDP
   ========================================================= */

static int send_file_data(
    const unsigned char* dest_mac,
    const char* peer_name,
    const char* path,
    uint32_t token)
{
    SceIoStat st;
    SceUID fd = -1;
    int data_pdp = -1;
    int success = 0;
    int cancelled = 0;
    int last_error = 0;
    DataPacket packet;
    uint64_t total = 0;
    uint64_t sent = 0;
    uint32_t seq = 0;
    uint32_t started = 0;

    TransferSessionGuard transfer_session;

    memset(&st, 0, sizeof(st));

    if (sceIoGetstat(path, &st) < 0) {
        set_status("Nao foi possivel ler o arquivo",
                   "Could not read the file");
        goto cleanup;
    }

    total = (uint64_t)st.st_size;

    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) {
        set_status("Nao foi possivel abrir o arquivo",
                   "Could not open the file");
        goto cleanup;
    }

    draw_loading(
        ui_text("Envio", "Sending"),
        ui_text("Preparando canal de dados...", "Preparing data channel...")
    );

    data_pdp = sceNetAdhocPdpCreate(
        g_local_mac,
        DATA_CLIENT_PORT,
        DATA_PDP_BUFFER_SIZE,
        0
    );

    if (data_pdp < 0) {
        set_status_error(
            "Falha ao abrir canal de dados:",
            "Failed to open data channel:",
            data_pdp
        );
        goto cleanup;
    }

    /*
     * Pequena janela para o firmware registrar o novo PDP antes do primeiro
     * datagrama. O receptor ja abriu DATA_SERVER_PORT antes de enviar ACCEPT.
     */
    sceKernelDelayThread(120000);

    /*
     * HEADER confiavel.
     */
    data_packet_init(&packet, DATA_PKT_HEADER, token, 0);
    packet.file_size = total;
    sanitize_filename(
        base_name(path),
        packet.file_name,
        sizeof(packet.file_name)
    );

    {
        int result = data_send_with_ack(
            data_pdp,
            dest_mac,
            DATA_SERVER_PORT,
            &packet,
            DATA_PKT_HEADER_ACK,
            &last_error
        );

        if (result < 0) {
            set_status("O receptor cancelou a transferencia",
                       "Receiver cancelled the transfer");
            cancelled = 1;
            goto cleanup;
        }

        if (result == 0) {
            set_status_error(
                "Falha no handshake de dados:",
                "Data handshake failed:",
                last_error
            );
            goto cleanup;
        }
    }

    started = now_us();

    while (sent < total && !osl_quit) {
        int want;
        int read_bytes;
        int result;

        oslReadKeys();

        if (osl_keys->pressed.circle) {
            DataPacket cancel_packet;

            data_packet_init(
                &cancel_packet,
                DATA_PKT_CANCEL,
                token,
                seq
            );

            data_send_packet(
                data_pdp,
                dest_mac,
                DATA_SERVER_PORT,
                &cancel_packet,
                &last_error
            );

            cancelled = 1;
            set_status("Envio cancelado", "Send cancelled");
            break;
        }

        want = DATA_CHUNK_SIZE;

        if (total - sent < (uint64_t)want)
            want = (int)(total - sent);

        data_packet_init(
            &packet,
            DATA_PKT_CHUNK,
            token,
            seq
        );

        read_bytes = sceIoRead(fd, packet.payload, want);

        if (read_bytes <= 0) {
            set_status("Falha lendo o arquivo local",
                       "Failed to read local file");
            break;
        }

        packet.payload_len = (uint16_t)read_bytes;
        packet.crc32 = data_crc32(packet.payload, read_bytes);

        result = data_send_with_ack(
            data_pdp,
            dest_mac,
            DATA_SERVER_PORT,
            &packet,
            DATA_PKT_ACK,
            &last_error
        );

        if (result < 0) {
            cancelled = 1;
            set_status("O receptor cancelou a transferencia",
                       "Receiver cancelled the transfer");
            break;
        }

        if (result == 0) {
            set_status_error(
                "Conexao de dados interrompida:",
                "Data connection interrupted:",
                last_error
            );
            break;
        }

        sent += (uint64_t)read_bytes;
        seq++;

        draw_transfer_screen(
            ui_text("Enviando", "Sending"),
            peer_name,
            base_name(path),
            sent,
            total,
            started
        );
    }

    if (!cancelled && sent == total) {
        int result;

        data_packet_init(
            &packet,
            DATA_PKT_FIN,
            token,
            seq
        );

        result = data_send_with_ack(
            data_pdp,
            dest_mac,
            DATA_SERVER_PORT,
            &packet,
            DATA_PKT_FIN_ACK,
            &last_error
        );

        if (result == 1) {
            success = 1;
            set_status("Arquivo enviado com sucesso",
                       "File sent successfully");
        } else if (result < 0) {
            set_status("O receptor cancelou ao finalizar",
                       "Receiver cancelled while finishing");
        } else {
            set_status_error(
                "Arquivo enviado, mas sem confirmacao final:",
                "File sent, but final confirmation failed:",
                last_error
            );
        }
    }

cleanup:
    if (fd >= 0)
        sceIoClose(fd);

    if (data_pdp >= 0)
        sceNetAdhocPdpDelete(data_pdp, 0);

    if (success)
        send_simple_response(dest_mac, PKT_DONE, token);
    else
        send_simple_response(dest_mac, PKT_CANCEL, token);

    return success;
}

/* =========================================================
   TRANSFERENCIA - RECEBIMENTO PDP
   ========================================================= */

int receive_file_offer(
    const AdPacket* offer,
    const unsigned char* sender_mac,
    const char* destination_dir)
{
    int data_pdp = -1;
    int last_error = 0;
    int timeout_count = 0;
    int accepted = 0;
    int success = 0;
    int cancelled = 0;
    uint32_t expected_seq = 0;
    uint64_t received = 0;
    uint32_t started = 0;
    unsigned short sender_data_port = DATA_CLIENT_PORT;
    DataPacket packet;
    char final_path[512] = {0};
    char part_path[544] = {0};
    SceUID fd = -1;

    if (!offer || !sender_mac || !destination_dir || !destination_dir[0])
        return 0;

    TransferSessionGuard transfer_session;

    draw_loading(
        ui_text("Receber", "Receive"),
        ui_text("Preparando canal de dados...", "Preparing data channel...")
    );

    data_pdp = sceNetAdhocPdpCreate(
        g_local_mac,
        DATA_SERVER_PORT,
        DATA_PDP_BUFFER_SIZE,
        0
    );

    if (data_pdp < 0) {
        send_simple_response(
            sender_mac,
            PKT_REJECT,
            offer->token
        );

        set_status_error(
            "Nao foi possivel abrir o receptor de dados:",
            "Could not open data receiver:",
            data_pdp
        );

        goto cleanup;
    }

    /*
     * Deixa o PDP de recepcao totalmente registrado antes de anunciar ACCEPT.
     */
    sceKernelDelayThread(120000);

    /*
     * O ACCEPT so e enviado depois que DATA_SERVER_PORT esta pronto.
     * Isso elimina a corrida que existia entre o aceite na UI e a criacao
     * do canal de transferencia.
     */
    send_simple_response(
        sender_mac,
        PKT_ACCEPT,
        offer->token
    );

    accepted = 1;

    draw_loading(
        ui_text("Receber", "Receive"),
        ui_text("Aguardando o remetente...", "Waiting for sender...")
    );

    /*
     * HEADER.
     */
    for (;;) {
        int rr = data_recv_packet(
            data_pdp,
            sender_mac,
            offer->token,
            &packet,
            &sender_data_port,
            12000000,
            &last_error
        );

        if (rr == 0) {
            set_status_error(
                "Timeout aguardando dados:",
                "Timed out waiting for data:",
                last_error
            );
            goto cleanup;
        }

        if (rr < 0)
            continue;

        if (packet.type == DATA_PKT_CANCEL) {
            cancelled = 1;
            set_status("O remetente cancelou a transferencia",
                       "Sender cancelled the transfer");
            goto cleanup;
        }

        if (packet.type != DATA_PKT_HEADER)
            continue;

        if (packet.file_size != offer->file_size ||
            strcmp(packet.file_name, offer->file_name) != 0) {
            set_status("Cabecalho de transferencia invalido",
                       "Invalid transfer header");
            goto cleanup;
        }

        data_send_ack(
            data_pdp,
            sender_mac,
            sender_data_port,
            DATA_PKT_HEADER_ACK,
            offer->token,
            packet.seq
        );

        break;
    }

    /*
     * O destino e escolhido manualmente pelo usuario ANTES do ACCEPT.
     * Nenhuma subpasta "ADShare" e criada automaticamente.
     * O nome original e preservado e, se ja existir, make_unique_path()
     * adiciona um sufixo para nao sobrescrever o arquivo existente.
     */
    make_unique_path(
        destination_dir,
        packet.file_name,
        final_path,
        sizeof(final_path)
    );

    if (!final_path[0]) {
        set_status("Nao foi possivel montar o caminho de destino",
                   "Could not build destination path");
        goto cleanup;
    }

    safe_copy(
        part_path,
        final_path,
        sizeof(part_path)
    );

    if (!safe_append(
            part_path,
            ".part",
            sizeof(part_path))) {
        set_status("Caminho de destino muito longo",
                   "Destination path is too long");
        goto cleanup;
    }

    fd = sceIoOpen(
        part_path,
        PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC,
        0777
    );

    if (fd < 0) {
        set_status("Nao foi possivel criar o arquivo",
                   "Could not create the file");
        goto cleanup;
    }

    started = now_us();

    while (!osl_quit) {
        int rr;

        oslReadKeys();

        if (osl_keys->pressed.circle) {
            DataPacket cancel_packet;

            data_packet_init(
                &cancel_packet,
                DATA_PKT_CANCEL,
                offer->token,
                expected_seq
            );

            data_send_packet(
                data_pdp,
                sender_mac,
                sender_data_port,
                &cancel_packet,
                &last_error
            );

            cancelled = 1;
            set_status("Recebimento cancelado",
                       "Receive cancelled");
            break;
        }

        rr = data_recv_packet(
            data_pdp,
            sender_mac,
            offer->token,
            &packet,
            &sender_data_port,
            DATA_RECV_TIMEOUT_US,
            &last_error
        );

        if (rr == 0) {
            timeout_count++;

            if (timeout_count >= 3) {
                set_status_error(
                    "Conexao de dados perdida:",
                    "Data connection lost:",
                    last_error
                );
                break;
            }

            continue;
        }

        if (rr < 0)
            continue;

        timeout_count = 0;

        if (packet.type == DATA_PKT_CANCEL) {
            cancelled = 1;
            set_status("O remetente cancelou a transferencia",
                       "Sender cancelled the transfer");
            break;
        }

        /*
         * Se o ACK do HEADER se perdeu, o remetente reenviara o HEADER.
         * Responde novamente sem reiniciar o arquivo.
         */
        if (packet.type == DATA_PKT_HEADER) {
            data_send_ack(
                data_pdp,
                sender_mac,
                sender_data_port,
                DATA_PKT_HEADER_ACK,
                offer->token,
                packet.seq
            );
            continue;
        }

        if (packet.type == DATA_PKT_CHUNK) {
            uint32_t crc;

            if (packet.seq < expected_seq) {
                /*
                 * Chunk duplicado: o ACK anterior provavelmente se perdeu.
                 */
                data_send_ack(
                    data_pdp,
                    sender_mac,
                    sender_data_port,
                    DATA_PKT_ACK,
                    offer->token,
                    packet.seq
                );
                continue;
            }

            if (packet.seq > expected_seq) {
                /*
                 * Stop-and-wait nao deveria gerar pacote futuro.
                 * Ignora para que o remetente reenvie a sequencia esperada.
                 */
                continue;
            }

            if (packet.payload_len == 0 ||
                packet.payload_len > DATA_CHUNK_SIZE) {
                continue;
            }

            crc = data_crc32(
                packet.payload,
                packet.payload_len
            );

            if (crc != packet.crc32) {
                /* Nao envia ACK; sender reenviara automaticamente. */
                continue;
            }

            if (received + packet.payload_len >
                offer->file_size) {
                set_status("Pacote excedeu o tamanho esperado",
                           "Packet exceeded expected file size");
                break;
            }

            if (sceIoWrite(
                    fd,
                    packet.payload,
                    packet.payload_len) != packet.payload_len) {
                set_status("Falha gravando no armazenamento",
                           "Failed writing to storage");
                break;
            }

            received += packet.payload_len;

            data_send_ack(
                data_pdp,
                sender_mac,
                sender_data_port,
                DATA_PKT_ACK,
                offer->token,
                packet.seq
            );

            expected_seq++;

            draw_transfer_screen(
                ui_text("Recebendo", "Receiving"),
                offer->nickname,
                offer->file_name,
                received,
                offer->file_size,
                started
            );

            continue;
        }

        if (packet.type == DATA_PKT_FIN) {
            if (received != offer->file_size) {
                set_status("Arquivo incompleto no FIN",
                           "File incomplete at FIN");
                break;
            }

            /*
             * Fecha e finaliza ANTES do FIN_ACK. Assim o sender so recebe
             * sucesso depois que o arquivo realmente existe no destino.
             */
            if (fd >= 0) {
                sceIoClose(fd);
                fd = -1;
            }

            sceIoRemove(final_path);

            if (sceIoRename(part_path, final_path) < 0) {
                set_status("Recebido, mas falhou ao finalizar o arquivo",
                           "Received, but failed to finalize the file");
                break;
            }

            /*
             * Repetimos o FIN_ACK algumas vezes. PDP e datagrama; caso um
             * ACK final se perca, o sender ainda recebe outro.
             */
            {
                int i;

                for (i = 0; i < 4; ++i) {
                    data_send_ack(
                        data_pdp,
                        sender_mac,
                        sender_data_port,
                        DATA_PKT_FIN_ACK,
                        offer->token,
                        packet.seq
                    );

                    sceKernelDelayThread(25000);
                }
            }

            success = 1;
            set_status("Arquivo recebido com sucesso",
                       "File received successfully");
            break;
        }
    }

cleanup:
    if (fd >= 0)
        sceIoClose(fd);

    if (data_pdp >= 0)
        sceNetAdhocPdpDelete(data_pdp, 0);

    if (!success && part_path[0])
        sceIoRemove(part_path);

    if (success) {
        char short_path[160];

        send_simple_response(
            sender_mac,
            PKT_DONE,
            offer->token
        );

        fit_text(
            final_path,
            short_path,
            sizeof(short_path),
            400
        );

        show_message(
            ui_text("Concluido", "Completed"),
            ui_text("Arquivo recebido", "File received"),
            short_path,
            ui_text("Salvo na pasta escolhida pelo usuario.",
                    "Saved in the folder selected by the user."),
            C_SUCCESS
        );
    } else if (accepted) {
        send_simple_response(
            sender_mac,
            PKT_CANCEL,
            offer->token
        );
    }

    (void)cancelled;
    return success;
}

/* =========================================================
   OFERTA DE ARQUIVO
   ========================================================= */

int send_offer_and_wait(
    const PeerInfo* peer,
    const char* path)
{
    SceIoStat st;
    AdPacket offer;
    uint32_t started;
    uint32_t last_send = 0;
    char file_size_text[64];
    char short_name[160];
    int result = 0;

    memset(&st, 0, sizeof(st));

    if (!peer ||
        !path ||
        sceIoGetstat(path, &st) < 0) {
        return 0;
    }

    fill_common_packet(&offer, PKT_OFFER);

    g_outgoing_token =
        now_us() ^
        ((uint32_t)g_local_mac[4] << 8) ^
        g_local_mac[5];

    offer.token = g_outgoing_token;
    offer.file_size = (uint64_t)st.st_size;

    sanitize_filename(
        base_name(path),
        offer.file_name,
        sizeof(offer.file_name)
    );

    safe_copy(
        offer.file_type,
        file_category(path),
        sizeof(offer.file_type)
    );

    memcpy(
        g_outgoing_peer_mac,
        peer->mac,
        6
    );

    g_outgoing_response = 0; /* 0=aguarda, 1=aceito, 2=escolhendo destino, -1=recusado */
    g_outgoing_waiting = 1;
    started = now_us();

    format_size(
        offer.file_size,
        file_size_text,
        sizeof(file_size_text)
    );

    fit_text(
        offer.file_name,
        short_name,
        sizeof(short_name),
        390
    );

    while (!osl_quit &&
           (uint32_t)(now_us() - started) <
               OFFER_TIMEOUT_US) {

        if (g_outgoing_response == 0 &&
            (last_send == 0 ||
             elapsed_us(last_send, 1200000u))) {
            send_packet_to(peer->mac, &offer);
            last_send = now_us();
        }

        poll_control_messages();

        if (g_outgoing_response == 1) {
            g_outgoing_waiting = 0;

            result = send_file_data(
                peer->mac,
                peer->nickname,
                path,
                g_outgoing_token
            );

            goto done;
        }

        if (g_outgoing_response < 0) {
            set_status(
                "O outro PSP recusou o arquivo",
                "The other PSP rejected the file"
            );
            goto done;
        }

        oslStartDrawing();
        draw_background();

        draw_header(
            APP_NAME,
            g_outgoing_response == 2
                ? ui_text("Escolhendo destino", "Choosing destination")
                : ui_text("Solicitacao", "Request")
        );

        draw_panel(
            28,
            59,
            452,
            205,
            C_PSN_BLUE
        );

        oslSetTextColor(C_TEXT);
        {
            const char* wait_message =
                g_outgoing_response == 2
                    ? ui_text(
                        "O outro PSP esta escolhendo onde salvar",
                        "The other PSP is choosing where to save"
                      )
                    : ui_text(
                        "Aguardando confirmacao do outro PSP",
                        "Waiting for confirmation from the other PSP"
                      );

            oslDrawString(
                center_x(wait_message),
                78,
                wait_message
            );
        }

        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(
            50,
            111,
            ui_text("Destino:", "To:")
        );

        oslSetTextColor(C_TEXT);
        oslDrawString(
            125,
            111,
            peer->nickname
        );

        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(
            50,
            136,
            ui_text("Arquivo:", "File:")
        );

        oslSetTextColor(C_TEXT);
        oslDrawString(
            125,
            136,
            short_name
        );

        oslSetTextColor(C_TEXT_DIM);
        oslDrawString(
            50,
            161,
            ui_text("Tamanho:", "Size:")
        );

        oslSetTextColor(C_TEXT);
        oslDrawString(
            125,
            161,
            file_size_text
        );

        {
            const ButtonHint hints[] = {
                {
                    PspButtonIcon::Circle,
                    ui_text("Cancelar solicitacao", "Cancel request")
                }
            };
            draw_button_hints_centered(hints, 1, 255);
        }

        oslEndDrawing();
        oslSyncFrame();
        oslReadKeys();

        if (osl_keys->pressed.circle) {
            send_simple_response(
                peer->mac,
                PKT_CANCEL,
                g_outgoing_token
            );

            set_status(
                "Solicitacao cancelada",
                "Request cancelled"
            );

            goto done;
        }

        sceKernelDelayThread(16000);
    }

    set_status(
        "Tempo esgotado aguardando resposta",
        "Timed out waiting for response"
    );

done:
    g_outgoing_waiting = 0;
    g_outgoing_response = 0;
    return result;
}

} // namespace adshare
