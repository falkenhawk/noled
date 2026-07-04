#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/syscon.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>

#include "cmd_probe.h"

#define NOLED_CMD_PROBE_REQUEST_PATH "ux0:data/noled/cmd_probe.bin"
#define NOLED_CMD_PROBE_LOG_PATH "ux0:data/noled/cmd_probe.log"
#define NOLED_CMD_PROBE_LINE_MAX 192

void _start() __attribute__ ((weak, alias ("module_start")));

static void append_char(char *line, int *len, char c)
{
	if (*len < NOLED_CMD_PROBE_LINE_MAX) {
		line[*len] = c;
		*len += 1;
	}
}

static void append_str(char *line, int *len, const char *value)
{
	while (*value != '\0') {
		append_char(line, len, *value);
		value++;
	}
}

static void append_hex_fixed(char *line, int *len, unsigned int value, int digits)
{
	int shift;

	for (shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
		unsigned int nibble = (value >> shift) & 0xF;
		append_char(line, len, (char)(nibble < 10 ? '0' + nibble : 'A' + nibble - 10));
	}
}

static void log_probe_result(unsigned int id,
	const NoledCmdProbeSpec *spec,
	int read_ret,
	int exec_ret,
	const SceSysconPacket *packet)
{
	SceUID fd;
	char line[NOLED_CMD_PROBE_LINE_MAX];
	int len = 0;

	ksceIoMkdir("ux0:data", 0777);
	ksceIoMkdir("ux0:data/noled", 0777);
	fd = ksceIoOpen(NOLED_CMD_PROBE_LOG_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND,
		0777);
	if (fd < 0) {
		return;
	}

	append_str(line, &len, "cmd=0x");
	append_hex_fixed(line, &len, spec->cmd, 4);
	append_str(line, &len, " payload=");
	append_str(line, &len, spec->label);
	append_str(line, &len, " id=");
	append_hex_fixed(line, &len, id, 2);
	append_str(line, &len, " flags=0x");
	append_hex_fixed(line, &len, spec->flags, 8);
	append_str(line, &len, " read=0x");
	append_hex_fixed(line, &len, (unsigned int)read_ret, 8);
	append_str(line, &len, " exec=0x");
	append_hex_fixed(line, &len, (unsigned int)exec_ret, 8);
	if (packet != 0) {
		append_str(line, &len, " status=0x");
		append_hex_fixed(line, &len, packet->status, 8);
		append_str(line, &len, " rxresult=0x");
		append_hex_fixed(line, &len, packet->rx[SCE_SYSCON_PACKET_RX_RESULT], 2);
		append_str(line, &len, " rxlen=");
		append_hex_fixed(line, &len, packet->rx[SCE_SYSCON_PACKET_RX_LENGTH], 2);
	}
	append_char(line, &len, '\n');
	ksceIoWrite(fd, line, len);
	ksceIoClose(fd);
}

static void clear_packet(SceSysconPacket *packet)
{
	unsigned int i;
	volatile unsigned char *bytes = (volatile unsigned char *)packet;

	for (i = 0; i < sizeof(*packet); i++) {
		bytes[i] = 0;
	}
}

static int read_probe_id(unsigned int *id)
{
	SceUID fd;
	unsigned char request[2] = {0, 0};
	NoledCmdProbeSpec spec;
	int ret;

	fd = ksceIoOpen(NOLED_CMD_PROBE_REQUEST_PATH, SCE_O_RDONLY, 0);
	if (fd < 0) {
		return fd;
	}

	ret = ksceIoRead(fd, request, sizeof(request));
	ksceIoClose(fd);
	if (ret != (int)sizeof(request)) {
		return ret < 0 ? ret : -1;
	}
	if (request[1] != 0 || noled_cmd_probe_get_spec(request[0], &spec) < 0) {
		return -2;
	}

	*id = request[0];
	return 0;
}

int module_start(SceSize argc, const void *args)
{
	SceSysconPacket packet;
	NoledCmdProbeSpec spec;
	unsigned int id = 0;
	unsigned int i;
	int read_ret;
	int exec_ret = -3;

	(void)argc;
	(void)args;

	clear_packet(&packet);
	noled_cmd_probe_clear_spec(&spec);
	read_ret = read_probe_id(&id);
	if (read_ret == 0) {
		noled_cmd_probe_get_spec(id, &spec);
		packet.tx[SCE_SYSCON_PACKET_TX_CMD_LO] = spec.cmd & 0xFF;
		packet.tx[SCE_SYSCON_PACKET_TX_CMD_HI] = spec.cmd >> 8;
		packet.tx[SCE_SYSCON_PACKET_TX_LENGTH] = (unsigned char)spec.data_len;
		for (i = 0; i < spec.data_len; i++) {
			packet.tx[SCE_SYSCON_PACKET_TX_DATA(i)] = spec.data[i];
		}
		ksceSysconWaitInitialized();
		exec_ret = ksceSysconCmdExec(&packet, spec.flags);
	}

	log_probe_result(id, &spec, read_ret, exec_ret, &packet);
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	(void)argc;
	(void)args;

	return SCE_KERNEL_STOP_SUCCESS;
}
