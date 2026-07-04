# Vita Slim Syscon Call-Site Analysis

Date: 2026-05-12

Inputs:

- `/private/tmp/noled_syscon_after_mode6_charger_wake_trace.log`
- `/private/tmp/noled_syscon_after_mode7_dolce_trace.log`
- `/private/tmp/noled_syscon_after_mode12_no_modulemgr.log`
- `/private/tmp/noled_module_map_after_mode12_no_modulemgr.log`

## Short Result

The useful transition traffic is in the earlier safe mode-6 and mode-7 traces, not in the latest corrected mode-12 trace. The corrected mode-12 run loaded successfully and wrote a module map, but its syscon trace captured only the periodic polling caller `0x00BCAF9F` plus one power-percent change. It did not capture `0x089A` or `0x0387`.

The next diagnostic build must therefore show live capture counters on the Vita screen and write a broader module map in the same boot. Repeating the old mode-12 flow without live confirmation is not useful.

## Low SceSyscon Caller Cluster

The mode-12 module map reports:

- `SceSyscon` exports: `0x00BD04FC-0x00BD053C`
- periodic polling caller: `0x00BCAF9F`

That caller is close below the SceSyscon export stub area and behaves like an internal SceSyscon text caller. The same periodic caller appears at different ASLR locations in older traces:

| Trace | Periodic caller | Normalize to mode-12 anchor |
| --- | ---: | ---: |
| mode 12 | `0x00BCAF9F` | `0x00000000` |
| mode 7 | `0x00B8AF9F` | `+0x00040000` |
| mode 6 | `0x00C1AF9F` | `-0x00050000` |

Using that anchor, the low callers normalize consistently:

| Old caller | Normalized current caller | Interpretation |
| ---: | ---: | --- |
| `0x00C1B349` / `0x00B8B349` | `0x00BCB349` | likely SceSyscon internal sync wrapper |
| `0x00C1AC11` / `0x00B8AC11` | `0x00BCAC11` | likely SceSyscon internal helper |
| `0x00C1B699` / `0x00B8B699` | `0x00BCB699` | likely SceSyscon internal helper |
| `0x00C1BA73` / `0x00B8BA73` | `0x00BCBA73` | likely SceSyscon internal helper |
| `0x00C1DAF5` / `0x00B8DAF5` | `0x00BCDAF5` | likely SceSyscon internal helper |
| `0x00C200CB` / `0x00B900CB` | `0x00BD00CB` | likely SceSyscon internal helper |

These low callers are probably not the real policy owner for the green/orange LED. They are the SceSyscon path through which other modules send commands.

## High Callers That Matter

These are the call sites that need module ownership or static reverse engineering.

### Command `0x089A`

Mode-6 trace:

- `A cmd=0x089A caller=0x018A70D5 tx=020000`
- `A cmd=0x089A caller=0x018A70D5 tx=000058`
- `A cmd=0x089A caller=0x018A70D5 tx=02005A`
- `Y cmd=0x089A caller=0x00C1B349 tx=040056`
- nearby `A cmd=0x0804 caller=0x018A7233`

Mode-7 trace:

- `A cmd=0x089A caller=0x018B70D5 tx=020000`
- `A cmd=0x089A caller=0x018B70D5 tx=000058`
- `Y cmd=0x089A caller=0x00B8B349 tx=040056`
- nearby `A cmd=0x0804 caller=0x018B7233`

Interpretation:

- The low `0x00..B349` caller is probably SceSyscon internal glue.
- The high `0x018A70D5` / `0x018B70D5` caller is more interesting and likely belongs to another kernel module that initiates charger/power-state work.
- `0x089A` is correlated with charger attach/detach, but it is not proven to be a direct LED-off control.

### Command `0x0387`

Mode-6 trace:

- `A cmd=0x0387 caller=0x017CC207 tx=0100000001000000FF`
- `A cmd=0x0387 caller=0x017CC207 tx=01FF000001000000FF`
- `A cmd=0x0387 caller=0x017CC643 tx=01100000000000006B`

Mode-7 trace:

- `A cmd=0x0387 caller=0x017D4207 tx=0100000001000000FF`
- `A cmd=0x0387 caller=0x017D4207 tx=01FF000001000000FF`
- `A cmd=0x0387 caller=0x017D4643 tx=01100000000000006B`

Hardware result:

- Mutating the `01 FF 00 00 01 00 00 00 xx` form disturbed standby/wake and touch.
- Steady green LED stayed lit.

Interpretation:

- `0x0387` is tied to standby/wake or device state, not a safe active-state green LED off switch.
- Do not mutate `0x0387` again without static analysis of the owning high caller.

## Current Unknown

The high caller modules are still unidentified. The first 16-name module map did not include enough modules, and the latest corrected mode-12 trace did not capture the transition traffic needed to pair call sites and module ranges in one run.

The next build addresses that with:

- live on-screen counters for `0x089A`, `0x0387`, `0x0804`, power events, hook markers, trace-ready, and module-map-ready;
- a broader taiHEN module query using all Sce module names from the installed VitaSDK 3.60 NID database;
- no syscon packet mutation, no SysconCtrlLED hook, no DolceLED hook, and no broad GPIO trace.
