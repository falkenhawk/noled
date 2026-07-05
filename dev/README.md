# dev harness - runtime testing and diagnostics

Everything under `dev/` is the development harness used for the PCH-2000 power LED
research (see [`docs/pch2000-power-led-research.md`](../docs/pch2000-power-led-research.md)):

- `main.c` - diagnostic build of the plugin (`noled_dev.skprx`): syscon tracer, runtime
  probe, GPIO sampler, all config modes
- `loader/` - VPK app that pairs with it: writes the mode config, loads the plugin at
  runtime, drives the one-command-per-press probe
- `cmd_probe.c` - minimal standalone syscon probe module (`noled_cmd_probe.skprx`),
  predecessor of the loader-driven probe

End users need none of it - the release plugin (`noled.skprx`, built from the top-level
`main.c`) is self-contained and contains no diagnostic code, reads no config file and
writes no logs. GitHub releases ship only `noled.skprx`; the harness is build-it-yourself.
Keep it around for probing syscon LED behavior on new hardware revisions or firmware.

## Build

The harness targets exist in the top-level CMake project but are not part of the default
build - request them explicitly:

```
cmake -S . -B build -DRELEASE=1
cmake --build build --target noled_dev.skprx noled_loader.vpk
cmake --build build --target noled_cmd_probe.skprx   # optional, standalone probe
```

Install `build/noled_loader.vpk` with VitaShell. The app bundles the diagnostic plugin
(packed inside the vpk under the expected name `noled.skprx`), copies it to
`ux0:data/noled/noled.skprx`, and loads it only for the current session (the kernel
module stays resident until reboot).

Remove the release plugin from the taiHEN config (and reboot) before a diagnostic
session: its power LED policy thread re-asserts the LED state every 10 seconds and would
fight the probe results.

## Modes

Selected with Left/Right before pressing Cross:

| mode | purpose |
| --- | --- |
| 13 (default) | runtime LED probe: one whitelisted syscon/LED command per button press |
| 14 | production power LED policy, end-to-end test of the release behavior |
| 15 | probe combined with the pass-through syscon tracer (capture while probing) |
| 12 | passive syscon caller-map trace |
| 10 | read-only GPIO sampler |

## Controls

- Cross (before load): commit the selected mode to the config file, copy and load the
  plugin
- mode 13/15 after load: Left/Right pick an LED candidate, Cross sends exactly one
  command - then look at the LED; the kernel response renders on screen
- mode 14 after load: no interaction; the screen describes the expected LED state
- Square/Triangle/Circle/Up/Down/Select: optional event markers written to the log
- Start: exit; rewrites the config to mode 14 so a diagnostic mode never leaks into the
  next boot; the loaded kernel module stays resident until reboot

## Safety discipline (learned the hard way, see the research doc)

- one command per press, observe the LED after every single one
- never sweep id/device ranges in loops
- never touch syscon command `0x387` (touchpanel - mutating it breaks touch wake)
- anything that goes wrong is fully reset by a reboot; nothing persists

## Logs

- `ux0:data/noled/loader.log` - loader-side events
- `ux0:data/noled/probe.log` - kernel probe/policy log; the `try` line is synced before
  each command executes, so it survives a freeze and identifies the culprit
- `ux0:data/noled/syscon_trace.log`, `module_map.log`, `gpio_sample.log` - tracer modes

Fetch over VitaShell FTP, reboot to unload the resident module.

## Host tests

The header logic (probe request validation, candidate patching, policy predicates, the
hardware-validated power LED constants) is covered by host-compilable tests:

```
for t in tests/test_*.c; do
  cc -Wall -Wextra -Werror -I. -o /tmp/noled_test "$t" && /tmp/noled_test || echo "FAIL: $t"
done
```
