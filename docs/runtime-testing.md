# Runtime testing and diagnostics (noled_loader.vpk)

The loader app is the development harness used for the PCH-2000 power LED research. End
users do not need it - the boot plugin (`noled.skprx`) is self-contained. Keep the loader
around for probing syscon LED behavior on new hardware revisions or firmware, or for
testing changes without touching the boot config.

Install `build/noled_loader.vpk` with VitaShell. The app bundles the current
`noled.skprx`, copies it to `ux0:data/noled/noled.skprx`, and loads it only for the
current session (the kernel module stays resident until reboot).

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
