# Current Next Steps

This file should be updated after every hardware test.

## 2026-07-05 Mode 14 VALIDATED + v8 Boot Support (awaiting upload)

Mode 14 (v7) passed the full hardware checklist:

- dark within ~2s of load; orange within ~2s of charger attach; dark
  within ~2s of unplug,
- standby/wake in both states re-establishes the correct state within
  ~2s of wake (green visible for up to ~2s after wake = the deliberate
  resume settle delay),
- while charging in standby the LED stays orange (syscon policy, we
  released it) - desired charge indication,
- the standby blink sequence is suppressed; one single final blink
  remains, generated inside syscon during the power transition, below
  anything commandable. Accepted as-is.
- no side effects during normal play.

v8 adds boot-plugin support plus universality (final build: VPK SHA-256
`94f3d63ab7777c6280226428eebab24c0c56052058dca8968739bead50191599`,
SKPRX `f4d296374d9fc52b5f3f8d7bd8195bc866def19ff19875092d9737ec64765edb`,
NOT yet uploaded - FTP down):

- led_candidate.bin is now fully OPTIONAL: unreadable/missing config at
  module_start starts the complete charger-aware policy immediately (no
  config-wait window); the policy loop lazily re-checks for ~60s and only
  an explicit mode-0 config releases back to classic-only.

- model autodetect: the power LED policy only runs when
  ksceSysconGetHardwareInfo() & 0xFF0000 >= 0x400000 - the same gate
  Sony's charge LED code uses for "this board has the bicolor status
  LED". PCH-1000 gets exactly the classic GPIO-only behavior.
- fixed a regression: the probe/policy/boot paths now install the
  original ksceGpioPortSet guard hook again, so the home button and game
  card LEDs cannot be re-lit by the OS on any model.
- loader config hygiene: the mode config is written only when a session
  is committed with Cross, and rewritten to mode 14 on Start-exit, so a
  stale diagnostic mode can never leak into the next boot.

- module_start detects boot context by the config file being unreadable
  (ux0 not mounted yet), applies the classic GPIO blocking, and starts a
  boot thread that goes dark immediately after syscon init, then re-reads
  the config for up to 120s once ux0 mounts:
  - explicit mode 0 config -> release to stock policy, classic-only,
  - any other mode or missing config -> run the charger-aware policy.
- boot install: copy build/noled.skprx to ux0:tai/, add under *KERNEL in
  config.txt. Runtime loader flow unchanged (mode 14 via VPK still works).

v8 boot test protocol:

1. install VPK (for the matching runtime loader), copy the NEW
   build/noled.skprx to ux0:tai/, add to config.txt under *KERNEL, reboot.
2. LED must go dark within a few seconds of the boot logo.
3. charger/standby checks as in the mode-14 checklist.
4. removal test: delete the config.txt line, reboot -> stock behavior.

## 2026-07-05 RELEASE FOUND + Charger-Aware Policy (v7, awaiting upload)

v6 mode-13 session results:

- PwmBlink (0x89D) drives the BLUE LED, not green: 0x0040/0x0080/0x00FF =
  blue on (no visible brightness difference), 0x0100 = blue off,
  0x1000/0x8000 = nothing, 0xFFFF = syscon error 0x8025 0203. Flicker-free
  green dimming does not exist in the known command set - dimming is
  closed for good.
- THE RELEASE: the BT2 pattern engine has a SINGLE SLOT. BT2 dev0 (1,0)
  evicts the dev1 off pattern and returns the indicator to syscon policy.
  dev2/dev3 do not (invalid devices, slot untouched).
- charger validation, all confirmed on hardware: dark + charger + release
  = ORANGE appears; unplug = switches to green (policy fully alive);
  re-assert dev1 (0,1) while charging = dark again.

Full control set, hardware-proven:

- dark:    ksceSysconCtrlLedBlinkType2(1, 0, 0, 1)
- release: ksceSysconCtrlLedBlinkType2(0, 0, 1, 0) -> syscon policy
  (green idle / orange charging)

v7 build (uploaded 2026-07-05, downloaded back and byte-compared identical;
VPK SHA-256
`2aa3a09149a2ff5cac9a977b68bf26ad7f68d15648525f4c4617045be2b0016a`,
SKPRX `8d976a376feb6f2ec4ea478f7f8ba56f736cba3452a65bc5e1d6f030ab92fd7c`):
KERNEL CHANGED - install VPK and REBOOT before testing.

- mode 14 is now the charger-aware production policy: dark when idle,
  release (orange) while actually charging, two-sample debounce on the
  charge state, re-apply ~1s after resume, 10s idempotent heartbeat.
  Full battery + charger attached counts as not charging = dark.
- mode 14 test protocol: load, LED dark in ~2s; plug charger -> orange in
  ~2s; unplug -> dark; standby/wake in both states -> correct state
  within ~2s of wake; play a while -> stays dark, no side effects.

After mode-14 validation the remaining work is the boot-plugin packaging
(config read retry for early boot when ux0 is not yet mounted, README,
release build).

## 2026-07-04 v5 Session + v6 Build (uploaded)

v5 session results:

- BT2 dev 0, dev 2, dev 3 force-on (1,0): NO visible effect, charger
  unplugged. The orange die is NOT addressable through BT2 devices; orange
  exists only in syscon's automatic policy. dev 1 unk=2 (1,0) also showed
  no visible difference from plain green.
- consequence: "orange while charging while powered on" cannot be rendered
  manually. Remaining hope for charger-aware behavior is finding a RELEASE
  command that returns the indicator to policy mode (wake does it; the
  mechanism is unknown).

v6 build (uploaded, byte-verified; VPK SHA-256
`656d08193a8e7caf1f6c677ac4306cfe273cb91b9ddb3561946929917dff8242`,
SKPRX `876405034dc85dbacac0c68e6d28a6ae504e540a9c83dc0dbdacf4a4476f5a1d`):
KERNEL CHANGED - install VPK and REBOOT before testing.

- new op PwmBlink (0x89D via NID 0x6F586D1A): the decompiled wrapper
  demands device 0x40 but the wire payload selector is a fixed 0x01 - the
  same selector BT2 uses for the green die. If it drives green, it is a
  true hardware PWM = flicker-free dimming. Probe values 0x0000-0xFFFF.
- new mode 14 POWER_LED_OFF: production policy test. Asserts BT2 dev1
  (0,1) at start, re-asserts ~1s after each resume (sysevent handler +
  10s heartbeat). No hooks. Loader shows expected behavior on screen.
- new mode 15 TRACE_PROBE: mode 6 pass-through tracer + probe thread in
  one boot, to capture what the resume path sends when it releases the
  off pattern (candidate release command for charger-aware policy).
- probe request log now records arg2/arg3.

Next sessions (one mode per boot):

1. mode 13 (default): PwmBlink value sweep - looking for clean green
   dimming; also the slot test (dev1 0/1 dark, then dev0/2/3 1/0 - does
   green return = single pattern slot?).
2. mode 14: end-to-end policy test - LED dark, standby/wake, dark again
   within ~2s, no side effects during play.
3. mode 15: assert dev1 0/1, standby, wake, exit, pull syscon_trace.log -
   find the resume-time LED command that released the pattern (if any).

## 2026-07-04 BT2 Pattern Engine Mapped (v4 session) + v5

BT2 = ksceSysconCtrlLedBlinkType2 (0x89F), dev 1 = green die. Hardware
results:

- times are blink on/off durations with a coarse unit (tens of ms): 1/0 =
  full on, 9/1 slightly dimmer (90 percent duty), 1/1 half, 1/9 low but
  with VISIBLE flicker/unevenness. BT2 dimming is a blinker, not a clean
  PWM; smallest nonzero time is 1, so flicker-free dim is not achievable
  with this command. Blue dims cleanly only because 0x89D CtrlLedPwmBlink
  is a real PWM hardwired to the blue device; green has no equivalent.
- (0,0) does NOT release the pattern. (1,0) forces the green die ON. Once
  any dev-1 pattern is set the engine owns the indicator: DolceLED blinks
  are ignored and charger orange does not appear (manual mode gates the
  whole bicolor package). Only wake or reboot restores stock policy.
- forced-on under an attached charger stays GREEN: dev 1 drives the green
  die specifically, color does not follow policy in manual mode.
- two-layer state confirmed: DolceLED blink state survives soft reboot and
  is cleared by suspend/wake; the BT2 pattern layer sits above it and is
  cleared by either.

v5 probe build (uploaded, byte-verified, VPK SHA-256
`7660c0350c46b1bceddd2b49f37db578d5b57e6cd91da95f273f6ac073d2353f`),
loader-only, works against the resident v3 kernel: hunts the ORANGE die
via BT2 dev 0 force-on (never tested with on-time > 0), plus dev 2/3
force-on and a dev-1 unk=2 color-select probe. If dev 0 lights orange,
the production plugin can render "dark when idle, orange while charging"
entirely in manual mode.

## 2026-07-04 THE GREEN POWER LED OFF COMMAND WAS FOUND (v3 session)

`ksceSysconCtrlLedBlinkType2(1, 0, 0, 1)` - syscon command 0x89F, payload
[dev=0x01, unk=0x00, on_time=0x0000, off_time=0x0001] - TURNS OFF the
PCH-2000 steady green power LED.

Observed properties (hardware confirmed):

- persists while awake; not overridden by ksceSysconCtrlDolceLED(0) ("on"),
  ChargeLedCtrl, or CtrlLED commands sent afterwards,
- suppresses even the standby-entry blink; LED stays dark through the
  whole suspend sequence,
- resume/wake restores stock steady green, so a production plugin only
  needs to re-assert after resume (and at boot),
- all probes returned 0; the 0x89F device namespace is distinct from
  0x891 (dev 0x40 there did nothing to the blue LED, dev 1 = green).

Negative results closing the hunt: CtrlLED (0x891) ids 1/2/3 accepted but
do not affect green; SceLed leds 4-7 invalid (0x803F0140), leds 0/1/3
accepted but invisible.

Charger test results (v3 build, same day):

- with the off pattern active, ORANGE IS ALSO SUPPRESSED: plugging or
  unplugging the charger leaves the LED dark, and no available command
  (DolceLED, ChargeLedCtrl, CtrlLED, other BT2 off-patterns) restored it.
  The dev-1 pattern gates the entire bicolor indicator.
- soft reboot DOES restore green (unlike the earlier blink-fast state,
  which had survived a soft reboot).
- before the off pattern, nothing affected orange while charging - it was
  immune to every charger-related command tested.

v4 probe build (uploaded, byte-verified, VPK SHA-256
`45205ba16a2addcce5a0779b002b8dcb055784028c002372a6ec9e32d5eaeb9d`):
loader-only change, works against the resident v3 kernel. Explores the
missing half of the BT2 dev-1 pattern engine: (0,0) release-to-policy,
(1,0) force on, (9,1)/(1,1)/(1,9) duty dimming, unk-byte variants, and
DolceLED interplay. Key questions: does (0,0) release the off state so
orange can show while charging, and does duty dimming work on green like
it does on blue via SceLed.

Production plugin design depends on the (0,0) result:

- if (0,0) releases to policy: plugin asserts (0,1) while unplugged and
  (0,0) while charging; re-assert on resume and power callbacks.
- if nothing releases it: plugin asserts (0,1) unconditionally (LED dark
  even while charging) or offers dim duty as a config alternative.

## 2026-07-04 Second Mode 13 Session (v2 build) + v3

Hardware results, second session:

- SceLed led 2 (blue) PWM mapping confirmed: on/off times 0/1 = fully OFF,
  1/1 and 1/0 and 1/9 = dim, 9/1 = bright. Brightness = duty ratio.
- SceLed leds 0, 1, 3 = no visible effect on green or anything else; with
  the earlier 2/4/5/6/7 results the whole SceLed 0-7 space is exhausted and
  does not reach the green power LED.
- raw 0x823 = rejected by Ernie: exec ret 0x8025023F (error byte 0x3F).
  Confirms the decompilation finding that the DolceLED family (0x820-0x822)
  has no off state. Also proves Ernie rejects unknown commands with a clean
  error instead of misbehaving.

Decompilation research (emu-russia/VitaTestSuite SysconElf_360.cpp):

- ksceSysconCtrlLED = cmd 0x891, payload byte led | (enable ? 0x80 : 0) -
  the only LED export with a true off bit.
- ksceSysconCtrlDolceLED = cmd 0x820 + state, state hard-limited to 0-2.
- sceSysconCtrlLedPwmBlink (0x89D) = fixed payload [0x01, pwm_lo, pwm_hi],
  wrapper arg-checked to device 0x40; the device byte is NOT in the payload,
  so it targets one hardwired LED (the blue one).
- sceSysconCtrlLedBlinkType2 (0x89F, NID 0xCB41B531) = payload
  [u8 dev, u8 unk, u16 on, u16 off], and the wrapper does NOT validate the
  device byte - it can address arbitrary devices. Prime remaining vector.

v3 build (uploaded, byte-verified):

- VPK SHA-256 `094ac8f8c32a34b0e982ccf8273d6ac515de44af3f240e1b9ccda90abd444f79`,
  SKPRX `fa7566fb7692493fd48da74466b35e7390e410da4319a64619aad7215a762214`.
- new op: ksceSysconCtrlLedBlinkType2 via runtime NID resolution, whitelist
  devices 0-3 and 0x40, times bounded to 10.
- focused candidate list: CtrlLED ids 1/2/3 OFF+ON (green die hunt, the top
  remaining candidates), BlinkType2 semantics probes on blue 0x40, then
  BlinkType2 devices 0-3 with 0/1 times (green off hunt), charger entries
  kept at the end. Answered candidates removed.
- requires reinstalling the VPK AND a reboot (the new op needs the v3
  kernel plugin; a resident v2 kernel would reject op 8 with ret -2).

## 2026-07-04 First Mode 13 Hardware Session - LED Map Found

First interactive probe session on PCH-2000 (v1 candidate list, no charger
attached) produced the LED map:

- `ksceSysconCtrlLED(0x40, 1)` turned ON the BLUE notification LED next to
  the power LED; `(0x40, 0)` visibly did nothing because it was already off.
  Device 0x40 = blue LED, not the power LED.
- `ksceLedSetMode(led 2, ...)`: OFF turned the blue LED off; ON with
  config on/off times 1/1 turned it back on DIMMED. SceLed led 2 = blue LED
  and the config times act as a PWM duty (dimming works!).
- `ksceSysconCtrlDolceLED(1)` = green power LED blinks slow;
  `ksceSysconCtrlDolceLED(2)` = green power LED blinks fast. CONFIRMED: the
  0x820-0x822 "DolceLED" family drives the PCH-2000 GREEN POWER LED.
  The blink state is sticky until something re-commands the LED; state 0
  (command 0x820) or an OS suspend/resume cycle restores steady green.
- The blink state SURVIVED a soft reboot via VitaShell: syscon does not
  reset with the SoC, and the OS does not re-command the power LED during
  boot. Only standby/wake cleared it (steady green after wake). Full
  poweroff/poweron was not tested. Consequence for the eventual production
  plugin: a syscon-level LED-off state should persist across soft reboots;
  re-assert points are cold boot and resume only.
- SceLed leds 4-7, CtrlLED led 0, ChargeLedCtrl, ScePower flag: no visible
  effect while unplugged (charge-path candidates need a charger session).
- Risky CtrlLED ids 1-3 were not tested.

Open question: does the DolceLED family have an OFF state? The wiki command
table has a gap at 0x823 (0x822 -> 0x840), so raw 0x823 is the prime probe.
SceLed leds 0/1/3 are untested and may also map to the green LED with full
OFF/dim control like led 2 has for blue.

## 2026-07-04 v2 Probe Build (awaiting upload)

Changes vs the tested v1:

- request protocol gains arg2/arg3 = SceLedConfiguration on/off times so
  brightness/PWM can be explored; validation bounds them to 0-10 and only
  for the SceLed op.
- new whitelisted raw op: syscon command 0x823 exactly, empty payload, as
  the "DolceLED off?" probe (RISKY entry, last in the list).
- candidate list v2: DolceLED 0 recovery near the top, SceLed leds 0/1/3
  OFF/ON pairs as green candidates, SceLed led 2 brightness lab, charger
  entries kept for a charger-attached session.
- loader fix: probe UI now stays reachable when the plugin is already
  resident from an earlier launch this boot (load failure no longer locks
  out sends).

Build/test: all 11 host tests pass, `cmake --build build -j8` clean.
VPK SHA-256 `7f2711db357b41c945119a003cb485554253498c271eaa856980b69961241431`,
bundled SKPRX `783df150a389a703de6cbf8e2495e6400772ca3a5aece3e3bcefe0f5ac07cbbd`.
Uploaded to `ftp://192.168.1.113:1337/ux0:/data/noled_loader.vpk`,
downloaded back and byte-compared identical.

Next session protocol: load (or relaunch if resident), ping, then
DolceLED 0 to restore steady green if still blinking, then SceLed led 0 OFF,
led 1 OFF, led 3 OFF (recovery ON after each), then the led 2 brightness
tests, and only at the end - with work saved - the raw 0x823 probe.

## 2026-07-04 Research Correction And New Mode 13

Offline research (henkaku wiki raw dumps, decompiled 3.60 SceSyscon from
emu-russia/VitaTestSuite, GitHub code search) corrected the core assumptions
of all earlier experiments:

- Syscon command `0x387` is in the touchpanel command range (`0x380` is
  `sceSysconGetTouchpanelDeviceInfo`). It is not an LED command. Mutating it
  broke touch wake because it mutated touchpanel suspend configuration. Never
  touch `0x387` again.
- The real LED command is `0x891`: `ksceSysconCtrlLED(led, enable)` sends a
  one-byte payload `led | (enable ? 0x80 : 0)`. The driver computes packet
  checksums itself.
- `ksceSysconCtrlDolceLED(state)` sends command `0x820 + state` with no
  payload: 0 = on, 1 = blink slow, 2 = blink fast. State 0 is an ON command,
  so the old "DolceLED(0) did not help" result was meaningless.
- `sceSysconCtrlLedPwmBlink` (command `0x89D`) rejects every device value
  except `0x40`, and ScePower drives blink paths through it. The standby
  blink is the power LED, so device `0x40` is the top candidate for the
  PCH-2000 power LED.
- ModernBT-Vita ships `ksceSysconCtrlLED(0, enable)` on retail hardware and
  documents led 0 as the Vita 1000/2000 charge LED - the same physical
  package as the green power LED on PCH-2000.
- `sceSysconSetChargeLedCtrl` (command `0x89E`, NID `0x9CA6EB70`) and
  `ScePowerForDriver_38415146(SceBool)` ("related to LED") are OS-level
  charge LED policy switches, untested so far.
- The old `ksceSysconCtrlLED(0..7, 0)` crash came from sweeping all ids in
  one loop with no observation between calls; it does not condemn single
  calls with known ids. Ids 4-7 stay banned regardless.
- No public plugin or fork has ever turned off the PCH-2000 power LED; this
  experiment is unclaimed territory.

New mode 13 (`NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE`, now the loader
default) sends exactly one curated LED command per Cross press while the
system is awake and steady green, with the result shown on screen:

- kernel side: no hooks, no debug handlers, no packet mutation; a poller
  thread reads `ux0:data/noled/probe_request.bin`, whitelists the request
  (`runtime_probe.h`), executes it once, writes
  `ux0:data/noled/probe_response.bin` and appends `ux0:data/noled/probe.log`
  (the "try" line is synced before execution so a freeze still leaves
  evidence of the candidate that caused it).
- loader side: Left/Right cycles candidates, Cross sends one, response and
  send status render on screen. Candidate order: CtrlLED dev 0x40, CtrlLED
  led 0, ChargeLedCtrl, ScePower LED flag, SceLed modes, DolceLED states,
  then risky CtrlLED ids 1-3 last.

Mode 13 hardware test protocol:

1. Reboot, launch loader, confirm mode 13, press Cross to load the plugin.
2. Send "ping" first and confirm a response arrives (thread alive).
3. Send "CtrlLED dev 0x40 OFF" and look at the green LED.
4. After every send: watch the LED for a few seconds, then send the paired
   recovery command and confirm the LED returns to stock before moving on.
5. Work down the list one command at a time. RISKY entries last, only with
   work saved; a freeze is recovered by long power hold, nothing persists.
6. After the session pull `loader.log` and `probe.log` over FTP.

Build/test status: all 11 host tests pass; `cmake --build build -j8` clean.
VPK SHA-256 `c5a627711f5fc21738b48ebd8d3504c69b88ec0e1fa5357414dd41c249b5db6b`,
bundled SKPRX `f1d0a63dc881bb655f1fcc2129f2e9fc9a886c01eb84aea4d233b664de66980b`.

Uploaded 2026-07-04 to `ftp://192.168.1.113:1337/ux0:/data/noled_loader.vpk`
(Vita now at .113, not .117), downloaded back and byte-compared identical.
Awaiting hardware test.

## Current Uploaded Diagnostics Build

Current uploaded build:

SHA-256:

`b570059ba0bc1437543041fb23f9cbe3c98a842d0e175fdec7772c2c1f271c27`

Bundled SKPRX SHA-256:

`551d9ab11201427279ee5362b0a81c28ced03038e3ba1557a8aeb12173c00f10`

Default mode:

`12 - NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE`

Purpose:

- Stop blind LED poking and identify which kernel module issues the syscon calls around charger state, standby blink, and wake.
- Cross starts passive syscon command tracing with raw caller addresses.
- A taiHEN module inventory is written once at startup without importing firmware-version-specific `SceModulemgrForKernel`.
- Power and charger changes are auto-logged; marker buttons are optional.
- Left/Right before Cross can switch to the old mode 10 read-only GPIO sampler if needed, but default mode 12 is the main path.

Safety policy:

- No syscon packet mutation.
- No DolceLED hook.
- No SysconCtrlLED hook.
- No broad GPIO clear/reset/mode hooks.
- No GPIO port 3 block.
- Syscon command hooks are pass-through only and were previously safe as mode 6.

Logs:

- `ux0:data/noled/loader.log`
- `ux0:data/noled/syscon_trace.log`
- `ux0:data/noled/module_map.log`
- `ux0:data/noled/gpio_sample.log` only if mode 10 is selected before Cross.

Build checks:

- Host tests passed:
  - `test_syscon_experiment_policy`
  - `test_gpio_trace_policy`
  - `test_led_candidate_patch`
  - `test_dolce_led_policy`
  - `test_syscon_trace`
  - `test_syscon_led_policy`
  - `test_cmd_probe`
  - `test_power_led_patch`
  - `test_led_policy`
- `cmake --build build -j8` passed.

Upload status:

- Uploaded to `ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk`.
- Downloaded back to `/private/tmp/noled_loader_uploaded_caller_trace_no_modulemgr_import.vpk`.
- `cmp -s build/noled_loader.vpk /private/tmp/noled_loader_uploaded_caller_trace_no_modulemgr_import.vpk` exited `0`.
- Uploaded SHA-256 matches local SHA-256: `b570059ba0bc1437543041fb23f9cbe3c98a842d0e175fdec7772c2c1f271c27`.

Important correction:

- The previous mode-12 build failed to load with `0x8002D003`.
- VitaSDK identifies `0x8002D003` as `SCE_KERNEL_ERROR_MODULEMGR_NO_LIB`.
- Root cause: the build imported `SceModulemgrForKernel` using 3.60 stubs, but that library/NID is not available on the tested firmware.
- The current uploaded build removes that import.

Minimal next test:

1. Launch noLED loader.
2. Confirm mode 12 is shown.
3. Press Cross once.
4. Plug charger, wait for orange, unplug charger, wait for green.
5. Optional but useful: one standby/wake cycle.
6. Return to VitaShell FTP and pull `loader.log`, `syscon_trace.log`, and `module_map.log`.

No marker buttons are required for this build.

## Last Mode 12 Run Result

Pulled logs:

- `/private/tmp/noled_loader_after_mode12_no_modulemgr.log`
- `/private/tmp/noled_syscon_after_mode12_no_modulemgr.log`
- `/private/tmp/noled_module_map_after_mode12_no_modulemgr.log`
- Log hashes: `58bc65ac3acc21179632a4e99a5476271e10d441127d2cf2a62dcd283a3188ac`, `8b0186e3deb6483389790071f2b09596732634c0faa3b31cfee460e546566372`, `f9c40f9a8483e1ad68771b76f9698a17bb6a6494e2b92bf3d0e84bc04ecc63ef`

Observed:

- Corrected mode 12 loaded successfully: `taiLoadStartKernelModule: 0x4001014F`.
- Module map wrote successfully using taiHEN module info; no `SceModulemgrForKernel` import was used.
- Syscon trace contains only periodic polling style commands from caller `0x00BCAF9F`.
- No `0x089A` or `0x0387` lines were present in this trace.
- Loader log shows power state changes after module load, but those did not correlate with captured `0x089A`/`0x0387` traffic in this run.

Interpretation:

This build fixed the load failure but did not capture the useful LED-transition path. Do not ask the tester to repeat this exact mode-12 plug/unplug flow. The next diagnostic must either capture a wider/lower-level path or use existing logs for offline static analysis.

## Last Completed Hardware Result

Current tested build:

SHA-256:

`b96dff2d7d4120981385108a77a4ef44764f676919171209eaee41b18415f9cd`

Bundled SKPRX SHA-256:

`5b3469f8c9775d122aa0b110acfc7efc3d8b98a5d3309543e95c282bc64f750f`

Default mode:

`10 - NOLED_LED_CANDIDATE_GPIO_READ_SAMPLER`

User result:

- Charger attach/detach was visible in the app's power state.
- Green/orange power LED behavior remained stock.
- No steady green-off effect was observed.

Pulled logs:

- `/private/tmp/noled_loader_after_gpio_read_diagnostics.log`
- `/private/tmp/noled_gpio_sample_after_diagnostics.log`
- Log hashes: `3c3b8d6da02a1d318702400b4b144896e0b1de8af763588599bb2b1f866a8548`, `5dfd2e65c2cf3fd502923b80866586f45a701292bc37deaae4f5dc5bc8375aed`

GPIO sampler evidence:

- No sampled GPIO line mapped cleanly to the steady green/orange power LED state.
- The busiest lines were bus 0 port 6 and bus 0 port 3 in both unplugged and plugged states.
- Counts by observed power state: bus0 port6 offline 742 / online 57; bus0 port3 offline 224 / online 21; bus0 port4 offline 35 / online 4.
- Charger attach briefly correlated with bus0 port5, bus0 port12, and bus1 port3 transitions, but these did not represent a stable steady-green enable.
- Interpretation: the steady Slim power LED is likely controlled below the public GPIO read/set path, through syscon/PMIC firmware or a register path not exposed by these GPIO APIs.

## Superseded Safe Replacement

Previously uploaded safe replacement:

SHA-256:

`784ee1a1d82bac8ae1b53eaa0d624904613bd99ddd9852f45d7ce357c9cbe737`

Default mode:

`6 - NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS`

This has been superseded on the Vita by the current mode-12 caller-map build.

## Older Completed Hardware Result

Current tested build:

SHA-256:

`34666cb611c2a080704a8e660efd52549edd9a08f8a436fbd0169f82375a8389`

Bundled SKPRX SHA-256:

`50fa0e69d02dbffcd24e2cda8ff88fef800d6065d5397cb1008e457408f94b75`

Default mode:

`9 - NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK`

User result:

- after pressing Cross, Vita froze and powered off,
- green power LED stayed lit during the failure,
- no standby was tested.

Build and host checks:

- `test_led_policy` passed.
- `test_syscon_experiment_policy` passed.
- `test_gpio_trace_policy` passed.
- `test_led_candidate_patch` passed.
- `test_dolce_led_policy` passed.
- `test_syscon_trace` passed.
- `test_syscon_led_policy` passed.
- `test_cmd_probe` passed.
- `test_power_led_patch` passed.
- `cmake --build build -j8` passed.

Upload status:

- Uploaded to `ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk`.
- Downloaded back to `/private/tmp/noled_loader_uploaded_mode9_gpio_port3_block.vpk`.
- `cmp -s build/noled_loader.vpk /private/tmp/noled_loader_uploaded_mode9_gpio_port3_block.vpk` exited `0`.
- Uploaded SHA-256 matches local SHA-256: `34666cb611c2a080704a8e660efd52549edd9a08f8a436fbd0169f82375a8389`.

Interpretation:

Mode 9 is unsafe and should not be run again. Blocking/clearing GPIO bus 0 port 3 does not suppress the steady green LED and can crash/power off the system immediately after module load.

## Current Next Build Direction

Do not build another blind mutation candidate until the mode-12 caller-map logs have been inspected.

The immediate goal is to map caller addresses for commands such as `0x089A`, `0x0387`, and standby/wake-related syscon traffic to module names and offsets. Then the next step should be static reverse engineering of those modules/functions, not more random GPIO/syscon writes.

## Previous Completed Hardware Result

Current tested build:

SHA-256:

`e81b77316e0fc6a0e8fafe354da32809c97dee6fe3925eeefcf1683cfef82851`

Bundled SKPRX SHA-256:

`995d321d757babf42600f460355a3ebcbcf9f0d7cf42c691374c4f08a17cfda9`

Default mode:

`8 - NOLED_LED_CANDIDATE_GPIO_TRACE_PASS`

User result:

- standby did not finish,
- green LED kept blinking well beyond 100 blinks,
- user forced recovery with long power hold into safe mode, then restarted.

Build and host checks:

- `test_gpio_trace_policy` passed.
- `test_syscon_experiment_policy` passed.
- `test_led_candidate_patch` passed.
- `test_dolce_led_policy` passed.
- `test_syscon_trace` passed.
- `test_led_policy` passed.
- `test_syscon_led_policy` passed.
- `test_cmd_probe` passed.
- `test_power_led_patch` passed.
- `cmake --build build -j8` passed.

Upload status:

- Uploaded to `ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk`.
- Downloaded back to `/private/tmp/noled_loader_uploaded_mode8_gpio_trace.vpk`.
- `cmp -s build/noled_loader.vpk /private/tmp/noled_loader_uploaded_mode8_gpio_trace.vpk` exited `0`.
- Uploaded SHA-256 matches local SHA-256: `e81b77316e0fc6a0e8fafe354da32809c97dee6fe3925eeefcf1683cfef82851`.

Safety policy:

- No syscon packet mutation.
- No DolceLED forcing.
- No SysconCtrlLED hook.

Interpretation:

Broad GPIO clear/set tracing is unsafe on this hardware. Use read-only sampling or narrowly scoped, justified hooks only.

<!-- Older historical sections below are kept for full context. -->

<!--
- No syscon trace.
- No syscon command hooks.
- No syscon packet mutation.
- No DolceLED hook.
- No SysconCtrlLED hook.
- No GPIO hooks.
- No GPIO writes.
- Kernel module only calls `ksceGpioPortRead` and power status helpers.

Logs:

- `ux0:data/noled/loader.log`
- `ux0:data/noled/gpio_sample.log`

Build checks:

- All host tests passed.
- `cmake --build build -j8` passed without warnings after removing the stale mode-cycling helper.

Upload status:

- Uploaded to `ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk`.
- Downloaded back to `/private/tmp/noled_loader_uploaded_gpio_read_diagnostics.vpk`.
- `cmp -s build/noled_loader.vpk /private/tmp/noled_loader_uploaded_gpio_read_diagnostics.vpk` exited `0`.
- Uploaded SHA-256 matches local SHA-256: `b96dff2d7d4120981385108a77a4ef44764f676919171209eaee41b18415f9cd`.

## Current Uploaded Safe Replacement

Current uploaded build:

SHA-256:

`784ee1a1d82bac8ae1b53eaa0d624904613bd99ddd9852f45d7ce357c9cbe737`

Bundled SKPRX SHA-256:

`50fa0e69d02dbffcd24e2cda8ff88fef800d6065d5397cb1008e457408f94b75`

Default mode:

`6 - NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS`

Safety status:

- Replaces unsafe mode-9 upload on Vita.
- Loader cycles only through modes 0-7, so unsafe GPIO modes 8/9 are not reachable through Left/Right.
- No packet mutation in default mode.
- No DolceLED hook in default mode.
- No SysconCtrlLED hook.
- No broad GPIO trace.
- No GPIO port 3 block.

Upload status:

- Uploaded to `ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk`.
- Downloaded back to `/private/tmp/noled_loader_uploaded_safe_mode6_replacement.vpk`.
- `cmp -s build/noled_loader.vpk /private/tmp/noled_loader_uploaded_safe_mode6_replacement.vpk` exited `0`.
- Uploaded SHA-256 matches local SHA-256: `784ee1a1d82bac8ae1b53eaa0d624904613bd99ddd9852f45d7ce357c9cbe737`.

## Last Completed Hardware Result

Current tested build:

SHA-256:

`34666cb611c2a080704a8e660efd52549edd9a08f8a436fbd0169f82375a8389`

Bundled SKPRX SHA-256:

`50fa0e69d02dbffcd24e2cda8ff88fef800d6065d5397cb1008e457408f94b75`

Default mode:

`9 - NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK`

User result:

- after pressing Cross, Vita froze and powered off,
- green power LED stayed lit during the failure,
- no standby was tested.

Build and host checks:

- `test_led_policy` passed.
- `test_syscon_experiment_policy` passed.
- `test_gpio_trace_policy` passed.
- `test_led_candidate_patch` passed.
- `test_dolce_led_policy` passed.
- `test_syscon_trace` passed.
- `test_syscon_led_policy` passed.
- `test_cmd_probe` passed.
- `test_power_led_patch` passed.
- `cmake --build build -j8` passed.

Upload status:

- Uploaded to `ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk`.
- Downloaded back to `/private/tmp/noled_loader_uploaded_mode9_gpio_port3_block.vpk`.
- `cmp -s build/noled_loader.vpk /private/tmp/noled_loader_uploaded_mode9_gpio_port3_block.vpk` exited `0`.
- Uploaded SHA-256 matches local SHA-256: `34666cb611c2a080704a8e660efd52549edd9a08f8a436fbd0169f82375a8389`.

Test request for this build:

1. Reboot before launching the VPK.
2. Launch noLED loader.
3. Confirm the screen says mode 9 / block GPIO bus 0 port 3.
4. Press Cross once.
5. Observe whether the steady green power LED changes while the app is active.
6. Plug charger in and observe whether orange charging LED still works.
7. Unplug charger and observe whether green returns or stays off.
8. Do not test standby in this first run.
9. Start VitaShell FTP or report the visible result.

Expected logs:

- Loader log should show `candidate initial: mode=9`.
- There will be no new syscon trace for mode 9 because syscon tracing is disabled for this candidate.

Interpretation:

Mode 9 is unsafe and should not be run again. Blocking/clearing GPIO bus 0 port 3 does not suppress the steady green LED and can crash/power off the system immediately after module load.

## Current Next Build Direction

Replace the uploaded VPK with a known-safe build before any more hardware work.

Safe replacement target:

- Default mode: `6 - NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS`, or mode 0 if only a no-syscon baseline is wanted.
- No packet mutation.
- No DolceLED hook.
- No SysconCtrlLED hook.
- No broad GPIO clear/reset/mode hooks.
- No GPIO port 3 block.

Next research should not manipulate GPIO port 3 directly. If GPIO information is still needed, use a no-I/O counter-only hook and read it after returning to the app, not high-volume trace logging during standby.

## Previous Completed Hardware Result

Current tested build:

SHA-256:

`e81b77316e0fc6a0e8fafe354da32809c97dee6fe3925eeefcf1683cfef82851`

Bundled SKPRX SHA-256:

`995d321d757babf42600f460355a3ebcbcf9f0d7cf42c691374c4f08a17cfda9`

Default mode:

`8 - NOLED_LED_CANDIDATE_GPIO_TRACE_PASS`

User result:

- standby did not finish,
- green LED kept blinking well beyond 100 blinks,
- user forced recovery with long power hold into safe mode, then restarted.

Build and host checks:

- `test_gpio_trace_policy` passed.
- `test_syscon_experiment_policy` passed.
- `test_led_candidate_patch` passed.
- `test_dolce_led_policy` passed.
- `test_syscon_trace` passed.
- `test_led_policy` passed.
- `test_syscon_led_policy` passed.
- `test_cmd_probe` passed.
- `test_power_led_patch` passed.
- `cmake --build build -j8` passed.

Upload status:

- Uploaded to `ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk`.
- Downloaded back to `/private/tmp/noled_loader_uploaded_mode8_gpio_trace.vpk`.
- `cmp -s build/noled_loader.vpk /private/tmp/noled_loader_uploaded_mode8_gpio_trace.vpk` exited `0`.
- Uploaded SHA-256 matches local SHA-256: `e81b77316e0fc6a0e8fafe354da32809c97dee6fe3925eeefcf1683cfef82851`.

Test request for this build:

1. Reboot before launching the VPK.
2. Launch noLED loader.
3. Confirm the screen says mode 8 / GPIO pass-through trace.
4. Press Cross once.
5. Press Start to leave the tracer resident.
6. Plug charger in, wait for orange LED, unplug, wait for green LED.
7. Put Vita into standby with the power button.
8. Count blinks and wake it.
9. Confirm touch behavior.
10. Start VitaShell FTP without rebooting so logs include the events.

Expected log evidence:

- `M marker=gpio_port_set led=<bus> enable=<port> ret=<ret>`
- `M marker=gpio_port_clear led=<bus> enable=<port> ret=<ret>`
- `M marker=gpio_port_reset led=<bus> enable=<port> ret=<ret>`
- `M marker=gpio_set_port_mode led=<bus> enable=<port> value=<mode> ret=<ret>`
- No `M marker=led_candidate_patch` entries should appear.

Pulled logs after this report:

- `/private/tmp/noled_loader_after_mode8_gpio_standby_stuck.log`
- `/private/tmp/noled_syscon_after_mode8_gpio_standby_stuck.log`

Log hashes:

- `26ad0df3843a7d0e068eab9f7e55519a5a02106d0ba4d8f553f2d61fe1e5be89`
- `43e8b83cb25ac6594008d60aedce3750192b1e0a596018edbdba17cd0e47c97f`

The syscon log confirms:

- no `M marker=led_candidate_patch` entries were found,
- log grew to 19,309 lines and seq 54,106,
- repeated `lost from=...` ranges show ring overflow from GPIO event volume,
- hot GPIO counts:
  - `gpio_port_clear bus=0 port=3`: 7,955
  - `gpio_port_set bus=0 port=3`: 2,650
  - `gpio_port_clear bus=0 port=2`: 2,125
  - `gpio_port_clear bus=1 port=0`: 2,126
  - `gpio_port_clear bus=0 port=7`: 2,127
  - `gpio_port_clear bus=0 port=6`: 1,271
  - `gpio_port_set bus=0 port=6`: 854
- final log tail was the same repeating GPIO pattern, ending with `gpio_port_clear led=0 enable=7`.

Interpretation:

The broad GPIO trace mode is unsafe. The extra `ksceGpioPortClear` tracing and high-volume logging are enough to disturb standby. Do not repeat mode 8.

Useful finding:

Bus 0 port 3 is a strong candidate for the blinking/power-indicator GPIO path because it toggled thousands of times during the stuck blink sequence.

## Current Next Build Direction

Build a narrower runtime-only candidate:

- Default mode: new `NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK`.
- No syscon trace.
- No syscon command hooks.
- No DolceLED hook.
- No extra GPIO clear/reset/mode hooks.
- Reuse only the already-safe original `ksceGpioPortSet` hook.
- Block `ksceGpioPortSet(0, 3)` and clear bus 0 port 3 once at module start.

First test should avoid standby:

1. Reboot.
2. Launch loader.
3. Confirm the mode says GPIO port 3 block.
4. Press Cross.
5. Observe whether the green power LED changes while the app is active.
6. Plug/unplug charger and observe green/orange behavior.
7. Start FTP or report result. Do not test standby until the active-state LED effect is known.

## Previous Completed Hardware Result

Current tested build:

SHA-256:

`d6018bffe08d5d1041666998fd8f9fa405549ea0d06ce3706abb606f148fe321`

Bundled SKPRX SHA-256:

`b12789b251f470ed5a2291ae514bef7b2b5c45142784e2a737d856e7e2c51ef5`

Default mode:

`7 - NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS`

User result:

- no visible LED behavior change,
- 10 standby blinks,
- touch worked normally,
- steady green power LED still lit after wake.

Build and host checks:

- `cc -Wall -Wextra -Werror -I. tests/test_syscon_experiment_policy.c ...` passed.
- `cc -Wall -Wextra -Werror -I. tests/test_led_candidate_patch.c ...` passed.
- `cc -Wall -Wextra -Werror -I. tests/test_dolce_led_policy.c ...` passed.
- `cc -Wall -Wextra -Werror -I. tests/test_syscon_led_policy.c ...` passed.
- `cmake --build build -j8` passed.

Upload status:

- Uploaded to `ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk`.
- Downloaded back to `/private/tmp/noled_loader_uploaded_mode7_dolce_trace.vpk`.
- `cmp -s build/noled_loader.vpk /private/tmp/noled_loader_uploaded_mode7_dolce_trace.vpk` exited `0`.
- Uploaded SHA-256 matches local SHA-256: `d6018bffe08d5d1041666998fd8f9fa405549ea0d06ce3706abb606f148fe321`.

Test request for this build:

1. Reboot before launching the VPK.
2. Launch noLED loader.
3. Confirm the screen says mode 7 / DolceLED pass-through trace.
4. Press Cross once.
5. Press Start to leave the tracer resident.
6. Plug charger in, wait for orange LED, unplug, wait for green LED.
7. Repeat charger plug/unplug once if practical.
8. Put Vita into standby with the power button.
9. Count blinks and wake it.
10. Confirm touch behavior.
11. Start VitaShell FTP without rebooting so logs include the events.

Expected log evidence:

- `M marker=dolce_led_hook led=<requested> enable=<effective> ret=<ret>`
- In mode 7, `led` and `enable` must match because the hook is trace-only.
- No `M marker=led_candidate_patch` entries should appear.

Pulled logs after this report:

- `/private/tmp/noled_loader_after_mode7_dolce_trace.log`
- `/private/tmp/noled_syscon_after_mode7_dolce_trace.log`

Log hashes:

- `5c01dcf0aa9534ace52822e0a8b582bb2f3fd72938692bddd0cd1f919ab571ae`
- `1ad2535bb2f67901020d474ba2b30c3b2824c6dd6f5c3c4e267ff255218fb665`

The syscon log confirms:

- no `M marker=led_candidate_patch` entries,
- hook install marker: `M marker=dolce_led_hook led=0 enable=1 ret=0x00062A57`,
- one real DolceLED call: `M marker=dolce_led_hook led=2 enable=2 ret=0x00000000`,
- charger attach/detach happened without real DolceLED calls,
- `0x0387` forms were passed through unchanged.

Interpretation:

The pass-through DolceLED hook itself was safe in this test. `ksceSysconCtrlDolceLED(2)` appears in the sleep path, not in the charger green/orange transition. This makes DolceLED unlikely to be the steady green powered-on LED control path.

## Next Direction

Next build is evidence gathering, still no mutation:

- Add a GPIO pass-through trace mode.
- Log all `ksceGpioPortSet`, `ksceGpioPortClear`, `ksceGpioPortReset`, and `ksceGpioSetPortMode` calls after the tracer is resident.
- Keep original GPIO blocking for known non-power LEDs.
- Do not enable packet mutation.
- Do not enable DolceLED forcing.
- Do not enable SysconCtrlLED.
- Test charger attach/detach and standby/wake again.

Purpose:

Determine whether the Vita Slim power LED transitions are visible as a GPIO port change on a bus/port not covered by the original noLED policy. If no GPIO calls correlate with green/orange or wake, the power LED is more likely driven by syscon/PMIC policy below the public GPIO layer.

## Last Completed Hardware Result

Current tested build:

SHA-256:

`07d4151a710ce83365352c8ee175487a8d51962dc313b4f5adb141c7dec090c9`

Default mode:

`6 - NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS`

User result:

- charger plug/unplug changed the LED between green and orange normally,
- standby/wake showed the usual 10 green blinks,
- touch worked normally after wake,
- steady green power LED still lit after wake.

Interpretation:

Mode 6 is a safe pass-through trace baseline. It confirms that syscon debug handlers plus command hooks are not enough to disturb wake/touch by themselves, and that charger transitions are visible in the syscon trace without mutating packets. It did not turn off the green LED.

Pulled logs after this report:

- `/private/tmp/noled_loader_after_mode6_charger_wake_trace.log`
- `/private/tmp/noled_syscon_after_mode6_charger_wake_trace.log`

Log hashes:

- `0f8d3762c3a76b974a278fc4165d7d01707f69de7667f6024be8649964ca5a2c`
- `fae906573a8dfe89b326b424f86bf562dc6dbc0787fd2514b5a5eda2950a9eb3`

The syscon log confirms:

- no `M marker=led_candidate_patch` entries,
- no `M marker=dolce_led_hook` entries,
- charger attach/detach was logged as power changes at seq 118/131 and 279/301,
- `0x089A` traffic appeared near charger transitions, including `tx=00005A` and `rx=DDF7`,
- standby/wake produced `0x0387` forms passed through unchanged.

The `0x0387` branch should not be treated as a viable LED-off solution at this point. `0x089A` is also not proven to be a direct green LED control path; it is only correlated with charger/power-state handling in the clean trace.

## Next Direction

Do not keep iterating blind `0x0387` mutations.

Current uploaded build is evidence gathering, not another packet mutation:

- Default mode: `7 - NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS`
- Keep command hooks enabled for caller/packet trace.
- Keep all packet mutation disabled.
- Enable the DolceLED hook only as pass-through trace.
- Do not enable SysconCtrlLED hook.
- Test charger attach/detach transitions, then standby/wake.

Expected UI text:

- `Default mode traces syscon commands and DolceLED calls`
- `Mode 7 is DolceLED pass-through trace; no mutations.`

Expected log evidence:

- `M marker=dolce_led_hook led=<requested> enable=<effective> ret=<ret>`
- In mode 7, `led` and `enable` must match because the hook is trace-only.
- No `M marker=led_candidate_patch` entries should appear.

## Previous Mode 6 Pass-Through Trace Build

Built artifact:

- VPK SHA-256: `07d4151a710ce83365352c8ee175487a8d51962dc313b4f5adb141c7dec090c9`
- SKPRX SHA-256: `85a00e84fa4fa447431bd98e9ca2e17e327dadc61d7440910ec682e6797b77dd`
- Default mode: `6 - NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS`
- Host tests passed: `test_syscon_experiment_policy`, `test_led_candidate_patch`, `test_dolce_led_policy`
- Build command passed: `cmake --build build -j8`

Upload status:

- Uploaded to `ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk`.
- Downloaded back to `/private/tmp/noled_loader_uploaded_mode6_trace.vpk`.
- `cmp -s build/noled_loader.vpk /private/tmp/noled_loader_uploaded_mode6_trace.vpk` exited `0`.
- Uploaded SHA-256 matches local SHA-256: `07d4151a710ce83365352c8ee175487a8d51962dc313b4f5adb141c7dec090c9`.

Test request used for this build:

1. Reboot before launching the VPK.
2. Launch noLED loader.
3. Confirm the screen says mode 6 / pass-through trace only.
4. Press Cross once.
5. Press Start to leave the tracer resident.
6. Plug charger in, wait for orange LED, unplug, wait for green LED.
7. Repeat charger plug/unplug once if practical.
8. Put Vita into standby with the power button.
9. Count blinks and wake it.
10. Confirm touch behavior.
11. Start VitaShell FTP without rebooting so logs include the events.

## If Mode 1 Is Safe

Then test mode 2. If mode 2 is unsafe, only the `FF/00` mutation is the problem. Continue exploring the mode 1 branch only.

## If Mode 1 Is Unsafe

Then test mode 2 anyway to determine whether both forms are unsafe or only mode 1 is unsafe.

## If Both Mode 1 And Mode 2 Are Unsafe

Stop mutating 0387 for production behavior. Keep syscon debug tracing and command hooks only for research. Do not claim a safe power-LED-off solution.
-->
