#ifndef NOLED_CMD_PROBE_H
#define NOLED_CMD_PROBE_H

#define NOLED_CMD_PROBE_DATA_MAX 4

#define NOLED_CMD_PROBE_088F_0010 0
#define NOLED_CMD_PROBE_088F_0110 1
#define NOLED_CMD_PROBE_088A_00 2
#define NOLED_CMD_PROBE_088A_01 3
#define NOLED_CMD_PROBE_089B_00 4
#define NOLED_CMD_PROBE_088F_000C 5

typedef struct NoledCmdProbeSpec {
	unsigned int cmd;
	unsigned int data_len;
	unsigned int flags;
	unsigned char data[NOLED_CMD_PROBE_DATA_MAX];
	const char *label;
} NoledCmdProbeSpec;

static inline void noled_cmd_probe_clear_spec(NoledCmdProbeSpec *spec)
{
	unsigned int i;

	spec->cmd = 0;
	spec->data_len = 0;
	spec->flags = 0;
	spec->label = "unknown";
	for (i = 0; i < NOLED_CMD_PROBE_DATA_MAX; i++) {
		spec->data[i] = 0;
	}
}

static inline int noled_cmd_probe_get_spec(unsigned int id, NoledCmdProbeSpec *spec)
{
	noled_cmd_probe_clear_spec(spec);

	switch (id) {
	case NOLED_CMD_PROBE_088F_0010:
		spec->cmd = 0x088f;
		spec->data_len = 3;
		spec->data[0] = 0x00;
		spec->data[1] = 0x10;
		spec->data[2] = 0x55;
		spec->label = "088F 00 10 55";
		return 0;
	case NOLED_CMD_PROBE_088F_0110:
		spec->cmd = 0x088f;
		spec->data_len = 3;
		spec->data[0] = 0x01;
		spec->data[1] = 0x10;
		spec->data[2] = 0x54;
		spec->label = "088F 01 10 54";
		return 0;
	case NOLED_CMD_PROBE_088A_00:
		spec->cmd = 0x088a;
		spec->data_len = 2;
		spec->data[0] = 0x00;
		spec->data[1] = 0x6b;
		spec->label = "088A 00 6B";
		return 0;
	case NOLED_CMD_PROBE_088A_01:
		spec->cmd = 0x088a;
		spec->data_len = 2;
		spec->data[0] = 0x01;
		spec->data[1] = 0x6a;
		spec->label = "088A 01 6A";
		return 0;
	case NOLED_CMD_PROBE_089B_00:
		spec->cmd = 0x089b;
		spec->data_len = 2;
		spec->data[0] = 0x00;
		spec->data[1] = 0x5a;
		spec->label = "089B 00 5A";
		return 0;
	case NOLED_CMD_PROBE_088F_000C:
		spec->cmd = 0x088f;
		spec->data_len = 3;
		spec->data[0] = 0x00;
		spec->data[1] = 0x0c;
		spec->data[2] = 0x59;
		spec->label = "088F 00 0C 59";
		return 0;
	default:
		return -1;
	}
}

static inline const char *noled_cmd_probe_label(unsigned int id)
{
	NoledCmdProbeSpec spec;

	return noled_cmd_probe_get_spec(id, &spec) == 0 ? spec.label : "unknown";
}

#endif
