"""Exercise actual driver send ownership across all compile-time backends."""
import argparse
from pathlib import Path
import subprocess
import tempfile
p = argparse.ArgumentParser()
p.add_argument('--cc', default='gcc')
args = p.parse_args()
sdk = Path(__file__).resolve().parents[2]
s = (sdk/'components/wireless/bluetooth/blestack/src/hci_onchip/hci_driver.c').read_text(encoding='utf-8')
a=s.index('static int hci_driver_send(')
z=s.index('\n#if defined(CONFIG_BT_HOST_HCI_TL)',a)
body=s[a:z]
fixture=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8_t;
#define EINVAL 22
#define BT_DBG(...) ((void)0)
#define BT_ERR(...) ((void)0)
#define BT_BUF_ACL_OUT 1
#define BT_BUF_CMD 2
struct net_buf { unsigned len,refs,type; };
static int result,calls;
static void net_buf_unref(struct net_buf *b) { assert(b->refs); --b->refs; }
static int backend(struct net_buf *b) { (void)b; ++calls; return result; }
#define bt_buf_get_type(b) ((b)->type)
#define bflb_hci_send backend
#define bl_hci_send backend
#define bl_onchiphci_send_2_controller backend
#define acl_handle backend
#define cmd_handle backend
/* DRIVER */
int main(void) {
    for(unsigned kind=1;kind<=2;++kind) {
        for(unsigned fail=0;fail<2;++fail) {
            struct net_buf b={8,kind==2?2:1,kind};
            unsigned before=b.refs; result=fail?-5:0; calls=0;
            assert(hci_driver_send(&b)==result && calls==1);
            assert(b.refs==(fail?before:before-1));
            if(fail) net_buf_unref(&b); /* caller rollback */
            if(kind==2) net_buf_unref(&b); /* command pending reference */
            assert(!b.refs);
        }
    }
    struct net_buf empty={0,1,1};calls=0;
    assert(hci_driver_send(&empty)==-EINVAL && !calls && empty.refs==1);
    net_buf_unref(&empty);
    puts("PASS HCI driver: success consumes once; failure retains; ACL/command/empty ownership");
}
'''
with tempfile.TemporaryDirectory(prefix='hci-owner-') as tmp:
    d=Path(tmp);(d/'test.c').write_text(fixture.replace('/* DRIVER */',body),encoding='utf-8')
    modes=[['BFLB_BLE'],['BFLB_BLE','CONFIG_BT_HOST_HCI_TL'],['BFLB_BLE','CONFIG_BT_HOST_HCI_TL','CONFIG_BT_HOST_HCI'],['CONFIG_BT_CONN']]
    for i,mode in enumerate(modes):
        exe=d/f'test{i}.exe'
        subprocess.run([args.cc,'-std=c11','-Wall','-Wextra','-Werror',*['-D'+v for v in mode],str(d/'test.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
