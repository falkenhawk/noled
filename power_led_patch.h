#ifndef NOLED_POWER_LED_PATCH_H
#define NOLED_POWER_LED_PATCH_H

#define NOLED_SYSCON_POWER_LED_CMD 0x089A
#define NOLED_POWER_LED_PATCH_CONFIG_PATH "ux0:data/noled/power_led_patch.bin"
#define NOLED_POWER_LED_PATCH_CONFIG_MAGIC 0x31504C4E
#define NOLED_POWER_LED_PATCH_DISABLED 0xFF
#define NOLED_POWER_LED_PATCH_MIN 0
#define NOLED_POWER_LED_PATCH_MAX 7

typedef struct NoledPowerLedPatchConfig {
	unsigned int magic;
	unsigned int state;
} NoledPowerLedPatchConfig;

static inline int noled_power_led_patch_state_valid(unsigned int state)
{
	return state == NOLED_POWER_LED_PATCH_DISABLED ||
		state <= NOLED_POWER_LED_PATCH_MAX;
}

static inline unsigned char noled_power_led_089a_checksum(unsigned int first,
	unsigned int second)
{
	return (unsigned char)(0x5A - ((first + second) & 0xFF));
}

static inline int noled_power_led_patch_config_state(const void *data,
	unsigned int size,
	unsigned int *state)
{
	const NoledPowerLedPatchConfig *config =
		(const NoledPowerLedPatchConfig *)data;

	if (data == 0 || state == 0 || size < sizeof(NoledPowerLedPatchConfig)) {
		return -1;
	}

	if (config->magic != NOLED_POWER_LED_PATCH_CONFIG_MAGIC ||
		!noled_power_led_patch_state_valid(config->state)) {
		return -1;
	}

	*state = config->state;
	return 0;
}

static inline int noled_power_led_patch_packet(unsigned int cmd,
	unsigned char *tx_data,
	unsigned int tx_len,
	unsigned int state)
{
	if (cmd != NOLED_SYSCON_POWER_LED_CMD ||
		tx_data == 0 ||
		tx_len < 3 ||
		state > NOLED_POWER_LED_PATCH_MAX) {
		return 0;
	}

	tx_data[0] = (unsigned char)state;
	tx_data[2] = noled_power_led_089a_checksum(tx_data[0], tx_data[1]);
	return 1;
}

#endif
