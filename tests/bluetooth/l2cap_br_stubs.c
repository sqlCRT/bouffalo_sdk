#include <stdint.h>
#include <assert.h>
#include "l2cap_br_trace.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>

#define CONTAINER_OF(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define BR_CHAN(p) CONTAINER_OF(p,struct bt_l2cap_br_chan,chan)
#define sys_le16_to_cpu(x) (x)
#define sys_cpu_to_le16(x) (x)
#define BT_DBG(...) ((void)0)
static int errors, connected, bounds, disconnected, responses, next_out;
static char last_error[128];
static jmp_buf bailout;
static void log_error(const char *fmt, ...) {
    va_list ap; va_start(ap,fmt); vsnprintf(last_error,sizeof(last_error),fmt,ap);
    va_end(ap); errors++;
}
#define BT_ERR(...) log_error(__VA_ARGS__)
#define BT_WARN(...) log_error(__VA_ARGS__)
#define BT_L2CAP_CONFIG 2
#define BT_L2CAP_CONNECTED 3
#define BT_L2CAP_CID_BR_SIG 1
#define BT_L2CAP_REJ_INVALID_CID 2
#define BT_L2CAP_REJ_NOT_UNDERSTOOD 0
#define BT_L2CAP_CONF_SUCCESS 0
#define BT_L2CAP_CONF_UNACCEPT 1
#define BT_L2CAP_CONF_REJECT 2
#define BT_L2CAP_CONF_UNKNOWN_OPTIONS 3
#define BT_L2CAP_CONF_OPT_MTU 1
#define BT_L2CAP_CONF_HINT 0x80
#define BT_L2CAP_CONF_MASK 0x7f
#define L2CAP_BR_DEFAULT_MTU 672
#define L2CAP_BR_MIN_MTU 48
#define BT_L2CAP_CONN_REQ 2
#define BT_L2CAP_CONN_RSP 3
#define BT_L2CAP_CONF_REQ 4
#define BT_L2CAP_CONF_RSP 5
#define BT_L2CAP_DISCONN_REQ 6
#define BT_L2CAP_DISCONN_RSP 7
#define BT_L2CAP_ECHO_REQ 8
#define BT_L2CAP_ECHO_RSP 9
#define BT_L2CAP_INFO_REQ 10
#define BT_L2CAP_INFO_RSP 11
#define L2CAP_FLAG_CONN_LCONF_DONE 0
#define L2CAP_FLAG_CONN_RCONF_DONE 1
#define L2CAP_FLAG_CONN_ACCEPTOR 2
#define L2CAP_FLAG_SIG_INFO_PENDING 4
#define L2CAP_FLAG_SIG_INFO_DONE 5
#define BT_L2CAP_INFO_SUCCESS 0
#define BT_L2CAP_INFO_FEAT_MASK 2
#define BT_L2CAP_INFO_FIXED_CHAN 3
#define L2CAP_FEAT_FIXED_CHAN_MASK 0x80
#define BT_L2CAP_BR_PENDING 1
#define EINVAL 22
#define BT_SECURITY_L0 0
#define BT_SECURITY_L1 1
#define BT_SECURITY_L2 2
#define BT_SECURITY_L3 3
#define BT_SECURITY_L4 4
#define ESRCH 3
#define BT_FEAT_HOST_SSP(f) ((f)[0] != 0)

struct bt_conn { int sec_level; int encrypt; struct { unsigned char features[1]; } br; };
struct bt_l2cap_chan;
struct ops { void (*connected)(struct bt_l2cap_chan *); };
struct bt_l2cap_chan { struct bt_conn *conn; struct ops *ops; int state,rtx_work,required_sec_level; uint8_t ident; };
struct endpoint { uint16_t cid,mtu; };
struct bt_l2cap_br_chan { struct bt_l2cap_chan chan; struct endpoint rx,tx; unsigned flags[1]; };
struct bt_l2cap_br { struct bt_l2cap_br_chan chan; uint8_t info_ident,info_fixed_chan; uint32_t info_feat_mask; };
struct bt_l2cap_info_rsp { uint16_t type,result; };
struct net_buf { uint8_t *data; size_t len; };
struct bt_l2cap_sig_hdr { uint8_t code,ident; uint16_t len; };
struct bt_l2cap_conf_req { uint16_t dcid,flags; };
struct bt_l2cap_conf_rsp { uint16_t scid,flags,result; uint8_t data[]; };
struct bt_l2cap_conf_opt { uint8_t type,len; uint8_t data[]; };
struct bt_l2cap_cmd_reject_cid_data { uint16_t scid,dcid; };
enum l2cap_br_conn_security_result { L2CAP_CONN_SECURITY_PASSED, L2CAP_CONN_SECURITY_REJECT, L2CAP_CONN_SECURITY_PENDING };
static struct bt_conn conn;
static struct bt_l2cap_br_chan ctrl;
static struct bt_l2cap_br sig;
static uint8_t output_bytes[32][128];
static struct net_buf outputs[32];
static int br_sig_pool;
static uint8_t reply_ident[4];

static int atomic_test_bit(unsigned *p,int b) { return !!(*p & (1u<<b)); }
static void atomic_set_bit(unsigned *p,int b) { *p |= 1u<<b; }
static void *net_buf_pull_mem(struct net_buf *b,size_t n) {
    void *p=b->data; if(n>b->len) { bounds++; longjmp(bailout,1); }
    b->data+=n; b->len-=n; return p;
}
static void *net_buf_pull(struct net_buf *b,size_t n) { return net_buf_pull_mem(b,n); }
static uint16_t net_buf_pull_le16(struct net_buf *b) { uint8_t *p=net_buf_pull_mem(b,2); return p[0]|p[1]<<8; }
static void *net_buf_add(struct net_buf *b,size_t n) { void *p=b->data+b->len; b->len+=n; return p; }
static struct net_buf *bt_l2cap_create_pdu(void *pool,int reserve) {
    struct net_buf *b=&outputs[next_out]; b->data=output_bytes[next_out++]; b->len=0; return b;
}
static void l2cap_br_conf_add_mtu(struct net_buf *b,uint16_t mtu) {
    uint8_t *p=net_buf_add(b,4); p[0]=1;p[1]=2;p[2]=mtu;p[3]=mtu>>8;
}
static struct bt_l2cap_chan *bt_l2cap_br_lookup_rx_cid(struct bt_conn *c,uint16_t cid) {
    return cid==ctrl.rx.cid ? &ctrl.chan:NULL;
}
static void bt_l2cap_chan_set_state(struct bt_l2cap_chan *c,int s) { c->state=s; }
static void got_connected(struct bt_l2cap_chan *c) { connected++; }
static struct ops chan_ops={got_connected};
static void k_delayed_work_cancel(int *t) { *t=0; }
static void bt_l2cap_chan_disconnect(struct bt_l2cap_chan *c) { disconnected++; }
static int send_error, trace_tx_errors, trace_tx_submitted;
static uint16_t trace_error_detail;
static uint8_t trace_error_command[2];
static int bt_l2cap_send_cb(struct bt_conn *c,int cid,struct net_buf *b,void *cb,void *arg) {
    responses++;
    /* Sending consumes the buffer, including on failure. */
    memset(b->data, 0xee, b->len);
    return send_error;
}
static void l2cap_br_send_reject(struct bt_conn *c,uint8_t id,int why,void *p,size_t n) { errors++; }
static void l2cap_br_send_conn_rsp(struct bt_conn *c,uint16_t scid,uint16_t dcid,uint8_t id,uint16_t result) { reply_ident[responses++]=id; }
static int bt_conn_set_security(struct bt_conn *c,int level) { return 0; }
#define STUB_HANDLER(name) static void name(struct bt_l2cap_br *c,uint8_t id,struct net_buf *b) {}

STUB_HANDLER(l2cap_br_info_req)
STUB_HANDLER(l2cap_br_disconn_req)
STUB_HANDLER(l2cap_br_conn_req)
STUB_HANDLER(l2cap_br_disconn_rsp)
STUB_HANDLER(l2cap_br_conn_rsp)
STUB_HANDLER(l2cap_br_echo_req)
STUB_HANDLER(l2cap_br_echo_resp)

static int atomic_test_and_clear_bit(unsigned *p,int b) { int old=atomic_test_bit(p,b); *p &= ~(1u<<b); return old; }
static uint32_t net_buf_pull_le32(struct net_buf *b) { uint8_t *p=net_buf_pull_mem(b,4); return p[0]|p[1]<<8|p[2]<<16|(uint32_t)p[3]<<24; }
static uint8_t net_buf_pull_u8(struct net_buf *b) { return *(uint8_t *)net_buf_pull_mem(b,1); }
static void l2cap_br_get_info(struct bt_l2cap_br *l,uint16_t t) {}
static void connect_optional_fixed_channels(struct bt_l2cap_br *l) {}
void bt_l2cap_br_trace(struct bt_conn *c,enum bt_l2cap_br_trace_event e,uint16_t cid,uint16_t detail,const uint8_t *data,size_t len) {
    if(e==BT_BR_TRACE_TX_QUEUE) ++trace_tx_submitted;
    if(e==BT_BR_TRACE_TX_ERROR) {
        ++trace_tx_errors; trace_error_detail=detail;
        assert(len==2); memcpy(trace_error_command,data,2);
    }
}
