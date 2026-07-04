# PS Vita Slim Power LED Handoff

Date: 2026-05-12

Hardware under test: PS Vita Slim / PCH-2000.

Goal: turn off the distracting green "powered on" LED on Vita Slim while keeping normal charging/orange LED behavior and without breaking standby/wake, LCD, touch, or safe boot behavior.

## Current Status

The original noLED GPIO hook still works for the non-power LEDs, but the Vita Slim green/orange power LED is not controlled by that GPIO path.

No safe software path has been found yet that turns off only the steady green power LED. Several syscon paths affect standby blinking or other wake behavior, but none safely suppress steady green.

The latest read-only GPIO sampler did not find a stable sampled GPIO line that maps to steady green/orange state. Bus 0 port 6 and bus 0 port 3 toggle heavily regardless of charger state, and charger attach briefly correlates with bus 0 port 5, bus 0 port 12, and bus 1 port 3, but none of those behaves like the steady green LED enable.

The current uploaded build is no longer another blind mutation attempt. It is mode 12, a passive syscon caller trace that logs raw return addresses and a taiHEN module inventory without importing firmware-version-specific `SceModulemgrForKernel`. The purpose is to identify the owner of `0x089A`, `0x0387`, and standby/wake-related traffic before any more hardware mutation is attempted.

The most important current finding:

`0x0387` packet mutation is sufficient to disturb standby/wake behavior. Mode 1, which mutates only the `FF/01` form, is unsafe by itself:

- first suspend/wake: touch unavailable after wake until d-pad was pressed, then recovered,
- second suspend/wake without reboot: standby blink count about 26, touch did not recover by button presses,
- steady green LED still lit after wake.

Known-safe baseline:

- mode 0, GPIO-only, no syscon debug handlers, no syscon hooks: touch wakes normally and power LED behaves stock.
- mode 5, syscon debug handlers only: touch wakes normally.
- mode 6, syscon command hooks pass-through: touch wakes normally.
- mode 6 with charger plug/unplug plus standby/wake: charger LED color changes normally, standby blink is stock, touch wakes normally, and no packet mutation markers appear.
- mode 7, DolceLED pass-through trace: charger LED color changes normally, standby blink is stock, touch wakes normally, and no packet mutation markers appear.
- mode 10, read-only GPIO sampler: charger color changes normally, no GPIO writes/hooks/mutations, but no steady-green GPIO control line was identified.

Therefore the current evidence points at actual `0x0387` mutation, not syscon tracing or pass-through command hooks.

## Public Research References

Original upstream issue:

- <https://github.com/rereprep/noled/issues/3>
- Opened on 2018-10-02.
- Still open as checked on 2026-05-12.
- The reported symptom matches this hardware session: the original plugin does not disable the Vita 2000/Slim green power LED.

VitaSDK syscon documentation:

- <https://docs.vitasdk.org/syscon_8h.html>
- Local installed header: `/Users/ovos/vitasdk/arm-vita-eabi/include/psp2kern/kernel/syscon.h`
- Local NID db: `/Users/ovos/vitasdk/share/vita-headers/db/360/SceSyscon.yml`

Relevant documented/exported facts:

- `SceSysconPacket` is `0x80` bytes and carries fixed-size `tx[32]` and `rx[32]` buffers.
- `ksceSysconSetDebugHandlers()` installs packet start/end callbacks.
- `ksceSysconCmdExec()`, `ksceSysconCmdExecAsync()`, `ksceSysconCmdSync()`, `ksceSysconReadCommand()`, and `ksceSysconSendCommand()` are exported by `SceSysconForDriver`.
- `ksceSysconCtrlLED(int led, int enable)` is documented as generic LED control, but the docs do not identify a Vita Slim green/orange power LED id.
- The VitaSDK 3.60 NID DB lists `ksceSysconCtrlLED` as `0x04EC7579` and `ksceSysconCtrlDolceLED` as `0x727F985A`.
- `ksceSysconGetTouchpanelDeviceInfo()` also exists in `SceSysconForDriver`, which is relevant only as a caution: syscon is involved with touch-panel state too, so LED-looking packet mutation can plausibly disturb wake/touch behavior.

No public source found so far gives a known safe Vita Slim software call that turns off only the steady green powered-on LED while leaving orange charging behavior intact.

## Safety Rules

Do not install these experimental builds as a boot plugin.

Use only the bundled VPK loader installed through VitaShell:

`ux0:/data/noled_loader.vpk`

Reboot between hardware test runs. The runtime-loaded kernel plugin intentionally stays resident until reboot.

Avoid these paths unless deliberately testing recovery:

- `ksceSysconCtrlLED` hook, even pass-through, caused unsafe wake/LCD behavior.
- direct `ksceSysconCtrlLED(led, 0)` probe loops caused black screen/freeze/poweroff.
- repeated `ksceSysconCtrlDolceLED(0)` probe loops interacted with standby/wake and are removed.

If the Vita becomes unresponsive, use long power hold recovery and reboot before further tests.

## Build And Upload

Local workspace:

`/Users/ovos/work/noled`

VitaSDK:

`/Users/ovos/vitasdk`

Build:

```sh
cmake --build build -j8
```

Upload to VitaShell FTP:

```sh
curl --fail --show-error --connect-timeout 4 --speed-time 8 --speed-limit 1 --max-time 16 \
  -T build/noled_loader.vpk \
  ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk
```

Verify uploaded VPK:

```sh
curl --fail --show-error --connect-timeout 4 --speed-time 8 --speed-limit 1 --max-time 16 \
  -o /private/tmp/noled_loader_uploaded.vpk \
  ftp://192.168.1.117:1337/ux0:/data/noled_loader.vpk

cmp -s build/noled_loader.vpk /private/tmp/noled_loader_uploaded.vpk
shasum -a 256 build/noled_loader.vpk /private/tmp/noled_loader_uploaded.vpk
```

Pull logs:

```sh
curl --fail --show-error --connect-timeout 4 --speed-time 8 --speed-limit 1 --max-time 16 \
  -o /private/tmp/noled_loader.log \
  ftp://192.168.1.117:1337/ux0:/data/noled/loader.log

curl --fail --show-error --connect-timeout 4 --speed-time 8 --speed-limit 1 --max-time 16 \
  -o /private/tmp/noled_syscon_trace.log \
  ftp://192.168.1.117:1337/ux0:/data/noled/syscon_trace.log

curl --fail --show-error --connect-timeout 4 --speed-time 8 --speed-limit 1 --max-time 16 \
  -o /private/tmp/noled_module_map.log \
  ftp://192.168.1.117:1337/ux0:/data/noled/module_map.log
```

Latest uploaded diagnostics build:

- Vita path: `ux0:/data/noled_loader.vpk`
- VPK SHA-256: `b570059ba0bc1437543041fb23f9cbe3c98a842d0e175fdec7772c2c1f271c27`
- Bundled SKPRX SHA-256: `551d9ab11201427279ee5362b0a81c28ced03038e3ba1557a8aeb12173c00f10`
- Default mode: `12 - NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE`
- Uploaded copy verified by downloading to `/private/tmp/noled_loader_uploaded_caller_trace_no_modulemgr_import.vpk` and byte-comparing.
- Left/Right before Cross can switch to mode 10 read-only GPIO sampler; after Cross the mode is intentionally fixed until reboot.
- Previous mode-12 build failed with `0x8002D003` / `SCE_KERNEL_ERROR_MODULEMGR_NO_LIB` because it imported `SceModulemgrForKernel`; this import has been removed.

## Code Map

Core plugin:

- `main.c`: runtime kernel plugin, hooks, syscon tracer, packet mutation.
- `led_policy.h`: original GPIO LED block policy.
- `led_candidate_patch.h`: mode constants and `0x0387` mutation logic.
- `dolce_led_policy.h`: DolceLED hook policy.
- `syscon_experiment_policy.h`: gates which syscon layers activate per mode.
- `syscon_trace.h`: packet parsing helpers and trace filters.

Loader app:

- `loader/main.c`: VPK app, writes mode config, copies bundled `noled.skprx`, loads plugin with `taiLoadStartKernelModule`.

Mode config on Vita:

`ux0:data/noled/led_candidate.bin`

Runtime plugin copied to:

`ux0:data/noled/noled.skprx`

Logs:

- `ux0:data/noled/loader.log`
- `ux0:data/noled/syscon_trace.log`
- `ux0:data/noled/module_map.log`
- `ux0:data/noled/gpio_sample.log` for mode 10 only.

## Current Modes

Mode 0: `NOLED_LED_CANDIDATE_PASS`

- GPIO hook only.
- No syscon trace.
- No syscon command hooks.
- No packet mutation.
- Touch wake normal.
- Power LED behaves stock.

Mode 1: `NOLED_LED_CANDIDATE_FORCE_0387_FF_01_SAFE`

- Syscon trace and command hooks enabled.
- Mutates only `0x0387` packets matching `01 FF 00 00 01 00 00 00 xx`.
- Rewrites to `01 00 00 00 01 00 00 00 6A`.
- Hardware result: unsafe. Touch failed after wake and eventually did not recover without reboot. Green LED unchanged.

Mode 2: `NOLED_LED_CANDIDATE_FORCE_0387_FF_00_10`

- Syscon trace and command hooks enabled.
- Mutates only `0x0387` packets matching `01 FF 00 00 00 00 00 00 xx`.
- Rewrites to `01 10 00 00 00 00 00 00 5B`.
- Hardware result so far: safe but ineffective in the observed two-standby test. The FF/00 packet form did not appear in that run, so no mode-2 patch marker was logged.

Mode 3: `NOLED_LED_CANDIDATE_FORCE_0387_FF_BOTH`

- Syscon trace and command hooks enabled.
- Mutates both mode 1 and mode 2 packet forms.
- No DolceLED hook.
- Latest test: touch delayed after wake, standby blink longer than usual, steady green LED unchanged.

Mode 4: `NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH`

- Mode 3 plus DolceLED hook.
- `ksceSysconCtrlDolceLED(0)` and `(1)` are forced to `0`.
- `ksceSysconCtrlDolceLED(2)` is now passed through because blocking it removed blink and correlated with touch not resuming.
- Repeated DolceLED off probe thread was removed.

Mode 5: `NOLED_LED_CANDIDATE_SYSCON_LED_TRACE_ONLY`

- Syscon debug handlers only.
- No syscon command hooks.
- No packet mutation.
- No DolceLED hook.
- Touch wake normal in hardware test.

Mode 6: `NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS`

- Syscon debug handlers and command hooks.
- No packet mutation.
- No DolceLED hook.
- Touch wake normal in hardware test.

Mode 7: `NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS`

- Syscon debug handlers and command hooks.
- DolceLED hook enabled only as pass-through trace.
- No packet mutation.
- No SysconCtrlLED hook.
- Purpose: determine whether `ksceSysconCtrlDolceLED(enable)` is called during charger attach/detach or standby/wake and what arguments Sony code requests.
- Current uploaded VPK SHA-256 for this test: `d6018bffe08d5d1041666998fd8f9fa405549ea0d06ce3706abb606f148fe321`.
- Hardware result: safe but ineffective. One real `ksceSysconCtrlDolceLED(2)` call was logged around the sleep path; charger green/orange transitions did not call DolceLED.

Mode 8: `NOLED_LED_CANDIDATE_GPIO_TRACE_PASS`

- Syscon debug handlers and command hooks.
- GPIO pass-through trace for `ksceGpioPortSet`, `ksceGpioPortClear`, `ksceGpioPortReset`, and `ksceGpioSetPortMode`.
- Original GPIO blocking remains only for bus 0 ports 6 and 7.
- No packet mutation.
- No DolceLED hook.
- No SysconCtrlLED hook.
- Current uploaded VPK SHA-256 for this test: `e81b77316e0fc6a0e8fafe354da32809c97dee6fe3925eeefcf1683cfef82851`.
- Hardware result: unsafe. Standby did not finish; green LED kept blinking beyond 100 blinks until forced reboot.

Mode 9: `NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK`

- No syscon trace.
- No syscon command hooks.
- No DolceLED hook.
- No extra GPIO clear/reset/mode hooks.
- Reuse only the original `ksceGpioPortSet` hook.
- Block `ksceGpioPortSet(0, 3)` and clear bus 0 port 3 once at module start.
- First test should avoid standby and only observe active-state green LED plus charger plug/unplug behavior.
- Current uploaded VPK SHA-256 for this test: `34666cb611c2a080704a8e660efd52549edd9a08f8a436fbd0169f82375a8389`.
- Hardware result: unsafe. Vita froze and powered off immediately after Cross/module load; green LED stayed lit during the failure.

Mode 10: `NOLED_LED_CANDIDATE_GPIO_READ_SAMPLER`

- Read-only GPIO sampler.
- No syscon trace.
- No syscon command hooks.
- No packet mutation.
- No DolceLED hook.
- No SysconCtrlLED hook.
- No GPIO hooks or writes.
- Logs sampled GPIO changes to `ux0:data/noled/gpio_sample.log`.
- Hardware result: safe but did not identify a stable GPIO line for steady green/orange power LED state.

Unsafe header-only mode 11: `NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED`

- Experimental SysconCtrlLED policy remains in headers/tests, but the unsafe hook was removed from `main.c`.
- Loader should not cycle into this mode.
- Do not re-enable without a careful recovery plan.

Mode 12: `NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE`

- Current uploaded default.
- Syscon debug handlers and command hooks enabled.
- Command hooks are pass-through only.
- No packet mutation.
- No DolceLED hook.
- No SysconCtrlLED hook.
- Writes `ux0:data/noled/module_map.log` using taiHEN module info for selected candidate modules.
- Syscon call lines keep raw `caller=0x...` return addresses.
- Does not import `SceModulemgrForKernel`; direct module-manager caller resolution caused `0x8002D003` on the tested firmware.
- Purpose: identify the firmware module/function issuing candidate power LED/syscon traffic before attempting another mutation.

## Important Reverse-Engineering Notes

VitaSDK 3.60 headers/db expose these relevant exports:

- `ksceSysconCtrlLED(int led, int enable)`, NID `0x04EC7579`.
- `ksceSysconCtrlDolceLED(int enable)`, NID `0x727F985A`.
- `ksceLedSetMode`, NID `0xEA24BE03`.

Observed behavior:

- `ksceLedSetMode`/public LED helpers do not affect Slim green/orange power LED.
- Original GPIO path controls Home/game-card style LEDs, not Slim power LED.
- Charger attach/detach updates power status and LED changes green/orange normally; that behavior appears managed outside the paths that were safe to mutate.
- `ksceSysconCtrlDolceLED(2)` occurs during standby/wake and appears tied to the normal standby blink path. Blocking state `2` removed the blink and correlated with worse touch/LCD wake behavior.
- `ksceSysconCtrlDolceLED(0)` calls return success, can affect blink behavior, but do not turn off steady green after wake.
- `ksceSysconCtrlLED` is unsafe to hook on this hardware in this plugin context. Even pass-through hook installation caused LCD/touch wake problems. Direct calls in a probe loop caused freeze/poweroff.
- The steady green power LED may be asserted by a lower-level syscon/PMIC policy during powered-on state, not by the exported APIs tested so far.
- Clean mode-6 charger/wake trace shows `0x089A` traffic near charger state changes. This is evidence of charger/power-state involvement only; it is not evidence that `0x089A` directly controls the green power LED.
- Mode-7 DolceLED pass-through trace showed no real DolceLED calls during charger green/orange transitions. The only real call was `enable=2` around the sleep path.
- Mode-10 read-only GPIO sampler showed no stable GPIO line for steady green/orange state; bus0 port6 and bus0 port3 toggle frequently in both charger states.
- The current next path is caller mapping plus static reverse engineering, not another blind write/probe.

## Evidence Chain

1. GPIO-only noLED is safe but does not control green power LED.
2. Syscon debug trace only is safe.
3. Syscon command hooks pass-through are safe.
4. Mode 1 `0x0387 FF/01` mutation alone causes touch wake failure and longer blink. Green LED stays lit.
5. DolceLED hook can change blink behavior, but steady green remains.
6. SysconCtrlLED hook/probes are unsafe and should remain disabled.
7. Clean mode-6 charger/wake trace keeps stock behavior and shows no patch markers, which supports continuing with trace-only semantic hooks before any further mutations.
8. Mode-7 DolceLED pass-through trace keeps stock behavior and shows only `ksceSysconCtrlDolceLED(2)` around sleep, not charger transitions.
9. Broad GPIO trace mode 8 is unsafe, but identified bus 0 port 3 as a strong blinking/power-indicator candidate.
10. Blocking/clearing GPIO bus 0 port 3 in mode 9 is unsafe and does not suppress steady green before failure.
11. Mode 10 read-only GPIO sampler is safe but did not reveal a stable GPIO-backed steady power LED control.
12. Mode 12 is now used to map syscon caller addresses to firmware modules and offsets for static RE.

## Latest Mode 1 Log Evidence

Pulled after the user reported mode 1 touch wake failure:

- `/private/tmp/noled_loader_after_mode1_touch_stuck.log`
- `/private/tmp/noled_syscon_after_mode1_touch_stuck.log`

Hashes:

- loader log: `52fe1a70ff610f42437cd7d63a37b99da5a7940b17b5178b903f7e7229d1ca0b`
- syscon log: `09686cb534d0384025a793dd50517c59ae3a16a3e2c30f145a1bd77b0f1ee3a8`

Loader evidence:

- `candidate initial: mode=1 result=0x00000000`
- `taiLoadStartKernelModule: 0x4001014F`
- `leaving tracer resident: 0x4001014F`

Syscon evidence:

- `M marker=led_candidate_patch led=1 enable=1 ret=0x03870001`
- `A cmd=0x0387 ... tx=01FF000001000000FF`
- `S cmd=0x0387 ... tx=01000000010000006A`
- The `FF/00` form was observed in the same log but did not produce a `led_candidate_patch` marker in mode 1.

## Latest Mode 6 Charger/Wake Trace Evidence

Pulled after the user reported stock behavior in mode 6:

- `/private/tmp/noled_loader_after_mode6_charger_wake_trace.log`
- `/private/tmp/noled_syscon_after_mode6_charger_wake_trace.log`

Hashes:

- loader log: `0f8d3762c3a76b974a278fc4165d7d01707f69de7667f6024be8649964ca5a2c`
- syscon log: `fae906573a8dfe89b326b424f86bf562dc6dbc0787fd2514b5a5eda2950a9eb3`

Loader evidence:

- latest run contains `candidate initial: mode=6 result=0x00000000`
- latest run contains `taiLoadStartKernelModule: 0x4001014F`
- latest run contains `leaving tracer resident: 0x4001014F`

Syscon evidence:

- no `M marker=led_candidate_patch` lines.
- no `M marker=dolce_led_hook` lines.
- charger attach/detach power markers include seq 118/131 and seq 279/301.
- `0x089A` appears near charger transitions, including `tx=00005A` and `rx=DDF7`.
- standby/wake includes `0x0387` forms passed through unchanged.

User-observed hardware result:

- green/orange charger color switching behaved normally,
- standby blink count was the usual 10,
- touch worked normally after wake,
- steady green LED remained lit after wake.

## Latest Mode 7 DolceLED Pass-Through Trace Evidence

Pulled after the user reported stock behavior in mode 7:

- `/private/tmp/noled_loader_after_mode7_dolce_trace.log`
- `/private/tmp/noled_syscon_after_mode7_dolce_trace.log`

Hashes:

- loader log: `5c01dcf0aa9534ace52822e0a8b582bb2f3fd72938692bddd0cd1f919ab571ae`
- syscon log: `1ad2535bb2f67901020d474ba2b30c3b2824c6dd6f5c3c4e267ff255218fb665`

Loader evidence:

- latest run contains `candidate initial: mode=7 result=0x00000000`
- latest run contains `taiLoadStartKernelModule: 0x4001014F`
- latest run contains `leaving tracer resident: 0x4001014F`

Syscon evidence:

- no `M marker=led_candidate_patch` lines.
- hook install marker: `M marker=dolce_led_hook led=0 enable=1 ret=0x00062A57`.
- one real hook call: `M marker=dolce_led_hook led=2 enable=2 ret=0x00000000`.
- charger attach/detach power markers happened without real DolceLED calls.
- standby/wake includes `0x0387` forms passed through unchanged.

User-observed hardware result:

- no visible power LED behavior change,
- standby blink count was the usual 10,
- touch worked normally after wake,
- steady green LED remained lit after wake.

## Latest Mode 8 GPIO Trace Evidence

Pulled after the user reported standby stuck blinking beyond 100 blinks:

- `/private/tmp/noled_loader_after_mode8_gpio_standby_stuck.log`
- `/private/tmp/noled_syscon_after_mode8_gpio_standby_stuck.log`

Hashes:

- loader log: `26ad0df3843a7d0e068eab9f7e55519a5a02106d0ba4d8f553f2d61fe1e5be89`
- syscon log: `43e8b83cb25ac6594008d60aedce3750192b1e0a596018edbdba17cd0e47c97f`

Loader evidence:

- latest run contains `candidate initial: mode=8 result=0x00000000`
- latest run contains `taiLoadStartKernelModule: 0x4001014F`
- latest run contains `leaving tracer resident: 0x4001014F`

Syscon/GPIO evidence:

- no `M marker=led_candidate_patch` lines.
- log has 19,309 lines and sequence numbers up to 54,106.
- repeated `lost from=...` ranges show ring overflow from GPIO event volume.
- hot GPIO counts:
  - `gpio_port_clear bus=0 port=3`: 7,955
  - `gpio_port_set bus=0 port=3`: 2,650
  - `gpio_port_clear bus=0 port=2`: 2,125
  - `gpio_port_clear bus=1 port=0`: 2,126
  - `gpio_port_clear bus=0 port=7`: 2,127
  - `gpio_port_clear bus=0 port=6`: 1,271
  - `gpio_port_set bus=0 port=6`: 854
- final log tail repeats the same GPIO pattern and ends with `gpio_port_clear led=0 enable=7`.

User-observed hardware result:

- standby did not finish,
- green LED kept blinking beyond 100 blinks,
- long power hold was required to recover.

Interpretation:

Do not repeat broad GPIO tracing. Hooking/logging `ksceGpioPortClear` at this volume is unsafe during standby. The useful signal is that bus 0 port 3 is heavily toggled during the blink path, so the next safe-ish test should isolate bus 0 port 3 using only the already-tested `ksceGpioPortSet` hook and no syscon trace.

## Latest Mode 9 GPIO Port 3 Block Evidence

User-observed hardware result:

- after pressing Cross, Vita froze and powered off,
- green power LED stayed lit during the failure,
- no standby was tested.

No logs were available at the time of the report because the unit powered off.

Interpretation:

Do not repeat mode 9. GPIO bus 0 port 3 appears to participate in blink/standby state, but forcing it low or blocking its set path is not a safe steady-green LED solution. The fact that the green LED stayed lit during the failure argues against bus 0 port 3 being the sole steady-green power LED enable.

Interpretation:

The `FF/01` rewrite alone is sufficient to reproduce the touch wake problem. It still does not suppress the steady green LED.

## Latest Mode 2 Log Evidence

Pulled after the user reported mode 2 was safe but did not affect the green LED:

- `/private/tmp/noled_loader_after_mode2_safe_ineffective.log`
- `/private/tmp/noled_syscon_after_mode2_safe_ineffective.log`

Hashes:

- loader log: `0be18dd286c75f2d86eb66d263831c731a8bf264ebd4aff6cf3bec9e4dff167f`
- syscon log: `6824d85725f1c3cd3e716822988fb1b3ed83b8a0fd23fe3ca25c75bf30997e1e`

Loader evidence:

- `candidate initial: mode=2 result=0x00000000`
- `taiLoadStartKernelModule: 0x4001014F`
- `leaving tracer resident: 0x4001014F`

User hardware result:

- green LED behaved normally,
- first standby: 10 blinks,
- first wake: LED restored, touch worked immediately,
- second standby/wake without reboot: normal again, 10 blinks and touch worked immediately.

Syscon evidence:

- No `M marker=led_candidate_patch` lines were present.
- The unsafe FF/01 form appeared and was passed through unchanged in mode 2:
  `A cmd=0x0387 ... tx=01FF000001000000FF`
- The FF/00 form `01FF000000000000xx` did not appear in this run.

Interpretation:

Mode 2 did not perturb standby/wake in this run, but it also did not actually exercise its mutation path. It does not provide evidence for a green LED control path.

## Latest Mode 3 Log Evidence

Pulled after the latest user report where touch was delayed after wake and standby blink was longer:

- `/private/tmp/noled_loader_after_mode3_touch_delay.log`
- `/private/tmp/noled_syscon_after_mode3_touch_delay.log`

Hashes:

- loader log: `28c6fe1ac3589f4d94e2608b43c70107bc182c411dfe8e155c4663853ae25b6b`
- syscon log: `ce950be5179384551780883fd14443675d657d9ee145aac5fe7956feaa9f9c21`

Loader evidence:

- `candidate initial: mode=3 result=0x00000000`
- `taiLoadStartKernelModule: 0x4001014F`
- `leaving tracer resident: 0x4001014F`

Syscon evidence:

- `M marker=led_candidate_patch led=1 enable=3 ret=0x03870001`
- `A cmd=0x0387 ... tx=01FF000001000000FF`
- `S cmd=0x0387 ... tx=01000000010000006A`
- `M marker=led_candidate_patch led=1 enable=3 ret=0x03870001`
- `A cmd=0x0387 ... tx=01FF0000000000005B`
- `S cmd=0x0387 ... tx=01100000000000005B`

Interpretation:

The bad run definitely mutated both tracked `0x0387` packet forms. That means mode 3 is not enough to identify which half of the mutation causes the delayed touch wake. The next build must isolate mode 1, then mode 2.

## Current Hypothesis

The green LED steady-on path on Vita Slim is not exposed through the safe public LED helpers, and not cleanly controlled by the tested `0x0387`, DolceLED, or SysconCtrlLED approaches.

`0x0387` likely participates in standby/wake or LED/touch-related syscon state. Mutating it can change blink timing, but also disturbs touch resume. It is not safe as currently patched.

The `0x0387` branch is currently weak:

- mode 1 directly breaks touch wake and does not turn off green.
- mode 2 was safe but did not encounter the target FF/00 packet in the latest run and did not turn off green.

Do not use `0x0387` mutation for production behavior unless a later log identifies a precise packet that is both safe and causally tied to the steady green LED.

## Known VPK Hashes

These are useful for matching user reports to code behavior:

- `ad1df56318481cf5f5569f71d1ddba8bab946e7fda086eebd7bf3c866716b0d8`: DolceLED hook/probe plus 0387 both.
- `ea113bd463e368ec9a44cdb16ef078db091ed343e786169212c89b012d43c6ee`: unsafe SysconCtrlLED force/probe build, caused freeze/poweroff.
- `363208c24ae3cd6e23052ffb4ea99a8f4eec8c667968dcb78d8beb9a8fe3ad10`: SysconCtrlLED trace-only/pass-through hook, caused LCD/touch wake problems.
- `875b1af40fb80d508230da9624f9df79df28e1d7295df93baf804dd9b49ab3cc`: rollback without SysconCtrlLED hook, mode 4 default.
- `0f8ba1e851a87873f586dcb5f874a511d274abaf0a42bbf59967e26bf53d9472`: DolceLED state `2` pass-through, probe removed, mode 4 default.
- `f4513b6b184d0fe48ee6559deb1103cbe98ec86c544d55ed29f70ebc702f4925`: GPIO-only baseline default mode 0, touch normal.
- `6d7a0391968786431351b3c9a0cf3dd0cc46fdc903582e04b29a23d70e4e7f45`: syscon debug trace only default mode 5, touch normal.
- `eabe2de19286a618df62e9e64302c9aadba7b890263ec3f978d2b07f58409c36`: syscon command hooks pass-through default mode 6, touch normal.
- `3d44ce817ad11a907077a57bacc949aecf695a08c4b08e0cbff31e5dcb007856`: 0387 mutation-only default mode 3, touch delayed and blink longer.
