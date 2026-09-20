#include "adshare.hpp"

namespace adshare {

OSL_FONT* g_font = NULL;

/* Etapas separadas para permitir cleanup seguro em falhas parciais. */
int g_net_initialized = 0;
int g_adhoc_initialized = 0;
int g_adhocctl_initialized = 0;
int g_adhoc_connected = 0;
int g_pdp_id = -1;
unsigned char g_local_mac[6] = {0};
char g_local_nickname[128] = "PSP";
char g_connected_group[16] = "ADSHARE";
int g_connected_channel = 0;
DeviceInfo g_device;

PeerInfo g_peers[MAX_PEERS];
int g_peer_count = 0;
int g_selected_peer = 0;

int g_has_pending_offer = 0;
AdPacket g_pending_offer;
unsigned char g_pending_offer_mac[6];

uint32_t g_outgoing_token = 0;
int g_outgoing_response = 0; /* 0=aguarda, 1=aceito, 2=escolhendo destino, -1=recusado */
unsigned char g_outgoing_peer_mac[6];
int g_outgoing_waiting = 0;
int g_transfer_busy = 0;

uint32_t g_last_hello = 0;
char g_status_line[128] = "Pronto";

UiLanguage g_ui_language = UiLanguage::PtBr;

const char* ui_text(const char* ptbr, const char* en)
{
    return g_ui_language == UiLanguage::English ? en : ptbr;
}

void init_ui_language()
{
    int lang = PSP_SYSTEMPARAM_LANGUAGE_PORTUGUESE;

    if (sceUtilityGetSystemParamInt(
            PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &lang) < 0) {
        g_ui_language = UiLanguage::PtBr;
        return;
    }

    g_ui_language =
        (lang == PSP_SYSTEMPARAM_LANGUAGE_PORTUGUESE)
        ? UiLanguage::PtBr
        : UiLanguage::English;
}

void toggle_ui_language()
{
    g_ui_language =
        (g_ui_language == UiLanguage::PtBr)
        ? UiLanguage::English
        : UiLanguage::PtBr;
}

static int exit_callback(int arg1, int arg2, void* common)
{
    (void)arg1;
    (void)arg2;
    (void)common;
    oslQuit();
    return 0;
}

static int callback_thread(SceSize args, void* argp)
{
    (void)args;
    (void)argp;
    const int cbid = sceKernelCreateCallback("ADShare Exit", exit_callback, nullptr);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

void setup_callbacks()
{
    const int thid = sceKernelCreateThread(
        "ADShareCallbacks",
        callback_thread,
        0x11,
        0xFA0,
        PSP_THREAD_ATTR_USER,
        nullptr
    );

    if (thid >= 0)
        sceKernelStartThread(thid, 0, nullptr);
}

} // namespace adshare
