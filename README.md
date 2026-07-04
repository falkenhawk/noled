# noled

noled is a simple plugin to turn off PS Vita status LEDs. Especially useful at night.

> [!IMPORTANT]
> ## NEW: the Vita 2000 power LED can finally be turned off
>
> This fork adds the **first known software control of the PCH-2000 green power LED** -
> requested in [rereprep/noled#3](https://github.com/rereprep/noled/issues/3) since 2018,
> unsolved until now. Fully automatic, no configuration:
>
> - power LED **dark** whenever the console is on
> - **orange charging indication preserved** - shows while actually charging, also in standby
> - re-applied by itself ~2 s after every wake; the standby blink sequence is gone too
> - Vita 1000 autodetected and keeps the classic behavior; removal restores everything stock
>
> Download `noled.skprx` from [**Releases**](https://github.com/falkenhawk/noled/releases)
> and see [Install](#install-boot-plugin) below.

The LED is not a SoC GPIO - it hangs off the syscon MCU ("Ernie") and is driven by its
autonomous power/charge policy, which is why every GPIO-based approach failed for years. The
off switch is `ksceSysconCtrlLedBlinkType2` (syscon command `0x89F`): an off-pattern for
device 1 parks the indicator dark, and claiming the engine's single pattern slot for device 0
releases it back to syscon policy. The full story - syscon LED command reference, the power
LED state model, hardware-verified results and the dead ends - is in
[`docs/pch2000-power-led-research.md`](docs/pch2000-power-led-research.md); the other files in
`docs/` are the raw session-by-session research trail.

https://youtu.be/YbxpdJ67VgE

## Install (boot plugin)

Copy `noled.skprx` to your tai folder (`ur0:tai/` recommended - internal memory, works
without a memory card; `ux0:tai/` works too) and add it to the `*KERNEL` section of your
taiHEN `config.txt`:

```
*KERNEL
ur0:tai/noled.skprx
```

Note: taiHEN reads `ur0:tai/config.txt` only when `ux0:tai/config.txt` does not exist - add
the line to whichever config is active on your system. Reboot afterwards.

No configuration file is needed. On PCH-2000 the plugin optionally honors
`ux0:data/noled/led_candidate.bin` (written by the diagnostic app): a missing file or mode 14
means the power LED policy, an explicit mode 0 restores stock power LED behavior while keeping
the classic LED blocking.

## Build

Install VitaSDK, then run:

```
cmake -S . -B build -DRELEASE=1
cmake --build build -j8
```

Outputs:

- `build/noled.skprx` - the kernel plugin (this is all end users need)
- `build/noled_loader.vpk` - development/diagnostic harness, see
  [`docs/runtime-testing.md`](docs/runtime-testing.md)

## Credits

All credits to @xerpi for the original plugin. The PCH-2000 power LED research and
implementation were done in this fork - see the
[research write-up](docs/pch2000-power-led-research.md) for the sources it builds on.

## Known issues

After resuming from standby, the power LED shows green for about 2 seconds before the policy
re-asserts and it goes dark (deliberate: the plugin lets the OS finish its resume LED handling
first, and it doubles as a wake indicator).
