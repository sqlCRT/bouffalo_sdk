#ifndef __BL616_SYS_H__
#define __BL616_SYS_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BL_RST_POWER_OFF = 0,
    BL_RST_HARDWARE_WATCHDOG,
    BL_RST_FATAL_EXCEPTION,
    BL_RST_SOFTWARE_WATCHDOG,
    BL_RST_SOFTWARE,
    BL_RST_HBN,
    BL_RST_BOD,
} BL_RST_REASON_E;

BL_RST_REASON_E bl_sys_rstinfo_get(void);
int bl_sys_rstinfo_set(BL_RST_REASON_E val);
int bl_sys_rstinfo_getsting(char *info);
char *bl_sys_rstinfo_getstring(void);
void bl_sys_rstinfo_init(void);
int bl_sys_reset_por(void);
void bl_sys_reset_system(void);
void bl_sys_reset_system_from_interface(void);

/*
 * Permanently disable the BL616/BL618 BootROM download interfaces only when
 * USB boot has not already been enabled by eFuse.
 *
 * This burns SW_USAGE_0.uartboot_disable (bit 4).  eFuse programming is
 * irreversible; callers must provide their own explicit factory/operator
 * confirmation before invoking bl_sys_disable_rom_download().  Returns -2
 * when SW_USAGE_0.usbboot_enable is already set and therefore cannot be
 * cleared.
 */
uint32_t bl_sys_get_sw_usage0(void);
bool bl_sys_rom_download_is_disabled(void);
bool bl_sys_usb_download_is_enabled(void);
int bl_sys_disable_rom_download(void);
int bl_sys_init(void);

void bl_cpu_sysmap_init(bool dcache_preload_en, bool dcache_amr_en);

#endif
