#ifndef NOLED_GPIO_TRACE_POLICY_H
#define NOLED_GPIO_TRACE_POLICY_H

#include "led_candidate_patch.h"

static inline int noled_gpio_trace_enabled(unsigned int mode)
{
	return mode == NOLED_LED_CANDIDATE_GPIO_TRACE_PASS;
}

#endif
