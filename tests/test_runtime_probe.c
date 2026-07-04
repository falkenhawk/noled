#include <assert.h>
#include <string.h>

#include "../runtime_probe.h"
#include "../loader/probe_candidates.h"

static NoledRuntimeProbeRequest make_request(unsigned int op,
	unsigned int arg0,
	unsigned int arg1)
{
	NoledRuntimeProbeRequest request;

	memset(&request, 0, sizeof(request));
	request.magic = NOLED_RUNTIME_PROBE_REQUEST_MAGIC;
	request.seq = 1;
	request.op = op;
	request.arg0 = arg0;
	request.arg1 = arg1;
	return request;
}

int main(void)
{
	NoledRuntimeProbeRequest request;
	unsigned int i;

	/* mode gating */
	assert(noled_runtime_probe_mode_enabled(NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE));
	assert(noled_runtime_probe_mode_enabled(NOLED_LED_CANDIDATE_TRACE_PROBE));
	assert(!noled_runtime_probe_mode_enabled(NOLED_LED_CANDIDATE_PASS));
	assert(!noled_runtime_probe_mode_enabled(NOLED_LED_CANDIDATE_POWER_LED_OFF));
	assert(!noled_runtime_probe_mode_enabled(NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE));
	assert(noled_power_led_off_mode_enabled(NOLED_LED_CANDIDATE_POWER_LED_OFF));
	assert(!noled_power_led_off_mode_enabled(NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_POWER_LED_OFF));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_TRACE_PROBE));

	/* PwmBlink: device 0x40 only, 16-bit value */
	request = make_request(NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0xFFFF);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0x10000);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 1, 0);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_PWM_BLINK, 0x40, 0);
	request.arg2 = 1;
	assert(!noled_runtime_probe_request_valid(&request));

	/* magic required */
	request = make_request(NOLED_RUNTIME_PROBE_OP_PING, 0, 0);
	request.magic = 0;
	assert(!noled_runtime_probe_request_valid(&request));
	assert(!noled_runtime_probe_request_valid(0));

	/* ping */
	request = make_request(NOLED_RUNTIME_PROBE_OP_PING, 0, 0);
	assert(noled_runtime_probe_request_valid(&request));

	/* CtrlLED: ids 0-3 and 0x40 with state 0/1 only */
	request = make_request(NOLED_RUNTIME_PROBE_OP_CTRL_LED, 0, 0);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_CTRL_LED, 3, 1);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_CTRL_LED, 0x40, 0);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_CTRL_LED, 4, 0);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_CTRL_LED, 7, 0);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_CTRL_LED, 0x41, 0);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_CTRL_LED, 0, 2);
	assert(!noled_runtime_probe_request_valid(&request));

	/* DolceLED: states 0-2 only */
	request = make_request(NOLED_RUNTIME_PROBE_OP_DOLCE_LED, 2, 0);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_DOLCE_LED, 3, 0);
	assert(!noled_runtime_probe_request_valid(&request));

	/* charge LED ctrl and power LED flag: bool arg */
	request = make_request(NOLED_RUNTIME_PROBE_OP_CHARGE_LED_CTRL, 1, 0);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_CHARGE_LED_CTRL, 2, 0);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_POWER_LED_FLAG, 0, 0);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_POWER_LED_FLAG, 2, 0);
	assert(!noled_runtime_probe_request_valid(&request));

	/* SceLed: led 0-7, mode 0-2, bounded times */
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_SET_MODE, 7, 1);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_SET_MODE, 2, 2);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_SET_MODE, 8, 0);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_SET_MODE, 2, 3);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_SET_MODE, 2, 1);
	request.arg2 = 10;
	request.arg3 = 10;
	assert(noled_runtime_probe_request_valid(&request));
	request.arg2 = 11;
	assert(!noled_runtime_probe_request_valid(&request));
	request.arg2 = 0;
	request.arg3 = 11;
	assert(!noled_runtime_probe_request_valid(&request));

	/* timing args are for SceLed only */
	request = make_request(NOLED_RUNTIME_PROBE_OP_PING, 0, 0);
	request.arg2 = 1;
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_DOLCE_LED, 0, 0);
	request.arg3 = 1;
	assert(!noled_runtime_probe_request_valid(&request));

	/* raw syscon command: only the 0x823 DolceLED-off probe */
	request = make_request(NOLED_RUNTIME_PROBE_OP_RAW_SYSCON_CMD,
		NOLED_RUNTIME_PROBE_RAW_CMD_DOLCE_OFF, 0);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_RAW_SYSCON_CMD, 0x820, 0);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_RAW_SYSCON_CMD, 0x824, 0);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_RAW_SYSCON_CMD,
		NOLED_RUNTIME_PROBE_RAW_CMD_DOLCE_OFF, 1);
	assert(!noled_runtime_probe_request_valid(&request));

	/* BlinkType2: devices 0-3 and 0x40, bounded args */
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 0x40, 0);
	request.arg3 = 1;
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 3, 0);
	assert(noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 4, 0);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 0x41, 0);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 0x40, 3);
	assert(!noled_runtime_probe_request_valid(&request));
	request = make_request(NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2, 0x40, 0);
	request.arg2 = 11;
	assert(!noled_runtime_probe_request_valid(&request));

	/* unknown op */
	request = make_request(99, 0, 0);
	assert(!noled_runtime_probe_request_valid(&request));

	/* seq newness */
	request = make_request(NOLED_RUNTIME_PROBE_OP_PING, 0, 0);
	request.seq = 5;
	assert(noled_runtime_probe_request_is_new(&request, 4));
	assert(!noled_runtime_probe_request_is_new(&request, 5));

	/* every shipped candidate must pass the kernel whitelist */
	for (i = 0; i < NOLED_PROBE_CANDIDATE_COUNT; i++) {
		request = make_request(noled_probe_candidates[i].op,
			noled_probe_candidates[i].arg0,
			noled_probe_candidates[i].arg1);
		request.arg2 = noled_probe_candidates[i].arg2;
		request.arg3 = noled_probe_candidates[i].arg3;
		assert(noled_runtime_probe_request_valid(&request));
		assert(noled_probe_candidates[i].label != 0);
	}

	/* candidate cycling wraps in both directions */
	assert(noled_probe_candidate_next(NOLED_PROBE_CANDIDATE_COUNT - 1) == 0);
	assert(noled_probe_candidate_prev(0) == NOLED_PROBE_CANDIDATE_COUNT - 1);
	assert(noled_probe_candidate_next(0) == 1);
	assert(noled_probe_candidate_prev(1) == 0);

	return 0;
}
