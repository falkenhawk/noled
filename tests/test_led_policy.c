#include <assert.h>

#include "../led_policy.h"

int main(void)
{
	assert(noled_should_block_gpio_set(NOLED_GPIO_BUS, NOLED_GPIO_HOME_BUTTON_PORT));
	assert(noled_should_block_gpio_set(NOLED_GPIO_BUS, NOLED_GPIO_GAME_CARD_PORT));
	assert(!noled_should_block_gpio_set(NOLED_GPIO_BUS, 5));
	assert(!noled_should_block_gpio_set(1, NOLED_GPIO_HOME_BUTTON_PORT));
	assert(!noled_should_block_gpio_set(NOLED_GPIO_BUS,
		NOLED_GPIO_POWER_LED_CANDIDATE_PORT));

	assert(noled_should_block_gpio_set_for_mode(NOLED_GPIO_BUS,
		NOLED_GPIO_POWER_LED_CANDIDATE_PORT,
		NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK));
	assert(!noled_should_block_gpio_set_for_mode(NOLED_GPIO_BUS,
		NOLED_GPIO_POWER_LED_CANDIDATE_PORT,
		NOLED_LED_CANDIDATE_GPIO_TRACE_PASS));
	assert(noled_should_block_gpio_set_for_mode(NOLED_GPIO_BUS,
		NOLED_GPIO_HOME_BUTTON_PORT,
		NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK));

	return 0;
}
