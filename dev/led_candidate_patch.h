#ifndef NOLED_LED_CANDIDATE_PATCH_H
#define NOLED_LED_CANDIDATE_PATCH_H

#include "led_policy.h"

#define NOLED_LED_CANDIDATE_CONFIG_PATH "ux0:data/noled/led_candidate.bin"
#define NOLED_LED_CANDIDATE_CONFIG_MAGIC 0x31434C4E

#define NOLED_LED_CANDIDATE_PASS 0
#define NOLED_LED_CANDIDATE_FORCE_0387_FF_01_SAFE 1
#define NOLED_LED_CANDIDATE_FORCE_0387_FF_00_10 2
#define NOLED_LED_CANDIDATE_FORCE_0387_FF_BOTH 3
#define NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH 4
#define NOLED_LED_CANDIDATE_SYSCON_LED_TRACE_ONLY 5
#define NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS 6
#define NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS 7
#define NOLED_LED_CANDIDATE_GPIO_TRACE_PASS 8
#define NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK 9
#define NOLED_LED_CANDIDATE_GPIO_READ_SAMPLER 10
#define NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED 11
#define NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE 12
#define NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE 13
#define NOLED_LED_CANDIDATE_POWER_LED_OFF 14
#define NOLED_LED_CANDIDATE_TRACE_PROBE 15
#define NOLED_LED_CANDIDATE_MAX NOLED_LED_CANDIDATE_TRACE_PROBE

#define NOLED_SYSCON_LED_CANDIDATE_0387 0x0387
#define NOLED_LED_CANDIDATE_0387_SUM 0x6C
#define NOLED_LED_CANDIDATE_0387_DATA_LEN 9

/* mode 9 experiment: additionally block GPIO bus 0 port 3, a power LED
 * candidate that turned out not to be a GPIO at all */
#define NOLED_GPIO_POWER_LED_CANDIDATE_PORT 3

typedef struct NoledLedCandidateConfig {
	unsigned int magic;
	unsigned int mode;
} NoledLedCandidateConfig;

static inline int noled_should_block_gpio_set_for_mode(int bus,
	int port,
	unsigned int mode)
{
	return noled_should_block_gpio_set(bus, port) ||
	       (mode == NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK &&
	        bus == NOLED_GPIO_BUS &&
	        port == NOLED_GPIO_POWER_LED_CANDIDATE_PORT);
}

static inline int noled_led_candidate_mode_valid(unsigned int mode)
{
	return mode <= NOLED_LED_CANDIDATE_MAX;
}

static inline unsigned char noled_led_candidate_0387_checksum(const unsigned char *tx_data)
{
	unsigned int i;
	unsigned int sum = 0;

	if (tx_data == 0) {
		return 0;
	}

	for (i = 0; i < NOLED_LED_CANDIDATE_0387_DATA_LEN - 1; i++) {
		sum += tx_data[i];
	}

	return (unsigned char)(NOLED_LED_CANDIDATE_0387_SUM - (sum & 0xFF));
}

static inline int noled_led_candidate_config_mode(const void *data,
	unsigned int size,
	unsigned int *mode)
{
	const NoledLedCandidateConfig *config =
		(const NoledLedCandidateConfig *)data;

	if (data == 0 || mode == 0 || size < sizeof(NoledLedCandidateConfig)) {
		return -1;
	}

	if (config->magic != NOLED_LED_CANDIDATE_CONFIG_MAGIC ||
		!noled_led_candidate_mode_valid(config->mode)) {
		return -1;
	}

	*mode = config->mode;
	return 0;
}

static inline int noled_led_candidate_is_0387_ff_01(unsigned int cmd,
	const unsigned char *tx_data,
	unsigned int tx_len)
{
	return cmd == NOLED_SYSCON_LED_CANDIDATE_0387 &&
		tx_data != 0 &&
		tx_len >= NOLED_LED_CANDIDATE_0387_DATA_LEN &&
		tx_data[0] == 0x01 &&
		tx_data[1] == 0xFF &&
		tx_data[2] == 0x00 &&
		tx_data[3] == 0x00 &&
		tx_data[4] == 0x01 &&
		tx_data[5] == 0x00 &&
		tx_data[6] == 0x00 &&
		tx_data[7] == 0x00;
}

static inline int noled_led_candidate_is_0387_ff_00(unsigned int cmd,
	const unsigned char *tx_data,
	unsigned int tx_len)
{
	return cmd == NOLED_SYSCON_LED_CANDIDATE_0387 &&
		tx_data != 0 &&
		tx_len >= NOLED_LED_CANDIDATE_0387_DATA_LEN &&
		tx_data[0] == 0x01 &&
		tx_data[1] == 0xFF &&
		tx_data[2] == 0x00 &&
		tx_data[3] == 0x00 &&
		tx_data[4] == 0x00 &&
		tx_data[5] == 0x00 &&
		tx_data[6] == 0x00 &&
		tx_data[7] == 0x00;
}

static inline void noled_led_candidate_set_0387_safe(unsigned char *tx_data)
{
	tx_data[0] = 0x01;
	tx_data[1] = 0x00;
	tx_data[2] = 0x00;
	tx_data[3] = 0x00;
	tx_data[4] = 0x01;
	tx_data[5] = 0x00;
	tx_data[6] = 0x00;
	tx_data[7] = 0x00;
	tx_data[8] = noled_led_candidate_0387_checksum(tx_data);
}

static inline void noled_led_candidate_set_0387_10(unsigned char *tx_data)
{
	tx_data[0] = 0x01;
	tx_data[1] = 0x10;
	tx_data[2] = 0x00;
	tx_data[3] = 0x00;
	tx_data[4] = 0x00;
	tx_data[5] = 0x00;
	tx_data[6] = 0x00;
	tx_data[7] = 0x00;
	tx_data[8] = noled_led_candidate_0387_checksum(tx_data);
}

static inline int noled_led_candidate_patch_packet(unsigned int cmd,
	unsigned char *tx_data,
	unsigned int tx_len,
	unsigned int mode)
{
	switch (mode) {
	case NOLED_LED_CANDIDATE_FORCE_0387_FF_01_SAFE:
		if (!noled_led_candidate_is_0387_ff_01(cmd, tx_data, tx_len)) {
			return 0;
		}
		noled_led_candidate_set_0387_safe(tx_data);
		return 1;
	case NOLED_LED_CANDIDATE_FORCE_0387_FF_00_10:
		if (!noled_led_candidate_is_0387_ff_00(cmd, tx_data, tx_len)) {
			return 0;
		}
		noled_led_candidate_set_0387_10(tx_data);
		return 1;
	case NOLED_LED_CANDIDATE_FORCE_0387_FF_BOTH:
	case NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH:
		if (noled_led_candidate_is_0387_ff_01(cmd, tx_data, tx_len)) {
			noled_led_candidate_set_0387_safe(tx_data);
			return 1;
		}
		if (noled_led_candidate_is_0387_ff_00(cmd, tx_data, tx_len)) {
			noled_led_candidate_set_0387_10(tx_data);
			return 1;
		}
		return 0;
	case NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED:
		if (noled_led_candidate_is_0387_ff_01(cmd, tx_data, tx_len)) {
			noled_led_candidate_set_0387_safe(tx_data);
			return 1;
		}
		if (noled_led_candidate_is_0387_ff_00(cmd, tx_data, tx_len)) {
			noled_led_candidate_set_0387_10(tx_data);
			return 1;
		}
		return 0;
	default:
		return 0;
	}
}

#endif
