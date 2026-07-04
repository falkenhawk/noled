#ifndef NOLED_SYSCON_TRACE_H
#define NOLED_SYSCON_TRACE_H

#define NOLED_SYSCON_TRACE_DATA_MAX 16

static inline unsigned int noled_syscon_packet_cmd(const unsigned char tx[32])
{
	return ((unsigned int)tx[1] << 8) | tx[0];
}

static inline unsigned int noled_syscon_clamp_data_len(unsigned int len)
{
	return len > NOLED_SYSCON_TRACE_DATA_MAX ? NOLED_SYSCON_TRACE_DATA_MAX : len;
}

static inline unsigned int noled_syscon_tx_data_len(const unsigned char tx[32])
{
	return noled_syscon_clamp_data_len(tx[2]);
}

static inline unsigned int noled_syscon_rx_data_len(const unsigned char rx[32])
{
	return noled_syscon_clamp_data_len(rx[2]);
}

static inline unsigned int noled_syscon_rx_result(const unsigned char rx[32])
{
	return rx[3];
}

static inline int noled_syscon_should_trace_cmd(unsigned int cmd)
{
	return cmd != 0x0104 && cmd != 0x0301;
}

#endif
