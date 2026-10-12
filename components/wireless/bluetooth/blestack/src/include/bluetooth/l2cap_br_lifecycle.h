#ifndef BT_L2CAP_BR_LIFECYCLE_H
#define BT_L2CAP_BR_LIFECYCLE_H
struct bt_conn;
/* Called after all channels are deleted. Borrowed connection; observers that
 * defer handling must take a reference. No packet data or tracing involved. */
void bt_l2cap_br_cleanup_complete(struct bt_conn *conn);
#endif
