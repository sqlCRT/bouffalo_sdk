"""Run the production parser with host-side buffers and channel callbacks.

Usage: python run_l2cap_br_tests.py --cc gcc
Also accepts a Windows TinyCC executable. Hardware/RTOS behavior is not mocked
as a claimed integration test; this exercises packet boundaries and IDs only.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

here = Path(__file__).resolve().parent
sdk = here.parents[1]
args = argparse.ArgumentParser()
args.add_argument('--cc', default='gcc')
args.add_argument('--source', type=Path, help='Optional source snapshot for regression comparison')
options = args.parse_args()
cc = options.cc
source = (options.source or sdk / 'components/wireless/bluetooth/blestack/src/host/l2cap_br.c').read_text(encoding='utf-8')

def extract(name):
    match = re.search(r'^static [^;{}]*\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    if not match:
        raise RuntimeError('Cannot extract ' + name)
    pos = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[match.start():pos]

functions = ['l2cap_br_info_rsp', 'l2cap_br_conf_opt_mtu', 'l2cap_br_conf_req',
             'l2cap_br_conf_rsp', 'l2cap_br_recv', 'l2cap_br_conn_req_reply']
unit = ('/* Copyright (c) 2016 Intel Corporation; SPDX-License-Identifier: Apache-2.0 */\n'
        + (here / 'l2cap_br_stubs.c').read_text(encoding='utf-8') + '\n'
        + '\n'.join(extract(name) for name in functions) + '\n'
        + (here / 'test_l2cap_br.c').read_text(encoding='utf-8'))
with tempfile.TemporaryDirectory(prefix='l2cap-br-test-') as tmp:
    path = Path(tmp)
    (path / 'test.c').write_text(unit, encoding='utf-8')
    include = sdk / 'components/wireless/bluetooth/blestack/src/include/bluetooth'
    subprocess.run([cc, '-std=c11', '-Wall', '-I' + str(include), str(path / 'test.c'),
                    '-o', str(path / 'test.exe')], check=True)
    subprocess.run([str(path / 'test.exe')], check=True)
