#include <assert.h>

#include "../power_led.h"

int main(void)
{
	/* the hardware-validated patterns must never drift: off parks device 1
	 * (green die) on an all-off pattern, release claims the single pattern
	 * slot for device 0 to hand the LED back to syscon policy */
	assert(NOLED_POWER_LED_OFF_DEV == 1);
	assert(NOLED_POWER_LED_OFF_UNK == 0);
	assert(NOLED_POWER_LED_OFF_ON_TIME == 0);
	assert(NOLED_POWER_LED_OFF_OFF_TIME == 1);

	assert(NOLED_POWER_LED_RELEASE_DEV == 0);
	assert(NOLED_POWER_LED_RELEASE_UNK == 0);
	assert(NOLED_POWER_LED_RELEASE_ON_TIME == 1);
	assert(NOLED_POWER_LED_RELEASE_OFF_TIME == 0);

	assert(NOLED_SYSCON_CTRL_LED_BLINK_TYPE2_NID == 0xCB41B531);

	/* model gate: only hardware info 0x40xxxx and above (PCH-2000 era) has
	 * the bicolor power/charge indicator */
	assert(!noled_power_led_indicator_supported(0x00000000));
	assert(!noled_power_led_indicator_supported(0x00300000));
	assert(!noled_power_led_indicator_supported(0x003F0000));
	assert(noled_power_led_indicator_supported(0x00400000));
	assert(noled_power_led_indicator_supported(0x00401000));
	assert(noled_power_led_indicator_supported(0x00600000));
	/* only the board-revision byte matters, not higher bits */
	assert(!noled_power_led_indicator_supported(0xFF300000));

	return 0;
}
