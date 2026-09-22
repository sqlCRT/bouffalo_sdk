#ifndef BFLB_USB_V2_H
#define BFLB_USB_V2_H

#include <stdbool.h>

/* Set before usb_dc_init().  This selects Full-Speed when native GIP Audio is
 * active, without requiring a second firmware image. */
void bflb_usb_v2_set_force_full_speed(bool force_full_speed);

#endif /* BFLB_USB_V2_H */
