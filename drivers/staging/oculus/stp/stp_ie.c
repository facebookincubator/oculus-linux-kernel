#include "stp_master.h"
#include "stp_pipeline.h"
#include "stp_master_common.h"
#include "stp_profiling.h"

#define STP_IE_PRIOFILING_NUM_REPORT 100000
#define STP_SPI_EXCEED_LIMIT	7000
#define STP_EXCEED_RX_PIPELINE_USAGE_LIMIT	(1024*512)
#define STP_IE_PRIOFILING_BANDWITH_PERIOD_US 5000000

enum stp_ie_log_freq_type {
	STP_IE_LOG_NEVER = 0,
	STP_IE_LOG_ALWAYS,
	STP_IE_LOG_PERIODIC,
	STP_IE_LOG_ONCE,
};

enum stp_ie_debug_set_type {
	STP_IE_DEBUG_NONE = 0,
	STP_IE_DEBUG_SET,
	STP_IE_DEBUG_RESET,
};

enum stp_ie_profiling_flags {
	STP_PROFILING_NONE = 0x00,
	STP_PROFILING_START = 0x01,
	STP_PROFILING_COUNT = 0x02,
	STP_PROFILING_AVERAGE_REF = 0x04,
	STP_PROFILING_AVERAGE_ITSELF = 0x08,
	STP_PROFILING_EXCEEDED_REF = 0x10,
	STP_PROFILING_EXCEEDED_ITSELF = 0x20,
	STP_PROFILING_EXCEEDED_VALUE = 0x40,
	STP_PROFILING_BANDWITH = 0x80,
};

struct stp_ie_log_type {
	unsigned int log_type;
	unsigned int freq;
	unsigned int counter;
} stp_ie_log_type;

struct stp_ie_stats_type {
	unsigned long *p_count;
	unsigned long *p_data;
} stp_ie_stats_type;

struct stp_ie_debug_type {
	bool *p_flag;
	unsigned int type;
} stp_ie_debug_type;

struct stp_ie_profiling_type {
	unsigned int type;
	int index_ref_time;
	unsigned int num_report;
	unsigned int exceeded;
} stp_ie_profiling_type;

struct stp_ie_profiling_data_type {
	uint64_t time;
	uint64_t prev_time;
	struct average {
		unsigned int counter;
		unsigned long total_time_ref;
		unsigned long total_time_self;
	} average;
	struct exceeded {
		unsigned int counter_ref;
		unsigned int counter_self;
	} exceeded;
	struct bandwith {
		uint64_t ref_time;
		unsigned long data;
	} bandwith;

	unsigned long total_counter;
} stp_ie_profiling_data_type;

const char *stp_ie_message[STP_IE_NUM] = {
	//STP_IE_RX_BAD_MAGIC_NUMBER
	"STP RX error: bad magic!",

	//STP_IE_RX_BAD_LEN
	"STP RX error: invalid size!",

	//STP_IE_RX_BAD_CRC
	"STP RX error: bad CRC!",

	//STP_IE_RX_PIPELINE_FULL
	"STP RX error: pipeline full!",

	//STP_IE_TX_BAD_MAGIC_NUMBER
	"STP TX error: bad magic!",

	//STP_IE_TX_BAD_LEN
	"STP TX error: invalid size!",

	//STP_IE_TX_BAD_CRC
	"STP TX error: bad CRC!",

	//STP_IE_TX_PIPELINE_FULL
	"STP TX error: pipeline full!",

	//STP_IE_MISMATCH_INIT
	"STP init error: bad magic!",

	//STP_IE_RX_OK
	"STP RX valid",

	//STP_IE_TX_OK
	"STP TX valid",

	//STP_IE_ENTER_SPI
	"STP Enter SPI",

	//STP_IE_EXIT_SPI
	"STP Exit SPI",

	//STP_IE_ENTER_WAIT_SLAVE
	"STP Enter wait slave",

	//STP_IE_EXIT_WAIT_SLAVE,
	"STP Exit wait slave",

	//STP_IE_ENTER_WAIT_READ
	"STP Enter wait for read",

	//STP_IE_EXIT_WAIT_READ
	"STP Exit wait for read",

	//STP_IE_ENTER_WAIT_WRITE
	"STP Enter wait for write",

	//STP_IE_EXIT_WAIT_WRITE
	"STP Exit wait for write",

	//STP_IE_ADD_DATA_RX_PIPELINE
	"STP Add data RX pipeline",

	//STP_IE_ADD_DATA_TX_PIPELINE
	"STP Add data TX pipeline",

	//STP_IE_ENTER_WAIT_DATA
	"STP Enter wait data",

	//STP_IE_EXIT_WAIT_DATA
	"STP Exit wait data",

	//STP_IE_IRQ_READY
	"STP IRQ ready",

	//STP_IE_SET_SIGNAL_SLAVE_READY
	"STP Set signal slave ready",

	//STP_IE_RESET_SIGNAL_SLAVE_READY
	"STP Reset signal slave ready",

	//STP_IE_IRQ_DATA
	"STP IRQ has data",

	//STP_IE_SET_SIGNAL_DATA
	"STP Set signal data",

	//STP_IE_RESET_SIGNAL_DATA
	"STP Reset signal data",

	//STP_IE_GET_DATA_RX_PIPELINE
	"STP Get data RX pipeline",

	//STP_IE_GET_DATA_TX_PIPELINE
	"STP Get data TX pipeline",

	//STP_IE_SLAVE_CONNECTED,
	"STP Slave connected",

	//STP_IE_SLAVE_DISCONNECTED,
	"STP Slave disconnected",

	//STP_IE_MASTER_CONNECTED,
	"STP Master connected",

	//STP_IE_MASTER_DISCONNECTED,
	"STP Master disconnected",

	//STP_IE_EXIT_INIT_STATE
	"STP Exit INIT state",
};

struct stp_ie_log_type stp_ie_log_info[STP_IE_NUM] = {
	//STP_IE_RX_BAD_MAGIC_NUMBER
	{STP_IE_LOG_ALWAYS, 0, 0},

	//STP_IE_RX_BAD_LEN
	{STP_IE_LOG_ALWAYS, 0, 0},

	//STP_IE_RX_BAD_CRC
	{STP_IE_LOG_PERIODIC, 10, 0},

	//STP_IE_RX_PIPELINE_FULL
//	{STP_IE_LOG_PERIODIC, 500, 0},
	{STP_IE_LOG_ONCE, 500, 0},

	//STP_IE_TX_BAD_MAGIC_NUMBER
	{STP_IE_LOG_ALWAYS, 0, 0},

	//STP_IE_TX_BAD_LEN
	{STP_IE_LOG_ALWAYS, 0, 0},

	//STP_IE_TX_BAD_CRC
	{STP_IE_LOG_PERIODIC, 10, 0},

	//STP_IE_TX_PIPELINE_FULL
	{STP_IE_LOG_PERIODIC, 50, 0},

	//STP_IE_MISMATCH_INIT
	{STP_IE_LOG_ALWAYS, 0, 0},

	//STP_IE_RX_OK
	{STP_IE_LOG_NEVER, 10, 0},

	//STP_IE_TX_OK
	{STP_IE_LOG_NEVER, 10, 0},

	//STP_IE_ENTER_SPI
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_EXIT_SPI
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_ENTER_WAIT_SLAVE
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_EXIT_WAIT_SLAVE,
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_ENTER_WAIT_READ
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_EXIT_WAIT_READ
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_ENTER_WAIT_WRITE
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_EXIT_WAIT_WRITE
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_ADD_DATA_RX_PIPELINE
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_ADD_DATA_TX_PIPELINE
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_ENTER_WAIT_DATA
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_EXIT_WAIT_DATA
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_IRQ_READY
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_SET_SIGNAL_SLAVE_READY
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_RESET_SIGNAL_SLAVE_READY
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_IRQ_DATA
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_SET_SIGNAL_DATA
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_RESET_SIGNAL_DATA
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_GET_DATA_RX_PIPELINE
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_GET_DATA_TX_PIPELINE
	{STP_IE_LOG_NEVER, 0, 0},

	//STP_IE_SLAVE_CONNECTED,
	{STP_IE_LOG_ALWAYS, 0, 0},

	//STP_IE_SLAVE_DISCONNECTED,
	{STP_IE_LOG_ALWAYS, 0, 0},

	//STP_IE_MASTER_CONNECTED,
	{STP_IE_LOG_ALWAYS, 0, 0},

	//STP_IE_MASTER_DISCONNECTED,
	{STP_IE_LOG_ALWAYS, 0, 0},

	//STP_IE_EXIT_INIT_STATE
	{STP_IE_LOG_NEVER, 0, 0},

};

const struct stp_ie_stats_type stp_ie_stats_info[STP_IE_NUM] = {
	//STP_IE_RX_BAD_MAGIC_NUMBER
	{&_stp_stats.rx_stats.failures.bad_magic_number, NULL},

	//STP_IE_RX_BAD_LEN
	{&_stp_stats.rx_stats.failures.invalid_size, NULL},

	//STP_IE_RX_BAD_CRC
	{&_stp_stats.rx_stats.failures.bad_crc, NULL},

	//STP_IE_RX_PIPELINE_FULL
	{&_stp_stats.rx_stats.failures.not_enough_space, NULL},

	//STP_IE_TX_BAD_MAGIC_NUMBER
	{&_stp_stats.tx_stats.failures.bad_magic_number, NULL},

	//STP_IE_TX_BAD_LEN
	{&_stp_stats.tx_stats.failures.invalid_size, NULL},

	//STP_IE_TX_BAD_CRC
	{&_stp_stats.tx_stats.failures.bad_crc, NULL},

	//STP_IE_TX_PIPELINE_FULL
	{&_stp_stats.tx_stats.failures.not_enough_space, NULL},

	//STP_IE_MISMATCH_INIT
	{&_stp_stats.mismatch_stats.total, NULL},

	//STP_IE_RX_OK
	{&_stp_stats.rx_stats.total_transactions,
		&_stp_stats.rx_stats.total_data},

	//STP_IE_TX_OK
	{&_stp_stats.tx_stats.total_transactions,
		&_stp_stats.tx_stats.total_data},

	//STP_IE_ENTER_SPI
	{NULL, NULL},

	//STP_IE_EXIT_SPI
	{NULL, NULL},

	//STP_IE_ENTER_WAIT_SLAVE
	{NULL, NULL},

	//STP_IE_EXIT_WAIT_SLAVE,
	{NULL, NULL},

	//STP_IE_ENTER_WAIT_READ
	{NULL, NULL},

	//STP_IE_EXIT_WAIT_READ
	{NULL, NULL},

	//STP_IE_ENTER_WAIT_WRITE
	{NULL, NULL},

	//STP_IE_EXIT_WAIT_WRITE
	{NULL, NULL},

	//STP_IE_ADD_DATA_RX_PIPELINE
	{NULL, &_stp_stats.rx_stats.total_data_pipeline},

	//STP_IE_ADD_DATA_TX_PIPELINE
	{NULL, &_stp_stats.tx_stats.total_data_pipeline},

	//STP_IE_ENTER_WAIT_DATA
	{NULL, NULL},

	//STP_IE_EXIT_WAIT_DATA
	{NULL, NULL},

	//STP_IE_IRQ_READY
	{&_stp_debug.irq_slave_ready_counter, NULL},

	//STP_IE_SET_SIGNAL_SLAVE_READY
	{NULL, NULL},

	//STP_IE_RESET_SIGNAL_SLAVE_READY
	{NULL, NULL},

	//STP_IE_IRQ_DATA
	{&_stp_debug.irq_slave_has_data_counter, NULL},

	//STP_IE_SET_SIGNAL_DATA
	{NULL, NULL},

	//STP_IE_RESET_SIGNAL_DATA
	{NULL, NULL},

	//STP_IE_GET_DATA_RX_PIPELINE
	{NULL, NULL},

	//STP_IE_GET_DATA_TX_PIPELINE
	{NULL, NULL},

	//STP_IE_SLAVE_CONNECTED,
	{NULL, NULL},

	//STP_IE_SLAVE_DISCONNECTED,
	{NULL, NULL},

	//STP_IE_MASTER_CONNECTED,
	{NULL, NULL},

	//STP_IE_MASTER_DISCONNECTED,
	{NULL, NULL},

	//STP_IE_EXIT_INIT_STATE
	{NULL, NULL},
};

struct stp_ie_debug_type stp_ie_debug_info[STP_IE_NUM] = {
	//STP_IE_RX_BAD_MAGIC_NUMBER
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_RX_BAD_LEN
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_RX_BAD_CRC
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_RX_PIPELINE_FULL
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_TX_BAD_MAGIC_NUMBER
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_TX_BAD_LEN
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_TX_BAD_CRC
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_TX_PIPELINE_FULL
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_MISMATCH_INIT
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_RX_OK
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_TX_OK
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_ENTER_SPI
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_EXIT_SPI
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_ENTER_WAIT_SLAVE
	{&_stp_debug.wait_for_slave, STP_IE_DEBUG_SET},

	//STP_IE_EXIT_WAIT_SLAVE,
	{&_stp_debug.wait_for_slave, STP_IE_DEBUG_RESET},

	//STP_IE_ENTER_WAIT_READ
	{&_stp_debug.wait_for_read, STP_IE_DEBUG_SET},

	//STP_IE_EXIT_WAIT_READ
	{&_stp_debug.wait_for_read, STP_IE_DEBUG_RESET},

	//STP_IE_ENTER_WAIT_WRITE
	{&_stp_debug.wait_for_write, STP_IE_DEBUG_SET},

	//STP_IE_EXIT_WAIT_WRITE
	{&_stp_debug.wait_for_write, STP_IE_DEBUG_RESET},

	//STP_IE_ADD_DATA_RX_PIPELINE
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_ADD_DATA_TX_PIPELINE
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_ENTER_WAIT_DATA
	{&_stp_debug.wait_for_data, STP_IE_DEBUG_SET},

	//STP_IE_EXIT_WAIT_DATA
	{&_stp_debug.wait_for_data, STP_IE_DEBUG_RESET},

	//STP_IE_IRQ_READY
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_SET_SIGNAL_SLAVE_READY
	{&_stp_debug.signal_slave_ready, STP_IE_DEBUG_SET},

	//STP_IE_RESET_SIGNAL_SLAVE_READY
	{&_stp_debug.signal_slave_ready, STP_IE_DEBUG_RESET},

	//STP_IE_IRQ_DATA
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_SET_SIGNAL_DATA
	{&_stp_debug.signal_data, STP_IE_DEBUG_SET},

	//STP_IE_RESET_SIGNAL_DATA
	{&_stp_debug.signal_data, STP_IE_DEBUG_RESET},

	//STP_IE_GET_DATA_RX_PIPELINE
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_GET_DATA_TX_PIPELINE
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_SLAVE_CONNECTED,
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_SLAVE_DISCONNECTED,
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_MASTER_CONNECTED,
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_MASTER_DISCONNECTED,
	{NULL, STP_IE_DEBUG_NONE},

	//STP_IE_EXIT_INIT_STATE
	{NULL, STP_IE_DEBUG_NONE},
};

struct stp_ie_profiling_type stp_ie_profiling_info[STP_IE_NUM] = {
	//STP_IE_RX_BAD_MAGIC_NUMBER
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_RX_BAD_LEN
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_RX_BAD_CRC
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_RX_PIPELINE_FULL
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_TX_BAD_MAGIC_NUMBER
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_TX_BAD_LEN
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_TX_BAD_CRC
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_TX_PIPELINE_FULL
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_MISMATCH_INIT
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_RX_OK
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_TX_OK
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_ENTER_SPI
	{STP_PROFILING_START, -1, 0, 0},
//	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_EXIT_SPI
	{STP_PROFILING_AVERAGE_REF | STP_PROFILING_EXCEEDED_REF,
		STP_IE_ENTER_SPI,
		STP_IE_PRIOFILING_NUM_REPORT, STP_SPI_EXCEED_LIMIT},
//	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_ENTER_WAIT_SLAVE
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_EXIT_WAIT_SLAVE,
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_ENTER_WAIT_READ
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_EXIT_WAIT_READ
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_ENTER_WAIT_WRITE
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_EXIT_WAIT_WRITE
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_ADD_DATA_RX_PIPELINE
	{STP_PROFILING_NONE, -1, 0, 0},
//	{STP_PROFILING_BANDWITH, -1, 0, 0},

	//STP_IE_ADD_DATA_TX_PIPELINE
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_ENTER_WAIT_DATA
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_EXIT_WAIT_DATA
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_IRQ_READY
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_SET_SIGNAL_SLAVE_READY
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_RESET_SIGNAL_SLAVE_READY
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_IRQ_DATA
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_SET_SIGNAL_DATA
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_RESET_SIGNAL_DATA
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_GET_DATA_RX_PIPELINE
	{STP_PROFILING_NONE, -1, 0, 0},
//	{STP_PROFILING_EXCEEDED_VALUE, -1, 0,
//		STP_EXCEED_RX_PIPELINE_USAGE_LIMIT},

	//STP_IE_GET_DATA_TX_PIPELINE
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_SLAVE_CONNECTED,
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_SLAVE_DISCONNECTED,
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_MASTER_CONNECTED,
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_MASTER_DISCONNECTED,
	{STP_PROFILING_NONE, -1, 0, 0},

	//STP_IE_EXIT_INIT_STATE
	{STP_PROFILING_NONE, -1, 0, 0},
};

static struct stp_ie_profiling_data_type stp_ie_profiling_data[STP_IE_NUM];

/*  Public APIs
 *-------------------------------------------------
 *-------------------------------------------------
 */

static void stp_ie_log(enum stp_internal_events event)
{
	if (stp_ie_log_info[event].log_type == STP_IE_LOG_ALWAYS) {
		STP_LOG_INFO("%s\n", stp_ie_message[event]);
	} else if (stp_ie_log_info[event].log_type == STP_IE_LOG_PERIODIC) {
		if (stp_ie_log_info[event].counter++ == 0) {
			STP_LOG_INFO("%s\n", stp_ie_message[event]);
		} else if (stp_ie_log_info[event].counter >=
			stp_ie_log_info[event].freq) {
			STP_LOG_INFO("%s (%d times)\n", stp_ie_message[event],
				stp_ie_log_info[event].counter);
			stp_ie_log_info[event].counter = 1;
		}
	} else if (stp_ie_log_info[event].log_type == STP_IE_LOG_ONCE) {
		if (stp_ie_log_info[event].counter == 0) {
			STP_LOG_INFO("%s\n", stp_ie_message[event]);
			stp_ie_log_info[event].counter++;
		}
	}
}

static void stp_ie_stats(enum stp_internal_events event, unsigned int data)
{
	if (stp_ie_stats_info[event].p_count != NULL)
		(*stp_ie_stats_info[event].p_count)++;

	if (stp_ie_stats_info[event].p_data != NULL)
		*stp_ie_stats_info[event].p_data += data;
}

static void stp_ie_debug(enum stp_internal_events event)
{
	if (stp_ie_debug_info[event].p_flag != NULL) {
		if (stp_ie_debug_info[event].type == STP_IE_DEBUG_SET)
			*stp_ie_debug_info[event].p_flag = true;
		else if (stp_ie_debug_info[event].type == STP_IE_DEBUG_RESET)
			*stp_ie_debug_info[event].p_flag = false;
	}
}

static void stp_ie_profiling(enum stp_internal_events event, unsigned int data)
{
	struct stp_ie_profiling_data_type *p_data;

	p_data = &stp_ie_profiling_data[event];

	if (stp_ie_profiling_info[event].type != STP_PROFILING_NONE) {
		stp_ie_profiling_data[event].prev_time =
			stp_ie_profiling_data[event].time;
		STP_PROFILING_GET_TIME(stp_ie_profiling_data[event].time);
		stp_ie_profiling_data[event].total_counter++;

		if (stp_ie_profiling_info[event].type &
			STP_PROFILING_AVERAGE_REF) {

			unsigned int delta_time;
			unsigned int index_ref =
				stp_ie_profiling_info[event].index_ref_time;

			delta_time = p_data->time -
				stp_ie_profiling_data[index_ref].time;

			p_data->average.counter++;
			p_data->average.total_time_ref +=
				delta_time;

			if (p_data->average.counter ==
				stp_ie_profiling_info[event].num_report) {
				STP_LOG_INFO("STP Average [%s:%s]: %lu us\n",
				stp_ie_message[index_ref],
				stp_ie_message[event], (unsigned long)
				(p_data->average.total_time_ref/
				p_data->average.counter));
				p_data->average.total_time_ref = 0;
				p_data->average.counter = 0;
			}
		}

		if (stp_ie_profiling_info[event].type &
			STP_PROFILING_EXCEEDED_REF) {
			unsigned int delta_time;
			unsigned int index_ref =
				stp_ie_profiling_info[event].index_ref_time;

			delta_time = stp_ie_profiling_data[event].time -
				stp_ie_profiling_data[index_ref].time;

			if (delta_time >
				stp_ie_profiling_info[event].exceeded) {
				p_data->exceeded.counter_ref++;
				STP_LOG_INFO(
					"STP Exceeded [%s:%s]: %d us (%d out of %lu)\n",
				stp_ie_message[index_ref],
				stp_ie_message[event], delta_time,
				p_data->exceeded.counter_ref,
				p_data->total_counter);
			}
		}

		if (stp_ie_profiling_info[event].type &
			STP_PROFILING_AVERAGE_ITSELF) {
			unsigned int delta_time;

			delta_time = stp_ie_profiling_data[event].time -
				stp_ie_profiling_data[event].prev_time;

			p_data->average.counter++;
			p_data->average.total_time_self += delta_time;

			if (p_data->average.counter ==
				stp_ie_profiling_info[event].num_report) {
				STP_LOG_INFO("STP Average [%s]: %lu us\n",
				stp_ie_message[event],
				(unsigned long)
				(p_data->average.total_time_self/
				p_data->average.counter));
				p_data->average.total_time_self = 0;
				p_data->average.counter = 0;
			}
		}

		if (stp_ie_profiling_info[event].type &
			STP_PROFILING_EXCEEDED_ITSELF) {
			unsigned int delta_time;

			delta_time = p_data->time -
				stp_ie_profiling_data[event].prev_time;

			if (delta_time >
				stp_ie_profiling_info[event].exceeded) {
				p_data->exceeded.counter_self++;
				STP_LOG_INFO(
					"STP Exceeded [%s]: %d us (%d out of %lu)\n",
					stp_ie_message[event], delta_time,
					p_data->exceeded.counter_self,
					p_data->total_counter);
			}
		}

		if (stp_ie_profiling_info[event].type &
			STP_PROFILING_EXCEEDED_VALUE) {

			if (data >
				stp_ie_profiling_info[event].exceeded) {
				STP_LOG_INFO(
					"STP Exceeded value [%s]: %d\n",
					stp_ie_message[event], data);
			}
		}

		if (stp_ie_profiling_info[event].type &
			STP_PROFILING_BANDWITH) {

			if (stp_ie_profiling_data[event].time -
				stp_ie_profiling_data[event].bandwith.ref_time >
				STP_IE_PRIOFILING_BANDWITH_PERIOD_US) {

				STP_LOG_INFO(
				"STP Bandwith [%s]: %lu bytes/sec\n",
				stp_ie_message[event],
				stp_ie_profiling_data[event].bandwith.data/
				(STP_IE_PRIOFILING_BANDWITH_PERIOD_US/1000000));
				stp_ie_profiling_data[event].bandwith.data =
					data;
				stp_ie_profiling_data[event].bandwith.ref_time =
					stp_ie_profiling_data[event].time;
			} else {
				stp_ie_profiling_data[event].bandwith.data +=
					data;
			}
		}
	}
}

int stp_ie_custom(enum stp_internal_events event, unsigned int data)
{
	int ret = STP_SUCCESS;

	(void)data;

	switch (event) {
	case STP_IE_EXIT_INIT_STATE:
		_stp_debug.irq_slave_ready_counter = 0;
		_stp_debug.irq_slave_has_data_counter = 0;
		break;
	default:
		break;
	}

	return ret;
}

int stp_ie_record(enum stp_internal_events event, unsigned int data)
{
	int ret = STP_SUCCESS;

	STP_ASSERT(event < STP_IE_NUM, "Invalid parameter\n");

	stp_ie_log(event);

	stp_ie_stats(event, data);

	stp_ie_debug(event);

	stp_ie_profiling(event, data);

	stp_ie_custom(event, data);

	return ret;
}

