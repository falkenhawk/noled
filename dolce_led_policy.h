#ifndef NOLED_DOLCE_LED_POLICY_H
#define NOLED_DOLCE_LED_POLICY_H

#include "led_candidate_patch.h"

#define NOLED_DOLCE_LED_OFF 0

static inline int noled_dolce_led_should_force_off(unsigned int mode)
{
	return mode == NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH ||
		mode == NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED;
}

static inline int noled_dolce_led_effective_enable(int requested_enable,
	unsigned int mode)
{
	if (!noled_dolce_led_should_force_off(mode)) {
		return requested_enable;
	}

	if (requested_enable == 2) {
		return requested_enable;
	}

	return NOLED_DOLCE_LED_OFF;
}

#endif
