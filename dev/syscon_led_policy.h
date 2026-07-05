#ifndef NOLED_SYSCON_LED_POLICY_H
#define NOLED_SYSCON_LED_POLICY_H

#include "led_candidate_patch.h"

static inline int noled_syscon_led_should_force_off(unsigned int mode,
	int power_online)
{
	return mode == NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED &&
		!power_online;
}

static inline int noled_syscon_led_effective_enable(int requested_enable,
	unsigned int mode,
	int power_online)
{
	if (!noled_syscon_led_should_force_off(mode, power_online)) {
		return requested_enable;
	}

	return 0;
}

#endif
