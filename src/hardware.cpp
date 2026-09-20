#include "adshare.hpp"

namespace adshare {

/* =========================================================
   DEVICE INFO - KERNEL / IDSTORAGE
   ========================================================= */

/*
 * O código abaixo evita APIs antigas de SystemControl que variam entre CFWs.
 *
 * - kuKernelGetModel(): geração real do PSP em user mode.
 * - LibPspExploit: executa adshare_kernel_probe() em contexto kernel.
 * - pspXploitFindFunction(): resolve apenas exports existentes no firmware.
 *
 * NIDs usados:
 *   sceIdStorageLookup           0x6FE062D1
 *   sceSysregGetTachyonVersion  0xE2A5D1EE
 *   sceSysconGetBaryonVersion   0x7EC5A957
 *   sceSysconGetPommelVersion   0xE7E87741
 *
 * A leitura da região usa IDStorage leaf 0x0100, offset 0xF5,
 * mesma técnica pública usada pelo pspIdent.
 */

#define NID_IDSTORAGE_LOOKUP     0x6FE062D1
#define NID_SYSREG_GET_TACHYON  0xE2A5D1EE
#define NID_SYSCON_GET_BARYON   0x7EC5A957
#define NID_SYSCON_GET_POMMEL   0xE7E87741

typedef int (*IdStorageLookupFn)(int key, int offset, void* buffer, int length);
typedef int (*SysregGetTachyonFn)(void);
typedef int (*SysconGetVersionFn)(int* version);

typedef struct HardwareProbeResult {
    volatile int completed;
    volatile int any_success;

    volatile int region_valid;
    volatile uint8_t region_id;

    volatile int tachyon_valid;
    volatile int baryon_valid;
    volatile int pommel_valid;

    volatile uint32_t tachyon;
    volatile uint32_t baryon;
    volatile uint32_t pommel;
} HardwareProbeResult;

static volatile HardwareProbeResult g_hwprobe;
static int g_hwprobe_attempted = 0;
static int g_hwprobe_status = -1;

/*
 * Tabela usada por pspIdent para transformar o byte de região do IDStorage
 * no sufixo comercial. Ex.: região 0x04 => sufixo 1 => PSP-3001.
 */
static const int g_model_region_suffix[16] = {
    0, 0, 0, 0, 1, 4, 5, 3, 10, 2, 6, 7, 8, 9, 0, 0
};

static const char* language_name(int lang)
{
    switch (lang) {
        case PSP_SYSTEMPARAM_LANGUAGE_JAPANESE:
            return ui_text("Japones", "Japanese");
        case PSP_SYSTEMPARAM_LANGUAGE_ENGLISH:
            return ui_text("Ingles", "English");
        case PSP_SYSTEMPARAM_LANGUAGE_FRENCH:
            return ui_text("Frances", "French");
        case PSP_SYSTEMPARAM_LANGUAGE_SPANISH:
            return ui_text("Espanhol", "Spanish");
        case PSP_SYSTEMPARAM_LANGUAGE_GERMAN:
            return ui_text("Alemao", "German");
        case PSP_SYSTEMPARAM_LANGUAGE_ITALIAN:
            return ui_text("Italiano", "Italian");
        case PSP_SYSTEMPARAM_LANGUAGE_DUTCH:
            return ui_text("Holandes", "Dutch");
        case PSP_SYSTEMPARAM_LANGUAGE_PORTUGUESE:
            return ui_text("Portugues", "Portuguese");
        case PSP_SYSTEMPARAM_LANGUAGE_RUSSIAN:
            return ui_text("Russo", "Russian");
        case PSP_SYSTEMPARAM_LANGUAGE_KOREAN:
            return ui_text("Coreano", "Korean");
        case PSP_SYSTEMPARAM_LANGUAGE_CHINESE_TRADITIONAL:
            return ui_text("Chines Trad.", "Chinese Trad.");
        case PSP_SYSTEMPARAM_LANGUAGE_CHINESE_SIMPLIFIED:
            return ui_text("Chines Simpl.", "Chinese Simpl.");
        default:
            return ui_text("Desconhecido", "Unknown");
    }
}

static void firmware_string(char* out, int out_size)
{
    unsigned int v = (unsigned int)sceKernelDevkitVersion();
    unsigned int major = (v >> 24) & 0xFF;
    unsigned int minor = (v >> 16) & 0xFF;
    unsigned int rev = (v >> 8) & 0xFF;

    if (rev == 0)
        snprintf(out, out_size, "%u.%02u", major, minor);
    else
        snprintf(out, out_size, "%u.%02u.%u", major, minor, rev);
}

static const char* region_name_from_id(uint8_t id)
{
    switch (id) {
        case 0x03: return ui_text("Japao", "Japan");
        case 0x04: return ui_text("America do Norte", "North America");
        case 0x05: return ui_text("Europa / Oriente Medio / Africa",
                                  "Europe / Middle East / Africa");
        case 0x06: return ui_text("Coreia", "Korea");
        case 0x07: return ui_text("Reino Unido", "United Kingdom");
        case 0x08: return ui_text("America Latina", "Latin America");
        case 0x09: return ui_text("Australia / Nova Zelandia",
                                  "Australia / New Zealand");
        case 0x0A: return "Hong Kong";
        case 0x0B: return "Taiwan";
        case 0x0C: return ui_text("Russia", "Russia");
        case 0x0D: return ui_text("China", "China");
        case 0x0E: return ui_text("AV / Teste", "AV / Test");
        default:   return ui_text("Desconhecida", "Unknown");
    }
}

static void generation_name(int gen, char* out, int out_size)
{
    if (gen >= 0 && gen <= 15)
        snprintf(out, out_size, "%02dg", gen + 1);
    else
        safe_copy(out, ui_text("Desconhecida", "Unknown"), out_size);
}

static int commercial_suffix_from_region(uint8_t region_id)
{
    if (region_id < 16)
        return g_model_region_suffix[region_id];

    return 0;
}

/*
 * Executada com privilégio kernel por pspXploitExecuteKernel().
 * Mantém a rotina curta: resolve quatro funções, lê os IDs e retorna.
 */
static void adshare_kernel_probe(void)
{
    int old_k1;
    int old_userlevel;
    const char* sysreg_module;

    IdStorageLookupFn idstorage_lookup = NULL;
    SysregGetTachyonFn get_tachyon = NULL;
    SysconGetVersionFn get_baryon = NULL;
    SysconGetVersionFn get_pommel = NULL;

    uint32_t address;
    int temp;
    uint8_t region_byte;

    old_k1 = pspSdkSetK1(0);
    old_userlevel = pspXploitSetUserLevel(8);

    /*
     * O próprio LibPspExploit recomenda reparar o kernel antes de resolver
     * exports após o exploit. É a mesma sequência usada pelo pspIdent.
     */
    pspXploitRepairKernel();

    sysreg_module =
        (pspXploitFindTextAddrByName("sceLowIO_Driver") == 0)
        ? "sceSYSREG_Driver"
        : "sceLowIO_Driver";

    address = pspXploitFindFunction(
        "sceIdStorage_Service",
        "sceIdStorage_driver",
        NID_IDSTORAGE_LOOKUP
    );
    if (address)
        idstorage_lookup = (IdStorageLookupFn)(uintptr_t)address;

    address = pspXploitFindFunction(
        sysreg_module,
        "sceSysreg_driver",
        NID_SYSREG_GET_TACHYON
    );
    if (address)
        get_tachyon = (SysregGetTachyonFn)(uintptr_t)address;

    address = pspXploitFindFunction(
        "sceSYSCON_Driver",
        "sceSyscon_driver",
        NID_SYSCON_GET_BARYON
    );
    if (address)
        get_baryon = (SysconGetVersionFn)(uintptr_t)address;

    address = pspXploitFindFunction(
        "sceSYSCON_Driver",
        "sceSyscon_driver",
        NID_SYSCON_GET_POMMEL
    );
    if (address)
        get_pommel = (SysconGetVersionFn)(uintptr_t)address;

    if (idstorage_lookup) {
        region_byte = 0xFF;
        if (idstorage_lookup(0x0100, 0xF5, &region_byte, 1) >= 0) {
            g_hwprobe.region_id = region_byte;
            g_hwprobe.region_valid = 1;
            g_hwprobe.any_success = 1;
        }
    }

    if (get_tachyon) {
        temp = get_tachyon();
        if (temp >= 0) {
            g_hwprobe.tachyon = (uint32_t)temp;
            g_hwprobe.tachyon_valid = 1;
            g_hwprobe.any_success = 1;
        }
    }

    if (get_baryon) {
        temp = 0;
        if (get_baryon(&temp) >= 0) {
            g_hwprobe.baryon = (uint32_t)temp;
            g_hwprobe.baryon_valid = 1;
            g_hwprobe.any_success = 1;
        }
    }

    if (get_pommel) {
        temp = 0;
        if (get_pommel(&temp) >= 0) {
            g_hwprobe.pommel = (uint32_t)temp;
            g_hwprobe.pommel_valid = 1;
            g_hwprobe.any_success = 1;
        }
    }

    g_hwprobe.completed = 1;

    pspXploitSetUserLevel(old_userlevel);
    pspSdkSetK1(old_k1);
}

/*
 * Faz a elevação apenas uma vez por execução do ADShare.
 * collect_device_info() pode ser chamado muitas vezes pela interface.
 */
static int probe_hardware_once(void)
{
    int ret;

    if (g_hwprobe_attempted)
        return g_hwprobe_status;

    g_hwprobe_attempted = 1;
    memset((void*)&g_hwprobe, 0, sizeof(g_hwprobe));

    ret = pspXploitInitKernelExploit();
    if (ret != 0) {
        g_hwprobe_status = ret;
        return ret;
    }

    ret = pspXploitDoKernelExploit();
    if (ret != 0) {
        g_hwprobe_status = ret;
        return ret;
    }

    pspXploitExecuteKernel(reinterpret_cast<void*>(adshare_kernel_probe));

    if (!g_hwprobe.completed || !g_hwprobe.any_success) {
        g_hwprobe_status = -1;
        return -1;
    }

    g_hwprobe_status = 0;
    return 0;
}

/*
 * Retorna um nome de placa somente quando Tachyon/Baryon/Pommel permitem
 * uma conclusão útil. Mapeamento adaptado da tabela pública do pspIdent.
 */
static void board_from_real_ids(
    int tachyon_valid, uint32_t tachyon,
    int baryon_valid, uint32_t baryon,
    int pommel_valid, uint32_t pommel,
    char* out, int out_size)
{
    if (!tachyon_valid) {
        safe_copy(out, "Tachyon indisponivel", out_size);
        return;
    }

    switch (tachyon) {
        case 0x00140000:
            if (baryon_valid) {
                if (baryon == 0x00010600) { safe_copy(out, "TA-079v1", out_size); return; }
                if (baryon == 0x00020600) { safe_copy(out, "TA-079v2", out_size); return; }
                if (baryon == 0x00030600) { safe_copy(out, "TA-079v3", out_size); return; }
            }
            safe_copy(out, ui_text("Familia TA-079", "TA-079 family"), out_size);
            return;

        case 0x00200000:
            if (baryon_valid) {
                if (baryon == 0x00030600) { safe_copy(out, "TA-079v4", out_size); return; }
                if (baryon == 0x00040600) { safe_copy(out, "TA-079v5", out_size); return; }
            }
            safe_copy(out, ui_text("Familia TA-079", "TA-079 family"), out_size);
            return;

        case 0x00300000:
            if (pommel_valid) {
                if (pommel == 0x00000103) { safe_copy(out, "TA-081v1", out_size); return; }
                if (pommel == 0x00000104) { safe_copy(out, "TA-081v2", out_size); return; }
            }
            safe_copy(out, ui_text("Familia TA-081", "TA-081 family"), out_size);
            return;

        case 0x00400000:
            if (baryon_valid) {
                if (baryon == 0x00114000) { safe_copy(out, "TA-082", out_size); return; }
                if (baryon == 0x00121000) { safe_copy(out, "TA-086", out_size); return; }
            }
            safe_copy(out, "TA-082 / TA-086", out_size);
            return;

        case 0x00500000:
            if (baryon_valid) {
                if (baryon == 0x0022B200) { safe_copy(out, "TA-085v1", out_size); return; }
                if (baryon == 0x00234000) { safe_copy(out, "TA-085v2", out_size); return; }

                if (baryon == 0x00243000 && pommel_valid) {
                    if (pommel == 0x00000123) {
                        safe_copy(out, "TA-088v1/v2", out_size);
                        return;
                    }
                    if (pommel == 0x00000132) {
                        safe_copy(out, "TA-090v1", out_size);
                        return;
                    }
                }
            }
            safe_copy(out, "TA-085 / TA-088 / TA-090", out_size);
            return;

        case 0x00600000:
            if (baryon_valid) {
                if (baryon == 0x00234000) {
                    safe_copy(out, "TA-088v3 / TA-085v2 hybrid", out_size);
                    return;
                }
                if (baryon == 0x00243000) {
                    safe_copy(out, "TA-088v3", out_size);
                    return;
                }
                if (baryon == 0x00263100) {
                    if (pommel_valid && pommel == 0x00000132) {
                        safe_copy(out, "TA-090v2", out_size);
                        return;
                    }
                    if (pommel_valid && pommel == 0x00000133) {
                        safe_copy(out, "TA-090v3", out_size);
                        return;
                    }
                    safe_copy(out, "TA-090v2/v3", out_size);
                    return;
                }
                if (baryon == 0x00285000) {
                    safe_copy(out, "TA-092", out_size);
                    return;
                }
            }
            safe_copy(out, "TA-088 / TA-090 / TA-092", out_size);
            return;

        case 0x00720000:
            safe_copy(out, "TA-091", out_size);
            return;

        case 0x00810000:
            if (baryon_valid) {
                if (baryon == 0x002C4000) {
                    if (pommel_valid && pommel == 0x00000141) {
                        safe_copy(out, "TA-093v1", out_size);
                        return;
                    }
                    if (pommel_valid && pommel == 0x00000143) {
                        safe_copy(out, "TA-093v2", out_size);
                        return;
                    }
                    safe_copy(out, "TA-093", out_size);
                    return;
                }
                if (baryon == 0x002E4000) {
                    safe_copy(out, "TA-095v1 (09g)", out_size);
                    return;
                }
                if (baryon == 0x012E4000) {
                    safe_copy(out, "TA-095v3 (07g)", out_size);
                    return;
                }
                if (baryon == 0x00323100) {
                    safe_copy(out, "TA-094 v1 (PSP Go)", out_size);
                    return;
                }
                if (baryon == 0x00324000) {
                    safe_copy(out, "TA-094 v2 (PSP Go)", out_size);
                    return;
                }
            }
            safe_copy(out, "TA-093 / TA-094 / TA-095", out_size);
            return;

        case 0x00820000:
            if (baryon_valid) {
                if (baryon == 0x002E4000) {
                    safe_copy(out, "TA-095v2 (09g)", out_size);
                    return;
                }
                if (baryon == 0x012E4000) {
                    safe_copy(out, "TA-095v4 (07g)", out_size);
                    return;
                }
            }
            safe_copy(out, ui_text("Familia TA-095", "TA-095 family"), out_size);
            return;

        case 0x00900000:
            safe_copy(out, "TA-096 / TA-097 (PSP Street)", out_size);
            return;

        default:
            snprintf(out, out_size, "%s (Tachyon 0x%08X)",
                     ui_text("Desconhecida", "Unknown"),
                     (unsigned int)tachyon);
            return;
    }
}

static void build_model_name(
    int generation,
    int region_valid,
    uint8_t region_id,
    char* out,
    int out_size)
{
    int suffix = region_valid ? commercial_suffix_from_region(region_id) : 0;

    /*
     * kuKernelGetModel() retorna 0 para 01g, 1 para 02g, 2 para 03g etc.
     * Quando o sufixo regional nao esta disponivel, nao inventamos um SKU.
     */
    switch (generation) {
        case 0:
            if (suffix > 0) snprintf(out, out_size, "PSP-10%02d", suffix);
            else safe_copy(out, "PSP-1000 series", out_size);
            break;

        case 1:
            if (suffix > 0) snprintf(out, out_size, "PSP-20%02d", suffix);
            else safe_copy(out, "PSP-2000 series", out_size);
            break;

        case 2: /* 03g */
        case 3: /* 04g */
        case 6: /* 07g */
        case 8: /* 09g */
            if (suffix > 0) snprintf(out, out_size, "PSP-30%02d", suffix);
            else safe_copy(out, "PSP-3000 series", out_size);
            break;

        case 4: /* 05g */
            if (suffix > 0) snprintf(out, out_size, "PSP-N10%02d", suffix);
            else safe_copy(out, "PSP Go / N1000 series", out_size);
            break;

        case 10: /* 11g */
            if (suffix > 0) snprintf(out, out_size, "PSP-E10%02d", suffix);
            else safe_copy(out, "PSP Street / E1000 series", out_size);
            break;

        default:
            snprintf(out, out_size, "PSP geracao %02dg", generation + 1);
            break;
    }
}

void collect_device_info(void)
{
    int lang = -1;
    int configured_channel = 0;
    char mac[32];
    int generation;
    int probe_ret;

    memset(&g_device, 0, sizeof(g_device));

    if (sceUtilityGetSystemParamString(
            PSP_SYSTEMPARAM_ID_STRING_NICKNAME,
            g_device.nickname,
            sizeof(g_device.nickname)) < 0) {
        safe_copy(g_device.nickname, "PSP", sizeof(g_device.nickname));
    }

    safe_copy(g_local_nickname, g_device.nickname, sizeof(g_local_nickname));
    firmware_string(g_device.firmware, sizeof(g_device.firmware));

    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &lang);
    sceUtilityGetSystemParamInt(
        PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL,
        &configured_channel
    );

    safe_copy(g_device.language, language_name(lang), sizeof(g_device.language));

    if (configured_channel == PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC)
        safe_copy(g_device.adhoc_channel,
                  ui_text("Automatico", "Automatic"),
                  sizeof(g_device.adhoc_channel));
    else
        snprintf(g_device.adhoc_channel, sizeof(g_device.adhoc_channel),
                 "%s %d", ui_text("Canal", "Channel"), configured_channel);

    if (sceWlanGetEtherAddr(g_local_mac) >= 0) {
        mac_to_string(g_local_mac, mac, sizeof(mac));
        safe_copy(g_device.mac, mac, sizeof(g_device.mac));
    } else {
        safe_copy(g_device.mac, ui_text("Indisponivel", "Unavailable"), sizeof(g_device.mac));
    }

    /*
     * Fonte primária do modelo: KUBridge. Não existe mais heurística por RAM.
     */
    generation = kuKernelGetModel();
    g_device.kernel_model_result = generation;
    g_device.model_generation = generation;
    generation_name(generation, g_device.generation,
                    sizeof(g_device.generation));

    /*
     * Fonte dos IDs físicos: rotina em kernel via LibPspExploit.
     * O resultado fica em cache para não repetir a elevação de privilégio.
     */
    probe_ret = probe_hardware_once();

    if (g_hwprobe.region_valid) {
        g_device.region_valid = 1;
        g_device.region_id = g_hwprobe.region_id;

        safe_copy(g_device.region,
                  region_name_from_id(g_device.region_id),
                  sizeof(g_device.region));

        snprintf(g_device.region_id_text,
                 sizeof(g_device.region_id_text),
                 "0x%02X -> %s %d",
                 (unsigned int)g_device.region_id,
                 ui_text("sufixo", "suffix"),
                 commercial_suffix_from_region(g_device.region_id));
    } else {
        safe_copy(g_device.region, ui_text("IDStorage indisponivel", "IDStorage unavailable"),
                  sizeof(g_device.region));
        safe_copy(g_device.region_id_text, ui_text("Indisponivel", "Unavailable"),
                  sizeof(g_device.region_id_text));
    }

    build_model_name(
        generation,
        g_device.region_valid,
        g_device.region_id,
        g_device.model,
        sizeof(g_device.model)
    );

    if (g_hwprobe.tachyon_valid) {
        g_device.tachyon_valid = 1;
        g_device.tachyon = g_hwprobe.tachyon;
        snprintf(g_device.tachyon_text, sizeof(g_device.tachyon_text),
                 "0x%08X", (unsigned int)g_device.tachyon);
    } else {
        safe_copy(g_device.tachyon_text, ui_text("Indisponivel", "Unavailable"),
                  sizeof(g_device.tachyon_text));
    }

    if (g_hwprobe.baryon_valid) {
        g_device.baryon_valid = 1;
        g_device.baryon = g_hwprobe.baryon;
        snprintf(g_device.baryon_text, sizeof(g_device.baryon_text),
                 "0x%08X", (unsigned int)g_device.baryon);
    } else {
        safe_copy(g_device.baryon_text, ui_text("Indisponivel", "Unavailable"),
                  sizeof(g_device.baryon_text));
    }

    if (g_hwprobe.pommel_valid) {
        g_device.pommel_valid = 1;
        g_device.pommel = g_hwprobe.pommel;
        snprintf(g_device.pommel_text, sizeof(g_device.pommel_text),
                 "0x%08X", (unsigned int)g_device.pommel);
    } else {
        safe_copy(g_device.pommel_text, ui_text("Indisponivel", "Unavailable"),
                  sizeof(g_device.pommel_text));
    }

    board_from_real_ids(
        g_device.tachyon_valid, g_device.tachyon,
        g_device.baryon_valid, g_device.baryon,
        g_device.pommel_valid, g_device.pommel,
        g_device.board_family, sizeof(g_device.board_family)
    );

    if (probe_ret == 0) {
        safe_copy(g_device.kernel_source,
                  "KUBridge + LibPspExploit + IDStorage",
                  sizeof(g_device.kernel_source));
        safe_copy(g_device.probe_status_text,
                  ui_text("Hardware lido em kernel", "Hardware read in kernel"),
                  sizeof(g_device.probe_status_text));
    } else {
        safe_copy(g_device.kernel_source,
                  ui_text("KUBridge (IDs kernel indisponiveis)", "KUBridge (kernel IDs unavailable)"),
                  sizeof(g_device.kernel_source));
        snprintf(g_device.probe_status_text,
                 sizeof(g_device.probe_status_text),
                 "Probe kernel: 0x%08X",
                 (unsigned int)probe_ret);
    }
}

} // namespace adshare
