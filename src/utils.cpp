#include "adshare.hpp"

namespace adshare {

/* =========================================================
   HELPERS GERAIS
   ========================================================= */

uint32_t now_us(void)
{
    return (uint32_t)sceKernelGetSystemTimeLow();
}

int elapsed_us(uint32_t start, uint32_t interval)
{
    return (uint32_t)(now_us() - start) >= interval;
}

void safe_copy(char* dst, const char* src, int dst_size)
{
    size_t len;

    if (!dst || dst_size <= 0)
        return;

    if (!src)
        src = "";

    len = strlen(src);
    if (len >= (size_t)dst_size)
        len = (size_t)dst_size - 1;

    if (len > 0)
        memcpy(dst, src, len);

    dst[len] = '\0';
}

/* Acrescenta texto sem ultrapassar o buffer. Retorna 1 se coube inteiro. */
void set_status(const char* ptbr, const char* en)
{
    safe_copy(g_status_line, ui_text(ptbr, en), sizeof(g_status_line));
}

void set_status_error(const char* ptbr, const char* en, int error)
{
    char text[128];

    if (error < 0) {
        snprintf(
            text,
            sizeof(text),
            "%s 0x%08X",
            ui_text(ptbr, en),
            (unsigned int)error
        );
    } else {
        snprintf(
            text,
            sizeof(text),
            "%s %s",
            ui_text(ptbr, en),
            ui_text("(sem resposta)", "(no response)")
        );
    }

    safe_copy(g_status_line, text, sizeof(g_status_line));
}

int safe_append(char* dst, const char* src, int dst_size)
{
    size_t used;
    size_t available;
    size_t len;
    int complete = 1;

    if (!dst || dst_size <= 0)
        return 0;

    if (!src)
        src = "";

    used = strlen(dst);
    if (used >= (size_t)dst_size) {
        dst[dst_size - 1] = '\0';
        return 0;
    }

    available = (size_t)dst_size - used - 1;
    len = strlen(src);

    if (len > available) {
        len = available;
        complete = 0;
    }

    if (len > 0)
        memcpy(dst + used, src, len);

    dst[used + len] = '\0';
    return complete;
}

int mac_equal(const unsigned char* a, const unsigned char* b)
{
    return memcmp(a, b, 6) == 0;
}

void mac_to_string(const unsigned char* mac, char* out, int out_size)
{
    snprintf(out, out_size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void format_size(uint64_t size, char* out, int out_size)
{
    if (size >= 1073741824ULL)
        snprintf(out, out_size, "%.2f GB", (double)size / 1073741824.0);
    else if (size >= 1048576ULL)
        snprintf(out, out_size, "%.2f MB", (double)size / 1048576.0);
    else if (size >= 1024ULL)
        snprintf(out, out_size, "%.1f KB", (double)size / 1024.0);
    else
        snprintf(out, out_size, "%llu B", (unsigned long long)size);
}

const char* extension_of(const char* name)
{
    const char* dot = strrchr(name ? name : "", '.');
    return dot ? dot : "";
}

int ext_equals(const char* name, const char* ext)
{
    const char* p = extension_of(name);
    while (*p && *ext) {
        if (tolower((unsigned char)*p) != tolower((unsigned char)*ext))
            return 0;
        p++;
        ext++;
    }
    return *p == '\0' && *ext == '\0';
}

const char* file_category(const char* name)
{
    if (ext_equals(name, ".jpg") || ext_equals(name, ".jpeg") ||
        ext_equals(name, ".png") || ext_equals(name, ".bmp") ||
        ext_equals(name, ".gif"))
        return "PHOTO";

    if (ext_equals(name, ".mp3") || ext_equals(name, ".wav") ||
        ext_equals(name, ".at3") || ext_equals(name, ".oma") ||
        ext_equals(name, ".m4a") || ext_equals(name, ".aac"))
        return "MUSIC";

    if (ext_equals(name, ".iso") || ext_equals(name, ".cso") ||
        ext_equals(name, ".zso") || ext_equals(name, ".pbp"))
        return "GAME";

    return "FILE";
}

const char* file_category_display(const char* category)
{
    if (!category)
        return ui_text("Arquivo", "File");

    if (!strcmp(category, "PHOTO"))
        return ui_text("Foto", "Photo");
    if (!strcmp(category, "MUSIC"))
        return ui_text("Musica", "Music");
    if (!strcmp(category, "GAME"))
        return ui_text("Jogo", "Game");

    /* Compatibilidade visual com builds antigos durante testes. */
    if (!strcmp(category, "Foto"))
        return ui_text("Foto", "Photo");
    if (!strcmp(category, "Musica"))
        return ui_text("Musica", "Music");
    if (!strcmp(category, "Jogo"))
        return ui_text("Jogo", "Game");

    return ui_text("Arquivo", "File");
}

const char* base_name(const char* path)
{
    const char* slash;
    if (!path)
        return "arquivo.bin";

    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

void sanitize_filename(const char* input, char* output, int output_size)
{
    int i, j = 0;
    const char* b = base_name(input);

    if (!output || output_size <= 0)
        return;

    for (i = 0; b[i] && j < output_size - 1; ++i) {
        unsigned char c = (unsigned char)b[i];
        if (c == '/' || c == '\\' || c == ':' || c < 32)
            output[j++] = '_';
        else
            output[j++] = (char)c;
    }

    output[j] = '\0';
    if (j == 0)
        safe_copy(output, "arquivo.bin", output_size);
}

int path_exists(const char* path)
{
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    return sceIoGetstat(path, &st) >= 0;
}

int directory_exists(const char* path)
{
    SceUID d = sceIoDopen(path);
    if (d >= 0) {
        sceIoDclose(d);
        return 1;
    }
    return 0;
}

void join_path(const char* dir, const char* name, char* out, int out_size)
{
    size_t len;

    if (!out || out_size <= 0)
        return;

    out[0] = '\0';
    safe_copy(out, dir ? dir : "", out_size);
    len = strlen(out);

    if (len > 0 && out[len - 1] != '/')
        safe_append(out, "/", out_size);

    safe_append(out, name ? name : "", out_size);
}

void make_unique_path(const char* dir, const char* filename, char* out, int out_size)
{
    char safe[192];
    char stem[160];
    char ext[32];
    char candidate[224];
    const char* dot;
    int n;

    sanitize_filename(filename, safe, sizeof(safe));
    dot = strrchr(safe, '.');

    if (dot) {
        int stem_len = (int)(dot - safe);
        if (stem_len >= (int)sizeof(stem))
            stem_len = (int)sizeof(stem) - 1;
        memcpy(stem, safe, (size_t)stem_len);
        stem[stem_len] = '\0';
        safe_copy(ext, dot, sizeof(ext));
    } else {
        safe_copy(stem, safe, sizeof(stem));
        ext[0] = '\0';
    }

    join_path(dir, safe, out, out_size);
    if (!path_exists(out))
        return;

    for (n = 1; n < 1000; ++n) {
        snprintf(candidate, sizeof(candidate), "%.150s_%d%.31s", stem, n, ext);
        join_path(dir, candidate, out, out_size);
        if (!path_exists(out))
            return;
    }

    /* Fallback deterministico caso existam 999 colisoes. */
    snprintf(candidate, sizeof(candidate), "ADShare_%08X%.31s",
             (unsigned int)now_us(), ext);
    join_path(dir, candidate, out, out_size);
}

} // namespace adshare
