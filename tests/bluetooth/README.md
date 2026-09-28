# BR L2CAP reconnection protocol regression

Restores only the protocol fixes from 7124faa0, previously withdrawn by
9bd72806 during the audio investigation:

- Bound each signalling command to its declared payload, then advance to the
  next command independently of how many bytes its handler consumed.
- Check successful Information Response payload sizes before pulling masks.
- Keep the incoming transaction identifier across Pending responses until a
  final response is sent.

The former trace hooks, send wrapper and disconnect-cleanup notification are
not restored. No audio/HID data-channel path, scheduler, heap allocation or
retry policy is changed. These fixes do not establish that every intermittent
DualSense reconnect failure has the same cause.

Run `python tests/bluetooth/run_l2cap_br_tests.py --cc <host-gcc>`.
The harness extracts the actual SDK parser/configuration/response functions;
buffers, RTOS and connection callbacks are host stubs. It covers separate and
coalesced requests/responses, both command orders, MTU options, short masks,
truncated options, oversized lengths, invalid identifiers, trailing fragments
and Pending/final identifier preservation. It is not a hardware integration test.

Use `--source <l2cap_br.c snapshot>` to check that the same cases detect a
regression in an earlier SDK version. Hardware confirmation still requires
repeated short-PS reconnects plus simultaneous audio/input checks.
