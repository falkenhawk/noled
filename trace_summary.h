#ifndef NOLED_TRACE_SUMMARY_H
#define NOLED_TRACE_SUMMARY_H

#include <string.h>

typedef struct NoledTraceSummary {
	unsigned int lines;
	unsigned int cmd_089a;
	unsigned int cmd_0387;
	unsigned int cmd_0804;
	unsigned int power_events;
	unsigned int hook_markers;
	unsigned int module_map_ready;
	unsigned int trace_ready;
} NoledTraceSummary;

static inline void noled_trace_summary_reset(NoledTraceSummary *summary)
{
	if (summary == 0) {
		return;
	}

	memset(summary, 0, sizeof(*summary));
}

static inline void noled_trace_summary_update_line(NoledTraceSummary *summary,
	const char *line)
{
	if (summary == 0 || line == 0) {
		return;
	}

	summary->lines++;
	if (strstr(line, "cmd=0x089A") != 0) {
		summary->cmd_089a++;
	}
	if (strstr(line, "cmd=0x0387") != 0) {
		summary->cmd_0387++;
	}
	if (strstr(line, "cmd=0x0804") != 0) {
		summary->cmd_0804++;
	}
	if (strstr(line, " P power ") != 0) {
		summary->power_events++;
	}
	if (strstr(line, "marker=hook_") != 0) {
		summary->hook_markers++;
	}
	if (strstr(line, "marker=module_map_ready") != 0) {
		summary->module_map_ready++;
	}
	if (strstr(line, "marker=trace_ready") != 0) {
		summary->trace_ready++;
	}
}

#endif
