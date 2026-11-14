"""Run actual Bluetooth deletion paths against real LittleFS/KV on emulated flash."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--cc', default='gcc')
p.add_argument('--case', choices=('all', 'settings', 'keys'), default='all')
a = p.parse_args()
sdk = Path(__file__).resolve().parents[2]
fs = sdk / 'components/fs/littlefs'
host = sdk / 'components/wireless/bluetooth/blestack/src/host'


def function(path, name):
    source = path.read_text(encoding='utf-8')
    match = re.search(r'^(?:int|void) ' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    pos, depth = source.index('{', match.start()) + 1, 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[match.start():pos]


# Reuse the SDK's flash emulator (only the hardware boundary is stubbed).
fixture = (fs / 'tests/test_kv.c').read_text(encoding='utf-8').split('int main(void)')[0]
fixture += r'''
#define BFLB_BLE 1
#define CONFIG_BT_SETTINGS 1
#define BT_KEYS_IRK 1
#define BT_DBG(...) ((void)0)
struct bt_keys { unsigned keys; unsigned char data[32]; };
static unsigned irk_deletions;
static void bt_id_del(struct bt_keys *keys) {
    assert(keys->keys & BT_KEYS_IRK);
    ++irk_deletions;
}
'''
fixture += re.search(r'^#define NV_KEY_POOL\s+.*$', (host / 'settings.h').read_text(encoding='utf-8'), re.M)[0] + '\n'
fixture += function(host / 'settings.c', 'settings_delete') + '\n'
fixture += function(host / 'keys.c', 'bt_keys_clear') + '\n'
fixture += r'''
static void assert_absent(const char *key) {
    unsigned char value[256];
    size_t saved = 123;
    assert(lfs_kv_get_blob(key, value, sizeof(value), &saved) == 0);
    assert(saved == 0);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    assert(lfs_kv_init() == LFS_KV_OK);
    assert(lfs_kv_set_blob("unrelated", "keep", 5) == LFS_KV_OK);
    if (argv[1][0] != 'k') {
        for (unsigned cycle = 0; cycle < 50; ++cycle) {
            assert(lfs_kv_set_blob("bt_blacklist", "blocked", 8) == LFS_KV_OK);
            size_t saved = 0;
            /* A size query returns zero copied bytes even when the key exists. */
            assert(lfs_kv_get_blob("bt_blacklist", NULL, 0, &saved) == 0);
            assert(saved == 8);
            assert(settings_delete("bt_blacklist") == LFS_KV_OK);
            remount();
            assert_absent("bt_blacklist");
            assert(settings_delete("bt_blacklist") == LFS_KV_OK);
            assert_value("unrelated", "keep", 5);
        }
        assert(lfs_kv_set_blob("bt_blacklist", "blocked", 8) == LFS_KV_OK);
        sync_error = LFS_ERR_IO;
        assert(settings_delete("bt_blacklist") == LFS_KV_ERR_WRITE);
        remount();
        assert(settings_delete("bt_blacklist") == LFS_KV_OK);
        puts("PASS settings deletion persists across remount, is idempotent, and reports flash errors");
    }
    if (argv[1][0] != 's') {
        for (unsigned cycle = 0; cycle < 50; ++cycle) {
            unsigned char synthetic_key[192];
            memset(synthetic_key, cycle, sizeof(synthetic_key));
            assert(lfs_kv_set_blob(NV_KEY_POOL, synthetic_key, sizeof(synthetic_key)) == LFS_KV_OK);
            struct bt_keys keys = { .keys = BT_KEYS_IRK };
            memset(keys.data, 0x5a, sizeof(keys.data));
            bt_keys_clear(&keys);
            struct bt_keys zero = {0};
            assert(memcmp(&keys, &zero, sizeof(keys)) == 0);
            assert(irk_deletions == cycle + 1);
            remount();
            assert_absent(NV_KEY_POOL);
            bt_keys_clear(&keys);
            assert(irk_deletions == cycle + 1);
            assert_value("unrelated", "keep", 5);
        }
        puts("PASS key-pool deletion survives remount; RAM clearing and IRK callbacks retained");
    }
    assert(lfs_unmount(mounted) == 0);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='bt-settings-') as td:
    d = Path(td)
    source = d / 'test.c'
    source.write_text(fixture, encoding='utf-8')
    exe = d / 'test.exe'
    subprocess.run([a.cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Wno-sign-compare',
                    '-DLFS_NO_DEBUG', '-DLFS_NO_WARN', '-DLFS_NO_ERROR',
                    '-I' + str(fs / 'tests/stubs'), '-I' + str(fs / 'littlefs'),
                    '-I' + str(fs / 'port'), '-I' + str(fs / 'lfs_kv'),
                    str(source), str(fs / 'lfs_kv/lfs_kv.c'), str(fs / 'littlefs/lfs.c'),
                    str(fs / 'littlefs/lfs_util.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe), a.case], check=True, timeout=30)
