#include "adshare.hpp"

namespace adshare {

/* =========================================================
   TRANSFERENCIA DE DADOS - PDP CONFIAVEL
   ========================================================= */

/*
 * O PTP do PSP pode falhar em alguns firmwares/CFWs mesmo quando a
 * descoberta PDP funciona. A partir do ADShare 2.1, a transferencia usa
 * um segundo socket PDP e implementa confiabilidade no proprio protocolo:
 *
 *   HEADER -> HEADER_ACK
 *   CHUNK(seq, CRC32) -> ACK(seq)
 *   FIN -> FIN_ACK
 *
 * Cada pacote perdido e reenviado. ACK perdido tambem e recuperado, pois o
 * receptor reconhece sequencias duplicadas e envia o ACK novamente.
 *
 * O payload fica em 1024 bytes para permanecer pequeno e estavel no WLAN
 * Ad Hoc real do PSP.
 *
 * v2.1.1: no PSP real, retorno 0 de PdpSend/PdpRecv pode representar
 * sucesso; no receive o tamanho valido vem em *dataLength. Apenas valores
 * negativos sao erros de API.
 */

uint32_t data_crc32(const unsigned char* data, int length)
{
    uint32_t crc = 0xFFFFFFFFu;
    int i;

    for (i = 0; i < length; ++i) {
        int bit;
        crc ^= data[i];

        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }

    return ~crc;
}

void data_packet_init(
    DataPacket* packet,
    int type,
    uint32_t token,
    uint32_t seq)
{
    memset(packet, 0, sizeof(*packet));
    memcpy(packet->magic, DATA_MAGIC, 4);
    packet->version = DATA_VERSION;
    packet->type = (uint8_t)type;
    packet->token = token;
    packet->seq = seq;
}

int data_packet_valid(const DataPacket* packet, int wire_len)
{
    int expected;

    if (!packet)
        return 0;

    if (wire_len < DATA_PACKET_BASE_SIZE)
        return 0;

    if (memcmp(packet->magic, DATA_MAGIC, 4) != 0)
        return 0;

    if (packet->version != DATA_VERSION)
        return 0;

    if (packet->payload_len > DATA_CHUNK_SIZE)
        return 0;

    expected = DATA_PACKET_BASE_SIZE + packet->payload_len;
    if (wire_len < expected)
        return 0;

    return 1;
}

int data_send_packet(
    int pdp,
    const unsigned char* dest_mac,
    unsigned short dest_port,
    const DataPacket* packet,
    int* error_out)
{
    int wire_len;
    int ret;

    if (!packet || !dest_mac)
        return 0;

    wire_len = DATA_PACKET_BASE_SIZE + packet->payload_len;

    /*
     * IMPORTANTE:
     * Em hardware PSP/implementacoes compativeis, retorno 0 pode significar
     * sucesso. Apenas valores NEGATIVOS sao tratados como erro.
     *
     * Isso tambem deixa o codigo compativel com implementacoes que retornam
     * a quantidade de bytes enviados.
     */
    ret = sceNetAdhocPdpSend(
        pdp,
        (unsigned char*)dest_mac,
        dest_port,
        (void*)packet,
        wire_len,
        1000000,
        0
    );

    if (ret < 0) {
        if (error_out)
            *error_out = ret;
        return 0;
    }

    if (error_out)
        *error_out = 0;

    return 1;
}

/*
 * Recebe um pacote do socket de dados e filtra pelo MAC/token.
 *
 * Retornos:
 *   1  pacote valido
 *   0  timeout/erro
 *  -1  pacote de outro peer/token (caller pode tentar novamente)
 */
int data_recv_packet(
    int pdp,
    const unsigned char* expected_mac,
    uint32_t token,
    DataPacket* out,
    unsigned short* source_port,
    unsigned int timeout_us,
    int* error_out)
{
    unsigned char src_mac[6];
    unsigned short src_port = 0;
    int len = sizeof(*out);
    int ret;

    memset(out, 0, sizeof(*out));
    memset(src_mac, 0, sizeof(src_mac));

    /*
     * No PSP real, sceNetAdhocPdpRecv() usa o ponteiro "len" para informar
     * quantos bytes chegaram. Retorno 0 NAO deve ser interpretado como
     * timeout se len > 0.
     *
     * O proprio fluxo de controle do ADShare ja funcionava assim:
     *   ret < 0  -> erro/sem pacote
     *   ret >= 0 -> verificar len
     */
    ret = sceNetAdhocPdpRecv(
        pdp,
        src_mac,
        &src_port,
        out,
        &len,
        timeout_us,
        0
    );

    if (ret < 0) {
        if (error_out)
            *error_out = ret;
        return 0;
    }

    /*
     * Chamada sem erro mas sem payload. Nao e um codigo de erro 0x00000000.
     */
    if (len <= 0) {
        if (error_out)
            *error_out = 0;
        return 0;
    }

    if (len > (int)sizeof(*out)) {
        if (error_out)
            *error_out = 0;
        return -1;
    }

    if (!data_packet_valid(out, len)) {
        if (error_out)
            *error_out = 0;
        return -1;
    }

    if (expected_mac && !mac_equal(src_mac, expected_mac)) {
        if (error_out)
            *error_out = 0;
        return -1;
    }

    if (out->token != token) {
        if (error_out)
            *error_out = 0;
        return -1;
    }

    if (source_port)
        *source_port = src_port;

    if (error_out)
        *error_out = 0;

    return 1;
}

int data_send_ack(
    int pdp,
    const unsigned char* dest_mac,
    unsigned short dest_port,
    int type,
    uint32_t token,
    uint32_t seq)
{
    DataPacket ack;
    int error = 0;

    data_packet_init(&ack, type, token, seq);
    return data_send_packet(pdp, dest_mac, dest_port, &ack, &error);
}

/*
 * Envia o mesmo pacote ate receber o ACK esperado.
 *
 * Retornos:
 *   1  confirmado
 *   0  falhou depois dos retries
 *  -1  peer cancelou
 */
int data_send_with_ack(
    int pdp,
    const unsigned char* dest_mac,
    unsigned short dest_port,
    DataPacket* packet,
    int ack_type,
    int* last_error)
{
    int attempt;

    for (attempt = 0; attempt < DATA_RETRY_COUNT && !osl_quit; ++attempt) {
        DataPacket response;
        int err = 0;
        int recv_result;

        if (!data_send_packet(
                pdp, dest_mac, dest_port, packet, &err)) {
            if (last_error)
                *last_error = err;
            sceKernelDelayThread(50000);
            continue;
        }

        for (;;) {
            recv_result = data_recv_packet(
                pdp,
                dest_mac,
                packet->token,
                &response,
                NULL,
                DATA_ACK_TIMEOUT_US,
                &err
            );

            if (recv_result == 0) {
                if (last_error)
                    *last_error = err;
                break; /* timeout -> reenvia */
            }

            if (recv_result < 0)
                continue; /* pacote alheio/invalido */

            if (response.type == DATA_PKT_CANCEL)
                return -1;

            if (response.type == ack_type &&
                response.seq == packet->seq) {
                return 1;
            }

            /*
             * ACK antigo pode chegar atrasado. Ignora e continua aguardando
             * ate o timeout desta tentativa.
             */
        }
    }

    return 0;
}

} // namespace adshare
