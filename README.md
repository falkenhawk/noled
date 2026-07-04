# noled
noled is a simple plugin to turn off PS Vita status LEDs. Especially useful at night. With the release of v1.2, it disables both the Home Button LED and Game Cartridge LED.

This fork additionally turns off the **Vita Slim / PCH-2000 green power LED** - the first known software solution (upstream issue rereprep/noled#3, open since 2018). The power LED is not a SoC GPIO: it hangs off the syscon MCU ("Ernie") and is driven by its autonomous policy. The off switch found by hardware probing is `ksceSysconCtrlLedBlinkType2` (syscon command `0x89F`): device 1 with on/off times 0/1 parks the indicator dark, and claiming the single pattern slot for device 0 releases it back to syscon policy. The plugin runs a small policy on top:

- power LED **dark** whenever the console is on and not charging
- **orange** as usual while actually charging (policy released)
- re-asserted automatically ~2 s after every wake and at boot
- standby blink sequence is suppressed too (a single final blink remains, generated inside syscon)
- charging indication while powered off or in standby is untouched

The full write-up of how the LED works and how the off switch was found is in
[`docs/pch2000-power-led-research.md`](docs/pch2000-power-led-research.md) - syscon LED command
reference, the power LED state model, hardware-verified results and the dead ends. The other
files in `docs/` are the raw session-by-session research trail.

**To use the plugin you only need `noled.skprx`** (see Boot install below). The
`noled_loader.vpk` app is a development/diagnostic harness - it was used to probe syscon live
during the research and is not needed for normal use.

https://youtu.be/YbxpdJ67VgE

## Build

Install VitaSDK, then run:

```
cmake -S . -B build -DRELEASE=1
cmake --build build -j8
```

Outputs:

- `build/noled.skprx` - kernel plugin for taiHEN boot config
- `build/noled_loader.vpk` - safer runtime test app

## Safer runtime testing

Install `build/noled_loader.vpk` with VitaShell before adding the plugin to `*KERNEL`. The app includes the current `noled.skprx`, copies it to `ux0:data/noled/noled.skprx`, and loads it only for the current session.

Controls:

- Cross: copy and load/start the plugin
- Left/Right before Cross: switch between the runtime LED probe (default), caller-map trace, and read-only GPIO sampler
- in mode 13 after loading: Left/Right pick an LED candidate, Cross sends exactly one command, then watch the LED
- Square/Triangle/Circle/Up/Down/Select: optional event markers
- Start: exit the app; the runtime-loaded plugin stays resident until reboot

Do not put the experimental tracer in the taiHEN boot config. For the current caller-map run, load it, perform charger plug/unplug and optional standby/wake, return to VitaShell, then fetch `ux0:data/noled/loader.log`, `ux0:data/noled/syscon_trace.log`, and `ux0:data/noled/module_map.log`. Reboot after fetching logs to unload it.

## Boot install

It is a kernel plugin so enable it in kernel section of your config.txt.

copy noled.skprx to tai folder (ux0:/tai)
edit config.txt:

*KERNEL

ux0:tai/noled.skprx

after that, reboot your vita and restart henkaku.

Boot behavior on PCH-2000: kernel plugins load before `ux0:` is mounted, so the plugin cannot read its config at start. It goes dark as early as syscon allows, then re-reads `ux0:data/noled/led_candidate.bin` once the card mounts: a missing config or any loader mode keeps the power LED policy on; an explicit mode 0 config restores stock power LED behavior (classic GPIO LED blocking only).

All credits to @xerpi, all i did was to compile the source.

A known issue is, after resuming from standby, the power LED shows green for about 2 seconds before the policy re-asserts and it goes dark.
