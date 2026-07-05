# Changelog

## [v2.1](https://github.com/falkenhawk/noled/releases/tag/v2.1) - 2026-07-05

- release plugin slimmed down to the production policy only: 5.3 KB, under 1 KB of
  resident kernel memory (v2.0: 30 KB / ~186 KB)
- zero file I/O: no config file read, no logs written - fixes v2.0 appending a heartbeat
  line to `ux0:data/noled/probe.log` every 10 seconds without bound; `ux0:data/noled/`
  is no longer used and can be deleted
- all research/diagnostic machinery (syscon tracer, runtime probe, GPIO sampler, loader
  app, standalone cmd probe) moved to [`dev/`](dev/) - built on demand, never shipped
  in releases
- LED behavior unchanged, re-verified on PCH-2000 hardware

## [v2.0](https://github.com/falkenhawk/noled/releases/tag/v2.0) - 2026-07-04

- first known software control of the PCH-2000 (Vita 2000 / Slim) green power LED,
  requested in [rereprep/noled#3](https://github.com/rereprep/noled/issues/3) since 2018:
  dark whenever the console is on, orange charging indication preserved (incl. in
  standby), re-applied automatically after every wake, standby blink sequence suppressed
- model autodetect: Vita 1000 keeps the classic behavior, removal restores everything
  stock
- the discovery: the power LED is not a SoC GPIO - it hangs off the syscon MCU ("Ernie"),
  and the off switch is `sceSysconCtrlLedBlinkType2` (syscon command `0x89F`); full
  write-up in [`docs/pch2000-power-led-research.md`](docs/pch2000-power-led-research.md)

Earlier versions released upstream at [rereprep/noled](https://github.com/rereprep/noled):

## [v1.2](https://github.com/rereprep/noled/releases/tag/v1.2) - 2017-08-16

- disables the game cartridge LED in addition to the home button LED

## [v1.1](https://github.com/rereprep/noled/releases/tag/v1.1) - 2017-07-30

- HENkaku Ensō compatibility

## [v1.0](https://github.com/rereprep/noled/releases/tag/v1.0) - 2017-04-23

- the original plugin by [xerpi](https://github.com/xerpi): disables the home button LED
  (GPIO clear plus a `ksceGpioPortSet` hook so the OS cannot re-light it)
