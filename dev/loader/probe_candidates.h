#ifndef NOLED_PROBE_CANDIDATES_H
#define NOLED_PROBE_CANDIDATES_H

#include "../runtime_probe.h"

/*
 * Mode 13 candidate list, v4, after the 2026-07-04 breakthrough:
 * - BlinkType2 (0x89F) dev 1 with on=0/off=1 turns OFF the whole PCH-2000
 *   power/charge indicator (green and orange), survives every other LED
 *   command and standby entry; wake and reboot restore it.
 * - this list explores the same pattern engine for RELEASE (0/0 = hand
 *   control back to syscon policy?) and for DUTY-BASED DIMMING of the
 *   green LED, mirroring the SceLed mapping proven on the blue LED
 *   (0/1 = off, 1/1 = dim, 9/1 = bright).
 * - kernel whitelist is unchanged from v3, so this loader also works
 *   against a resident v3 kernel plugin.
 */

typedef struct NoledProbeCandidate {
	unsigned int op;
	unsigned int arg0;
	unsigned int arg1;
	unsigned int arg2;
	unsigned int arg3;
	const char *label;
} NoledProbeCandidate;

static const NoledProbeCandidate noled_probe_candidates[] = {
	{NOLED_RUNTIME_PROBE_OP_PING, 0, 0, 0, 0,
		"ping kernel probe thread (no syscon)"},
	{NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 1, 0, 0, 1,
		"BT2 dev1 0/1  green OFF (the winner)"},
	{NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 1, 0, 1, 0,
		"BT2 dev1 1/0  green forced on (restore)"},
	{NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0x0000, 0, 0,
		"PwmBlink 0x0000 (clean green dim hunt)"},
	{NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0x0040, 0, 0,
		"PwmBlink 0x0040"},
	{NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0x0080, 0, 0,
		"PwmBlink 0x0080"},
	{NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0x00FF, 0, 0,
		"PwmBlink 0x00FF"},
	{NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0x0100, 0, 0,
		"PwmBlink 0x0100"},
	{NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0x1000, 0, 0,
		"PwmBlink 0x1000"},
	{NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0x8000, 0, 0,
		"PwmBlink 0x8000"},
	{NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0xFFFF, 0, 0,
		"PwmBlink 0xFFFF"},
	{NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 0, 0, 1, 0,
		"BT2 dev0 1/0  (slot test after dev1 off)"},
	{NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 2, 0, 1, 0,
		"BT2 dev2 1/0  (slot test after dev1 off)"},
	{NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 3, 0, 1, 0,
		"BT2 dev3 1/0  (slot test after dev1 off)"},
	{NOLED_RUNTIME_PROBE_OP_DOLCE_LED, 0, 0, 0, 0,
		"DolceLED 0 steady green (reference)"},
	{NOLED_RUNTIME_PROBE_OP_DOLCE_LED, 1, 0, 0, 0,
		"DolceLED 1 blink slow (wake-marker aid)"},
};

#define NOLED_PROBE_CANDIDATE_COUNT \
	(sizeof(noled_probe_candidates) / sizeof(noled_probe_candidates[0]))

static inline unsigned int noled_probe_candidate_next(unsigned int index)
{
	return (index + 1) % NOLED_PROBE_CANDIDATE_COUNT;
}

static inline unsigned int noled_probe_candidate_prev(unsigned int index)
{
	return (index + NOLED_PROBE_CANDIDATE_COUNT - 1) %
		NOLED_PROBE_CANDIDATE_COUNT;
}

#endif
