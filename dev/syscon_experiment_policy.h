#ifndef NOLED_SYSCON_EXPERIMENT_POLICY_H
#define NOLED_SYSCON_EXPERIMENT_POLICY_H

#include "led_candidate_patch.h"

static inline int noled_syscon_experiment_enabled(unsigned int mode)
{
	return mode != NOLED_LED_CANDIDATE_PASS &&
		mode != NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK &&
		mode != NOLED_LED_CANDIDATE_GPIO_READ_SAMPLER &&
		mode != NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE &&
		mode != NOLED_LED_CANDIDATE_POWER_LED_OFF;
}

static inline int noled_syscon_command_hooks_enabled(unsigned int mode)
{
	return noled_syscon_experiment_enabled(mode) &&
		mode != NOLED_LED_CANDIDATE_SYSCON_LED_TRACE_ONLY;
}

static inline int noled_syscon_dolce_hook_enabled(unsigned int mode)
{
	return mode == NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH ||
		mode == NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS ||
		mode == NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED;
}

#endif
