#include <assert.h>

#include "../cmd_probe.h"

int main(void)
{
	NoledCmdProbeSpec spec;

	assert(noled_cmd_probe_get_spec(NOLED_CMD_PROBE_088F_0010, &spec) == 0);
	assert(spec.cmd == 0x088f);
	assert(spec.data_len == 3);
	assert(spec.flags == 0);
	assert(spec.data[0] == 0x00);
	assert(spec.data[1] == 0x10);
	assert(spec.data[2] == 0x55);

	assert(noled_cmd_probe_get_spec(NOLED_CMD_PROBE_088F_0110, &spec) == 0);
	assert(spec.cmd == 0x088f);
	assert(spec.data[0] == 0x01);
	assert(spec.data[1] == 0x10);
	assert(spec.data[2] == 0x54);

	assert(noled_cmd_probe_get_spec(NOLED_CMD_PROBE_088A_00, &spec) == 0);
	assert(spec.cmd == 0x088a);
	assert(spec.data_len == 2);
	assert(spec.data[0] == 0x00);
	assert(spec.data[1] == 0x6b);

	assert(noled_cmd_probe_get_spec(NOLED_CMD_PROBE_088A_01, &spec) == 0);
	assert(spec.cmd == 0x088a);
	assert(spec.data[0] == 0x01);
	assert(spec.data[1] == 0x6a);

	assert(noled_cmd_probe_get_spec(NOLED_CMD_PROBE_089B_00, &spec) == 0);
	assert(spec.cmd == 0x089b);
	assert(spec.data_len == 2);
	assert(spec.data[0] == 0x00);
	assert(spec.data[1] == 0x5a);

	assert(noled_cmd_probe_get_spec(NOLED_CMD_PROBE_088F_000C, &spec) == 0);
	assert(spec.cmd == 0x088f);
	assert(spec.data[0] == 0x00);
	assert(spec.data[1] == 0x0c);
	assert(spec.data[2] == 0x59);

	assert(noled_cmd_probe_get_spec(99, &spec) < 0);
	assert(noled_cmd_probe_label(NOLED_CMD_PROBE_088A_01)[0] == '0');

	return 0;
}
