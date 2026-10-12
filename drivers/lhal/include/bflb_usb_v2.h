#ifndef BFLB_USB_V2_H
#define BFLB_USB_V2_H

#include <stdbool.h>
#include <stdint.h>

/* Set before usb_dc_init().  This selects Full-Speed when native GIP Audio is
 * active, without requiring a second firmware image. */
void bflb_usb_v2_set_force_full_speed(bool force_full_speed);

/* Set before usb_dc_init(). Keep the PHY unplugged throughout initialization
 * so the application can publish its descriptors before attaching. Default
 * false preserves automatic attachment for other SDK applications. */
void bflb_usb_v2_set_start_detached(bool detached);

/* Opt-in arbitration for audio interrupt IN 0x83 and native HID OUT 0x03.
 * Configure while the USB device is stopped, before interface registration.
 * Other modes leave this disabled. Poll from task context with USB IRQ excluded. */
void bflb_usb_v2_set_ep3_arbitration(bool enabled);
void bflb_usb_v2_poll_ep3_arbitration(uint8_t busid);

#endif /* BFLB_USB_V2_H */
