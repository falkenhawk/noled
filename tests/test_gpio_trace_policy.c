#include <assert.h>

#include "../gpio_trace_policy.h"

int main(void)
{
	assert(!noled_gpio_trace_enabled(NOLED_LED_CANDIDATE_PASS));
	assert(!noled_gpio_trace_enabled(NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS));
	assert(!noled_gpio_trace_enabled(NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS));
	assert(noled_gpio_trace_enabled(NOLED_LED_CANDIDATE_GPIO_TRACE_PASS));
	assert(!noled_gpio_trace_enabled(NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK));
	assert(!noled_gpio_trace_enabled(NOLED_LED_CANDIDATE_GPIO_READ_SAMPLER));
	assert(!noled_gpio_trace_enabled(NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED));
	assert(!noled_gpio_trace_enabled(NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE));

	return 0;
}
