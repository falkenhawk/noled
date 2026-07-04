#ifndef NOLED_RUNTIME_PROBE_H
#define NOLED_RUNTIME_PROBE_H

#include "led_candidate_patch.h"

/*
 * Mode 13 runtime probe protocol.
 *
 * The loader writes one request per button press to the request file. The
 * kernel plugin polls that file, executes at most one new request per poll,
 * and writes the result to the response file. No hooks, no packet mutation,
 * no probe loops: one user action equals one syscon/LED interaction.
 *
 * Research facts this design relies on:
 * - decompiled 3.60 SceSyscon: ksceSysconCtrlLED(led, enable) sends syscon
 *   command 0x891 with a one-byte payload of led | (enable ? 0x80 : 0).
 * - ModernBT-Vita ships ksceSysconCtrlLED(0, enable) on retail hardware and
 *   documents led 0 as the Vita 1000/2000 charge LED, which on PCH-2000 is
 *   the same bi-color package as the green power LED.
 * - wiki.henkaku.xyz/vita/SceSyscon documents device 0x40 for the same call
 *   ("maybe PS button or CP power LED"); PSP ancestry suggests led 2 = power.
 * - syscon command 0x89E is sceSysconSetChargeLedCtrl(SceBool state); it is a
 *   no-op on the oldest motherboards, so it exists for newer boards like
 *   PCH-2000. Used by ScePowerForDriver_38415146 which is "related to LED".
 * - decompiled sceSysconCtrlDolceLED(state) maps state to command
 *   0x820 + state with empty payload: 0 = on, 1 = blink slow, 2 = blink
 *   fast. There is no documented off state.
 * - command 0x387 is in the touchpanel range (0x380 = touchpanel info). It
 *   must never be sent or mutated by this plugin; earlier 0x387 mutation is
 *   what broke touch wake.
 * - the earlier hardware freeze came from a ksceSysconCtrlLED(0..7, 0) loop
 *   with no per-id observation; ids 4-7 stay banned here.
 */

#define NOLED_RUNTIME_PROBE_REQUEST_PATH "ux0:data/noled/probe_request.bin"
#define NOLED_RUNTIME_PROBE_RESPONSE_PATH "ux0:data/noled/probe_response.bin"
#define NOLED_RUNTIME_PROBE_LOG_PATH "ux0:data/noled/probe.log"

#define NOLED_RUNTIME_PROBE_REQUEST_MAGIC 0x51524C4E /* NLRQ */
#define NOLED_RUNTIME_PROBE_RESPONSE_MAGIC 0x53524C4E /* NLRS */

#define NOLED_RUNTIME_PROBE_OP_PING 1
#define NOLED_RUNTIME_PROBE_OP_CTRL_LED 2
#define NOLED_RUNTIME_PROBE_OP_DOLCE_LED 3
#define NOLED_RUNTIME_PROBE_OP_CHARGE_LED_CTRL 4
#define NOLED_RUNTIME_PROBE_OP_POWER_LED_FLAG 5
#define NOLED_RUNTIME_PROBE_OP_LED_SET_MODE 6
#define NOLED_RUNTIME_PROBE_OP_RAW_SYSCON_CMD 7
/* sceSysconCtrlLedBlinkType2 (cmd 0x89F): (u8 device, u8 unk, u16 on, u16 off);
 * hardware confirmed dev 1 = green power LED die, on=0/off=1 = LED dark */
#define NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2 8
/* sceSysconCtrlLedPwmBlink (cmd 0x89D): wrapper requires device 0x40 but the
 * wire payload selector is a fixed 0x01 - possibly a true PWM on the green
 * die (flicker-free dimming); 16-bit value semantics unknown, probed live */
#define NOLED_RUNTIME_PROBE_OP_PWM_BLINK 9

/* the only raw command allowed: the gap right after the DolceLED family
 * (0x820 on / 0x821 blink slow / 0x822 blink fast, confirmed on hardware to
 * drive the PCH-2000 green power LED), probing whether state 3 = off exists
 * in Ernie firmware; the kernel wrapper rejects state > 2 */
#define NOLED_RUNTIME_PROBE_RAW_CMD_DOLCE_OFF 0x823

/* wiki-documented sceSysconCtrlLED device value */
#define NOLED_RUNTIME_PROBE_CTRL_LED_DEVICE_UNK 0x40

/* runtime NID resolution targets */
#define NOLED_SYSCON_MODULE_NAME "SceSyscon"
#define NOLED_SYSCON_FOR_DRIVER_LIB_NID 0x60A35F64
#define NOLED_SYSCON_SET_CHARGE_LED_CTRL_NID 0x9CA6EB70
#define NOLED_SYSCON_CTRL_LED_BLINK_TYPE2_NID 0xCB41B531
#define NOLED_SYSCON_CTRL_LED_PWM_BLINK_NID 0x6F586D1A

/* the hardware-confirmed power LED off pattern: BlinkType2 dev 1, unk 0,
 * on_time 0, off_time 1 */
#define NOLED_POWER_LED_OFF_DEV 1
#define NOLED_POWER_LED_OFF_UNK 0
#define NOLED_POWER_LED_OFF_ON_TIME 0
#define NOLED_POWER_LED_OFF_OFF_TIME 1

/* the hardware-confirmed release: the pattern engine has a single slot, so
 * claiming it for dev 0 (drives nothing visible) evicts the dev 1 off
 * pattern and returns the indicator to syscon policy (green idle, orange
 * while charging) */
#define NOLED_POWER_LED_RELEASE_DEV 0
#define NOLED_POWER_LED_RELEASE_UNK 0
#define NOLED_POWER_LED_RELEASE_ON_TIME 1
#define NOLED_POWER_LED_RELEASE_OFF_TIME 0
#define NOLED_POWER_MODULE_NAME "ScePower"
#define NOLED_POWER_FOR_DRIVER_LIB_NID 0x1590166F
#define NOLED_POWER_LED_FLAG_NID 0x38415146

typedef struct NoledRuntimeProbeRequest {
	unsigned int magic;
	unsigned int seq;
	unsigned int op;
	unsigned int arg0;
	unsigned int arg1;
	/* LED_SET_MODE only: SceLedConfiguration on/off times; hardware showed
	 * mode ON with times 1/1 dims the LED, so these act as a PWM duty */
	unsigned int arg2;
	unsigned int arg3;
} NoledRuntimeProbeRequest;

typedef struct NoledRuntimeProbeResponse {
	unsigned int magic;
	unsigned int seq;
	unsigned int op;
	unsigned int arg0;
	unsigned int arg1;
	int resolve_ret;
	int exec_ret;
} NoledRuntimeProbeResponse;

static inline int noled_runtime_probe_request_valid(const NoledRuntimeProbeRequest *request)
{
	if (request == 0 ||
		request->magic != NOLED_RUNTIME_PROBE_REQUEST_MAGIC) {
		return 0;
	}

	/* only the SceLed and BlinkType2 ops use the timing args */
	if (request->op != NOLED_RUNTIME_PROBE_OP_LED_SET_MODE &&
		request->op != NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2 &&
		(request->arg2 != 0 || request->arg3 != 0)) {
		return 0;
	}

	if (request->op == NOLED_RUNTIME_PROBE_OP_PWM_BLINK) {
		/* wrapper rejects any device but 0x40; 16-bit value */
		return request->arg0 == NOLED_RUNTIME_PROBE_CTRL_LED_DEVICE_UNK &&
			request->arg1 <= 0xFFFF;
	}

	switch (request->op) {
	case NOLED_RUNTIME_PROBE_OP_PING:
		return 1;
	case NOLED_RUNTIME_PROBE_OP_CTRL_LED:
		/* never sweep ids: the old 0..7 loop froze hardware; 4-7 banned */
		return (request->arg0 <= 3 ||
			request->arg0 == NOLED_RUNTIME_PROBE_CTRL_LED_DEVICE_UNK) &&
			(request->arg1 == 0 || request->arg1 == 1);
	case NOLED_RUNTIME_PROBE_OP_DOLCE_LED:
		/* 0 = on, 1 = blink slow, 2 = blink fast */
		return request->arg0 <= 2;
	case NOLED_RUNTIME_PROBE_OP_CHARGE_LED_CTRL:
	case NOLED_RUNTIME_PROBE_OP_POWER_LED_FLAG:
		return request->arg0 == 0 || request->arg0 == 1;
	case NOLED_RUNTIME_PROBE_OP_LED_SET_MODE:
		/* SceLed pattern engine: led 0-7, mode 0-2, bounded PWM times */
		return request->arg0 <= 7 && request->arg1 <= 2 &&
			request->arg2 <= 10 && request->arg3 <= 10;
	case NOLED_RUNTIME_PROBE_OP_RAW_SYSCON_CMD:
		/* exactly one whitelisted raw command, empty payload */
		return request->arg0 == NOLED_RUNTIME_PROBE_RAW_CMD_DOLCE_OFF &&
			request->arg1 == 0;
	case NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2:
		/* device byte is in the payload and unchecked by the wrapper;
		 * allow the known blue device 0x40 plus the 0-3 id space probed
		 * one at a time for the green power LED */
		return (request->arg0 <= 3 ||
			request->arg0 == NOLED_RUNTIME_PROBE_CTRL_LED_DEVICE_UNK) &&
			request->arg1 <= 2 &&
			request->arg2 <= 10 && request->arg3 <= 10;
	default:
		return 0;
	}
}

static inline int noled_runtime_probe_request_is_new(const NoledRuntimeProbeRequest *request,
	unsigned int last_seq)
{
	return noled_runtime_probe_request_valid(request) &&
		request->seq != last_seq;
}

static inline int noled_runtime_probe_mode_enabled(unsigned int mode)
{
	/* mode 15 combines the pass-through tracer with the probe so the
	 * resume-time LED traffic that releases the off pattern can be
	 * captured while a pattern is active */
	return mode == NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE ||
		mode == NOLED_LED_CANDIDATE_TRACE_PROBE;
}

static inline int noled_power_led_off_mode_enabled(unsigned int mode)
{
	return mode == NOLED_LED_CANDIDATE_POWER_LED_OFF;
}

#endif
