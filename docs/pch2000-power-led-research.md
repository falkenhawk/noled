# Turning Off the PS Vita PCH-2000 Power LED in Software

A study of the Vita's syscon LED control interface, with the first known working method
for suppressing the PCH-2000 green power indicator.

Date: 2026-07-05. Hardware under test: PS Vita Slim (PCH-2000 series), firmware 3.60+
with HENkaku/taiHEN. All results below are from live hardware experiments unless marked
as sourced from static reverse engineering.

## Abstract

The PCH-2000 green/orange power indicator is not connected to a SoC GPIO and cannot be
controlled by any of the previously used LED-blocking techniques. It hangs off the system
controller MCU ("Ernie"/syscon), which drives it autonomously from its own power/charge
policy. We map the complete OS-visible syscon LED command surface on PCH-2000 hardware
and identify `sceSysconCtrlLedBlinkType2` (syscon command `0x89F`) as the control that
reaches the green die: an off-pattern (`device 1, on_time 0, off_time 1`) parks the LED
dark, and the engine's single pattern slot provides a release mechanism (claim the slot
for device 0) that restores the stock policy, including orange-while-charging. A small
kernel plugin policy built on these two commands keeps the LED dark when idle and orange
while charging, surviving suspend/resume. To our knowledge no prior public software
controlled this LED (upstream request open since 2018).

## 1. Background and prior work

The original noled plugin (xerpi/rereprep) disables the PS button LED and game card LED
by clearing SoC GPIO bus 0 ports 7 and 6 and hooking `ksceGpioPortSet` so the OS cannot
re-enable them. This works on all models but does not affect the PCH-2000 power LED; the
gap was reported in 2018 ([rereprep/noled#3](https://github.com/rereprep/noled/issues/3))
and never solved. A commenter on
that issue swept the GPIO space exhaustively and only managed to turn off the backlight,
which is consistent with the known GPIO map (vitasdk `psp2kern/lowio/gpio.h` defines only
`LED_GAMECARD` = port 6 and `LED_PS_BUTTON` = port 7; SKGleba's baremetal projects use
the same two plus devkit GPOs).

The decisive physical observation: the PCH-2000 indicator works while the console is
fully powered off (orange while charging). Only syscon is alive in that state, so the
LED must be driven by syscon, out of reach of any SoC GPIO manipulation.

One prior public data point existed for the syscon path: ModernBT-Vita flashes the
"charge LED on retail Vita-1000/2000" with `ksceSysconCtrlLED(0, enable)`.

## 2. LED topology on PCH-2000

Three externally visible indicator elements were distinguished experimentally:

- the power/charge indicator: one light pipe fed by a bicolor package - a green die
  (powered on) and an orange die (charging). Colors swap in place.
- a blue notification LED adjacent to it.
- (the PS button and game card LEDs of the PCH-1000 remain GPIO-driven and are handled
  by the classic noled mechanism.)

## 3. Methodology

Earlier attempts (documented in the sibling files in `docs/`) mutated syscon packets in
flight during suspend and probed exported functions in unsupervised loops; both
approaches caused instability whose root cause was misattributed for a long time. The
key methodology change was a runtime probe harness (loader VPK + kernel module, modes
13/15 in this repo): the kernel side executes exactly one whitelisted command per
button press, logs a synced "try" line before executing (freeze forensics), and the
tester observes the physical LED after every single command. No hooks, no packet
mutation, no loops. All findings below were obtained this way, cross-checked against
static RE of the decompiled 3.60 SceSyscon module (emu-russia/VitaTestSuite).

A critical correction that unblocked the work: syscon command `0x387`, previously
believed to be an LED command, is in the touchpanel range (`0x380` =
`sceSysconGetTouchpanelDeviceInfo`). Mutating `0x387` packets corrupts touchpanel
suspend configuration - the "LED experiments break touch wake" symptom was entirely
self-inflicted. Do not touch `0x387`.

## 4. Syscon LED command reference (PCH-2000, hardware-verified)

Packet basics (wiki.henkaku.xyz, confirmed by decomp): tx layout is
`[cmd_lo, cmd_hi, len, data..., checksum]` where `len` counts data plus the checksum
byte; the checksum (binary negation of the byte sum) is computed by the SceSyscon
driver, not the caller. Syscon returns errors as `0x80250200 | rx[3]`.

### 4.1 `0x891` - `sceSysconCtrlLED(led, enable)`

Decomp: one payload byte, `led | (enable ? 0x80 : 0)`.

| led id | result on PCH-2000 |
| --- | --- |
| 0 | charge (orange) die; publicly exercised by ModernBT-Vita; no visible effect while unplugged |
| 1, 2, 3 | accepted (`ret 0`), no visible effect on any LED |
| 0x40 | the blue notification LED, on/off works |
| 4-7 | never re-tested individually; a historical unsupervised `0..7` sweep produced black screen and poweroff, left unattributed |

The green die is not addressable through `0x891`.

### 4.2 `0x820`-`0x822` - `sceSysconCtrlDolceLED(state)`

Decomp: command number is `0x820 + state`, empty payload, wrapper rejects `state > 2`.
The vitasdk name ("Dolce" = PSTV) is misleading: on PCH-2000 this family drives the
green power LED.

| state | command | effect on PCH-2000 |
| --- | --- | --- |
| 0 | `0x820` | steady on (restore) |
| 1 | `0x821` | green blinks slowly |
| 2 | `0x822` | green blinks fast (this is what the OS sends at standby entry) |
| 3 | `0x823` | does not exist: wrapper rejects it, and raw injection is refused by Ernie with error `0x3F` (`0x8025023F`) |

There is no off state in this family.

### 4.3 `0x89D` - `sceSysconCtrlLedPwmBlink(device, value)`

Decomp: wrapper rejects any `device != 0x40`; payload is `[0x01, value_lo, value_hi]`
(the device argument is never transmitted). Hardware: this is a true hardware PWM for
the blue LED only. Observed value behavior: `0x0040`/`0x0080`/`0x00FF` = blue on (no
obvious brightness steps), `0x0100` = blue off, `0x1000`/`0x8000` = nothing visible,
`0xFFFF` = syscon error `0x03`. The green die has no PWM channel: flicker-free green
dimming is not achievable with any known command.

### 4.4 `0x89E` - `sceSysconSetChargeLedCtrl(state)`

Decomp: two payload bytes; gated on `sceSysconGetHardwareInfo() & 0xFF0000 >= 0x400000`
(older boards lack the hardware). Hardware: no visible effect on the PCH-2000 indicator
in any tested state, plugged or unplugged. Its gate is however the correct model
discriminator for "this board has the bicolor status LED" and is reused by our plugin.

### 4.5 `0x89F` - `sceSysconCtrlLedBlinkType2(dev, unk, on_time, off_time)` - the control

Decomp: payload `[dev u8, unk u8, on u16 LE, off u16 LE]`, and the wrapper performs no
validation of `dev`. Hardware results:

| dev | effect |
| --- | --- |
| 1 | the green power die. `on=0, off=1` = LED dark; `on=1, off=0` = forced steady green; intermediate ratios blink with a coarse time base (unit in the tens of ms - "dimming" via duty is visibly flickery) |
| 0 | drives nothing visible, but a pattern set on it occupies the engine's slot (see below) |
| 2, 3 | accepted (`ret 0`), drive nothing visible, do NOT claim the slot from dev 1 |
| 0x40 | accepted, no visible effect (the `0x89F` device namespace is distinct from `0x891`) |

`unk` made no observable difference (0, 1, 2 tested).

### 4.6 SceLed / `ksceLedSetMode(led, mode, config)` (ScePower's `SceLedForDriver`)

| led | result on PCH-2000 |
| --- | --- |
| 0, 1, 3 | accepted (`ret 0`), no visible output |
| 2 | the blue notification LED: mode 0 = off, mode 1 = on, and the `SceLedConfiguration` on/off times act as a duty cycle (`0/1` = off, `1/1` dim, `9/1` bright) |
| 4-7 | invalid: error `0x803F0140` |

## 5. The power LED state model

Two independent state layers sit above the base policy, with different reset semantics:

1. Base policy (Ernie firmware): green when the SoC is on, orange while charging,
   off in standby (orange if charging). Runs even with the SoC off.
2. DolceLED state (`0x820`-`0x822`): a commanded steady/blink state. Survives an OS
   soft reboot (syscon does not reset with the SoC); cleared by a suspend/resume cycle.
3. BlinkType2 pattern: a single-slot pattern engine layered on top. While any pattern
   occupies the slot, the indicator is in manual mode: DolceLED commands are ignored
   and the base policy (including charger orange) does not reach the LED. Cleared by
   suspend/resume AND by a soft reboot.

The single slot is the release mechanism: setting a pattern for dev 0 (which drives
nothing) evicts a dev 1 pattern and drops the indicator back to the base policy. This
was verified live: dark indicator + charger + dev 0 pattern = orange appears; unplug =
switches to green; re-assert dev 1 off-pattern = dark again while charging.

Additional observations: with the off-pattern active the OS-commanded standby blink is
suppressed (a single final blink remains, produced inside syscon during the power
transition, below anything commandable); the boot-time green comes from the base policy
before any OS code runs, so a plugin must assert the pattern at boot and after resume.

## 6. The resulting plugin

The production policy (this repo, `main.c`, boot-capable kernel plugin):

- model gate: policy runs only when `ksceSysconGetHardwareInfo() & 0xFF0000 >= 0x400000`
  (Sony's own charge-LED hardware test). Other boards keep the classic GPIO behavior.
- not charging: assert `ksceSysconCtrlLedBlinkType2(1, 0, 0, 1)` - indicator dark.
- actually charging: assert `ksceSysconCtrlLedBlinkType2(0, 0, 1, 0)` - slot released,
  base policy paints orange; charge-state changes are debounced over two samples.
- re-assert ~1 s after every resume (sysevent handler) plus a 10 s idempotent heartbeat.
- the classic GPIO LED blocking (`ksceGpioPortClear` + `ksceGpioPortSet` hook for bus 0
  ports 6/7) is kept on all models.

Nothing persists across a reboot; removing the plugin restores fully stock behavior.

## 7. Dead ends (so nobody repeats them)

- GPIO: the power LED has no GPIO port; exhaustive sweeps only kill the backlight.
- `0x387` mutation: touchpanel command; breaks touch wake, never affects LEDs.
- `ksceSysconCtrlLED` id sweeps: ids are not sequential LED indices; unsupervised
  sweeps crashed the console; the green die is not in this namespace anyway.
- DolceLED off state / raw `0x823`: does not exist (Ernie error `0x3F`).
- flicker-free green dimming: no PWM channel exists for the green die; BlinkType2 duty
  "dimming" flickers visibly at its coarse time base.
- forcing orange manually: the orange die is not addressable by any tested command;
  orange is exclusively base-policy behavior (hence the release-based design).

## 8. Open questions

- the `unk` byte of `0x89F` (no observed effect) and the exact time base of its blink
  engine.
- identity of `0x89F` devices 0, 2, 3 and the `0x89D` value format (bitfield?).
- whether the same commands drive the PSTV white LED (likely, given the shared wrapper)
  and PCH-1000 behavior of `0x89F` dev 1 - untested; the plugin's model gate keeps it
  unsent there.
- the source of the single residual blink during the standby transition.

## 9. Sources

- live hardware probing on PCH-2000 (this repo: probe harness in `loader/` and
  `runtime_probe.h`, session-by-session results in the other `docs/` files)
- Vita Development Wiki: [Ernie](https://wiki.henkaku.xyz/vita/Ernie) (packet format,
  checksum), [SceSyscon](https://wiki.henkaku.xyz/vita/SceSyscon) (command table, NIDs,
  wrapper prototypes), [ScePower](https://wiki.henkaku.xyz/vita/ScePower)
  (`sceLedSetModeForDriver`, `SceLedConfiguration`)
- decompiled 3.60 SceSyscon module:
  [emu-russia/VitaTestSuite `Syscon/SysconElf_360.cpp`](https://github.com/emu-russia/VitaTestSuite/blob/master/Syscon/SysconElf_360.cpp)
  (payload construction of every wrapper referenced above)
- [Brendonm17/ModernBT-Vita `src/notify.c`](https://github.com/Brendonm17/ModernBT-Vita/blob/main/src/notify.c)
  (`ksceSysconCtrlLED(0, x)` documented as the retail Vita 1000/2000 charge LED)
- [SonicMastr/libraries `include/kernel/syscon.h`](https://github.com/SonicMastr/libraries/blob/master/include/kernel/syscon.h)
  (independent documentation of the DolceLED states and CtrlLED on/off contract)
- [vitasdk/vita-headers](https://github.com/vitasdk/vita-headers) (kernel headers and the
  NID database, incl.
  [`psp2kern/lowio/gpio.h`](https://github.com/vitasdk/vita-headers/blob/master/include/psp2kern/lowio/gpio.h)
  with the only two LED GPIO ports);
  [SKGleba/psp2ref `hardware/gpio.h`](https://github.com/SKGleba/psp2ref/blob/master/hardware/gpio.h)
  and SKGleba's baremetal projects (independent GPIO LED map)
- [rereprep/noled](https://github.com/rereprep/noled) - the original GPIO mechanism this
  fork extends - and [issue #3](https://github.com/rereprep/noled/issues/3), the 2018
  PCH-2000 power LED request this work answers
