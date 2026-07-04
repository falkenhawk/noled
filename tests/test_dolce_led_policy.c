#include <assert.h>

#include "../dolce_led_policy.h"

int main(void)
{
	assert(noled_dolce_led_effective_enable(1,
		NOLED_LED_CANDIDATE_PASS) == 1);
	assert(noled_dolce_led_effective_enable(0,
		NOLED_LED_CANDIDATE_PASS) == 0);
	assert(noled_dolce_led_effective_enable(1,
		NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH) == 0);
	assert(noled_dolce_led_effective_enable(0,
		NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH) == 0);
	assert(noled_dolce_led_effective_enable(2,
		NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH) == 2);
	assert(noled_dolce_led_effective_enable(0,
		NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS) == 0);
	assert(noled_dolce_led_effective_enable(1,
		NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS) == 1);
	assert(noled_dolce_led_effective_enable(2,
		NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS) == 2);
	assert(noled_dolce_led_should_force_off(
		NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH));
	assert(!noled_dolce_led_should_force_off(
		NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS));
	assert(!noled_dolce_led_should_force_off(
		NOLED_LED_CANDIDATE_SYSCON_LED_TRACE_ONLY));
	assert(noled_dolce_led_should_force_off(
		NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED));
	assert(!noled_dolce_led_should_force_off(
		NOLED_LED_CANDIDATE_FORCE_0387_FF_BOTH));

	return 0;
}
