# Vita Slim noLED Hardware Test Matrix

This matrix records hardware-observed behavior on Vita Slim/PCH-2000. It is intentionally conservative: if a behavior was not directly observed, it is marked unknown rather than inferred.

## Legend

- Touch OK: touch input works immediately after LCD wakes.
- Touch delayed: touch unavailable for several seconds after LCD wakes, then recovers.
- Touch stuck: touch did not recover without reboot in that test.
- LED steady green: the normal powered-on green LED after wake.
- Blink: standby LED blinking near the end of sleep transition.

## Results

| Mode / Build | Active Pieces | Power LED Result | Touch/LCD Result | Safety |
| --- | --- | --- | --- | --- |
| Original GPIO noLED | GPIO hook only | Green power LED unchanged | Normal | Safe |
| Public LED helpers / `ksceLedSetMode` | SceLed helper attempts | No visible green/orange effect | No useful effect | Ineffective |
| Charger observation | No mutation | LED switches green/orange with charger state | Normal | Observation only |
| 0x089A patch attempts | Syscon mutation attempts | Charger state changed/logged, green not off | No solution | Abandoned |
| 0x088F / 0x088A / 0x088E probes | Syscon mutation attempts | No reliable green-off result | Some instability across tests | Abandoned |
| Mode 5 | Syscon debug handlers only | Stock blink/steady green | Touch OK | Safe |
| Mode 6 | Syscon command hooks pass-through | Stock blink/steady green; charger green/orange transitions logged | Touch OK | Safe |
| Mode 7 | Syscon command hooks pass-through plus DolceLED pass-through trace | Stock blink/steady green; charger green/orange transitions did not call DolceLED | Touch OK | Safe, ineffective |
| Mode 8 | Broad GPIO pass-through trace plus existing non-power GPIO block | Standby blink continued beyond 100 blinks | Standby stuck until forced reboot | Unsafe |
| Mode 9 | Block GPIO bus 0 port 3 set only, no syscon trace | Green stayed lit during failure | Freeze/poweroff after Cross/module load | Unsafe |
| Mode 10 | Read-only GPIO sampler, no hooks/writes | Green/orange behavior stock; no stable GPIO control line found | Normal in observed charger test | Safe, diagnostic only |
| Mode 12 | Passive syscon caller-map trace | Not tested yet on hardware | Unknown; based on mode 6 pass-through path | Superseded upload |
| Mode 13 | Runtime LED probe: one whitelisted command per press, no hooks | Not tested yet on hardware | No hooks installed; single wrapper calls only | Current build, awaiting hardware test |
| Mode 1 | 0387 FF/01 mutation only | About 26 standby blinks on second suspend, steady green unchanged | Touch delayed then stuck after second wake | Unsafe |
| Mode 2 | 0387 FF/00 mutation only | Normal 10 blinks, steady green unchanged | Touch OK on two standby/wake cycles | Safe in observed run, ineffective; mutation path not hit |
| Mode 3 | 0387 both-form mutation, no DolceLED | Blink longer than usual, steady green unchanged | Touch delayed | Unsafe for production |
| Mode 4 earlier | 0387 mutation + DolceLED forced off | No final standby blink, steady green unchanged | Touch delayed/stuck in some tests | Unsafe |
| Mode 4 with DolceLED state 2 pass-through | 0387 mutation + DolceLED hook, state 2 allowed | Blink returned, steady green unchanged | Touch still delayed | Unsafe due 0387 |
| SysconCtrlLED direct probe | Direct `ksceSysconCtrlLED(0..7, 0)` loop | No useful green-off result | Black screen/freeze/poweroff | Unsafe |
| SysconCtrlLED pass-through hook | Hook installed, pass-through | No useful green-off result | LCD/touch wake broke | Unsafe |

## Current Isolation Results

The sequence below is the strongest evidence so far:

1. Mode 0 GPIO-only: touch normal.
2. Mode 5 syscon debug trace only: touch normal.
3. Mode 6 syscon command hooks pass-through: touch normal.
4. Mode 1 command hooks plus `0387 FF/01` mutation: touch delayed then stuck.
5. Mode 3 command hooks plus both 0387 mutations: touch delayed.

Conclusion: command-hook installation itself is not the touch problem. The `0387 FF/01` mutation is already enough to break touch wake behavior.

Latest pulled mode-3 logs:

- `/private/tmp/noled_loader_after_mode3_touch_delay.log`
- `/private/tmp/noled_syscon_after_mode3_touch_delay.log`

The syscon log contains two `led_candidate_patch` markers in mode 3:

- `01 FF 00 00 01 00 00 00 xx` rewritten to `01 00 00 00 01 00 00 00 6A`.
- `01 FF 00 00 00 00 00 00 xx` rewritten to `01 10 00 00 00 00 00 00 5B`.

This is why the next tests are mode 1 and mode 2 separately.

Mode 1 log evidence:

- `/private/tmp/noled_loader_after_mode1_touch_stuck.log`
- `/private/tmp/noled_syscon_after_mode1_touch_stuck.log`
- Log hashes: `52fe1a70ff610f42437cd7d63a37b99da5a7940b17b5178b903f7e7229d1ca0b`, `09686cb534d0384025a793dd50517c59ae3a16a3e2c30f145a1bd77b0f1ee3a8`
- Two patch markers: `M marker=led_candidate_patch led=1 enable=1 ret=0x03870001`
- Observed patch: `01 FF 00 00 01 00 00 00 xx` -> `01 00 00 00 01 00 00 00 6A`
- User result: first wake touch restored after d-pad; second wake touch did not recover by button presses; green LED unchanged.

Mode 2 log evidence:

- `/private/tmp/noled_loader_after_mode2_safe_ineffective.log`
- `/private/tmp/noled_syscon_after_mode2_safe_ineffective.log`
- Log hashes: `0be18dd286c75f2d86eb66d263831c731a8bf264ebd4aff6cf3bec9e4dff167f`, `6824d85725f1c3cd3e716822988fb1b3ed83b8a0fd23fe3ca25c75bf30997e1e`
- No `led_candidate_patch` markers were present.
- The FF/01 form appeared and was passed through unchanged, as expected in mode 2.
- The FF/00 form did not appear in this run.
- User result: green LED normal, 10 blinks, touch normal immediately on two wake cycles.

Mode 6 charger/wake trace evidence:

- `/private/tmp/noled_loader_after_mode6_charger_wake_trace.log`
- `/private/tmp/noled_syscon_after_mode6_charger_wake_trace.log`
- Log hashes: `0f8d3762c3a76b974a278fc4165d7d01707f69de7667f6024be8649964ca5a2c`, `fae906573a8dfe89b326b424f86bf562dc6dbc0787fd2514b5a5eda2950a9eb3`
- No `led_candidate_patch` markers were present.
- No `dolce_led_hook` markers were present because mode 6 did not hook `ksceSysconCtrlDolceLED`.
- Charger attach/detach produced power markers at seq 118/131 and 279/301.
- `0x089A` traffic appeared near charger transitions; treat this as charger/power-state correlation, not as a proven LED-off control.
- User result: charger color switching normal, 10 standby blinks, steady green unchanged, touch OK.

Mode 7 DolceLED pass-through trace evidence:

- `/private/tmp/noled_loader_after_mode7_dolce_trace.log`
- `/private/tmp/noled_syscon_after_mode7_dolce_trace.log`
- Log hashes: `5c01dcf0aa9534ace52822e0a8b582bb2f3fd72938692bddd0cd1f919ab571ae`, `1ad2535bb2f67901020d474ba2b30c3b2824c6dd6f5c3c4e267ff255218fb665`
- No `led_candidate_patch` markers were present.
- Hook install marker: `M marker=dolce_led_hook led=0 enable=1 ret=0x00062A57`.
- One real DolceLED call: `M marker=dolce_led_hook led=2 enable=2 ret=0x00000000`.
- Charger attach/detach happened without real DolceLED calls.
- User result: no visible power LED behavior change, 10 standby blinks, touch OK.

Mode 8 broad GPIO trace evidence:

- `/private/tmp/noled_loader_after_mode8_gpio_standby_stuck.log`
- `/private/tmp/noled_syscon_after_mode8_gpio_standby_stuck.log`
- Log hashes: `26ad0df3843a7d0e068eab9f7e55519a5a02106d0ba4d8f553f2d61fe1e5be89`, `43e8b83cb25ac6594008d60aedce3750192b1e0a596018edbdba17cd0e47c97f`
- No `led_candidate_patch` markers were present.
- Log reached seq 54,106 with repeated ring-overflow `lost from=...` ranges.
- Hot GPIO counts: bus 0 port 3 clear 7,955; bus 0 port 3 set 2,650; bus 0 port 2 clear 2,125; bus 1 port 0 clear 2,126; bus 0 port 7 clear 2,127; bus 0 port 6 clear 1,271; bus 0 port 6 set 854.
- User result: standby stuck with green LED blinking beyond 100 blinks; forced reboot required.
- Interpretation: broad GPIO clear/set tracing is unsafe, but bus 0 port 3 is a strong blink/power-indicator candidate.

Mode 9 GPIO bus 0 port 3 block evidence:

- VPK SHA-256: `34666cb611c2a080704a8e660efd52549edd9a08f8a436fbd0169f82375a8389`
- User result: after pressing Cross, Vita froze and powered off; green LED stayed lit during the failure.
- No standby was tested.
- No logs were available at the time of the report because the unit powered off.
- Interpretation: do not repeat. Bus 0 port 3 is not a safe steady-green LED off path.

Mode 10 read-only GPIO sampler evidence:

- `/private/tmp/noled_loader_after_gpio_read_diagnostics.log`
- `/private/tmp/noled_gpio_sample_after_diagnostics.log`
- Log hashes: `3c3b8d6da02a1d318702400b4b144896e0b1de8af763588599bb2b1f866a8548`, `5dfd2e65c2cf3fd502923b80866586f45a701292bc37deaae4f5dc5bc8375aed`
- No hooks, no GPIO writes, no syscon trace, no packet mutation.
- Busiest sampled lines: bus0 port6 offline 742 / online 57; bus0 port3 offline 224 / online 21; bus0 port4 offline 35 / online 4.
- Charger attach briefly correlated with bus0 port5, bus0 port12, and bus1 port3 transitions.
- Interpretation: no stable sampled GPIO state maps cleanly to steady green/orange power LED state.

Mode 12 passive syscon caller-map trace:

- Previous VPK SHA-256 `ba77b5edef5f0459658958d3182d1c3fdf583dfea79d0bedf145a99f94d13dc8` failed to load with `0x8002D003`, which VitaSDK identifies as `SCE_KERNEL_ERROR_MODULEMGR_NO_LIB`.
- Root cause: direct `SceModulemgrForKernel` import is firmware-version-specific and was unavailable on the tested firmware.
- Current uploaded VPK SHA-256: `b570059ba0bc1437543041fb23f9cbe3c98a842d0e175fdec7772c2c1f271c27`.
- Bundled SKPRX SHA-256: `551d9ab11201427279ee5362b0a81c28ced03038e3ba1557a8aeb12173c00f10`.
- Writes raw `caller=0x...` syscon call return addresses and a taiHEN module inventory in `ux0:data/noled/module_map.log`.
- No mutation and no DolceLED/SysconCtrlLED hook.
- Purpose: map candidate syscon traffic to firmware modules/functions before further static reverse engineering.
- First corrected run loaded successfully and wrote module info, but captured only periodic polling caller `0x00BCAF9F`; no `0x089A` or `0x0387` transition traffic appeared. Do not repeat the same mode-12 plug/unplug flow unchanged.

## Next Test Slots

Use a reboot before each test.

### Replacement Upload: Safe Baseline

Purpose: replace the unsafe mode-9 VPK before further testing.

Expected default should be a known-safe mode:

`NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS`

or GPIO-only mode 0 if no tracing is needed.

### Completed Test F: GPIO Bus 0 Port 3 Block

Purpose: test whether the high-volume bus 0 port 3 GPIO toggles are the green power indicator path, without broad tracing or syscon hooks.

Expected upload default should be the new port-3 block mode:

`NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK`

Built for this test:

`34666cb611c2a080704a8e660efd52549edd9a08f8a436fbd0169f82375a8389`

Upload verified:

- Vita path: `ux0:/data/noled_loader.vpk`
- Downloaded copy: `/private/tmp/noled_loader_uploaded_mode9_gpio_port3_block.vpk`
- Byte compare: passed.

Observe:

- Does the steady green LED change while the app is active after Cross?
- Does charger attach still turn orange?
- Does charger detach restore green or stay off?

Do not test standby in the first run.

Mutation policy:

- Block `ksceGpioPortSet(0, 3)`.
- Clear bus 0 port 3 once at module start.
- Existing original GPIO blocking remains for known non-power LEDs.
- No syscon trace.
- No syscon packet mutation.
- No DolceLED hook.
- No SysconCtrlLED hook.

### Completed Test E: GPIO Pass-Through Trace

Purpose: determine whether Slim power LED transitions appear as GPIO operations on a bus/port not covered by the original noLED policy.

Expected upload default should be mode 8:

`NOLED_LED_CANDIDATE_GPIO_TRACE_PASS`

Built for this test:

`e81b77316e0fc6a0e8fafe354da32809c97dee6fe3925eeefcf1683cfef82851`

Upload verified:

- Vita path: `ux0:/data/noled_loader.vpk`
- Downloaded copy: `/private/tmp/noled_loader_uploaded_mode8_gpio_trace.vpk`
- Byte compare: passed.

Observe:

- Any `gpio_port_set`, `gpio_port_clear`, `gpio_port_reset`, or `gpio_set_port_mode` markers near charger plug/unplug.
- Any GPIO markers near standby blink and wake.
- Whether merely tracing extra GPIO functions affects blink, wake, LCD, or touch.

Mutation policy:

- Existing original GPIO blocking remains only for known non-power LEDs.
- No syscon packet mutation.
- No DolceLED forcing.
- No SysconCtrlLED hook.

### Completed Test D: DolceLED Pass-Through Trace

Purpose: capture semantic `ksceSysconCtrlDolceLED(enable)` calls during charger transitions and standby/wake without changing the requested value.

Expected upload default should be mode 7:

`NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS`

Built for this test:

`d6018bffe08d5d1041666998fd8f9fa405549ea0d06ce3706abb606f148fe321`

Upload verified:

- Vita path: `ux0:/data/noled_loader.vpk`
- Downloaded copy: `/private/tmp/noled_loader_uploaded_mode7_dolce_trace.vpk`
- Byte compare: passed.

Observe:

- Are `dolce_led_hook` markers logged during charger plug/unplug?
- Are `dolce_led_hook` markers logged during standby/wake?
- What requested `enable` values are seen?
- Does merely pass-through hooking DolceLED affect blink, wake, LCD, or touch?

Mutation policy:

- No packet mutation.
- DolceLED hook only logs and passes through the requested value.
- No SysconCtrlLED hook.

### Completed Test C: Pass-Through Charger/Wake Trace

Purpose: capture a clean trace around the only known real LED color transitions, without any mutation.

Expected upload default should be mode 6:

`NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS`

Built for this test:

`07d4151a710ce83365352c8ee175487a8d51962dc313b4f5adb141c7dec090c9`

Upload verified:

- Vita path: `ux0:/data/noled_loader.vpk`
- Downloaded copy: `/private/tmp/noled_loader_uploaded_mode6_trace.vpk`
- Byte compare: passed.

Observe:

- Charger attach: green to orange transition.
- Charger detach: orange to green transition.
- Standby blink count/timing.
- Wake green LED timing.
- Touch wake behavior.

Mutation policy:

- No packet mutation.
- No DolceLED hook.
- No SysconCtrlLED hook.

### Test A: Mode 1 Only

Purpose: isolate the `0387 FF/01` mutation.

Expected upload default should be mode 1:

`NOLED_LED_CANDIDATE_FORCE_0387_FF_01_SAFE`

Built for this test:

`4a1127570ad7b0d64bea41c2f58c03de3897330a2c9cdadf77e49e6b7007f8f2`

Upload verified:

- Vita path: `ux0:/data/noled_loader.vpk`
- Downloaded copy: `/private/tmp/noled_loader_uploaded_mode1.vpk`
- Byte compare: passed.

Observe:

- Does touch delay after wake?
- Does standby blink count/timing change?
- Does steady green power LED change?

Result:

- Green LED unchanged.
- 10 blinks on standby.
- Touch worked immediately on two wake cycles.
- No mode-2 patch marker appeared in the syscon log, so the target FF/00 packet form was not exercised in this test.

Result:

- Touch delayed after first wake and stuck after second wake.
- Standby blink count about 26 on second suspend.
- Steady green power LED unchanged.
- Unsafe. Do not use as production behavior.

### Test B: Mode 2 Only

Purpose: isolate the `0387 FF/00` mutation.

Expected upload default should be mode 2:

`NOLED_LED_CANDIDATE_FORCE_0387_FF_00_10`

Built for this test:

`0dad0477108a8c5024d44f22cb6e67f3943d8c947b9d941f3dd87f44be907c59`

Upload verified:

- Vita path: `ux0:/data/noled_loader.vpk`
- Downloaded copy: `/private/tmp/noled_loader_uploaded_mode2.vpk`
- Byte compare: passed.

Observe:

- Does touch delay after wake?
- Does standby blink count/timing change?
- Does steady green power LED change?

## Log Evidence To Capture After Each Test

Pull:

- `ux0:data/noled/loader.log`
- `ux0:data/noled/syscon_trace.log`

Search for:

```sh
rg -n "led_candidate|led_candidate_patch|dolce_led|0387|088E|088A|088F|0822|lost" /private/tmp/noled_syscon_trace.log
```

Important lines:

- `M marker=led_candidate_config led=<mode>`
- `M marker=led_candidate_patch`
- `A cmd=0x0387 ... tx=...`
- `S/E cmd=0x0387 ... tx=...`
