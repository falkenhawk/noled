#ifndef NOLED_POWER_LED_H
#define NOLED_POWER_LED_H

/*
 * PCH-2000 power LED control, hardware-validated.
 *
 * The power LED is not a SoC GPIO: it hangs off the syscon MCU ("Ernie"),
 * driven by its autonomous power/charge policy. The one command that
 * overrides that policy is sceSysconCtrlLedBlinkType2 (syscon cmd 0x89F,
 * payload [u8 device, u8 unk, u16 on_time, u16 off_time]); the kernel
 * wrapper does not validate the device byte, and device 1 is the green
 * power LED die. Full story: docs/pch2000-power-led-research.md.
 *
 * vitasdk ships no stub for the wrapper, so it is resolved at runtime by
 * NID via taihenModuleUtils' module_get_export_func.
 */
#define NOLED_SYSCON_MODULE_NAME "SceSyscon"
#define NOLED_SYSCON_FOR_DRIVER_LIB_NID 0x60A35F64
#define NOLED_SYSCON_CTRL_LED_BLINK_TYPE2_NID 0xCB41B531

/* off: park device 1 (green die) on an all-off pattern - LED dark */
#define NOLED_POWER_LED_OFF_DEV 1
#define NOLED_POWER_LED_OFF_UNK 0
#define NOLED_POWER_LED_OFF_ON_TIME 0
#define NOLED_POWER_LED_OFF_OFF_TIME 1

/* release: the pattern engine has a single slot, so claiming it for
 * device 0 (drives nothing visible) evicts the device 1 off pattern and
 * returns the indicator to syscon policy (green idle, orange charging) */
#define NOLED_POWER_LED_RELEASE_DEV 0
#define NOLED_POWER_LED_RELEASE_UNK 0
#define NOLED_POWER_LED_RELEASE_ON_TIME 1
#define NOLED_POWER_LED_RELEASE_OFF_TIME 0

static inline int noled_power_led_indicator_supported(unsigned int hardware_info)
{
	/* the same gate Sony's charge LED code uses: boards below hardware
	 * info 0x40xxxx (original PCH-1000 era) lack the bicolor power/charge
	 * indicator, and the BlinkType2 device namespace is unverified there.
	 * On such boards only the classic GPIO LED blocking applies. */
	return (hardware_info & 0xFF0000) >= 0x400000;
}

#endif
