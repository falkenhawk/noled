#include <stdio.h>
#include <string.h>
#include <taihen.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>

#include "../led_candidate_patch.h"
#include "../trace_summary.h"
#include "probe_candidates.h"

#include "debugScreen.h"

#define printf psvDebugScreenPrintf

#define PLUGIN_SOURCE_PATH "app0:noled.skprx"
#define PLUGIN_RUNTIME_DIR "ux0:data/noled"
#define PLUGIN_RUNTIME_PATH "ux0:data/noled/noled.skprx"
#define LOADER_LOG_PATH "ux0:data/noled/loader.log"
#define SYSCON_TRACE_LOG_PATH "ux0:data/noled/syscon_trace.log"
#define MODULE_MAP_LOG_PATH "ux0:data/noled/module_map.log"
#define COPY_BUFFER_SIZE 4096
#define TRACE_READ_BUFFER_SIZE 512
#define TRACE_LINE_BUFFER_SIZE 384
#define DIAG_MODE_COUNT 5

#define ON_PRESS(button) ((pad.buttons & (button)) && !(old_pad.buttons & (button)))

typedef struct PowerState {
	int online;
	int charging;
	int percent;
} PowerState;

typedef struct TraceStatus {
	NoledTraceSummary summary;
	int trace_ret;
	int trace_bytes;
	int module_map_ret;
	int module_map_size;
} TraceStatus;

static SceUID log_fd = -1;

static void log_open(void)
{
	sceIoMkdir("ux0:data", 0777);
	sceIoMkdir(PLUGIN_RUNTIME_DIR, 0777);

	log_fd = sceIoOpen(LOADER_LOG_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND,
		0777);
	if (log_fd >= 0) {
		const char marker[] = "\n--- noLED passive caller tracer start ---\n";

		sceIoWrite(log_fd, marker, sizeof(marker) - 1);
		sceIoSyncByFd(log_fd, 0);
	}
}

static void log_close(void)
{
	if (log_fd >= 0) {
		const char marker[] = "--- noLED passive caller tracer exit ---\n";

		sceIoWrite(log_fd, marker, sizeof(marker) - 1);
		sceIoSyncByFd(log_fd, 0);
		sceIoClose(log_fd);
		log_fd = -1;
	}
}

static void log_line(const char *message, int value)
{
	char line[128];
	int len;

	if (log_fd < 0) {
		return;
	}

	len = snprintf(line, sizeof(line), "%s: 0x%08X\n", message, (unsigned int)value);
	if (len > 0) {
		if (len > (int)sizeof(line)) {
			len = sizeof(line);
		}
		sceIoWrite(log_fd, line, len);
		sceIoSyncByFd(log_fd, 0);
	}
}

static void log_power_state(const char *message, const PowerState *state)
{
	char line[160];
	int len;

	if (log_fd < 0) {
		return;
	}

	len = snprintf(line,
		sizeof(line),
		"%s: online=%d charging=%d percent=%d\n",
		message,
		state->online,
		state->charging,
		state->percent);
	if (len > 0) {
		if (len > (int)sizeof(line)) {
			len = sizeof(line);
		}
		sceIoWrite(log_fd, line, len);
	}
}

static void log_diag_event(const char *message, const PowerState *state)
{
	char line[192];
	int len;

	if (log_fd < 0) {
		return;
	}

	len = snprintf(line,
		sizeof(line),
		"diag event: %s online=%d charging=%d percent=%d\n",
		message,
		state->online,
		state->charging,
		state->percent);
	if (len > 0) {
		if (len > (int)sizeof(line)) {
			len = sizeof(line);
		}
		sceIoWrite(log_fd, line, len);
		sceIoSyncByFd(log_fd, 0);
	}
}

static const char *candidate_mode_label(unsigned int mode)
{
	switch (mode) {
	case NOLED_LED_CANDIDATE_PASS:
		return "pass-through";
	case NOLED_LED_CANDIDATE_FORCE_0387_FF_01_SAFE:
		return "force 0387 FF/01 to safe";
	case NOLED_LED_CANDIDATE_FORCE_0387_FF_00_10:
		return "force 0387 FF/00 to 10";
	case NOLED_LED_CANDIDATE_FORCE_0387_FF_BOTH:
		return "force both 0387 FF forms";
	case NOLED_LED_CANDIDATE_DOLCE_OFF_0387_BOTH:
		return "DolceLED off + both 0387 FF forms";
	case NOLED_LED_CANDIDATE_SYSCON_LED_TRACE_ONLY:
		return "syscon debug trace only";
	case NOLED_LED_CANDIDATE_SYSCON_COMMAND_HOOKS_PASS:
		return "syscon command hooks pass-through";
	case NOLED_LED_CANDIDATE_DOLCE_LED_TRACE_PASS:
		return "DolceLED pass-through trace";
	case NOLED_LED_CANDIDATE_GPIO_TRACE_PASS:
		return "GPIO pass-through trace";
	case NOLED_LED_CANDIDATE_GPIO_PORT3_BLOCK:
		return "block GPIO bus 0 port 3";
	case NOLED_LED_CANDIDATE_GPIO_READ_SAMPLER:
		return "read-only GPIO sampler";
	case NOLED_LED_CANDIDATE_SYSCON_LED_OFF_WHEN_UNPLUGGED:
		return "SysconCtrlLED off when unplugged";
	case NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE:
		return "syscon caller map trace";
	case NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE:
		return "runtime LED probe (one command per press)";
	case NOLED_LED_CANDIDATE_POWER_LED_OFF:
		return "POWER LED OFF policy (production test)";
	case NOLED_LED_CANDIDATE_TRACE_PROBE:
		return "probe + tracer (release-hunt capture)";
	default:
		return "unknown";
	}
}

static unsigned int diag_mode_for_index(unsigned int index)
{
	switch (index % DIAG_MODE_COUNT) {
	case 1:
		return NOLED_LED_CANDIDATE_POWER_LED_OFF;
	case 2:
		return NOLED_LED_CANDIDATE_TRACE_PROBE;
	case 3:
		return NOLED_LED_CANDIDATE_SYSCON_CALLER_MAP_TRACE;
	case 4:
		return NOLED_LED_CANDIDATE_GPIO_READ_SAMPLER;
	default:
		return NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE;
	}
}

static int is_probe_ui_mode(unsigned int mode)
{
	return mode == NOLED_LED_CANDIDATE_RUNTIME_LED_PROBE ||
		mode == NOLED_LED_CANDIDATE_TRACE_PROBE;
}

static void log_candidate_mode(const char *message, unsigned int mode, int result)
{
	char line[160];
	int len;

	if (log_fd < 0) {
		return;
	}

	len = snprintf(line,
		sizeof(line),
		"%s: mode=%u result=0x%08X\n",
		message,
		mode,
		(unsigned int)result);
	if (len > 0) {
		if (len > (int)sizeof(line)) {
			len = sizeof(line);
		}
		sceIoWrite(log_fd, line, len);
	}
}

static int write_candidate_config(unsigned int mode)
{
	NoledLedCandidateConfig config;
	SceUID fd;
	int written;

	if (!noled_led_candidate_mode_valid(mode)) {
		return -1;
	}

	sceIoMkdir("ux0:data", 0777);
	sceIoMkdir(PLUGIN_RUNTIME_DIR, 0777);

	config.magic = NOLED_LED_CANDIDATE_CONFIG_MAGIC;
	config.mode = mode;

	fd = sceIoOpen(NOLED_LED_CANDIDATE_CONFIG_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
		0777);
	if (fd < 0) {
		return fd;
	}

	written = sceIoWrite(fd, &config, sizeof(config));
	sceIoClose(fd);
	if (written < 0) {
		return written;
	}

	return written == (int)sizeof(config) ? 0 : -1;
}

typedef struct ProbeUiState {
	unsigned int index;
	unsigned int sent_seq;
	int send_ret;
	int response_ok;
	NoledRuntimeProbeResponse response;
} ProbeUiState;

static ProbeUiState probe_ui;

/* set after the first Cross press in probe mode; the probe UI must stay
 * reachable even when taiLoadStartKernelModule fails because the plugin is
 * already resident from an earlier launch this boot */
static int probe_load_attempted = 0;

static void probe_delete_stale_files(void)
{
	sceIoRemove(NOLED_RUNTIME_PROBE_REQUEST_PATH);
	sceIoRemove(NOLED_RUNTIME_PROBE_RESPONSE_PATH);
}

static void log_probe_send(const NoledProbeCandidate *candidate,
	unsigned int seq,
	int ret)
{
	char line[192];
	int len;

	if (log_fd < 0) {
		return;
	}

	len = snprintf(line,
		sizeof(line),
		"probe send: seq=%u op=%u arg0=0x%02X arg1=0x%02X ret=0x%08X label=%s\n",
		seq,
		candidate->op,
		candidate->arg0,
		candidate->arg1,
		(unsigned int)ret,
		candidate->label);
	if (len > 0) {
		if (len > (int)sizeof(line)) {
			len = sizeof(line);
		}
		sceIoWrite(log_fd, line, len);
		sceIoSyncByFd(log_fd, 0);
	}
}

static int probe_send_candidate(void)
{
	const NoledProbeCandidate *candidate = &noled_probe_candidates[probe_ui.index];
	NoledRuntimeProbeRequest request;
	SceUID fd;
	int written;

	request.magic = NOLED_RUNTIME_PROBE_REQUEST_MAGIC;
	request.seq = probe_ui.sent_seq + 1;
	request.op = candidate->op;
	request.arg0 = candidate->arg0;
	request.arg1 = candidate->arg1;
	request.arg2 = candidate->arg2;
	request.arg3 = candidate->arg3;

	fd = sceIoOpen(NOLED_RUNTIME_PROBE_REQUEST_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
		0777);
	if (fd < 0) {
		return fd;
	}

	written = sceIoWrite(fd, &request, sizeof(request));
	sceIoClose(fd);
	if (written != (int)sizeof(request)) {
		return written < 0 ? written : -1;
	}

	probe_ui.sent_seq = request.seq;
	return 0;
}

static void probe_poll_response(void)
{
	NoledRuntimeProbeResponse response;
	SceUID fd;
	int read_len;

	fd = sceIoOpen(NOLED_RUNTIME_PROBE_RESPONSE_PATH, SCE_O_RDONLY, 0);
	if (fd < 0) {
		return;
	}

	read_len = sceIoRead(fd, &response, sizeof(response));
	sceIoClose(fd);
	if (read_len != (int)sizeof(response) ||
		response.magic != NOLED_RUNTIME_PROBE_RESPONSE_MAGIC) {
		return;
	}

	probe_ui.response = response;
	probe_ui.response_ok = 0;
}

static PowerState read_power_state(void)
{
	PowerState state;

	state.online = scePowerIsPowerOnline();
	state.charging = scePowerIsBatteryCharging();
	state.percent = scePowerGetBatteryLifePercent();
	return state;
}

static void trace_summary_finish_line(NoledTraceSummary *summary,
	char *line,
	unsigned int *line_len)
{
	if (*line_len >= TRACE_LINE_BUFFER_SIZE) {
		*line_len = TRACE_LINE_BUFFER_SIZE - 1;
	}
	line[*line_len] = '\0';
	noled_trace_summary_update_line(summary, line);
	*line_len = 0;
}

static TraceStatus read_trace_status(void)
{
	TraceStatus status;
	SceIoStat stat;
	char read_buffer[TRACE_READ_BUFFER_SIZE];
	char line[TRACE_LINE_BUFFER_SIZE];
	unsigned int line_len = 0;
	SceUID fd;
	int read_len;

	noled_trace_summary_reset(&status.summary);
	status.trace_ret = 0;
	status.trace_bytes = 0;
	status.module_map_ret = sceIoGetstat(MODULE_MAP_LOG_PATH, &stat);
	status.module_map_size = status.module_map_ret >= 0 ? (int)stat.st_size : 0;

	fd = sceIoOpen(SYSCON_TRACE_LOG_PATH, SCE_O_RDONLY, 0);
	if (fd < 0) {
		status.trace_ret = fd;
		return status;
	}

	while ((read_len = sceIoRead(fd, read_buffer, sizeof(read_buffer))) > 0) {
		int i;

		status.trace_bytes += read_len;
		for (i = 0; i < read_len; i++) {
			char c = read_buffer[i];

			if (c == '\n') {
				trace_summary_finish_line(&status.summary, line, &line_len);
				continue;
			}
			if (line_len < TRACE_LINE_BUFFER_SIZE - 1) {
				line[line_len++] = c;
			}
		}
	}

	if (line_len > 0) {
		trace_summary_finish_line(&status.summary, line, &line_len);
	}

	status.trace_ret = read_len < 0 ? read_len : 0;
	sceIoClose(fd);
	return status;
}

static int power_state_changed(const PowerState *left, const PowerState *right)
{
	return left->online != right->online ||
	       left->charging != right->charging ||
	       left->percent != right->percent;
}

static int copy_plugin(void)
{
	char buffer[COPY_BUFFER_SIZE];
	SceUID in;
	SceUID out;
	int read_len;
	int ret = 0;

	sceIoMkdir("ux0:data", 0777);
	sceIoMkdir(PLUGIN_RUNTIME_DIR, 0777);

	in = sceIoOpen(PLUGIN_SOURCE_PATH, SCE_O_RDONLY, 0);
	if (in < 0) {
		return in;
	}

	out = sceIoOpen(PLUGIN_RUNTIME_PATH,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
		0777);
	if (out < 0) {
		sceIoClose(in);
		return out;
	}

	while ((read_len = sceIoRead(in, buffer, sizeof(buffer))) > 0) {
		int written_total = 0;

		while (written_total < read_len) {
			int written = sceIoWrite(out,
				buffer + written_total,
				read_len - written_total);
			if (written < 0) {
				ret = written;
				goto finish;
			}

			written_total += written;
		}
	}

	if (read_len < 0) {
		ret = read_len;
	}

finish:
	sceIoClose(out);
	sceIoClose(in);
	return ret;
}

static void draw_status(SceUID modid,
	int copy_ret,
	int load_ret,
	const PowerState *power_state,
	unsigned int candidate_mode,
	int candidate_ret,
	const TraceStatus *trace_status)
{
	int probe_mode = is_probe_ui_mode(candidate_mode);

	psvDebugScreenClear(0);

	printf("noLED Vita Slim diagnostics\n\n");
	printf("Bundled plugin : %s\n", PLUGIN_SOURCE_PATH);
	printf("Runtime plugin : %s\n\n", PLUGIN_RUNTIME_PATH);
	if (probe_mode && (modid >= 0 || probe_load_attempted)) {
		printf("Left/Right pick LED candidate\n");
		printf("Cross      send ONE command, then watch the LED\n");
		printf("Up/Down    mark LED visible/change in log\n");
		printf("Select     checkpoint, Start exit\n\n");
		if (modid < 0) {
			printf("Load failed: OK if plugin already resident\n");
			printf("this boot; sends still work below.\n\n");
		}
	} else {
		printf("Left/Right choose safe diagnostic before Cross\n");
		printf("Cross      copy and start selected diagnostic\n");
		printf("Square     mark charger plug moment\n");
		printf("Triangle   mark charger unplug moment\n");
		printf("Circle     mark standby/wake moment\n");
		printf("Up/Down    mark LED visible/change\n");
		printf("Select     checkpoint, Start exit\n\n");
	}
	printf("Module id : 0x%08X %s\n",
		(unsigned int)modid,
		modid >= 0 ? "(loaded)" : "(not loaded)");
	printf("Copy ret  : 0x%08X\n", (unsigned int)copy_ret);
	printf("Load ret  : 0x%08X\n", (unsigned int)load_ret);
	printf("Config ret: 0x%08X\n\n", (unsigned int)candidate_ret);
	printf("Mode      : %u - %s\n\n",
		candidate_mode,
		candidate_mode_label(candidate_mode));
	printf("Power online : %d\n", power_state->online);
	printf("Charging     : %d\n", power_state->charging);
	printf("Battery      : %d%%\n\n", power_state->percent);
	if (probe_mode) {
		const NoledProbeCandidate *candidate =
			&noled_probe_candidates[probe_ui.index];

		printf("Candidate %u/%u:\n", probe_ui.index + 1,
			(unsigned int)NOLED_PROBE_CANDIDATE_COUNT);
		printf("  %s\n\n", candidate->label);
		printf("Sent seq   : %u (write ret 0x%08X)\n",
			probe_ui.sent_seq,
			(unsigned int)probe_ui.send_ret);
		if (probe_ui.response_ok == 0) {
			printf("Response   : seq=%u op=%u arg0=0x%03X arg1=0x%02X\n",
				probe_ui.response.seq,
				probe_ui.response.op,
				probe_ui.response.arg0,
				probe_ui.response.arg1);
			printf("             resolve=0x%08X ret=0x%08X%s\n\n",
				(unsigned int)probe_ui.response.resolve_ret,
				(unsigned int)probe_ui.response.exec_ret,
				probe_ui.sent_seq != probe_ui.response.seq ?
					" (waiting for new seq)" : "");
		} else {
			printf("Response   : none yet\n\n");
		}
		printf("Send ONE command, then look at the power LED.\n");
		printf("If anything misbehaves: reboot fully resets syscon.\n");
		printf("RISKY entries: save your work before sending.\n\n");
		printf("Log: ux0:data/noled/probe.log\n");
	} else if (candidate_mode == NOLED_LED_CANDIDATE_POWER_LED_OFF &&
		modid >= 0) {
		printf("POWER LED policy is running:\n");
		printf("dark when idle, orange while charging.\n\n");
		printf("Expected right now: %s\n\n",
			power_state->charging ? "ORANGE (charging)" : "dark within ~2s");
		printf("Test 1: standby, wake -> correct state again\n");
		printf("        within ~2 seconds after wake.\n");
		printf("Test 2: plug charger -> orange within ~2s;\n");
		printf("        unplug -> dark within ~2s.\n");
		printf("Test 3: play a while; LED stays dark.\n\n");
		printf("Log: ux0:data/noled/probe.log (policy lines)\n");
	} else if (trace_status != 0) {
		printf("Trace ret/bytes : 0x%08X / %d\n",
			(unsigned int)trace_status->trace_ret,
			trace_status->trace_bytes);
		printf("Trace lines     : %u\n", trace_status->summary.lines);
		printf("Seen 089A/0387  : %u / %u\n",
			trace_status->summary.cmd_089a,
			trace_status->summary.cmd_0387);
		printf("Seen 0804/power : %u / %u\n",
			trace_status->summary.cmd_0804,
			trace_status->summary.power_events);
		printf("Hooks/map/ready : %u / %u / %u\n",
			trace_status->summary.hook_markers,
			trace_status->summary.module_map_ready,
			trace_status->summary.trace_ready);
		printf("Module map      : 0x%08X / %d bytes\n\n",
			(unsigned int)trace_status->module_map_ret,
			trace_status->module_map_size);
	}
	if (!probe_mode) {
		printf("Mode 13: runtime LED probe, one command per press.\n");
		printf("Mode 12: passive syscon caller map + live counts.\n");
		printf("Mode 10: read-only GPIO sampler.\n");
		printf("Power and charger changes are auto-logged.\n\n");
		printf("Logs: ux0:data/noled/loader.log\n");
		printf("      ux0:data/noled/syscon_trace.log\n");
		printf("      ux0:data/noled/module_map.log\n");
		printf("      ux0:data/noled/gpio_sample.log\n\n");
		printf("Stay here until counts change, then exit to FTP.\n");
	}
}

int main(int argc, char *argv[])
{
	SceCtrlData pad = {0};
	SceCtrlData old_pad = {0};
	SceUID modid = -1;
	int copy_ret = 0;
	int load_ret = 0;
	unsigned int diag_mode_index = 0;
	unsigned int candidate_mode = diag_mode_for_index(diag_mode_index);
	int candidate_ret = 0;
	PowerState power_state;
	TraceStatus trace_status;
	int trace_redraw_ticks = 0;

	(void)argc;
	(void)argv;

	log_open();
	log_line("main entered", 0);
	log_line("psvDebugScreenInit", psvDebugScreenInit());
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
	log_line("sceCtrlSetSamplingMode", 0);
	probe_ui.index = 0;
	probe_ui.sent_seq = 0;
	probe_ui.send_ret = 0;
	probe_ui.response_ok = -1;
	probe_delete_stale_files();
	/* the config is only written when a session is committed with Cross,
	 * and rewritten to the production mode on exit, so a browsed-but-not
	 * -started diagnostic mode can never leak into the next boot */
	log_candidate_mode("candidate initial", candidate_mode, candidate_ret);
	power_state = read_power_state();
	trace_status = read_trace_status();
	log_power_state("power initial", &power_state);
	draw_status(modid,
		copy_ret,
		load_ret,
		&power_state,
		candidate_mode,
		candidate_ret,
		&trace_status);

	while (1) {
		PowerState new_power_state;
		int probe_active;
		int cross_loaded_now = 0;

		old_pad = pad;
		sceCtrlPeekBufferPositive(0, &pad, 1);
		probe_active = is_probe_ui_mode(candidate_mode) &&
			(modid >= 0 || probe_load_attempted);

		if (ON_PRESS(SCE_CTRL_CROSS) && modid < 0 && !probe_active) {
			log_line("cross pressed", 0);
			candidate_ret = write_candidate_config(candidate_mode);
			log_candidate_mode("candidate committed", candidate_mode, candidate_ret);
			copy_ret = copy_plugin();
			log_line("copy_plugin", copy_ret);
			if (copy_ret >= 0) {
				modid = taiLoadStartKernelModule(PLUGIN_RUNTIME_PATH, 0, NULL, 0);
				load_ret = modid;
				log_line("taiLoadStartKernelModule", load_ret);
			}
			probe_load_attempted = 1;
			cross_loaded_now = 1;
			trace_status = read_trace_status();
			draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
		}

		if (ON_PRESS(SCE_CTRL_CROSS) && !cross_loaded_now &&
			is_probe_ui_mode(candidate_mode) &&
			(modid >= 0 || probe_load_attempted)) {
			probe_ui.send_ret = probe_send_candidate();
			log_probe_send(&noled_probe_candidates[probe_ui.index],
				probe_ui.sent_seq,
				probe_ui.send_ret);
			draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
		}

		if (ON_PRESS(SCE_CTRL_SQUARE)) {
			log_diag_event("charger_plug_marker", &power_state);
			trace_status = read_trace_status();
			draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
		}

		if (ON_PRESS(SCE_CTRL_TRIANGLE)) {
			log_diag_event("charger_unplug_marker", &power_state);
			trace_status = read_trace_status();
			draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
		}

		if (ON_PRESS(SCE_CTRL_CIRCLE)) {
			log_diag_event("standby_wake_marker", &power_state);
			trace_status = read_trace_status();
			draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
		}

		if (ON_PRESS(SCE_CTRL_UP)) {
			log_diag_event("led_visible_marker", &power_state);
			trace_status = read_trace_status();
			draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
		}

		if (ON_PRESS(SCE_CTRL_DOWN)) {
			log_diag_event("led_changed_marker", &power_state);
			trace_status = read_trace_status();
			draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
		}

		if (ON_PRESS(SCE_CTRL_SELECT)) {
			log_diag_event("checkpoint_marker", &power_state);
			trace_status = read_trace_status();
			draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
		}

		if ((ON_PRESS(SCE_CTRL_RIGHT) || ON_PRESS(SCE_CTRL_LEFT)) &&
			modid < 0 && !probe_active) {
			diag_mode_index++;
			candidate_mode = diag_mode_for_index(diag_mode_index);
			log_candidate_mode("candidate selected", candidate_mode, 0);
			trace_status = read_trace_status();
			draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
		}

		if ((ON_PRESS(SCE_CTRL_RIGHT) || ON_PRESS(SCE_CTRL_LEFT)) &&
			(modid >= 0 || probe_active)) {
			if (is_probe_ui_mode(candidate_mode)) {
				probe_ui.index = ON_PRESS(SCE_CTRL_RIGHT) ?
					noled_probe_candidate_next(probe_ui.index) :
					noled_probe_candidate_prev(probe_ui.index);
				draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
			} else {
				log_diag_event("mode_change_after_load_ignored", &power_state);
				trace_status = read_trace_status();
				draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
			}
		}

		if (ON_PRESS(SCE_CTRL_START)) {
			int exit_ret;

			log_line("start pressed", 0);
			if (modid >= 0) {
				log_line("leaving tracer resident", modid);
			}
			/* persist the production mode so the boot plugin always finds
			 * the power LED policy regardless of this session's mode */
			exit_ret = write_candidate_config(NOLED_LED_CANDIDATE_POWER_LED_OFF);
			log_candidate_mode("candidate exit default",
				NOLED_LED_CANDIDATE_POWER_LED_OFF,
				exit_ret);
			break;
		}

		new_power_state = read_power_state();
		if (power_state_changed(&power_state, &new_power_state)) {
			power_state = new_power_state;
			log_power_state("power changed", &power_state);
			trace_status = read_trace_status();
			draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
			trace_redraw_ticks = 0;
		}

		if (modid >= 0 || probe_active) {
			trace_redraw_ticks++;
			if (is_probe_ui_mode(candidate_mode)) {
				if (trace_redraw_ticks >= 15) {
					unsigned int old_seq = probe_ui.response.seq;
					int old_ok = probe_ui.response_ok;

					probe_poll_response();
					if (probe_ui.response_ok != old_ok ||
						probe_ui.response.seq != old_seq) {
						draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
					}
					trace_redraw_ticks = 0;
				}
			} else if (trace_redraw_ticks >= 30) {
				trace_status = read_trace_status();
				draw_status(modid, copy_ret, load_ret, &power_state, candidate_mode, candidate_ret, &trace_status);
				trace_redraw_ticks = 0;
			}
		}

		sceDisplayWaitVblankStart();
		sceKernelDelayThread(10000);
	}

	log_close();
	sceKernelExitProcess(0);
	return 0;
}
