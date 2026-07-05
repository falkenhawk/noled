#include <stdint.h>

#include <taihen.h>
#include <psp2common/kernel/modulemgr.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/syscon.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>
#include <psp2kern/lowio/gpio.h>
#include <psp2kern/power.h>

#include "led_candidate_patch.h"
#include "led_policy.h"
#include "dolce_led_policy.h"
#include "gpio_trace_policy.h"
#include "runtime_probe.h"
#include "syscon_experiment_policy.h"
#include "syscon_trace.h"

#define SCE_LOWIO_KSCE_GPIO_PORT_SET_NID 0xD454A584
#define SCE_LOWIO_KSCE_GPIO_PORT_CLEAR_NID 0xF6310435
#define SCE_LOWIO_KSCE_GPIO_PORT_RESET_NID 0xA1B5A462
#define SCE_LOWIO_KSCE_GPIO_SET_PORT_MODE_NID 0x372022A4
#define SCE_SYSCON_KSCE_SYSCON_CMD_EXEC_NID 0x9ADDCA4A
#define SCE_SYSCON_KSCE_SYSCON_CMD_EXEC_ASYNC_NID 0xC2224E82
#define SCE_SYSCON_KSCE_SYSCON_CMD_SYNC_NID 0x6E517D22
#define SCE_SYSCON_KSCE_SYSCON_READ_COMMAND_NID 0x299B1CE7
#define SCE_SYSCON_KSCE_SYSCON_SEND_COMMAND_NID 0xE26488B9
#define SCE_SYSCON_KSCE_SYSCON_CTRL_DOLCE_LED_NID 0x727F985A
#define NOLED_SYSCON_TRACE_LOG_PATH "ux0:data/noled/syscon_trace.log"
#define NOLED_GPIO_SAMPLE_LOG_PATH "ux0:data/noled/gpio_sample.log"
#define NOLED_MODULE_MAP_LOG_PATH "ux0:data/noled/module_map.log"
#define NOLED_SYSCON_TRACE_RING_SIZE 2048
#define NOLED_SYSCON_TRACE_THREAD_PRIORITY 0x10000100
#define NOLED_SYSCON_TRACE_THREAD_STACK 0x4000
#define NOLED_SYSCON_TRACE_FLUSH_DELAY_US 100000
#define NOLED_SYSCON_TRACE_FLUSH_LOOPS 3000
#define NOLED_GPIO_SAMPLE_THREAD_PRIORITY 0x10000100
#define NOLED_GPIO_SAMPLE_THREAD_STACK 0x4000
#define NOLED_GPIO_SAMPLE_DELAY_US 100000
#define NOLED_GPIO_SAMPLE_LOOPS 6000
#define NOLED_RUNTIME_PROBE_THREAD_PRIORITY 0x10000100
#define NOLED_RUNTIME_PROBE_THREAD_STACK 0x4000
#define NOLED_RUNTIME_PROBE_DELAY_US 200000
#define NOLED_RUNTIME_PROBE_LOOPS 9000
#define NOLED_POWER_LED_POLICY_DELAY_US 500000
#define NOLED_POWER_LED_POLICY_RESUME_SETTLE_US 500000
#define NOLED_POWER_LED_POLICY_HEARTBEAT_LOOPS 20
#define NOLED_BOOT_CONFIG_WAIT_TRIES 120
#define NOLED_RUNTIME_PROBE_PING_ALIVE 0x4E4C4544
#define NOLED_RUNTIME_PROBE_ERR_REJECTED -2
#define NOLED_RUNTIME_PROBE_ERR_UNRESOLVED -3
#define NOLED_GPIO_SAMPLE_PORT_COUNT 22
#define NOLED_TRACE_PHASE_START 'S'
#define NOLED_TRACE_PHASE_END 'E'
#define NOLED_TRACE_PHASE_CALL_EXEC 'C'
#define NOLED_TRACE_PHASE_CALL_EXEC_ASYNC 'A'
#define NOLED_TRACE_PHASE_CALL_SYNC 'Y'
#define NOLED_TRACE_PHASE_CALL_READ 'R'
#define NOLED_TRACE_PHASE_CALL_SEND 'D'
#define NOLED_TRACE_PHASE_POWER 'P'
#define NOLED_TRACE_PHASE_MARKER 'M'
#define NOLED_TRACE_LINE_MAX 320
#define NOLED_MARKER_TRACE_READY 1
#define NOLED_MARKER_HOOK_CMD_EXEC 2
#define NOLED_MARKER_HOOK_CMD_EXEC_ASYNC 3
#define NOLED_MARKER_HOOK_CMD_SYNC 4
#define NOLED_MARKER_HOOK_READ_COMMAND 5
#define NOLED_MARKER_HOOK_SEND_COMMAND 6
#define NOLED_MARKER_HOOK_GPIO 7
#define NOLED_MARKER_LED_CANDIDATE_CONFIG 8
#define NOLED_MARKER_LED_CANDIDATE_PATCH 9
#define NOLED_MARKER_DOLCE_LED_HOOK 11
#define NOLED_MARKER_GPIO_PORT_SET 12
#define NOLED_MARKER_GPIO_PORT_CLEAR 13
#define NOLED_MARKER_GPIO_PORT_RESET 14
#define NOLED_MARKER_GPIO_SET_PORT_MODE 15
#define NOLED_MARKER_HOOK_GPIO_PORT_CLEAR 16
#define NOLED_MARKER_HOOK_GPIO_PORT_RESET 17
#define NOLED_MARKER_HOOK_GPIO_SET_PORT_MODE 18
#define NOLED_MARKER_MODULE_MAP_READY 19

void _start() __attribute__ ((weak, alias ("module_start")));
int ksceSysconCtrlDolceLED(int enable);

/* SceLedConfiguration per wiki.henkaku.xyz/vita/ScePower, size must be 0x14 */
typedef struct NoledSceLedConfiguration {
	unsigned int size;
	unsigned int on_time;
	unsigned int off_time;
	int time_limit;
	unsigned int end_blink_state;
} NoledSceLedConfiguration;

int ksceLedSetMode(int led, int mode, NoledSceLedConfiguration *config);

/* taihenModuleUtils export for runtime NID resolution */
extern int module_get_export_func(SceUID pid,
	const char *modname,
	uint32_t libnid,
	uint32_t funcnid,
	uintptr_t *func);
	
static SceUID hook_gpio_port_set_uid = -1;
static tai_hook_ref_t hook_gpio_port_set_ref;
static SceUID hook_gpio_port_clear_uid = -1;
static tai_hook_ref_t hook_gpio_port_clear_ref;
static SceUID hook_gpio_port_reset_uid = -1;
static tai_hook_ref_t hook_gpio_port_reset_ref;
static SceUID hook_gpio_set_port_mode_uid = -1;
static tai_hook_ref_t hook_gpio_set_port_mode_ref;
static SceUID hook_syscon_cmd_exec_uid = -1;
static tai_hook_ref_t hook_syscon_cmd_exec_ref;
static SceUID hook_syscon_cmd_exec_async_uid = -1;
static tai_hook_ref_t hook_syscon_cmd_exec_async_ref;
static SceUID hook_syscon_cmd_sync_uid = -1;
static tai_hook_ref_t hook_syscon_cmd_sync_ref;
static SceUID hook_syscon_read_command_uid = -1;
static tai_hook_ref_t hook_syscon_read_command_ref;
static SceUID hook_syscon_send_command_uid = -1;
static tai_hook_ref_t hook_syscon_send_command_ref;
static SceUID hook_syscon_ctrl_dolce_led_uid = -1;
static tai_hook_ref_t hook_syscon_ctrl_dolce_led_ref;
static volatile int syscon_trace_running = 0;
static volatile int gpio_sample_running = 0;
static volatile int runtime_probe_running = 0;
static SceUID runtime_probe_thread_uid = -1;
static volatile int power_led_policy_running = 0;
static volatile int power_led_policy_resumed = 0;
static SceUID power_led_policy_thread_uid = -1;
static volatile int led_candidate_config_loaded = 0;
static SceUID boot_policy_thread_uid = -1;
static volatile unsigned int led_candidate_mode = NOLED_LED_CANDIDATE_PASS;
static volatile unsigned int syscon_trace_next_seq = 1;
static unsigned int syscon_trace_read_seq = 1;
static SceUID syscon_trace_thread_uid = -1;
static SceUID gpio_sample_thread_uid = -1;

typedef struct NoledGpioSamplePort {
	int bus;
	int port;
} NoledGpioSamplePort;

typedef struct NoledSysconTraceEvent {
	unsigned int seq;
	unsigned int phase;
	unsigned int cmd;
	unsigned int tx_len;
	unsigned int rx_len;
	unsigned int rx_result;
	unsigned int packet_status;
	unsigned int packet_time;
	unsigned int caller_addr;
	unsigned int flags;
	unsigned char tx_data[NOLED_SYSCON_TRACE_DATA_MAX];
	unsigned char rx_data[NOLED_SYSCON_TRACE_DATA_MAX];
} NoledSysconTraceEvent;

static NoledSysconTraceEvent syscon_trace_ring[NOLED_SYSCON_TRACE_RING_SIZE];
static const NoledGpioSamplePort gpio_sample_ports[NOLED_GPIO_SAMPLE_PORT_COUNT] = {
	{0, 0},
	{0, 1},
	{0, 2},
	{0, 3},
	{0, 4},
	{0, 5},
	{0, 6},
	{0, 7},
	{0, 8},
	{0, 9},
	{0, 10},
	{0, 11},
	{0, 12},
	{0, 13},
	{0, 14},
	{0, 15},
	{0, 28},
	{1, 0},
	{1, 1},
	{1, 2},
	{1, 3},
	{1, 4}
};

static const char *tai_module_candidates[] = {
	"SceAVConfig",
	"SceAppMgr",
	"SceAppUtil",
	"SceAtrac",
	"SceAudio",
	"SceAudioin",
	"SceAvPlayer",
	"SceAvcodec",
	"SceAvcodecUser",
	"SceBbmc",
	"SceBgAppUtil",
	"SceBt",
	"SceCamera",
	"SceClipboard",
	"SceClockgen",
	"SceCodecEnginePerf",
	"SceCodecEngineWrapper",
	"SceCommonDialog",
	"SceCompat",
	"SceCoredump",
	"SceCtrl",
	"SceDeci4pDbgp",
	"SceDeci4pDtracep",
	"SceDeci4pUserp",
	"SceDisplay",
	"SceDriverUser",
	"SceError",
	"SceExcpmgr",
	"SceFace",
	"SceFiber",
	"SceFios2Kernel",
	"SceGameUpdate",
	"SceGps",
	"SceGpuEs4",
	"SceGxm",
	"SceHandwriting",
	"SceHid",
	"SceIdStorage",
	"SceIme",
	"SceIncomingDialog",
	"SceIofilemgr",
	"SceJpegArm",
	"SceJpegEncArm",
	"SceKernelBootimage",
	"SceKernelDmacMgr",
	"SceKernelIntrMgr",
	"SceKernelModulemgr",
	"SceKernelThreadMgr",
	"SceLcd",
	"SceLibDbg",
	"SceLibFios2",
	"SceLibG729",
	"SceLibHttp",
	"SceLibJson",
	"SceLibKernel",
	"SceLibLocation",
	"SceLibLocationExtension",
	"SceLibMono",
	"SceLibMonoBridge",
	"SceLibMp4Recorder",
	"SceLibMtp",
	"SceLibNetCtl",
	"SceLibPgf",
	"SceLibPspnetAdhoc",
	"SceLibPvf",
	"SceLibRudp",
	"SceLibSsl",
	"SceLibXml",
	"SceLibc",
	"SceLiveAreaUtil",
	"SceLowio",
	"SceLsdb",
	"SceMotionDev",
	"SceMsif",
	"SceMtpIfDriver",
	"SceMusicExport",
	"SceNearDialogUtil",
	"SceNearUtil",
	"SceNet",
	"SceNetAdhocMatching",
	"SceNetPs",
	"SceNgs",
	"SceNgsUser",
	"SceNotificationUtil",
	"SceNpActivity",
	"SceNpBasic",
	"SceNpCommerce2",
	"SceNpCommon",
	"SceNpDrm",
	"SceNpManager",
	"SceNpMatching2",
	"SceNpMessage",
	"SceNpParty",
	"SceNpScore",
	"SceNpSignaling",
	"SceNpSnsFacebook",
	"SceNpTrophy",
	"SceNpTus",
	"SceNpUtility",
	"SceNpWebApi",
	"SceOled",
	"ScePaf",
	"ScePamgr",
	"ScePerf",
	"ScePfsMgr",
	"ScePhotoExport",
	"ScePower",
	"SceProcessmgr",
	"ScePromoterUtil",
	"SceRazorCapture",
	"SceRazorHud",
	"SceRegistryMgr",
	"SceRtc",
	"SceSas",
	"SceSblACMgr",
	"SceSblAuthMgr",
	"SceSblGcAuthMgr",
	"SceSblPostSsMgr",
	"SceSblSmschedProxy",
	"SceSblSsMgr",
	"SceSblSsSmComm",
	"SceSblUpdateMgr",
	"SceScreenShot",
	"SceSdif",
	"SceShaccCg",
	"SceShellSvc",
	"SceShutterSound",
	"SceSmart",
	"SceSqlite",
	"SceSulpha",
	"SceSyscon",
	"SceSysmem",
	"SceSysmodule",
	"SceSystemGesture",
	"SceSystimer",
	"SceTeleportClient",
	"SceTeleportServer",
	"SceTouch",
	"SceTriggerUtil",
	"SceUdcd",
	"SceUlobjMgr",
	"SceUlt",
	"SceUsbAudio",
	"SceUsbPspcm",
	"SceUsbSerial",
	"SceUsbServ",
	"SceUsbd",
	"SceUsbstorVStorDriver",
	"SceVideoExport",
	"SceVoice",
	"SceVoiceQoS",
	"SceVshBridge",
	"SceWlanBt"
};
#define NOLED_TAI_MODULE_CANDIDATE_COUNT \
	((unsigned int)(sizeof(tai_module_candidates) / sizeof(tai_module_candidates[0])))

static void noled_syscon_trace_start(SceSysconPacket *packet);
static void noled_syscon_trace_end(SceSysconPacket *packet);
static void trace_record_marker_value(unsigned int marker,
	unsigned int led,
	unsigned int enable,
	unsigned int value,
	int result);
static void refresh_led_candidate_config(void);
static int apply_led_candidate_patch(SceSysconPacket *packet);
static int noled_syscon_ctrl_dolce_led(int enable);
static SceUID noled_gpio_port_set(int bus, int port);
static SceUID noled_gpio_port_clear(int bus, int port);
static SceUID noled_gpio_port_reset(int bus, int port);
static int noled_gpio_set_port_mode(int bus, int port, int mode);
static void start_gpio_sampler(void);
static void start_runtime_probe(void);
static void start_power_led_off_policy(void);
static int dump_kernel_module_map(void);

static SceSysconDebugHandlers syscon_trace_handlers = {
	sizeof(SceSysconDebugHandlers),
	noled_syscon_trace_start,
	noled_syscon_trace_end
};

static void trace_append_char(char *line, int *len, char c)
{
	if (*len < NOLED_TRACE_LINE_MAX) {
		line[*len] = c;
		*len += 1;
	}
}

static void trace_append_str(char *line, int *len, const char *value)
{
	while (*value != '\0') {
		trace_append_char(line, len, *value);
		value++;
	}
}

static void trace_append_dec_u32(char *line, int *len, unsigned int value)
{
	char digits[10];
	int count = 0;

	if (value == 0) {
		trace_append_char(line, len, '0');
		return;
	}

	while (value > 0 && count < (int)sizeof(digits)) {
		digits[count] = (char)('0' + (value % 10));
		value /= 10;
		count++;
	}

	while (count > 0) {
		count--;
		trace_append_char(line, len, digits[count]);
	}
}

static void trace_append_hex_fixed(char *line, int *len, unsigned int value, int digits)
{
	int shift;

	for (shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
		unsigned int nibble = (value >> shift) & 0xF;
		trace_append_char(line, len, (char)(nibble < 10 ? '0' + nibble : 'A' + nibble - 10));
	}
}

static void trace_append_bytes(char *line, int *len, const unsigned char *data, unsigned int data_len)
{
	unsigned int i;

	if (data_len == 0) {
		trace_append_char(line, len, '-');
		return;
	}

	for (i = 0; i < data_len; i++) {
		trace_append_hex_fixed(line, len, data[i], 2);
	}
}

static void trace_write_str(SceUID fd, const char *value)
{
	int len = 0;

	while (value[len] != '\0') {
		len++;
	}

	ksceIoWrite(fd, value, len);
}

static void trace_copy_packet_data(unsigned char dst[NOLED_SYSCON_TRACE_DATA_MAX],
	const unsigned char *src,
	unsigned int len)
{
	unsigned int i;

	len = noled_syscon_clamp_data_len(len);

	for (i = 0; i < NOLED_SYSCON_TRACE_DATA_MAX; i++) {
		dst[i] = 0;
	}

	if (src == 0) {
		return;
	}

	for (i = 0; i < len; i++) {
		dst[i] = src[i];
	}
}

static int trace_is_call_phase(unsigned int phase)
{
	return phase == NOLED_TRACE_PHASE_CALL_EXEC ||
		phase == NOLED_TRACE_PHASE_CALL_EXEC_ASYNC ||
		phase == NOLED_TRACE_PHASE_CALL_SYNC ||
		phase == NOLED_TRACE_PHASE_CALL_READ ||
		phase == NOLED_TRACE_PHASE_CALL_SEND;
}

static void trace_record(unsigned int phase, SceSysconPacket *packet)
{
	unsigned int seq;
	unsigned int cmd;
	unsigned int tx_len;
	unsigned int rx_len;
	NoledSysconTraceEvent *event;

	if (!syscon_trace_running || packet == 0) {
		return;
	}

	cmd = noled_syscon_packet_cmd(packet->tx);
	if (!noled_syscon_should_trace_cmd(cmd)) {
		return;
	}

	tx_len = noled_syscon_tx_data_len(packet->tx);
	rx_len = phase == NOLED_TRACE_PHASE_END ? noled_syscon_rx_data_len(packet->rx) : 0;
	seq = syscon_trace_next_seq;
	event = &syscon_trace_ring[(seq - 1) % NOLED_SYSCON_TRACE_RING_SIZE];

	event->seq = 0;
	event->phase = phase;
	event->cmd = cmd;
	event->tx_len = tx_len;
	event->rx_len = rx_len;
	event->rx_result = phase == NOLED_TRACE_PHASE_END ? noled_syscon_rx_result(packet->rx) : 0;
	event->packet_status = packet->status;
	event->packet_time = packet->time;
	event->caller_addr = 0;
	event->flags = 0;
	trace_copy_packet_data(event->tx_data, &packet->tx[SCE_SYSCON_PACKET_TX_DATA(0)], tx_len);
	trace_copy_packet_data(event->rx_data, &packet->rx[SCE_SYSCON_PACKET_RX_DATA(0)], rx_len);
	event->seq = seq;
	syscon_trace_next_seq = seq + 1;
}

static void trace_record_packet_call(unsigned int phase,
	SceSysconPacket *packet,
	unsigned int flags,
	const void *caller)
{
	unsigned int seq;
	unsigned int cmd;
	unsigned int tx_len;
	NoledSysconTraceEvent *event;

	if (!syscon_trace_running || packet == 0) {
		return;
	}

	cmd = noled_syscon_packet_cmd(packet->tx);
	if (!noled_syscon_should_trace_cmd(cmd)) {
		return;
	}

	tx_len = noled_syscon_tx_data_len(packet->tx);
	seq = syscon_trace_next_seq;
	event = &syscon_trace_ring[(seq - 1) % NOLED_SYSCON_TRACE_RING_SIZE];
	event->seq = 0;
	event->phase = phase;
	event->cmd = cmd;
	event->tx_len = tx_len;
	event->rx_len = 0;
	event->rx_result = 0;
	event->packet_status = packet->status;
	event->packet_time = packet->time;
	event->caller_addr = (unsigned int)caller;
	event->flags = flags;
	trace_copy_packet_data(event->tx_data, &packet->tx[SCE_SYSCON_PACKET_TX_DATA(0)], tx_len);
	trace_copy_packet_data(event->rx_data, 0, 0);
	event->seq = seq;
	syscon_trace_next_seq = seq + 1;
}

static void trace_record_command_call(unsigned int phase,
	unsigned int cmd,
	const void *buffer,
	unsigned int size,
	const void *caller)
{
	unsigned int seq;
	unsigned int tx_len;
	NoledSysconTraceEvent *event;

	if (!syscon_trace_running) {
		return;
	}

	if (!noled_syscon_should_trace_cmd(cmd)) {
		return;
	}

	tx_len = noled_syscon_clamp_data_len(size);
	seq = syscon_trace_next_seq;
	event = &syscon_trace_ring[(seq - 1) % NOLED_SYSCON_TRACE_RING_SIZE];
	event->seq = 0;
	event->phase = phase;
	event->cmd = cmd;
	event->tx_len = tx_len;
	event->rx_len = 0;
	event->rx_result = 0;
	event->packet_status = size;
	event->packet_time = 0;
	event->caller_addr = (unsigned int)caller;
	event->flags = size;
	trace_copy_packet_data(event->tx_data, buffer, tx_len);
	trace_copy_packet_data(event->rx_data, 0, 0);
	event->seq = seq;
	syscon_trace_next_seq = seq + 1;
}

static void trace_record_marker(unsigned int marker, unsigned int led, unsigned int enable, int result)
{
	trace_record_marker_value(marker, led, enable, 0, result);
}

static void trace_record_marker_value(unsigned int marker,
	unsigned int led,
	unsigned int enable,
	unsigned int value,
	int result)
{
	unsigned int seq;
	NoledSysconTraceEvent *event;

	if (!syscon_trace_running) {
		return;
	}

	seq = syscon_trace_next_seq;
	event = &syscon_trace_ring[(seq - 1) % NOLED_SYSCON_TRACE_RING_SIZE];
	event->seq = 0;
	event->phase = NOLED_TRACE_PHASE_MARKER;
	event->cmd = marker;
	event->tx_len = 2;
	event->rx_len = 0;
	event->rx_result = 0;
	event->packet_status = (unsigned int)result;
	event->packet_time = 0;
	event->caller_addr = 0;
	event->flags = value;
	trace_copy_packet_data(event->tx_data, 0, 0);
	event->tx_data[0] = (unsigned char)led;
	event->tx_data[1] = (unsigned char)enable;
	trace_copy_packet_data(event->rx_data, 0, 0);
	event->seq = seq;
	syscon_trace_next_seq = seq + 1;
}

static void trace_record_power_state(void)
{
	unsigned int seq;
	NoledSysconTraceEvent *event;

	if (!syscon_trace_running) {
		return;
	}

	seq = syscon_trace_next_seq;
	event = &syscon_trace_ring[(seq - 1) % NOLED_SYSCON_TRACE_RING_SIZE];
	event->seq = 0;
	event->phase = NOLED_TRACE_PHASE_POWER;
	event->cmd = 0xFFFF;
	event->tx_len = 3;
	event->rx_len = 0;
	event->rx_result = 0;
	event->packet_status = 0;
	event->packet_time = 0;
	event->caller_addr = 0;
	event->flags = 0;
	trace_copy_packet_data(event->tx_data, 0, 0);
	event->tx_data[0] = (unsigned char)kscePowerIsPowerOnline();
	event->tx_data[1] = (unsigned char)kscePowerIsBatteryCharging();
	event->tx_data[2] = (unsigned char)kscePowerGetBatteryLifePercent();
	trace_copy_packet_data(event->rx_data, 0, 0);
	event->seq = seq;
	syscon_trace_next_seq = seq + 1;
}

static void noled_syscon_trace_start(SceSysconPacket *packet)
{
	trace_record(NOLED_TRACE_PHASE_START, packet);
}

static void noled_syscon_trace_end(SceSysconPacket *packet)
{
	trace_record(NOLED_TRACE_PHASE_END, packet);
}

static const char *trace_marker_name(unsigned int marker)
{
	switch (marker) {
	case NOLED_MARKER_TRACE_READY:
		return "trace_ready";
	case NOLED_MARKER_HOOK_CMD_EXEC:
		return "hook_cmd_exec";
	case NOLED_MARKER_HOOK_CMD_EXEC_ASYNC:
		return "hook_cmd_exec_async";
	case NOLED_MARKER_HOOK_CMD_SYNC:
		return "hook_cmd_sync";
	case NOLED_MARKER_HOOK_READ_COMMAND:
		return "hook_read_command";
	case NOLED_MARKER_HOOK_SEND_COMMAND:
		return "hook_send_command";
	case NOLED_MARKER_HOOK_GPIO:
		return "hook_gpio";
	case NOLED_MARKER_HOOK_GPIO_PORT_CLEAR:
		return "hook_gpio_port_clear";
	case NOLED_MARKER_HOOK_GPIO_PORT_RESET:
		return "hook_gpio_port_reset";
	case NOLED_MARKER_HOOK_GPIO_SET_PORT_MODE:
		return "hook_gpio_set_port_mode";
	case NOLED_MARKER_MODULE_MAP_READY:
		return "module_map_ready";
	case NOLED_MARKER_LED_CANDIDATE_CONFIG:
		return "led_candidate_config";
	case NOLED_MARKER_LED_CANDIDATE_PATCH:
		return "led_candidate_patch";
	case NOLED_MARKER_DOLCE_LED_HOOK:
		return "dolce_led_hook";
	case NOLED_MARKER_GPIO_PORT_SET:
		return "gpio_port_set";
	case NOLED_MARKER_GPIO_PORT_CLEAR:
		return "gpio_port_clear";
	case NOLED_MARKER_GPIO_PORT_RESET:
		return "gpio_port_reset";
	case NOLED_MARKER_GPIO_SET_PORT_MODE:
		return "gpio_set_port_mode";
	default:
		return "unknown";
	}
}

static int trace_marker_has_value(unsigned int marker)
{
	return marker == NOLED_MARKER_GPIO_SET_PORT_MODE;
}

static void trace_write_lost(SceUID fd, unsigned int from_seq, unsigned int to_seq)
{
	char line[NOLED_TRACE_LINE_MAX];
	int len = 0;

	trace_append_str(line, &len, "lost from=");
	trace_append_dec_u32(line, &len, from_seq);
	trace_append_str(line, &len, " to=");
	trace_append_dec_u32(line, &len, to_seq);
	trace_append_char(line, &len, '\n');
	ksceIoWrite(fd, line, len);
}

static void trace_write_event(SceUID fd, const NoledSysconTraceEvent *event)
{
	char line[NOLED_TRACE_LINE_MAX];
	int len = 0;

	if (event->phase == NOLED_TRACE_PHASE_POWER) {
		trace_append_dec_u32(line, &len, event->seq);
		trace_append_str(line, &len, " P power online=");
		trace_append_dec_u32(line, &len, event->tx_data[0]);
		trace_append_str(line, &len, " charging=");
		trace_append_dec_u32(line, &len, event->tx_data[1]);
		trace_append_str(line, &len, " percent=");
		trace_append_dec_u32(line, &len, event->tx_data[2]);
		trace_append_char(line, &len, '\n');
		ksceIoWrite(fd, line, len);
		return;
	}

	if (event->phase == NOLED_TRACE_PHASE_MARKER) {
		trace_append_dec_u32(line, &len, event->seq);
		trace_append_str(line, &len, " M marker=");
		trace_append_str(line, &len, trace_marker_name(event->cmd));
		trace_append_str(line, &len, " led=");
		trace_append_dec_u32(line, &len, event->tx_data[0]);
		trace_append_str(line, &len, " enable=");
		trace_append_dec_u32(line, &len, event->tx_data[1]);
		if (trace_marker_has_value(event->cmd)) {
			trace_append_str(line, &len, " value=0x");
			trace_append_hex_fixed(line, &len, event->flags, 8);
		}
		trace_append_str(line, &len, " ret=0x");
		trace_append_hex_fixed(line, &len, event->packet_status, 8);
		trace_append_char(line, &len, '\n');
		ksceIoWrite(fd, line, len);
		return;
	}

	if (trace_is_call_phase(event->phase)) {
		trace_append_dec_u32(line, &len, event->seq);
		trace_append_char(line, &len, ' ');
		trace_append_char(line, &len, (char)event->phase);
		trace_append_str(line, &len, " cmd=0x");
		trace_append_hex_fixed(line, &len, event->cmd, 4);
		if (event->phase == NOLED_TRACE_PHASE_CALL_READ ||
			event->phase == NOLED_TRACE_PHASE_CALL_SEND) {
			trace_append_str(line, &len, " size=");
			trace_append_dec_u32(line, &len, event->flags);
		} else {
			trace_append_str(line, &len, " flags=0x");
			trace_append_hex_fixed(line, &len, event->flags, 8);
		}
		trace_append_str(line, &len, " caller=0x");
		trace_append_hex_fixed(line, &len, event->caller_addr, 8);
		trace_append_str(line, &len, " txlen=");
		trace_append_dec_u32(line, &len, event->tx_len);
		trace_append_str(line, &len, " tx=");
		trace_append_bytes(line, &len, event->tx_data, event->tx_len);
		trace_append_char(line, &len, '\n');
		ksceIoWrite(fd, line, len);
		return;
	}

	trace_append_dec_u32(line, &len, event->seq);
	trace_append_char(line, &len, ' ');
	trace_append_char(line, &len, (char)event->phase);
	trace_append_str(line, &len, " cmd=0x");
	trace_append_hex_fixed(line, &len, event->cmd, 4);
	trace_append_str(line, &len, " txlen=");
	trace_append_dec_u32(line, &len, event->tx_len);
	trace_append_str(line, &len, " tx=");
	trace_append_bytes(line, &len, event->tx_data, event->tx_len);
	trace_append_str(line, &len, " rxlen=");
	trace_append_dec_u32(line, &len, event->rx_len);
	trace_append_str(line, &len, " result=0x");
	trace_append_hex_fixed(line, &len, event->rx_result, 2);
	trace_append_str(line, &len, " rx=");
	trace_append_bytes(line, &len, event->rx_data, event->rx_len);
	trace_append_str(line, &len, " status=0x");
	trace_append_hex_fixed(line, &len, event->packet_status, 8);
	trace_append_str(line, &len, " time=0x");
	trace_append_hex_fixed(line, &len, event->packet_time, 8);
	trace_append_char(line, &len, '\n');
	ksceIoWrite(fd, line, len);
}

static void trace_flush(SceUID fd)
{
	while (syscon_trace_read_seq < syscon_trace_next_seq) {
		unsigned int seq = syscon_trace_read_seq;
		const NoledSysconTraceEvent *event =
			&syscon_trace_ring[(seq - 1) % NOLED_SYSCON_TRACE_RING_SIZE];

		if (event->seq != seq) {
			if (event->seq > seq) {
				trace_write_lost(fd, seq, event->seq - 1);
				syscon_trace_read_seq = event->seq;
				continue;
			}
			break;
		}

		trace_write_event(fd, event);
		syscon_trace_read_seq++;
	}
}

static void trace_sync_fd(SceUID fd)
{
	int status = 0;

	ksceIoSyncByFd(fd, &status);
}

static void trace_zero_memory(void *ptr, unsigned int size)
{
	unsigned int i;
	unsigned char *bytes = (unsigned char *)ptr;

	if (bytes == 0) {
		return;
	}

	for (i = 0; i < size; i++) {
		bytes[i] = 0;
	}
}

static void trace_append_fixed_string(char *line,
	int *len,
	const char *value,
	unsigned int value_size)
{
	unsigned int i;

	if (value == 0) {
		trace_append_str(line, len, "?");
		return;
	}

	for (i = 0; i < value_size && value[i] != '\0'; i++) {
		trace_append_char(line, len, value[i]);
	}
}

static void module_map_write_summary(SceUID fd, const char *key, int value)
{
	char line[NOLED_TRACE_LINE_MAX];
	int len = 0;

	trace_append_str(line, &len, key);
	trace_append_str(line, &len, "=0x");
	trace_append_hex_fixed(line, &len, (unsigned int)value, 8);
	trace_append_char(line, &len, '\n');
	ksceIoWrite(fd, line, len);
}

static void module_map_write_tai_module(SceUID fd,
	unsigned int index,
	const char *requested_name,
	const tai_module_info_t *info,
	int result)
{
	char line[NOLED_TRACE_LINE_MAX];
	int len = 0;

	trace_append_str(line, &len, "tai_module index=");
	trace_append_dec_u32(line, &len, index);
	trace_append_str(line, &len, " requested=");
	trace_append_str(line, &len, requested_name);
	trace_append_str(line, &len, " modid=0x");
	trace_append_hex_fixed(line, &len, (unsigned int)info->modid, 8);
	trace_append_str(line, &len, " ret=0x");
	trace_append_hex_fixed(line, &len, (unsigned int)result, 8);
	trace_append_str(line, &len, " name=");
	trace_append_fixed_string(line, &len, info->name, sizeof(info->name));
	trace_append_str(line, &len, " nid=0x");
	trace_append_hex_fixed(line, &len, info->module_nid, 8);
	trace_append_str(line, &len, " exports=0x");
	trace_append_hex_fixed(line, &len, (unsigned int)info->exports_start, 8);
	trace_append_str(line, &len, "-0x");
	trace_append_hex_fixed(line, &len, (unsigned int)info->exports_end, 8);
	trace_append_str(line, &len, " imports=0x");
	trace_append_hex_fixed(line, &len, (unsigned int)info->imports_start, 8);
	trace_append_str(line, &len, "-0x");
	trace_append_hex_fixed(line, &len, (unsigned int)info->imports_end, 8);
	trace_append_char(line, &len, '\n');
	ksceIoWrite(fd, line, len);
}

static int dump_kernel_module_map(void)
{
	SceUID fd;
	int successes = 0;
	int last_ret = 0;
	unsigned int i;

	ksceIoMkdir("ux0:data", 0777);
	ksceIoMkdir("ux0:data/noled", 0777);
	fd = ksceIoOpen(NOLED_MODULE_MAP_LOG_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
		0777);
	if (fd < 0) {
		return fd;
	}

	trace_write_str(fd, "--- noLED kernel module map start ---\n");
	trace_write_str(fd, "module manager ForKernel import intentionally avoided; raw syscon caller addresses stay in syscon_trace.log\n");
	module_map_write_summary(fd, "candidate_count",
		NOLED_TAI_MODULE_CANDIDATE_COUNT);

	for (i = 0; i < NOLED_TAI_MODULE_CANDIDATE_COUNT; i++) {
		tai_module_info_t info;
		int info_ret;

		trace_zero_memory(&info, sizeof(info));
		info.size = sizeof(info);
		info_ret = taiGetModuleInfoForKernel(KERNEL_PID,
			tai_module_candidates[i],
			&info);
		last_ret = info_ret;
		if (info_ret >= 0) {
			successes++;
		}
		module_map_write_tai_module(fd,
			i,
			tai_module_candidates[i],
			&info,
			info_ret);
	}

	module_map_write_summary(fd, "success_count", successes);
	trace_write_str(fd, "--- noLED kernel module map stop ---\n");
	trace_sync_fd(fd);
	ksceIoClose(fd);
	return successes > 0 ? 0 : last_ret;
}

static void gpio_sample_write_line(SceUID fd,
	unsigned int seq,
	const char *kind,
	int bus,
	int port,
	int value,
	int online,
	int charging,
	int percent)
{
	char line[NOLED_TRACE_LINE_MAX];
	int len = 0;

	trace_append_str(line, &len, "seq=");
	trace_append_dec_u32(line, &len, seq);
	trace_append_str(line, &len, " type=");
	trace_append_str(line, &len, kind);
	trace_append_str(line, &len, " bus=");
	trace_append_dec_u32(line, &len, (unsigned int)bus);
	trace_append_str(line, &len, " port=");
	trace_append_dec_u32(line, &len, (unsigned int)port);
	trace_append_str(line, &len, " value=0x");
	trace_append_hex_fixed(line, &len, (unsigned int)value, 8);
	trace_append_str(line, &len, " online=");
	trace_append_dec_u32(line, &len, (unsigned int)online);
	trace_append_str(line, &len, " charging=");
	trace_append_dec_u32(line, &len, (unsigned int)charging);
	trace_append_str(line, &len, " percent=");
	trace_append_dec_u32(line, &len, (unsigned int)percent);
	trace_append_char(line, &len, '\n');
	ksceIoWrite(fd, line, len);
}

static void gpio_sample_write_marker(SceUID fd, const char *message)
{
	trace_write_str(fd, "marker=");
	trace_write_str(fd, message);
	trace_write_str(fd, "\n");
}

static int gpio_sample_thread(SceSize argc, void *args)
{
	SceUID fd;
	int last_values[NOLED_GPIO_SAMPLE_PORT_COUNT];
	int last_online = -1;
	int last_charging = -1;
	int last_percent = -1;
	unsigned int seq = 0;
	unsigned int i;
	int loops;

	(void)argc;
	(void)args;

	for (i = 0; i < NOLED_GPIO_SAMPLE_PORT_COUNT; i++) {
		last_values[i] = 0x7fffffff;
	}

	ksceIoMkdir("ux0:data", 0777);
	ksceIoMkdir("ux0:data/noled", 0777);
	fd = ksceIoOpen(NOLED_GPIO_SAMPLE_LOG_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
		0777);
	if (fd < 0) {
		gpio_sample_running = 0;
		return 0;
	}

	gpio_sample_write_marker(fd, "read_only_gpio_sampler_start");
	trace_sync_fd(fd);

	for (loops = 0; gpio_sample_running && loops < NOLED_GPIO_SAMPLE_LOOPS; loops++) {
		int online = kscePowerIsPowerOnline();
		int charging = kscePowerIsBatteryCharging();
		int percent = kscePowerGetBatteryLifePercent();

		if (online != last_online ||
			charging != last_charging ||
			percent != last_percent) {
			gpio_sample_write_line(fd,
				seq,
				"power",
				0,
				0,
				0,
				online,
				charging,
				percent);
			last_online = online;
			last_charging = charging;
			last_percent = percent;
		}

		for (i = 0; i < NOLED_GPIO_SAMPLE_PORT_COUNT; i++) {
			int value = ksceGpioPortRead(gpio_sample_ports[i].bus,
				gpio_sample_ports[i].port);

			if (value != last_values[i]) {
				gpio_sample_write_line(fd,
					seq,
					"gpio",
					gpio_sample_ports[i].bus,
					gpio_sample_ports[i].port,
					value,
					online,
					charging,
					percent);
				last_values[i] = value;
			}
		}

		if ((loops % 10) == 0) {
			trace_sync_fd(fd);
		}
		seq++;
		ksceKernelDelayThread(NOLED_GPIO_SAMPLE_DELAY_US);
	}

	gpio_sample_write_marker(fd, "read_only_gpio_sampler_stop");
	trace_sync_fd(fd);
	ksceIoClose(fd);
	gpio_sample_running = 0;
	return 0;
}

static void start_gpio_sampler(void)
{
	gpio_sample_running = 1;
	gpio_sample_thread_uid = ksceKernelCreateThread("noled_gpio_sample",
		gpio_sample_thread,
		NOLED_GPIO_SAMPLE_THREAD_PRIORITY,
		NOLED_GPIO_SAMPLE_THREAD_STACK,
		0,
		0,
		0);
	if (gpio_sample_thread_uid >= 0) {
		int ret = ksceKernelStartThread(gpio_sample_thread_uid, 0, 0);
		if (ret < 0) {
			ksceKernelDeleteThread(gpio_sample_thread_uid);
			gpio_sample_thread_uid = -1;
			gpio_sample_running = 0;
		}
	} else {
		gpio_sample_running = 0;
	}
}

static int runtime_probe_resolve(const char *modname,
	unsigned int libnid,
	unsigned int funcnid,
	uintptr_t *func)
{
	if (*func != 0) {
		return 0;
	}
	return module_get_export_func(KERNEL_PID, modname, libnid, funcnid, func);
}

static void runtime_probe_execute(const NoledRuntimeProbeRequest *request,
	NoledRuntimeProbeResponse *response)
{
	static uintptr_t charge_led_ctrl_func = 0;
	static uintptr_t power_led_flag_func = 0;
	static uintptr_t led_blink_type2_func = 0;

	response->magic = NOLED_RUNTIME_PROBE_RESPONSE_MAGIC;
	response->seq = request->seq;
	response->op = request->op;
	response->arg0 = request->arg0;
	response->arg1 = request->arg1;
	response->resolve_ret = 0;
	response->exec_ret = 0;

	switch (request->op) {
	case NOLED_RUNTIME_PROBE_OP_PING:
		response->exec_ret = NOLED_RUNTIME_PROBE_PING_ALIVE;
		break;
	case NOLED_RUNTIME_PROBE_OP_CTRL_LED:
		response->exec_ret = ksceSysconCtrlLED((int)request->arg0,
			(int)request->arg1);
		break;
	case NOLED_RUNTIME_PROBE_OP_DOLCE_LED:
		response->exec_ret = ksceSysconCtrlDolceLED((int)request->arg0);
		break;
	case NOLED_RUNTIME_PROBE_OP_CHARGE_LED_CTRL:
		response->resolve_ret = runtime_probe_resolve(NOLED_SYSCON_MODULE_NAME,
			NOLED_SYSCON_FOR_DRIVER_LIB_NID,
			NOLED_SYSCON_SET_CHARGE_LED_CTRL_NID,
			&charge_led_ctrl_func);
		if (response->resolve_ret == 0 && charge_led_ctrl_func != 0) {
			response->exec_ret = ((int (*)(int))charge_led_ctrl_func)((int)request->arg0);
		} else {
			response->exec_ret = NOLED_RUNTIME_PROBE_ERR_UNRESOLVED;
		}
		break;
	case NOLED_RUNTIME_PROBE_OP_POWER_LED_FLAG:
		response->resolve_ret = runtime_probe_resolve(NOLED_POWER_MODULE_NAME,
			NOLED_POWER_FOR_DRIVER_LIB_NID,
			NOLED_POWER_LED_FLAG_NID,
			&power_led_flag_func);
		if (response->resolve_ret == 0 && power_led_flag_func != 0) {
			response->exec_ret = ((int (*)(int))power_led_flag_func)((int)request->arg0);
		} else {
			response->exec_ret = NOLED_RUNTIME_PROBE_ERR_UNRESOLVED;
		}
		break;
	case NOLED_RUNTIME_PROBE_OP_LED_SET_MODE: {
		NoledSceLedConfiguration config;

		config.size = sizeof(config);
		config.on_time = request->arg2;
		config.off_time = request->arg3;
		config.time_limit = -1;
		config.end_blink_state = request->arg1 == 1 ? 1 : 0;
		response->exec_ret = ksceLedSetMode((int)request->arg0,
			(int)request->arg1,
			&config);
		break;
	}
	case NOLED_RUNTIME_PROBE_OP_LED_BLINK_TYPE2:
		response->resolve_ret = runtime_probe_resolve(NOLED_SYSCON_MODULE_NAME,
			NOLED_SYSCON_FOR_DRIVER_LIB_NID,
			NOLED_SYSCON_CTRL_LED_BLINK_TYPE2_NID,
			&led_blink_type2_func);
		if (response->resolve_ret == 0 && led_blink_type2_func != 0) {
			response->exec_ret = ((int (*)(int, int, int, int))led_blink_type2_func)(
				(int)request->arg0,
				(int)request->arg1,
				(int)request->arg2,
				(int)request->arg3);
		} else {
			response->exec_ret = NOLED_RUNTIME_PROBE_ERR_UNRESOLVED;
		}
		break;
	case NOLED_RUNTIME_PROBE_OP_PWM_BLINK: {
		static uintptr_t pwm_blink_func = 0;

		response->resolve_ret = runtime_probe_resolve(NOLED_SYSCON_MODULE_NAME,
			NOLED_SYSCON_FOR_DRIVER_LIB_NID,
			NOLED_SYSCON_CTRL_LED_PWM_BLINK_NID,
			&pwm_blink_func);
		if (response->resolve_ret == 0 && pwm_blink_func != 0) {
			response->exec_ret = ((int (*)(int, int))pwm_blink_func)(
				(int)request->arg0,
				(int)request->arg1);
		} else {
			response->exec_ret = NOLED_RUNTIME_PROBE_ERR_UNRESOLVED;
		}
		break;
	}
	case NOLED_RUNTIME_PROBE_OP_RAW_SYSCON_CMD: {
		SceSysconPacket packet;

		trace_zero_memory(&packet, sizeof(packet));
		packet.tx[SCE_SYSCON_PACKET_TX_CMD_LO] = request->arg0 & 0xFF;
		packet.tx[SCE_SYSCON_PACKET_TX_CMD_HI] = (request->arg0 >> 8) & 0xFF;
		/* empty payload: length 1 covers only the driver-added checksum,
		 * matching the decompiled DolceLED wrapper's sender call */
		packet.tx[SCE_SYSCON_PACKET_TX_LENGTH] = 1;
		response->exec_ret = ksceSysconCmdExec(&packet, 0);
		/* syscon error byte for the log; 0x80250200 | rx[3] is the code */
		response->resolve_ret = (int)packet.rx[3];
		break;
	}
	default:
		response->exec_ret = NOLED_RUNTIME_PROBE_ERR_REJECTED;
		break;
	}
}

static int runtime_probe_read_request(NoledRuntimeProbeRequest *request)
{
	SceUID fd;
	int read_len;

	fd = ksceIoOpen(NOLED_RUNTIME_PROBE_REQUEST_PATH, SCE_O_RDONLY, 0);
	if (fd < 0) {
		return fd;
	}

	read_len = ksceIoRead(fd, request, sizeof(*request));
	ksceIoClose(fd);
	return read_len == (int)sizeof(*request) ? 0 : -1;
}

static void runtime_probe_write_response(const NoledRuntimeProbeResponse *response)
{
	SceUID fd;

	fd = ksceIoOpen(NOLED_RUNTIME_PROBE_RESPONSE_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
		0777);
	if (fd < 0) {
		return;
	}

	ksceIoWrite(fd, response, sizeof(*response));
	ksceIoClose(fd);
}

static void runtime_probe_log_request(SceUID fd, const NoledRuntimeProbeRequest *request)
{
	char line[NOLED_TRACE_LINE_MAX];
	int len = 0;

	if (fd < 0) {
		return;
	}

	/* written and synced before execution so a hardware freeze still leaves
	 * evidence of which candidate was being sent */
	trace_append_str(line, &len, "try seq=");
	trace_append_dec_u32(line, &len, request->seq);
	trace_append_str(line, &len, " op=");
	trace_append_dec_u32(line, &len, request->op);
	trace_append_str(line, &len, " arg0=0x");
	trace_append_hex_fixed(line, &len, request->arg0, 4);
	trace_append_str(line, &len, " arg1=0x");
	trace_append_hex_fixed(line, &len, request->arg1, 4);
	trace_append_str(line, &len, " arg2=0x");
	trace_append_hex_fixed(line, &len, request->arg2, 4);
	trace_append_str(line, &len, " arg3=0x");
	trace_append_hex_fixed(line, &len, request->arg3, 4);
	trace_append_char(line, &len, '\n');
	ksceIoWrite(fd, line, len);
	trace_sync_fd(fd);
}

static void runtime_probe_log_response(SceUID fd, const NoledRuntimeProbeResponse *response)
{
	char line[NOLED_TRACE_LINE_MAX];
	int len = 0;

	if (fd < 0) {
		return;
	}

	trace_append_str(line, &len, "done seq=");
	trace_append_dec_u32(line, &len, response->seq);
	trace_append_str(line, &len, " op=");
	trace_append_dec_u32(line, &len, response->op);
	trace_append_str(line, &len, " resolve=0x");
	trace_append_hex_fixed(line, &len, (unsigned int)response->resolve_ret, 8);
	trace_append_str(line, &len, " ret=0x");
	trace_append_hex_fixed(line, &len, (unsigned int)response->exec_ret, 8);
	trace_append_char(line, &len, '\n');
	ksceIoWrite(fd, line, len);
	trace_sync_fd(fd);
}

static int runtime_probe_thread(SceSize argc, void *args)
{
	NoledRuntimeProbeRequest request;
	NoledRuntimeProbeResponse response;
	unsigned int last_seq = 0;
	int loops = 0;
	SceUID log_fd;

	(void)argc;
	(void)args;

	ksceSysconWaitInitialized();
	ksceIoMkdir("ux0:data", 0777);
	ksceIoMkdir("ux0:data/noled", 0777);
	log_fd = ksceIoOpen(NOLED_RUNTIME_PROBE_LOG_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND,
		0777);
	if (log_fd >= 0) {
		trace_write_str(log_fd, "--- noLED runtime LED probe start ---\n");
		trace_sync_fd(log_fd);
	}

	/* adopt any stale request so an old file can never fire a command */
	if (runtime_probe_read_request(&request) == 0 &&
		request.magic == NOLED_RUNTIME_PROBE_REQUEST_MAGIC) {
		last_seq = request.seq;
	}

	while (runtime_probe_running) {
		if (runtime_probe_read_request(&request) == 0 &&
			request.magic == NOLED_RUNTIME_PROBE_REQUEST_MAGIC &&
			request.seq != last_seq) {
			last_seq = request.seq;
			if (noled_runtime_probe_request_valid(&request)) {
				runtime_probe_log_request(log_fd, &request);
				runtime_probe_execute(&request, &response);
			} else {
				response.magic = NOLED_RUNTIME_PROBE_RESPONSE_MAGIC;
				response.seq = request.seq;
				response.op = request.op;
				response.arg0 = request.arg0;
				response.arg1 = request.arg1;
				response.resolve_ret = 0;
				response.exec_ret = NOLED_RUNTIME_PROBE_ERR_REJECTED;
			}
			runtime_probe_write_response(&response);
			runtime_probe_log_response(log_fd, &response);
		}

		ksceKernelDelayThread(NOLED_RUNTIME_PROBE_DELAY_US);
		loops++;
		if (loops >= NOLED_RUNTIME_PROBE_LOOPS) {
			runtime_probe_running = 0;
		}
	}

	if (log_fd >= 0) {
		trace_write_str(log_fd, "--- noLED runtime LED probe stop ---\n");
		ksceIoClose(log_fd);
	}

	return 0;
}

static void start_runtime_probe(void)
{
	runtime_probe_running = 1;
	runtime_probe_thread_uid = ksceKernelCreateThread("noled_runtime_probe",
		runtime_probe_thread,
		NOLED_RUNTIME_PROBE_THREAD_PRIORITY,
		NOLED_RUNTIME_PROBE_THREAD_STACK,
		0,
		0,
		0);
	if (runtime_probe_thread_uid >= 0) {
		int ret = ksceKernelStartThread(runtime_probe_thread_uid, 0, 0);
		if (ret < 0) {
			ksceKernelDeleteThread(runtime_probe_thread_uid);
			runtime_probe_thread_uid = -1;
			runtime_probe_running = 0;
		}
	} else {
		runtime_probe_running = 0;
	}
}

static int power_led_off_sysevent_handler(int resume, int eventid, void *args, void *opt)
{
	(void)eventid;
	(void)args;
	(void)opt;

	if (resume) {
		power_led_policy_resumed = 1;
	}
	return 0;
}

static int power_led_policy_apply(SceUID log_fd, int release)
{
	static uintptr_t blink_type2_func = 0;
	int resolve_ret;
	int exec_ret = NOLED_RUNTIME_PROBE_ERR_UNRESOLVED;
	char line[NOLED_TRACE_LINE_MAX];
	int len = 0;

	resolve_ret = runtime_probe_resolve(NOLED_SYSCON_MODULE_NAME,
		NOLED_SYSCON_FOR_DRIVER_LIB_NID,
		NOLED_SYSCON_CTRL_LED_BLINK_TYPE2_NID,
		&blink_type2_func);
	if (resolve_ret == 0 && blink_type2_func != 0) {
		if (release) {
			exec_ret = ((int (*)(int, int, int, int))blink_type2_func)(
				NOLED_POWER_LED_RELEASE_DEV,
				NOLED_POWER_LED_RELEASE_UNK,
				NOLED_POWER_LED_RELEASE_ON_TIME,
				NOLED_POWER_LED_RELEASE_OFF_TIME);
		} else {
			exec_ret = ((int (*)(int, int, int, int))blink_type2_func)(
				NOLED_POWER_LED_OFF_DEV,
				NOLED_POWER_LED_OFF_UNK,
				NOLED_POWER_LED_OFF_ON_TIME,
				NOLED_POWER_LED_OFF_OFF_TIME);
		}
	}

	if (log_fd >= 0) {
		trace_append_str(line, &len, "policy apply release=");
		trace_append_dec_u32(line, &len, (unsigned int)release);
		trace_append_str(line, &len, " resolve=0x");
		trace_append_hex_fixed(line, &len, (unsigned int)resolve_ret, 8);
		trace_append_str(line, &len, " ret=0x");
		trace_append_hex_fixed(line, &len, (unsigned int)exec_ret, 8);
		trace_append_char(line, &len, '\n');
		ksceIoWrite(log_fd, line, len);
		trace_sync_fd(log_fd);
	}

	return exec_ret;
}

static int power_led_off_policy_thread(SceSize argc, void *args)
{
	SceUID log_fd;
	int loops_since_apply = 0;
	int applied_state = -1;
	int stable_charging = 0;
	int last_sample = -1;
	int config_checks = NOLED_BOOT_CONFIG_WAIT_TRIES;

	(void)argc;
	(void)args;

	ksceSysconWaitInitialized();
	ksceIoMkdir("ux0:data", 0777);
	ksceIoMkdir("ux0:data/noled", 0777);
	log_fd = ksceIoOpen(NOLED_RUNTIME_PROBE_LOG_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND,
		0777);
	if (log_fd >= 0) {
		trace_write_str(log_fd, "--- noLED power LED policy start (dark idle, orange charging) ---\n");
		trace_sync_fd(log_fd);
	}

	stable_charging = kscePowerIsBatteryCharging() > 0;
	last_sample = stable_charging;

	while (power_led_policy_running) {
		int sample = kscePowerIsBatteryCharging() > 0;

		if (!led_candidate_config_loaded && config_checks > 0) {
			/* the config file is optional; if one appears late (boot
			 * before ux0 mounted) and explicitly asks for classic-only
			 * mode, hand the LED back to syscon policy and stop */
			config_checks--;
			refresh_led_candidate_config();
			if (led_candidate_config_loaded &&
				led_candidate_mode == NOLED_LED_CANDIDATE_PASS) {
				power_led_policy_apply(log_fd, 1);
				break;
			}
		}

		/* two equal consecutive samples before switching, so brief
		 * charge-state flaps do not toggle the LED */
		if (sample == last_sample) {
			stable_charging = sample;
		}
		last_sample = sample;

		if (power_led_policy_resumed) {
			/* let the resume path finish restoring the stock LED state
			 * before overriding it again */
			ksceKernelDelayThread(NOLED_POWER_LED_POLICY_RESUME_SETTLE_US);
			power_led_policy_resumed = 0;
			applied_state = -1;
		}

		if (applied_state != stable_charging ||
			loops_since_apply >= NOLED_POWER_LED_POLICY_HEARTBEAT_LOOPS) {
			/* charging -> release to syscon policy (orange); idle -> dark;
			 * the heartbeat re-apply is an idempotent safety net */
			power_led_policy_apply(log_fd, stable_charging);
			applied_state = stable_charging;
			loops_since_apply = 0;
		}

		ksceKernelDelayThread(NOLED_POWER_LED_POLICY_DELAY_US);
		loops_since_apply++;
	}

	if (log_fd >= 0) {
		trace_write_str(log_fd, "--- noLED power LED policy stop ---\n");
		ksceIoClose(log_fd);
	}

	return 0;
}

static int power_led_indicator_supported(void)
{
	/* the same gate Sony's charge LED code uses: boards below hardware
	 * info 0x40xxxx (original PCH-1000 era) lack the bicolor power/charge
	 * indicator, and the BlinkType2 device namespace is unverified there.
	 * On such boards only the classic GPIO LED blocking applies. */
	return (((unsigned int)ksceSysconGetHardwareInfo()) & 0xFF0000) >=
		0x400000;
}

static int boot_policy_thread(SceSize argc, void *args)
{
	(void)argc;
	(void)args;

	/* boot context: the config was unreadable at module_start (ux0 not
	 * mounted yet, or simply no file - it is optional). Run the full
	 * policy immediately; the policy loop itself honors a late-appearing
	 * explicit classic-only config. */
	ksceSysconWaitInitialized();
	if (!power_led_indicator_supported()) {
		return 0;
	}

	power_led_policy_running = 1;
	ksceKernelRegisterSysEventHandler("znoled_led_policy",
		power_led_off_sysevent_handler,
		0);
	return power_led_off_policy_thread(0, 0);
}

static void start_boot_policy(void)
{
	boot_policy_thread_uid = ksceKernelCreateThread("noled_boot_policy",
		boot_policy_thread,
		NOLED_RUNTIME_PROBE_THREAD_PRIORITY,
		NOLED_RUNTIME_PROBE_THREAD_STACK,
		0,
		0,
		0);
	if (boot_policy_thread_uid >= 0) {
		if (ksceKernelStartThread(boot_policy_thread_uid, 0, 0) < 0) {
			ksceKernelDeleteThread(boot_policy_thread_uid);
			boot_policy_thread_uid = -1;
		}
	}
}

static void start_power_led_off_policy(void)
{
	ksceSysconWaitInitialized();
	if (!power_led_indicator_supported()) {
		return;
	}

	power_led_policy_running = 1;
	power_led_policy_thread_uid = ksceKernelCreateThread("noled_led_policy",
		power_led_off_policy_thread,
		NOLED_RUNTIME_PROBE_THREAD_PRIORITY,
		NOLED_RUNTIME_PROBE_THREAD_STACK,
		0,
		0,
		0);
	if (power_led_policy_thread_uid >= 0) {
		int ret = ksceKernelStartThread(power_led_policy_thread_uid, 0, 0);
		if (ret < 0) {
			ksceKernelDeleteThread(power_led_policy_thread_uid);
			power_led_policy_thread_uid = -1;
			power_led_policy_running = 0;
			return;
		}
		ksceKernelRegisterSysEventHandler("znoled_led_policy",
			power_led_off_sysevent_handler,
			0);
	} else {
		power_led_policy_running = 0;
	}
}

static int syscon_trace_thread(SceSize argc, void *args)
{
	SceUID fd;
	int last_online = -1;
	int last_charging = -1;
	int last_percent = -1;
	int loops = 0;

	(void)argc;
	(void)args;

	ksceIoMkdir("ux0:data", 0777);
	ksceIoMkdir("ux0:data/noled", 0777);
	fd = ksceIoOpen(NOLED_SYSCON_TRACE_LOG_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
		0777);

	if (fd >= 0) {
		trace_write_str(fd, "--- noLED syscon trace start ---\n");
	trace_write_str(fd, "phase C=cmd_exec A=cmd_exec_async Y=cmd_sync R=read_command D=send_command S=start E=end P=power M=marker; passive poweroff capture; tx/rx bytes are truncated to 16 bytes; polling commands are filtered\n");
		trace_sync_fd(fd);
	}

	while (syscon_trace_running) {
		int online = kscePowerIsPowerOnline();
		int charging = kscePowerIsBatteryCharging();
		int percent = kscePowerGetBatteryLifePercent();

		refresh_led_candidate_config();

		if (online != last_online || charging != last_charging || percent != last_percent) {
			trace_record_power_state();
			last_online = online;
			last_charging = charging;
			last_percent = percent;
		}

		if (fd >= 0) {
			trace_flush(fd);
			trace_sync_fd(fd);
		}
		ksceKernelDelayThread(NOLED_SYSCON_TRACE_FLUSH_DELAY_US);
		loops++;
		if (loops >= NOLED_SYSCON_TRACE_FLUSH_LOOPS) {
			syscon_trace_running = 0;
		}
	}

	if (fd >= 0) {
		trace_flush(fd);
		trace_write_str(fd, "--- noLED syscon trace stop ---\n");
		ksceIoClose(fd);
	}

	return 0;
}

static void start_syscon_trace(void)
{
	int ret;

	syscon_trace_next_seq = 1;
	syscon_trace_read_seq = 1;
	syscon_trace_running = 1;

	syscon_trace_thread_uid = ksceKernelCreateThread("noled_syscon_trace",
		syscon_trace_thread,
		NOLED_SYSCON_TRACE_THREAD_PRIORITY,
		NOLED_SYSCON_TRACE_THREAD_STACK,
		0,
		0,
		0);
	if (syscon_trace_thread_uid >= 0) {
		ret = ksceKernelStartThread(syscon_trace_thread_uid, 0, 0);
		if (ret < 0) {
			ksceKernelDeleteThread(syscon_trace_thread_uid);
			syscon_trace_thread_uid = -1;
			syscon_trace_running = 0;
			return;
		}
	} else {
		syscon_trace_running = 0;
		return;
	}

	ksceSysconWaitInitialized();
	ret = ksceSysconSetDebugHandlers(&syscon_trace_handlers);
	if (ret >= 0) {
		if (led_candidate_mode == NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE) {
			int map_ret = dump_kernel_module_map();

			trace_record_marker(NOLED_MARKER_MODULE_MAP_READY,
				0,
				0,
				map_ret);
		}
		trace_record_power_state();
		trace_record_marker(NOLED_MARKER_TRACE_READY, 0, 0, ret);
	}
}

static int read_led_candidate_mode(unsigned int *mode)
{
	NoledLedCandidateConfig config;
	SceUID fd;
	int read_len;

	if (mode == 0) {
		return -1;
	}

	fd = ksceIoOpen(NOLED_LED_CANDIDATE_CONFIG_PATH, SCE_O_RDONLY, 0);
	if (fd < 0) {
		*mode = NOLED_LED_CANDIDATE_PASS;
		return fd;
	}

	read_len = ksceIoRead(fd, &config, sizeof(config));
	ksceIoClose(fd);
	if (read_len < 0) {
		*mode = NOLED_LED_CANDIDATE_PASS;
		return read_len;
	}

	if (noled_led_candidate_config_mode(&config,
		(unsigned int)read_len,
		mode) < 0) {
		*mode = NOLED_LED_CANDIDATE_PASS;
		return -1;
	}

	return 0;
}

static void refresh_led_candidate_config(void)
{
	unsigned int mode;
	int ret = read_led_candidate_mode(&mode);

	if (ret < 0) {
		mode = NOLED_LED_CANDIDATE_PASS;
	} else {
		led_candidate_config_loaded = 1;
	}

	if (mode != led_candidate_mode) {
		led_candidate_mode = mode;
		trace_record_marker(NOLED_MARKER_LED_CANDIDATE_CONFIG, mode, 0, ret);
	}
}

static int apply_led_candidate_patch(SceSysconPacket *packet)
{
	unsigned int cmd;
	unsigned int tx_len;
	unsigned int mode;
	unsigned char *tx_data;
	unsigned int original_first;
	unsigned int patched_first;
	int patch_result;

	if (packet == 0) {
		return 0;
	}

	mode = led_candidate_mode;
	if (mode == NOLED_LED_CANDIDATE_PASS) {
		return 0;
	}

	cmd = noled_syscon_packet_cmd(packet->tx);
	tx_len = noled_syscon_tx_data_len(packet->tx);
	tx_data = &packet->tx[SCE_SYSCON_PACKET_TX_DATA(0)];

	original_first = tx_data[0];
	patch_result = noled_led_candidate_patch_packet(cmd, tx_data, tx_len, mode);
	if (patch_result != 0) {
		patched_first = tx_data[0];
		trace_record_marker(NOLED_MARKER_LED_CANDIDATE_PATCH,
			original_first,
			mode,
			patch_result < 0 ? ((cmd << 16) | 0xFFFF) :
				((cmd << 16) | patched_first));
	}

	return patch_result;
}

static SceUID noled_gpio_port_set(int bus, int port)
{
	SceUID ret;

	if (noled_should_block_gpio_set_for_mode(bus,
		port,
		led_candidate_mode)) {
		ret = 0;
		if (noled_gpio_trace_enabled(led_candidate_mode)) {
			trace_record_marker(NOLED_MARKER_GPIO_PORT_SET,
				(unsigned int)bus,
				(unsigned int)port,
				ret);
		}
		return 0;
	}

	ret = TAI_CONTINUE(SceUID, hook_gpio_port_set_ref, bus, port);
	if (noled_gpio_trace_enabled(led_candidate_mode)) {
		trace_record_marker(NOLED_MARKER_GPIO_PORT_SET,
			(unsigned int)bus,
			(unsigned int)port,
			ret);
	}
	return ret;
}

static SceUID noled_gpio_port_clear(int bus, int port)
{
	SceUID ret = TAI_CONTINUE(SceUID, hook_gpio_port_clear_ref, bus, port);

	if (noled_gpio_trace_enabled(led_candidate_mode)) {
		trace_record_marker(NOLED_MARKER_GPIO_PORT_CLEAR,
			(unsigned int)bus,
			(unsigned int)port,
			ret);
	}
	return ret;
}

static SceUID noled_gpio_port_reset(int bus, int port)
{
	SceUID ret = TAI_CONTINUE(SceUID, hook_gpio_port_reset_ref, bus, port);

	if (noled_gpio_trace_enabled(led_candidate_mode)) {
		trace_record_marker(NOLED_MARKER_GPIO_PORT_RESET,
			(unsigned int)bus,
			(unsigned int)port,
			ret);
	}
	return ret;
}

static int noled_gpio_set_port_mode(int bus, int port, int mode)
{
	int ret = TAI_CONTINUE(int,
		hook_gpio_set_port_mode_ref,
		bus,
		port,
		mode);

	if (noled_gpio_trace_enabled(led_candidate_mode)) {
		trace_record_marker_value(NOLED_MARKER_GPIO_SET_PORT_MODE,
			(unsigned int)bus,
			(unsigned int)port,
			(unsigned int)mode,
			ret);
	}
	return ret;
}

static int noled_syscon_ctrl_dolce_led(int enable)
{
	unsigned int mode = led_candidate_mode;
	int effective_enable = noled_dolce_led_effective_enable(enable, mode);
	int ret = TAI_CONTINUE(int,
		hook_syscon_ctrl_dolce_led_ref,
		effective_enable);

	trace_record_marker(NOLED_MARKER_DOLCE_LED_HOOK,
		(unsigned int)enable,
		(unsigned int)effective_enable,
		ret);
	return ret;
}

static int noled_syscon_cmd_exec(SceSysconPacket *packet, unsigned int flags)
{
	trace_record_packet_call(NOLED_TRACE_PHASE_CALL_EXEC,
		packet,
		flags,
		__builtin_return_address(0));
	if (apply_led_candidate_patch(packet) < 0) {
		return 0;
	}
	return TAI_CONTINUE(int, hook_syscon_cmd_exec_ref, packet, flags);
}

static int noled_syscon_cmd_exec_async(SceSysconPacket *packet,
	unsigned int flags,
	SceSysconCmdExecAsyncCallback cb,
	void *argp)
{
	trace_record_packet_call(NOLED_TRACE_PHASE_CALL_EXEC_ASYNC,
		packet,
		flags,
		__builtin_return_address(0));
	if (apply_led_candidate_patch(packet) < 0) {
		return 0;
	}
	return TAI_CONTINUE(int,
		hook_syscon_cmd_exec_async_ref,
		packet,
		flags,
		cb,
		argp);
}

static int noled_syscon_cmd_sync(SceSysconPacket *packet, int no_wait)
{
	trace_record_packet_call(NOLED_TRACE_PHASE_CALL_SYNC,
		packet,
		(unsigned int)no_wait,
		__builtin_return_address(0));
	if (apply_led_candidate_patch(packet) < 0) {
		return 0;
	}
	return TAI_CONTINUE(int, hook_syscon_cmd_sync_ref, packet, no_wait);
}

static int noled_syscon_read_command(unsigned short cmd, void *buffer, SceSize size)
{
	trace_record_command_call(NOLED_TRACE_PHASE_CALL_READ,
		cmd,
		buffer,
		size,
		__builtin_return_address(0));
	return TAI_CONTINUE(int, hook_syscon_read_command_ref, cmd, buffer, size);
}

static int noled_syscon_send_command(unsigned short cmd, const void *buffer, SceSize size)
{
	trace_record_command_call(NOLED_TRACE_PHASE_CALL_SEND,
		cmd,
		buffer,
		size,
		__builtin_return_address(0));
	return TAI_CONTINUE(int, hook_syscon_send_command_ref, cmd, buffer, size);
}

static void disable_gpio_leds(void)
{
	ksceGpioPortClear(NOLED_GPIO_BUS, NOLED_GPIO_HOME_BUTTON_PORT);
	ksceGpioPortClear(NOLED_GPIO_BUS, NOLED_GPIO_GAME_CARD_PORT);
	if (led_candidate_mode == NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK) {
		ksceGpioPortClear(NOLED_GPIO_BUS,
			NOLED_GPIO_POWER_LED_CANDIDATE_PORT);
	}
}

static void install_gpio_block_hook(void)
{
	/* the original noled hook: keeps the OS from re-lighting the home
	 * button and game card LEDs after resume or notifications; model
	 * independent and required in every production path */
	hook_gpio_port_set_uid = taiHookFunctionExportForKernel(KERNEL_PID,
		&hook_gpio_port_set_ref,
		"SceLowio",
		TAI_ANY_LIBRARY,
		SCE_LOWIO_KSCE_GPIO_PORT_SET_NID,
		noled_gpio_port_set);
	trace_record_marker(NOLED_MARKER_HOOK_GPIO, 0, 0, hook_gpio_port_set_uid);
}

int module_start(SceSize argc, const void *args)
{
	int enable_syscon_experiment;
	int enable_syscon_command_hooks;
	int enable_syscon_dolce_hook;
	int enable_gpio_trace;

	(void)argc;
	(void)args;

	refresh_led_candidate_config();
	if (!led_candidate_config_loaded) {
		/* boot plugin context: ux0 is not mounted yet, so the config is
		 * unreadable. Apply the classic behavior now and defer the power
		 * LED policy decision to the boot thread. */
		disable_gpio_leds();
		install_gpio_block_hook();
		start_boot_policy();
		return SCE_KERNEL_START_SUCCESS;
	}
	if (led_candidate_mode == NOLED_LED_CANDIDATE_GPIO_READ_SAMPLER) {
		start_gpio_sampler();
		return SCE_KERNEL_START_SUCCESS;
	}
	if (led_candidate_mode == NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE) {
		/* probe client: no syscon hooks or debug handlers; keep the
		 * original GPIO LED blocking and its guard hook */
		disable_gpio_leds();
		install_gpio_block_hook();
		start_runtime_probe();
		return SCE_KERNEL_START_SUCCESS;
	}
	if (noled_power_led_off_mode_enabled(led_candidate_mode)) {
		/* production policy: classic GPIO LED blocking plus the power LED
		 * policy (dark idle, orange while charging); no syscon hooks */
		disable_gpio_leds();
		install_gpio_block_hook();
		start_power_led_off_policy();
		return SCE_KERNEL_START_SUCCESS;
	}

	enable_syscon_experiment = noled_syscon_experiment_enabled(led_candidate_mode);
	enable_syscon_command_hooks = noled_syscon_command_hooks_enabled(led_candidate_mode);
	enable_syscon_dolce_hook = noled_syscon_dolce_hook_enabled(led_candidate_mode);
	enable_gpio_trace = noled_gpio_trace_enabled(led_candidate_mode);
	if (enable_syscon_experiment) {
		start_syscon_trace();
		refresh_led_candidate_config();
		enable_syscon_command_hooks = noled_syscon_command_hooks_enabled(led_candidate_mode);
		enable_syscon_dolce_hook = noled_syscon_dolce_hook_enabled(led_candidate_mode);
		enable_gpio_trace = noled_gpio_trace_enabled(led_candidate_mode);
	}
	disable_gpio_leds();

	if (enable_syscon_command_hooks) {
		hook_syscon_cmd_exec_uid = taiHookFunctionExportForKernel(KERNEL_PID,
			&hook_syscon_cmd_exec_ref,
			"SceSyscon",
			TAI_ANY_LIBRARY,
			SCE_SYSCON_KSCE_SYSCON_CMD_EXEC_NID,
			noled_syscon_cmd_exec);
		trace_record_marker(NOLED_MARKER_HOOK_CMD_EXEC, 0, 0, hook_syscon_cmd_exec_uid);

		hook_syscon_cmd_exec_async_uid = taiHookFunctionExportForKernel(KERNEL_PID,
			&hook_syscon_cmd_exec_async_ref,
			"SceSyscon",
			TAI_ANY_LIBRARY,
			SCE_SYSCON_KSCE_SYSCON_CMD_EXEC_ASYNC_NID,
			noled_syscon_cmd_exec_async);
		trace_record_marker(NOLED_MARKER_HOOK_CMD_EXEC_ASYNC, 0, 0, hook_syscon_cmd_exec_async_uid);

		hook_syscon_cmd_sync_uid = taiHookFunctionExportForKernel(KERNEL_PID,
			&hook_syscon_cmd_sync_ref,
			"SceSyscon",
			TAI_ANY_LIBRARY,
			SCE_SYSCON_KSCE_SYSCON_CMD_SYNC_NID,
			noled_syscon_cmd_sync);
		trace_record_marker(NOLED_MARKER_HOOK_CMD_SYNC, 0, 0, hook_syscon_cmd_sync_uid);

		hook_syscon_read_command_uid = taiHookFunctionExportForKernel(KERNEL_PID,
			&hook_syscon_read_command_ref,
			"SceSyscon",
			TAI_ANY_LIBRARY,
			SCE_SYSCON_KSCE_SYSCON_READ_COMMAND_NID,
			noled_syscon_read_command);
		trace_record_marker(NOLED_MARKER_HOOK_READ_COMMAND, 0, 0, hook_syscon_read_command_uid);

		hook_syscon_send_command_uid = taiHookFunctionExportForKernel(KERNEL_PID,
			&hook_syscon_send_command_ref,
			"SceSyscon",
			TAI_ANY_LIBRARY,
			SCE_SYSCON_KSCE_SYSCON_SEND_COMMAND_NID,
			noled_syscon_send_command);
		trace_record_marker(NOLED_MARKER_HOOK_SEND_COMMAND, 0, 0, hook_syscon_send_command_uid);
	}

	if (enable_syscon_dolce_hook) {
		hook_syscon_ctrl_dolce_led_uid = taiHookFunctionExportForKernel(KERNEL_PID,
			&hook_syscon_ctrl_dolce_led_ref,
			"SceSyscon",
			TAI_ANY_LIBRARY,
			SCE_SYSCON_KSCE_SYSCON_CTRL_DOLCE_LED_NID,
			noled_syscon_ctrl_dolce_led);
		trace_record_marker(NOLED_MARKER_DOLCE_LED_HOOK,
			0,
			1,
			hook_syscon_ctrl_dolce_led_uid);
	}

	install_gpio_block_hook();

	if (enable_gpio_trace) {
		hook_gpio_port_clear_uid = taiHookFunctionExportForKernel(KERNEL_PID,
			&hook_gpio_port_clear_ref,
			"SceLowio",
			TAI_ANY_LIBRARY,
			SCE_LOWIO_KSCE_GPIO_PORT_CLEAR_NID,
			noled_gpio_port_clear);
		trace_record_marker(NOLED_MARKER_HOOK_GPIO_PORT_CLEAR,
			0,
			0,
			hook_gpio_port_clear_uid);

		hook_gpio_port_reset_uid = taiHookFunctionExportForKernel(KERNEL_PID,
			&hook_gpio_port_reset_ref,
			"SceLowio",
			TAI_ANY_LIBRARY,
			SCE_LOWIO_KSCE_GPIO_PORT_RESET_NID,
			noled_gpio_port_reset);
		trace_record_marker(NOLED_MARKER_HOOK_GPIO_PORT_RESET,
			0,
			0,
			hook_gpio_port_reset_uid);

		hook_gpio_set_port_mode_uid = taiHookFunctionExportForKernel(KERNEL_PID,
			&hook_gpio_set_port_mode_ref,
			"SceLowio",
			TAI_ANY_LIBRARY,
			SCE_LOWIO_KSCE_GPIO_SET_PORT_MODE_NID,
			noled_gpio_set_port_mode);
		trace_record_marker(NOLED_MARKER_HOOK_GPIO_SET_PORT_MODE,
			0,
			0,
			hook_gpio_set_port_mode_uid);
	}

	if (led_candidate_mode == NOLED_LED_CANDIDATE_TRACE_PROBE) {
		start_runtime_probe();
	}

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	(void)argc;
	(void)args;

	/*
	 * This tracer intentionally stays resident after runtime loading. Clearing
	 * syscon debug handlers while packets are active proved unsafe on hardware.
	 */
	syscon_trace_running = 0;
	gpio_sample_running = 0;
	runtime_probe_running = 0;
	power_led_policy_running = 0;
	return SCE_KERNEL_STOP_FAIL;
}
