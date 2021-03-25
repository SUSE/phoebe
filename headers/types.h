// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_FILENAME_LENGTH 255
#define MAX_INTERFACE_NAME_LENGTH 255
#define MAX_COMMAND_LENGTH 1024
#define MAX_PROC_STRING_LENGTH 256
#define MAX_PLUGINS 16

// adjust BUFFER_SIZE to suit longest line
#define BUFFER_SIZE 1024 * 1024
#define MAXERRS 5
#define RET_OK 0
#define RET_FAIL -1
#ifndef FALSE // might be allready defined in json.h
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef struct app_settings_s {
    unsigned int max_learning_values;
    unsigned int saving_loop;
    double accuracy;
    unsigned int approx_function;
    unsigned int grace_period;
    double stats_collection_period;
    double inference_loop_period;
    char plugins_path[MAX_FILENAME_LENGTH];
    char rates_filename[MAX_FILENAME_LENGTH];
} app_settings_t;

#define USEC_IN_SEC 1000000 /* Expressed in microseconds; 1s = 10^6usec */

#define MAX_CPU_NAME_LENGTH 7

#define NUM_TUNING_PARAMS 53

// Offsets into CSV file data
typedef struct tuning_params_s {
    unsigned long int transfer_rate;
    unsigned long int drop_rate;
    unsigned long int errors_rate;
    unsigned long int fifo_errors_rate;
    double cpu_usage_percentage;
    unsigned short rx_ring_size;
    unsigned short tx_ring_size;
    unsigned short cores;
    unsigned short governor; /* Requires a INTEGER to STRING mapping for the
                                setting eventually */
    unsigned int cpu_speed;
    unsigned short io_scheduler; /* Requires a INTEGER to STRING mapping for the
                                    setting eventually */
    unsigned short task_scheduler; /* Requires a INTEGER to STRING mapping for
                                      the setting eventually */
    unsigned int kernel_sched_min_granularity_ns;
    unsigned int kernel_sched_wakeup_granularity_ns;
    unsigned int kernel_sched_migration_cost_ns;
    unsigned short kernel_numa_balancing;
    /* According to man proc(5) the maximum value is 2^22 */
    unsigned int kernel_pid_max;
    unsigned int net_core_netdev_max_backlog;
    unsigned int net_core_netdev_budget;
    unsigned int net_core_somaxconn;
    unsigned short net_core_busy_poll;
    unsigned short net_core_busy_read;
    unsigned long net_core_rmem_max;
    unsigned long net_core_wmem_max;
    unsigned long net_core_rmem_default;
    unsigned long net_core_wmem_default;
    unsigned short tcp_fastopen;
    unsigned short tcp_low_latency;
    unsigned short tcp_sack;
    unsigned long tcp_rmem0;
    unsigned long tcp_rmem1;
    unsigned long tcp_rmem2;
    unsigned long tcp_wmem0;
    unsigned long tcp_wmem1;
    unsigned long tcp_wmem2;
    unsigned int tcp_max_syn_backlog;
    unsigned short tcp_tw_reuse;
    unsigned short tcp_tw_recycle;
    unsigned short tcp_timestamps;
    unsigned int tcp_syn_retries;
    unsigned short rx_interrupt_coalesce_usecs;
    unsigned short rx_interrupt_max_coalesce_frames;
    unsigned short tx_interrupt_coalesce_usecs;
    unsigned short tx_interrupt_max_coalesce_frames;
    unsigned short rx_checksum_offload;
    unsigned short tx_checksum_offload;
    unsigned short general_segmentation_offload;
    unsigned short tcp_segmentation_offload;
    unsigned short general_receive_offload;
    unsigned short large_receive_offload;
    unsigned short rx_vlan_offload;
    unsigned short tx_vlan_offload;
    unsigned short rx_hash;
} tuning_params_t;

typedef struct weights_reference_s {
    double transfer_rate_weight;
    double drop_rate_weight;
    double errors_rate_weight;
    double fifo_errors_rate_weight;
} weights_reference_t;

#define NOT_SET 255

#define EMEA 0
#define NA 1
#define LAT 2
#define APAC 3

typedef unsigned short GEOGRAPHY;

#define RETAIL 0
#define AUTOMOTIVE 1
#define SERVICE 2

typedef unsigned short BIZ;

#define THROUGHPUT 0
#define LATENCY 1
#define POWER 2

typedef unsigned short BEHAVIOR;

typedef struct labels_s {
    GEOGRAPHY geo;
    BIZ business;
    BEHAVIOR optimize_for;
} label_t;

typedef struct all_values_s {
    tuning_params_t *parameters;
    label_t *labels;
    unsigned int totalLength;
    unsigned int validValues;
} all_values_t;

typedef struct cpu_raw_stats_s {
    char cpu[MAX_CPU_NAME_LENGTH];
    unsigned int user;
    unsigned int nice;
    unsigned int system;
    unsigned int idle;
    unsigned int iowait;
    unsigned int irq;
    unsigned int softirq;
    unsigned int steal;
    unsigned int guest;
    unsigned int guest_nice;
} cpu_raw_stats_t;

typedef struct cpu_stats_s {
    unsigned int idleTotal;
    unsigned int nonIdleTotal;
} cpu_stats_t;

typedef struct if_raw_stats_s {
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
    uint64_t rx_fifo_err;
    uint64_t tx_fifo_err;
    uint64_t bytes_in;
    uint64_t bytes_out;
} if_raw_stats_t;

typedef struct if_stats_s {
    uint64_t bytes_total;
    uint64_t errors_total;
    uint64_t dropped_total;
    uint64_t fifo_err_total;
} if_stats_t;

typedef struct if_rates_s {
    uint64_t transfer_rate;
    uint64_t errors_rate;
    uint64_t drop_rate;
    uint64_t fifo_err_rate;
    uint64_t min_transfer_rate;
    uint64_t max_transfer_rate;
} if_rates_t;

typedef struct if_ring_size_s {
    int rx;
    int tx;
} if_ring_size_t;

typedef struct if_coalesce_s {
    int rx_coalesce_usecs;
    int rx_max_coalesced_frames;
    int tx_coalesce_usecs;
    int tx_max_coalesced_frames;
} if_coalesce_t;

typedef struct if_offloads_s {
    bool rx_chksum_offload;
    bool tx_chksum_offload;
    bool general_segmentation_offload;
    bool tcp_segmentation_offload;
    bool general_receive_offload;
    bool large_receive_offload;
    bool rx_vlan_offload;
    bool tx_vlan_offload;
    bool rx_hash;
} if_offloads_t;

#define MINUTES_IN_USEC (60 * 1000000)

#define APPEND_TO_FILE_NAME "trained_data.csv"

#define SETTINGS_DEFAULT_PATH "/etc/phoebe/settings.json"

#endif
