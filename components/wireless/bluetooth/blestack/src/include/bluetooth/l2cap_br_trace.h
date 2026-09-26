/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BT_L2CAP_BR_TRACE_H
#define BT_L2CAP_BR_TRACE_H

#include <stdint.h>
#include <stddef.h>
struct bt_conn;
enum bt_l2cap_br_trace_event {
    BT_BR_TRACE_OPEN,
    BT_BR_TRACE_RX,
    BT_BR_TRACE_TX_QUEUE,
    BT_BR_TRACE_RTX,
    BT_BR_TRACE_CLEANED,
    BT_BR_TRACE_TX_ERROR,
};

/* Optional non-blocking observer. Data is borrowed for this call only.
 * TX_QUEUE is a host submission, not an acknowledgement by the peer.
 * TX_ERROR reports a failed submission (detail is a signed 16-bit errno;
 * data holds the command code and identifier).
 * CLEANED runs after every channel has been deleted, while conn is valid.
 * The default weak implementation does nothing; never print in this hook. */
void bt_l2cap_br_trace(struct bt_conn *conn, enum bt_l2cap_br_trace_event event,
                      uint16_t cid, uint16_t detail,
                      const uint8_t *data, size_t len);
#endif
