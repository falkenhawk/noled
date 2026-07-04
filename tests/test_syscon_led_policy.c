#include <assert.h>

#include "../syscon_led_policy.h"

int main(void)
{
	assert(!noled_syscon_led_should_force_off(NOLED_LED_CANDIDATE_PASS, 0));
	assert(!noled_syscon_led_should_force_off(NOLED_LED_CANDIDATE_SYSCON_LED_TRACE_ONLY, 0));
	assert(noled_syscon_led_should_force_off(NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED, 0));
	assert(!noled_syscon_led_should_force_off(NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED, 1));

	assert(noled_syscon_led_effective_enable(1,
		NOLED_LED_CANDIDATE_PASS,
		0) == 1);
	assert(noled_syscon_led_effective_enable(1,
		NOLED_LED_CANDIDATE_SYSCON_LED_TRACE_ONLY,
		0) == 1);
	assert(noled_syscon_led_effective_enable(1,
		NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED,
		0) == 0);
	assert(noled_syscon_led_effective_enable(1,
		NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED,
		1) == 1);
	assert(noled_syscon_led_effective_enable(0,
		NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED,
		0) == 0);

	return 0;
}
