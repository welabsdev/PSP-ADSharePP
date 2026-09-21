#pragma once

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspnet.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <psputility_sysparam.h>
#include <pspwlan.h>
#include <pspsysmem.h>
#include <psppower.h>
#include <psprtc.h>
#include <kubridge.h>

/*
 * libpspexploit is a C library. Its installed header does not provide
 * C++ linkage guards on every PSPDEV/CFW-SDK version, so when included
 * directly from C++ the compiler may generate mangled references such as
 * _Z26pspXploitInitKernelExploitv while libpspexploit.a exports the plain
 * C symbol pspXploitInitKernelExploit.
 */
extern "C" {
#include <libpspexploit.h>
}

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <oslib/oslib.h>

namespace adshare {

// -------------------------------------------------------------------------
// Application / protocol configuration
// -------------------------------------------------------------------------
inline constexpr char APP_NAME[] = "ADShare++";
inline constexpr char APP_VERSION[] = "2.3.3";
inline constexpr char APP_AUTHOR[] = "welabsdev";

inline constexpr char ADHOC_GROUP[] = "ADSHARE";
inline constexpr char ADHOC_PRODUCT[] = "ADSHARE01";
static_assert(sizeof(ADHOC_PRODUCT) == 10,
              "ADHOC_PRODUCT must contain exactly 9 characters plus NUL");

inline constexpr int CTRL_PORT = 31000;
inline constexpr int DATA_SERVER_PORT = 31001;
inline constexpr int DATA_CLIENT_PORT = 31002;

inline constexpr int PDP_BUFFER_SIZE = 0x4000;
inline constexpr int DATA_PDP_BUFFER_SIZE = 0x8000;
inline constexpr int DATA_CHUNK_SIZE = 1024;
inline constexpr int DATA_RETRY_COUNT = 16;
inline constexpr unsigned int DATA_ACK_TIMEOUT_US = 1500000u;
inline constexpr unsigned int DATA_RECV_TIMEOUT_US = 8000000u;
inline constexpr int MAX_PEERS = 8;
inline constexpr int MAX_FILES = 128;
inline constexpr unsigned int PEER_TIMEOUT_US = 12000000u;
inline constexpr unsigned int HELLO_INTERVAL_US = 1000000u;
inline constexpr unsigned int OFFER_TIMEOUT_US = 300000000u;

inline constexpr char PROTOCOL_MAGIC[] = "ADSH";
inline constexpr uint8_t PROTOCOL_VERSION = 3;
inline constexpr char DATA_MAGIC[] = "ADD2";
inline constexpr uint8_t DATA_VERSION = 2;

// -------------------------------------------------------------------------
// UI colors
// Keep RGBA as macros because OSLib implementations differ between packages.
// This preserves the same semantics as the working C build.
// -------------------------------------------------------------------------
#define C_BG            RGBA(12, 12, 14, 255)
#define C_BAR_BG        RGBA(20, 20, 24, 255)
#define C_PANEL         RGBA(25, 25, 30, 255)
#define C_PANEL_2       RGBA(31, 31, 37, 255)
#define C_PSN_BLUE      RGBA(0, 112, 204, 255)
#define C_PSN_BLUE_SOFT RGBA(0, 112, 204, 80)
#define C_TEXT          RGBA(240, 240, 240, 255)
#define C_TEXT_DIM      RGBA(145, 145, 155, 255)
#define C_TEXT_DARK     RGBA(98, 98, 108, 255)
#define C_HIGHLIGHT     RGBA(255, 255, 255, 24)
#define C_HIGH_BORDER   RGBA(0, 112, 204, 210)
#define C_SUCCESS       RGBA(65, 205, 105, 255)
#define C_ERROR         RGBA(225, 75, 75, 255)
#define C_WARNING       RGBA(235, 180, 55, 255)
#define C_FOLDER        RGBA(220, 180, 50, 255)
#define C_FOLDER_DARK   RGBA(190, 145, 32, 255)
#define C_SCROLL_BG     RGBA(34, 34, 40, 255)
#define C_SCROLL_BAR    RGBA(150, 150, 160, 255)

/* PSP face-button colors used by the graphical control hints. */
#define C_BTN_CROSS      RGBA(90, 180, 255, 255)
#define C_BTN_CIRCLE     RGBA(255, 105, 120, 255)
#define C_BTN_SQUARE     RGBA(255, 125, 205, 255)
#define C_BTN_TRIANGLE   RGBA(105, 225, 145, 255)
#define C_BTN_NEUTRAL    RGBA(190, 190, 200, 255)

// -------------------------------------------------------------------------
// Wire protocol structures. These stay POD/packed intentionally because they
// are sent directly through the PSP Ad Hoc PDP transport.
// -------------------------------------------------------------------------
enum PacketType {
    PKT_HELLO   = 1,
    PKT_OFFER   = 2,
    PKT_ACCEPT  = 3,
    PKT_REJECT  = 4,
    PKT_CANCEL  = 5,
    PKT_DONE    = 6,
    PKT_PREPARE = 7
};

struct __attribute__((packed)) AdPacket {
    char magic[4];
    uint8_t version;
    uint8_t type;
    uint16_t reserved;
    uint32_t token;
    unsigned char sender_mac[6];
    char nickname[32];
    char device[32];
    char firmware[16];
    uint64_t file_size;
    char file_name[128];
    char file_type[16];
};

enum DataPacketType {
    DATA_PKT_HEADER     = 1,
    DATA_PKT_HEADER_ACK = 2,
    DATA_PKT_CHUNK      = 3,
    DATA_PKT_ACK        = 4,
    DATA_PKT_FIN        = 5,
    DATA_PKT_FIN_ACK    = 6,
    DATA_PKT_CANCEL     = 7
};

struct __attribute__((packed)) DataPacket {
    char magic[4];
    uint8_t version;
    uint8_t type;
    uint16_t payload_len;
    uint32_t token;
    uint32_t seq;
    uint32_t crc32;
    uint64_t file_size;
    char file_name[128];
    unsigned char payload[DATA_CHUNK_SIZE];
};

inline constexpr int DATA_PACKET_BASE_SIZE =
    static_cast<int>(offsetof(DataPacket, payload));

struct PeerInfo {
    int used;
    unsigned char mac[6];
    char nickname[32];
    char device[32];
    char firmware[16];
    uint32_t last_seen;
};

struct FileEntry {
    char name[256];
    int is_dir;
    SceOff size;
};

struct DeviceInfo {
    char nickname[128];
    char firmware[24];
    char model[64];
    char generation[16];
    char board_family[96];
    char region[64];
    char language[32];
    char mac[32];
    char adhoc_channel[32];
    char kernel_source[64];

    char region_id_text[32];
    char tachyon_text[32];
    char baryon_text[32];
    char pommel_text[32];
    char probe_status_text[64];

    int model_generation;
    int kernel_model_result;

    int region_valid;
    uint8_t region_id;

    int tachyon_valid;
    int baryon_valid;
    int pommel_valid;
    uint32_t tachyon;
    uint32_t baryon;
    uint32_t pommel;
};

enum class UiLanguage : uint8_t {
    PtBr = 0,
    English = 1
};

// -------------------------------------------------------------------------
// Graphical PSP button hints
// -------------------------------------------------------------------------
enum class PspButtonIcon : uint8_t {
    Cross,
    Circle,
    Square,
    Triangle,
    L,
    R,
    LR,
    Start,
    Select
};

struct ButtonHint {
    PspButtonIcon button;
    const char* label;
};

// -------------------------------------------------------------------------
// Shared runtime state
// -------------------------------------------------------------------------
extern OSL_FONT* g_font;
extern int g_net_initialized;
extern int g_adhoc_initialized;
extern int g_adhocctl_initialized;
extern int g_adhoc_connected;
extern int g_pdp_id;
extern unsigned char g_local_mac[6];
extern char g_local_nickname[128];
extern char g_connected_group[16];
extern int g_connected_channel;
extern DeviceInfo g_device;
extern PeerInfo g_peers[MAX_PEERS];
extern int g_peer_count;
extern int g_selected_peer;
extern int g_has_pending_offer;
extern AdPacket g_pending_offer;
extern unsigned char g_pending_offer_mac[6];
extern uint32_t g_outgoing_token;
extern int g_outgoing_response;
extern unsigned char g_outgoing_peer_mac[6];
extern int g_outgoing_waiting;
extern int g_transfer_busy;
extern uint32_t g_last_hello;
extern char g_status_line[128];
extern UiLanguage g_ui_language;

// state.cpp
const char* ui_text(const char* ptbr, const char* en);
void init_ui_language();
void toggle_ui_language();
void setup_callbacks();

// utils.cpp
uint32_t now_us();
int elapsed_us(uint32_t start, uint32_t interval);
void safe_copy(char* dst, const char* src, int dst_size);
void set_status(const char* ptbr, const char* en);
void set_status_error(const char* ptbr, const char* en, int error);
int safe_append(char* dst, const char* src, int dst_size);
int mac_equal(const unsigned char* a, const unsigned char* b);
void mac_to_string(const unsigned char* mac, char* out, int out_size);
void format_size(uint64_t size, char* out, int out_size);
const char* extension_of(const char* name);
int ext_equals(const char* name, const char* ext);
const char* file_category(const char* name);
const char* file_category_display(const char* category);
const char* base_name(const char* path);
void sanitize_filename(const char* input, char* output, int output_size);
int path_exists(const char* path);
int directory_exists(const char* path);
void join_path(const char* dir, const char* name, char* out, int out_size);
void make_unique_path(const char* dir, const char* filename, char* out, int out_size);

// hardware.cpp
void collect_device_info();

// ui.cpp
int text_width(const char* text);
int center_x(const char* text);
void fit_text(const char* src, char* dst, int dst_size, int max_width);
void draw_background();
void draw_header(const char* title, const char* right);
void draw_footer(const char* text);
int button_icon_width(PspButtonIcon button);
void draw_button_icon(PspButtonIcon button, int x, int y);
int button_hint_width(PspButtonIcon button, const char* label);
void draw_button_hint(PspButtonIcon button, const char* label,
                      int x, int y, unsigned int text_color = C_TEXT_DIM);
void draw_button_hints_centered(const ButtonHint* hints, int count,
                                int y, int gap = 9,
                                unsigned int text_color = C_TEXT_DIM);
void draw_panel(int x1, int y1, int x2, int y2, unsigned int border);
void draw_psp_icon(int x, int y, unsigned int color);
void draw_file_icon(int x, int y, const char* category, int selected);
void draw_folder_icon(int x, int y, int selected);
void draw_loading(const char* title, const char* message);
void show_message(const char* right, const char* title,
                  const char* line1, const char* line2,
                  unsigned int border);

// adhoc.cpp
int start_adhoc();
void stop_adhoc();
void fill_common_packet(AdPacket* p, int type);
int send_packet_to(const unsigned char* mac, AdPacket* p);
void send_hello();
void send_simple_response(const unsigned char* mac, int type, uint32_t token);
void poll_control_messages();
int channel_selector_screen();
int peer_index_from_visible(int visible_index);

// data_protocol.cpp
uint32_t data_crc32(const unsigned char* data, int length);
void data_packet_init(DataPacket* packet, int type, uint32_t token, uint32_t seq);
int data_packet_valid(const DataPacket* packet, int wire_len);
int data_send_packet(int pdp, const unsigned char* dest_mac,
                     unsigned short dest_port, const DataPacket* packet,
                     int* error_out);
int data_recv_packet(int pdp, const unsigned char* expected_mac,
                     uint32_t token, DataPacket* out,
                     unsigned short* source_port, unsigned int timeout_us,
                     int* error_out);
int data_send_ack(int pdp, const unsigned char* dest_mac,
                  unsigned short dest_port, int type,
                  uint32_t token, uint32_t seq);
int data_send_with_ack(int pdp, const unsigned char* dest_mac,
                       unsigned short dest_port, DataPacket* packet,
                       int ack_type, int* last_error);

// transfer.cpp
int receive_file_offer(const AdPacket* offer,
                       const unsigned char* sender_mac,
                       const char* destination_dir);
int send_offer_and_wait(const PeerInfo* peer, const char* path);

// browser.cpp
int destination_folder_browser(char* selected_dir, int selected_size);
int file_browser(char* selected_path, int selected_size);

// screens.cpp
void device_info_screen();
void draw_incoming_offer();
void draw_connection_panel(int selected_peer_index);
void draw_main_screen();
int start_screen();

} // namespace adshare
