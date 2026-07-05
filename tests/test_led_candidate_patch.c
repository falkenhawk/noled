#include <assert.h>

#include "../dev/led_candidate_patch.h"

int main(void)
{
	NoledLedCandidateConfig config;
	unsigned int mode = 99;
	unsigned char data_0387_ff_01[9] = {0x01, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x6B};
	unsigned char data_0387_ff_00[9] = {0x01, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C};
	unsigned char data_0387_ff_probe[9] = {0x01, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x6B};
	unsigned char data_0387_trace_ff_01[9] = {0x01, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x6B};
	unsigned char data_0387_trace_ff_00[9] = {0x01, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C};
	unsigned char data_0387_gpio_trace[9] = {0x01, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x6B};
	unsigned char data_0387_gpio_port3[9] = {0x01, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x6B};
	unsigned char data_0387_gpio_sampler[9] = {0x01, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x6B};
	unsigned char data_0387_10[9] = {0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5B};
	unsigned char data_0387_other[9] = {0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x6A};

	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_PASS));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_FORCE_0387_FF_01_SAFE));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_FORCE_0387_FF_00_10));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_FORCE_0387_FF_BOTH));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_SYSCON_LED_TRACE_ONLY));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_GPIO_TRACE_PASS));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_GPIO_READ_SAMPLER));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED));
	assert(noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE));
	assert(!noled_led_candidate_mode_valid(NOLED_LED_CANDIDATE_MAX + 1));

	assert(noled_led_candidate_0387_checksum(data_0387_ff_01) == 0x6B);
	assert(noled_led_candidate_0387_checksum(data_0387_ff_00) == 0x6C);
	assert(noled_led_candidate_0387_checksum(data_0387_10) == 0x5B);
	assert(noled_led_candidate_0387_checksum(data_0387_other) == 0x6A);

	config.magic = NOLED_LED_CANDIDATE_CONFIG_MAGIC;
	config.mode = NOLED_LED_CANDIDATE_FORCE_0387_FF_01_SAFE;
	assert(noled_led_candidate_config_mode(&config, sizeof(config), &mode) == 0);
	assert(mode == NOLED_LED_CANDIDATE_FORCE_0387_FF_01_SAFE);

	config.mode = NOLED_LED_CANDIDATE_MAX + 1;
	assert(noled_led_candidate_config_mode(&config, sizeof(config), &mode) < 0);

	config.magic = 0;
	config.mode = NOLED_LED_CANDIDATE_PASS;
	assert(noled_led_candidate_config_mode(&config, sizeof(config), &mode) < 0);

	assert(noled_led_candidate_is_0387_ff_01(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_ff_01,
		sizeof(data_0387_ff_01)));
	assert(noled_led_candidate_is_0387_ff_00(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_ff_00,
		sizeof(data_0387_ff_00)));

	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_ff_01,
		sizeof(data_0387_ff_01),
		NOLED_LED_CANDIDATE_FORCE_0387_FF_01_SAFE) == 1);
	assert(data_0387_ff_01[0] == 0x01);
	assert(data_0387_ff_01[1] == 0x00);
	assert(data_0387_ff_01[4] == 0x01);
	assert(data_0387_ff_01[8] == 0x6A);

	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_ff_00,
		sizeof(data_0387_ff_00),
		NOLED_LED_CANDIDATE_FORCE_0387_FF_00_10) == 1);
	assert(data_0387_ff_00[0] == 0x01);
	assert(data_0387_ff_00[1] == 0x10);
	assert(data_0387_ff_00[4] == 0x00);
	assert(data_0387_ff_00[8] == 0x5B);

	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_other,
		sizeof(data_0387_other),
		NOLED_LED_CANDIDATE_FORCE_0387_FF_BOTH) == 0);
	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_10,
		sizeof(data_0387_10),
		NOLED_LED_CANDIDATE_FORCE_0387_FF_BOTH) == 0);
	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_ff_probe,
		sizeof(data_0387_ff_probe),
		NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH) == 1);
	assert(data_0387_ff_probe[1] == 0x00);
	assert(data_0387_ff_probe[8] == 0x6A);

	data_0387_ff_probe[1] = 0xFF;
	data_0387_ff_probe[8] = 0x6B;
	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_ff_probe,
		sizeof(data_0387_ff_probe),
		NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED) == 1);
	assert(data_0387_ff_probe[1] == 0x00);
	assert(data_0387_ff_probe[8] == 0x6A);

	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_trace_ff_01,
		sizeof(data_0387_trace_ff_01),
		NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS) == 0);
	assert(data_0387_trace_ff_01[1] == 0xFF);
	assert(data_0387_trace_ff_01[8] == 0x6B);

	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_trace_ff_00,
		sizeof(data_0387_trace_ff_00),
		NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS) == 0);
	assert(data_0387_trace_ff_00[1] == 0xFF);
	assert(data_0387_trace_ff_00[8] == 0x6C);

	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_gpio_trace,
		sizeof(data_0387_gpio_trace),
		NOLED_LED_CANDIDATE_GPIO_TRACE_PASS) == 0);
	assert(data_0387_gpio_trace[1] == 0xFF);
	assert(data_0387_gpio_trace[8] == 0x6B);

	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_gpio_port3,
		sizeof(data_0387_gpio_port3),
		NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK) == 0);
	assert(data_0387_gpio_port3[1] == 0xFF);
	assert(data_0387_gpio_port3[8] == 0x6B);

	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_gpio_sampler,
		sizeof(data_0387_gpio_sampler),
		NOLED_LED_CANDIDATE_GPIO_READ_SAMPLER) == 0);
	assert(data_0387_gpio_sampler[1] == 0xFF);
	assert(data_0387_gpio_sampler[8] == 0x6B);

	unsigned char data_0387_caller_map[] =
		{0x01, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x6B};
	assert(noled_led_candidate_patch_packet(NOLED_SYSCON_LED_CANDIDATE_0387,
		data_0387_caller_map,
		sizeof(data_0387_caller_map),
		NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE) == 0);
	assert(data_0387_caller_map[1] == 0xFF);
	assert(data_0387_caller_map[8] == 0x6B);

	return 0;
}
