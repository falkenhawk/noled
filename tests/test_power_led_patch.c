#include <assert.h>
#include <string.h>

#include "../power_led_patch.h"

int main(void)
{
	NoledPowerLedPatchConfig config;
	unsigned int state = 0;
	unsigned char data[3] = {0x02, 0x00, 0x5A};

	assert(noled_power_led_patch_state_valid(NOLED_POWER_LED_PATCH_DISABLED));
	assert(noled_power_led_patch_state_valid(0));
	assert(noled_power_led_patch_state_valid(7));
	assert(!noled_power_led_patch_state_valid(8));

	assert(noled_power_led_089a_checksum(0x00, 0x00) == 0x5A);
	assert(noled_power_led_089a_checksum(0x02, 0x00) == 0x58);
	assert(noled_power_led_089a_checksum(0x04, 0x00) == 0x56);

	config.magic = NOLED_POWER_LED_PATCH_CONFIG_MAGIC;
	config.state = 3;
	assert(noled_power_led_patch_config_state(&config, sizeof(config), &state) == 0);
	assert(state == 3);

	config.state = 8;
	assert(noled_power_led_patch_config_state(&config, sizeof(config), &state) < 0);

	config.magic = 0;
	config.state = 1;
	assert(noled_power_led_patch_config_state(&config, sizeof(config), &state) < 0);

	memset(data, 0, sizeof(data));
	data[0] = 0x02;
	data[1] = 0x00;
	data[2] = 0x5A;
	assert(noled_power_led_patch_packet(NOLED_SYSCON_POWER_LED_CMD,
		data,
		sizeof(data),
		1) == 1);
	assert(data[0] == 0x01);
	assert(data[1] == 0x00);
	assert(data[2] == 0x59);

	assert(noled_power_led_patch_packet(0x089B, data, sizeof(data), 2) == 0);
	assert(noled_power_led_patch_packet(NOLED_SYSCON_POWER_LED_CMD,
		data,
		2,
		2) == 0);
	assert(noled_power_led_patch_packet(NOLED_SYSCON_POWER_LED_CMD,
		data,
		sizeof(data),
		NOLED_POWER_LED_PATCH_DISABLED) == 0);

	return 0;
}
