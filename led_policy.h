#ifndef NOLED_LED_POLICY_H
#define NOLED_LED_POLICY_H

#include "led_candidate_patch.h"

#define NOLED_GPIO_BUS 0
#define NOLED_GPIO_POWER_LED_CANDIDATE_PORT 3
#define NOLED_GPIO_HOME_BUTTON_PORT 7
#define NOLED_GPIO_GAME_CARD_PORT 6

static inline int noled_should_block_gpio_set(int bus, int port)
{
	return bus == NOLED_GPIO_BUS &&
	       (port == NOLED_GPIO_HOME_BUTTON_PORT ||
	        port == NOLED_GPIO_GAME_CARD_PORT);
}

static inline int noled_should_block_gpio_set_for_mode(int bus,
	int port,
	unsigned int mode)
{
	return noled_should_block_gpio_set(bus, port) ||
	       (mode == NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK &&
	        bus == NOLED_GPIO_BUS &&
	        port == NOLED_GPIO_POWER_LED_CANDIDATE_PORT);
}

#endif
