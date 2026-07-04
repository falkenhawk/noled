#include <assert.h>

#include "../trace_summary.h"

int main(void)
{
	NoledTraceSummary summary;

	noled_trace_summary_reset(&summary);
	assert(summary.lines == 0);
	assert(summary.cmd_089a == 0);
	assert(summary.cmd_0387 == 0);
	assert(summary.cmd_0804 == 0);
	assert(summary.power_events == 0);
	assert(summary.hook_markers == 0);
	assert(summary.module_map_ready == 0);
	assert(summary.trace_ready == 0);

	noled_trace_summary_update_line(&summary,
		"5 M marker=hook_cmd_exec led=0 enable=0 ret=0x00062731\n");
	noled_trace_summary_update_line(&summary,
		"18 A cmd=0x089A flags=0x00000001 caller=0x018A70D5 txlen=3 tx=020000\n");
	noled_trace_summary_update_line(&summary,
		"21 A cmd=0x0804 flags=0x00000001 caller=0x018A7233 txlen=1 tx=00\n");
	noled_trace_summary_update_line(&summary,
		"353 A cmd=0x0387 flags=0x00000001 caller=0x017CC207 txlen=9 tx=0100000001000000FF\n");
	noled_trace_summary_update_line(&summary,
		"269 P power online=0 charging=0 percent=71\n");
	noled_trace_summary_update_line(&summary,
		"2 M marker=module_map_ready led=0 enable=0 ret=0x00000000\n");
	noled_trace_summary_update_line(&summary,
		"4 M marker=trace_ready led=0 enable=0 ret=0x00000000\n");

	assert(summary.lines == 7);
	assert(summary.cmd_089a == 1);
	assert(summary.cmd_0387 == 1);
	assert(summary.cmd_0804 == 1);
	assert(summary.power_events == 1);
	assert(summary.hook_markers == 1);
	assert(summary.module_map_ready == 1);
	assert(summary.trace_ready == 1);

	noled_trace_summary_update_line(&summary, 0);
	assert(summary.lines == 7);

	return 0;
}
