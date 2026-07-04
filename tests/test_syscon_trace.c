#include <assert.h>

#include "../syscon_trace.h"

int main(void)
{
	unsigned char tx[32] = {0};
	unsigned char rx[32] = {0};

	tx[0] = 0x34;
	tx[1] = 0x12;
	tx[2] = 3;
	tx[3] = 0xaa;
	tx[4] = 0xbb;
	tx[5] = 0xcc;

	rx[2] = 2;
	rx[3] = 0x5a;
	rx[4] = 0x11;
	rx[5] = 0x22;

	assert(noled_syscon_packet_cmd(tx) == 0x1234);
	assert(noled_syscon_tx_data_len(tx) == 3);
	assert(noled_syscon_rx_data_len(rx) == 2);
	assert(noled_syscon_rx_result(rx) == 0x5a);

	tx[2] = 0xff;
	rx[2] = 0xff;
	assert(noled_syscon_tx_data_len(tx) == NOLED_SYSCON_TRACE_DATA_MAX);
	assert(noled_syscon_rx_data_len(rx) == NOLED_SYSCON_TRACE_DATA_MAX);
	assert(noled_syscon_should_trace_cmd(0x089a) == 1);
	assert(noled_syscon_should_trace_cmd(0x088a) == 1);
	assert(noled_syscon_should_trace_cmd(0x088f) == 1);
	assert(noled_syscon_should_trace_cmd(0x089b) == 1);
	assert(noled_syscon_should_trace_cmd(0x0804) == 1);
	assert(noled_syscon_should_trace_cmd(0x0104) == 0);
	assert(noled_syscon_should_trace_cmd(0x0301) == 0);

	return 0;
}
