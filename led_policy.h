#ifndef NOLED_LED_POLICY_H
#define NOLED_LED_POLICY_H

/* the classic noled targets: home button and game card LEDs are plain SoC
 * GPIO lines on every Vita model */
#define NOLED_GPIO_BUS 0
#define NOLED_GPIO_HOME_BUTTON_PORT 7
#define NOLED_GPIO_GAME_CARD_PORT 6

static inline int noled_should_block_gpio_set(int bus, int port)
{
	return bus == NOLED_GPIO_BUS &&
	       (port == NOLED_GPIO_HOME_BUTTON_PORT ||
	        port == NOLED_GPIO_GAME_CARD_PORT);
}

#endif
