// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <netinet/ip.h>
#include <netlink/attr.h>
#include <netlink/cache.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/rtnl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "stats.h"
#include "utils.h"

int _positionToInserElementAt(unsigned long transferRate,
                              all_values_t *destParams, unsigned int *pivot) {
    for (*pivot = 0; *pivot < destParams->validValues; (*pivot)++) {
        if (destParams->parameters[*pivot].transfer_rate > transferRate)
            break;

        if (destParams->parameters[*pivot].transfer_rate == transferRate) {
            write_adv_log("%s (RET_FAIL): %ld == %ld\n", __func__,
                          destParams->parameters[*pivot].transfer_rate,
                          transferRate);
            return RET_FAIL;
        }
    }
    return RET_OK;
}

void _repositionElements(all_values_t *destParams, int pivot) {

    assert(pivot < destParams->totalLength);

    for (unsigned int i = destParams->validValues; i > pivot; i--) {
        memcpy(&destParams->parameters[i], &destParams->parameters[i - 1],
               sizeof(tuning_params_t));
    }
    return;
}

extern inline void retrieveNumberOfCores(unsigned int *threads) {
    *threads = (unsigned int) sysconf(_SC_NPROCESSORS_CONF);
}

extern inline unsigned short digits(unsigned long int num) {
    return ((num == 0) ? 1 : (log10(num) + 1));
}

char *onOrOff(unsigned int input) {
    if (input) {
        return "on";
    }
    return "off";
}

static inline double calculateDerivedValue(long int currentTransferRate,
                                           long int refTransferRate,
                                           double value, double epsilon) {
    double res;

    if (currentTransferRate < refTransferRate) {
        res = value - (value * epsilon);
    } else {
        res = value + (value * epsilon);
    }
    write_adv_log("currentTransferRate=%ld, refTransferRate=%ld, value=%lf, "
                  "tolerance=%lf, res=%lf\n",
                  currentTransferRate, refTransferRate, value, epsilon, res);
    return res;
}

extern inline double adjustValue(unsigned int pivot, double pivotValue,
                                 unsigned int refIndex, double pivotPrevValue,
                                 double pivotNextValue, double epsilon,
                                 unsigned int approx_function) {
    double res;
    if (pivot < refIndex) {
        if (pivotValue < pivotPrevValue)
            res = pivotPrevValue +
                  calculateTolerance(pivotPrevValue, epsilon, approx_function);
    }
    if (pivot > refIndex) {
        if (pivotValue > pivotNextValue)
            res = pivotNextValue -
                  calculateTolerance(pivotPrevValue, epsilon, approx_function);
    }
    return res;
}

static inline int calcFieldOffset(unsigned int field) {
    switch (field) {
    case RX_RING_SIZE:
        return offsetof(tuning_params_t, rx_ring_size);

    case TX_RING_SIZE:
        return offsetof(tuning_params_t, tx_ring_size);

    case NET_CORE_NETDEV_MAX_BACKLOG:
        return offsetof(tuning_params_t, net_core_netdev_max_backlog);

    case NET_CORE_NETDEV_BUDGET:
        return offsetof(tuning_params_t, net_core_netdev_budget);

    case NET_CORE_SOMAXCONN:
        return offsetof(tuning_params_t, net_core_somaxconn);

    case NET_CORE_RMEM_MAX:
        return offsetof(tuning_params_t, net_core_rmem_max);

    case TCP_MAX_SYN_BACKLOG:
        return offsetof(tuning_params_t, tcp_max_syn_backlog);

    case NET_CORE_RMEM_DEFAULT:
        return offsetof(tuning_params_t, net_core_rmem_default);

    case NET_CORE_WMEM_MAX:
        return offsetof(tuning_params_t, net_core_wmem_max);

    case NET_CORE_WMEM_DEFAULT:
        return offsetof(tuning_params_t, net_core_wmem_default);

    case TCP_RMEM_0:
        return offsetof(tuning_params_t, tcp_rmem0);

    case TCP_RMEM_1:
        return offsetof(tuning_params_t, tcp_rmem1);

    case TCP_RMEM_2:
        return offsetof(tuning_params_t, tcp_rmem2);

    case TCP_WMEM_0:
        return offsetof(tuning_params_t, tcp_wmem0);

    case TCP_WMEM_1:
        return offsetof(tuning_params_t, tcp_wmem1);

    case TCP_WMEM_2:
        return offsetof(tuning_params_t, tcp_wmem2);
    }
    return -1;
}

extern inline double calcDerivedValue(tuning_params_t *parameters,
                                      unsigned int refIndex,
                                      unsigned int transferRate,
                                      unsigned int pivot, unsigned int field,
                                      double epsilon,
                                      unsigned int approx_function) {

    double res = NAN;
    tuning_params_t *item = &parameters[refIndex];
    tuning_params_t *pivotPrev = &parameters[pivot - 1];
    tuning_params_t *pivotNext = &parameters[pivot + 1];

    int offset = calcFieldOffset(field);
    if (offset < 0) {
        write_log("offset = %d", offset);
        return (double)offset;
    }

    if (field == RX_RING_SIZE || field == TX_RING_SIZE) {
        unsigned short referenceValue =
            *(unsigned short *)((char *)item + offset);
        unsigned short pivotPrevValue =
            *(unsigned short *)((char *)pivotPrev + offset);
        unsigned short pivotNextValue =
            *(unsigned short *)((char *)pivotNext + offset);

        res = calculateDerivedValue(transferRate,
                                    parameters[refIndex].transfer_rate,
                                    referenceValue, epsilon);

        res = adjustValue(pivot, refIndex, res, pivotPrevValue, pivotNextValue,
                          epsilon, approx_function);
    } else if (field == NET_CORE_NETDEV_BUDGET || field == NET_CORE_SOMAXCONN ||
               field == NET_CORE_NETDEV_MAX_BACKLOG ||
               field == TCP_MAX_SYN_BACKLOG) {
        unsigned int referenceValue = *(unsigned int *)((char *)item + offset);
        unsigned int pivotPrevValue =
            *(unsigned int *)((char *)pivotPrev + offset);
        unsigned int pivotNextValue =
            *(unsigned int *)((char *)pivotNext + offset);

        res = calculateDerivedValue(transferRate,
                                    parameters[refIndex].transfer_rate,
                                    referenceValue, epsilon);

        res = adjustValue(pivot, refIndex, res, pivotPrevValue, pivotNextValue,
                          epsilon, approx_function);
    } else if (field == NET_CORE_RMEM_MAX || field == NET_CORE_RMEM_DEFAULT ||
               field == NET_CORE_WMEM_MAX || field == NET_CORE_WMEM_DEFAULT ||
               field == TCP_RMEM_0 || field == TCP_RMEM_1 ||
               field == TCP_RMEM_2 || field == TCP_WMEM_0 ||
               field == TCP_WMEM_1 || field == TCP_WMEM_2) {
        unsigned long referenceValue =
            *(unsigned long *)((char *)item + offset);
        unsigned long pivotPrevValue =
            *(unsigned long *)((char *)pivotPrev + offset);
        unsigned long pivotNextValue =
            *(unsigned long *)((char *)pivotNext + offset);

        res = calculateDerivedValue(transferRate,
                                    parameters[refIndex].transfer_rate,
                                    referenceValue, epsilon);
        if (res > UINT_MAX || res < 0.) {
            write_adv_log("value res is out of range for unsigned int: %e\n",
                          res);
        }
        const unsigned int refIndex =
            res > UINT_MAX ? UINT_MAX : (res < 0 ? 0 : res);
        res = adjustValue(pivot, refIndex, refIndex, pivotPrevValue,
                          pivotNextValue, epsilon, approx_function);
    }
    return res;
}

extern inline double
calculateWeightedValue(unsigned long currentTransferRate, uint64_t dropRate,
                       uint64_t errorsRate, uint64_t fifoErrorsRate,
                       weights_reference_t *weights, double bias) {

    double weightedValue = currentTransferRate * weights->transfer_rate_weight +
                           dropRate * weights->drop_rate_weight +
                           errorsRate * weights->errors_rate_weight +
                           fifoErrorsRate * weights->fifo_errors_rate_weight;

    weightedValue += bias;

    return weightedValue;
}

extern inline double calculateAccuracy(unsigned short dig) {
    double eps = 1;
    for (int i = 0; i < dig; i++)
        eps /= 10;
    return eps;
}

extern inline double calculateEpsilon(double weightedValue, double accuracy) {
    unsigned short zeros = digits(weightedValue) * accuracy;
    double eps = calculateAccuracy(zeros);

    return eps;
}

extern inline double calculateTolerance(double weightedValue, double eps,
                                        unsigned int approx_function) {
    double toleranceValue = weightedValue * eps;

    switch (approx_function) {
    case 0:
        write_adv_log("No approximation function set.\n");
        break;
    case 1:
        toleranceValue = sqrt(toleranceValue);
        break;
    case 2:
        toleranceValue = pow(toleranceValue, 2);
        break;
    case 3:
        toleranceValue = log10(toleranceValue);
        break;
    case 4:
        toleranceValue = log(toleranceValue);
        break;
    default:
        write_log("This is an error case. It should never happen.\n");
        break;
    }

    return toleranceValue;
}

void printTable(all_values_t *values) {
#ifdef PRINT_TABLE
    unsigned int i;

    printf("\n############################################## TABLE CONTENT "
           "BEGIN ##############################################\n");
    for (i = 0; i < values->validValues; i++) {
        printf("[%d] transfer_rate=%ld,"
               "drop_rate=%ld,"
               "errors_rate=%ld,"
               "fifo_errors_rate=%ld,"
               "cpu_usage_percentage=%lf,"
               "rx_ring_size=%hd,"
               "tx_ring_size=%hd,"
               "cores=%hd,"
               "governor=%hd,"
               "cpu_speed=%d,"
               "io_scheduler=%hd,"
               "task_scheduler=%hd,"
               "kernel_sched_min_granularity_ns=%d,"
               "kernel_sched_wakeup_granularity_ns=%d,"
               "kernel_sched_migration_cost_ns=%d,"
               "kernel_numa_balancing=%hd,"
               "kernel_pid_max=%hd,"
               "net_core_netdev_max_backlog=%d,"
               "net_core_netdev_budget=%d,"
               "net_core_somaxconn=%d,"
               "net_core_busy_poll=%hd,"
               "net_core_busy_read=%hd,"
               "net_core_rmem_max=%ld,"
               "net_core_wmem_max=%ld,"
               "net_core_rmem_default=%ld,"
               "net_core_wmem_default=%ld,"
               "tcp_fastopen=%hd,"
               "tcp_low_latency=%hd,"
               "tcp_sack=%hd,"
               "tcp_rmem0=%ld,"
               "tcp_rmem1=%ld,"
               "tcp_rmem2=%ld,"
               "tcp_wmem0=%ld,"
               "tcp_wmem1=%ld,"
               "tcp_wmem2=%ld,"
               "tcp_max_syn_backlog=%d,"
               "tcp_tw_reuse=%hd,"
               "tcp_tw_recycle=%hd,"
               "tcp_timestamps=%hd,"
               "tcp_syn_retries=%d,"
               "rx_interrupt_coalesce_usec=%hd,"
               "rx_interrupt_max_coalesce_frames=%hd,"
               "tx_interrupt_coalesce_usecs=%hd,"
               "tx_interrupt_max_coalesce_frames=%hd,"
               "rx_checksum_offload=%hd,"
               "tx_checksum_offload=%hd,"
               "general_segmentation_offload=%hd,"
               "tcp_segmentation_offload=%hd,"
               "general_receive_offload=%hd,"
               "large_receive_offload=%hd,"
               "rx_vlan_offload=%hd,"
               "tx_vlan_offload=%hd,"
               "rx_hash=%hd\n\n",
               i, values->parameters[i].transfer_rate,
               values->parameters[i].drop_rate,
               values->parameters[i].errors_rate,
               values->parameters[i].fifo_errors_rate,
               values->parameters[i].cpu_usage_percentage,
               values->parameters[i].rx_ring_size,
               values->parameters[i].tx_ring_size, values->parameters[i].cores,
               values->parameters[i].governor, values->parameters[i].cpu_speed,
               values->parameters[i].io_scheduler,
               values->parameters[i].task_scheduler,
               values->parameters[i].kernel_sched_min_granularity_ns,
               values->parameters[i].kernel_sched_wakeup_granularity_ns,
               values->parameters[i].kernel_sched_migration_cost_ns,
               values->parameters[i].kernel_numa_balancing,
               values->parameters[i].kernel_pid_max,
               values->parameters[i].net_core_netdev_max_backlog,
               values->parameters[i].net_core_netdev_budget,
               values->parameters[i].net_core_somaxconn,
               values->parameters[i].net_core_busy_poll,
               values->parameters[i].net_core_busy_read,
               values->parameters[i].net_core_rmem_max,
               values->parameters[i].net_core_wmem_max,
               values->parameters[i].net_core_rmem_default,
               values->parameters[i].net_core_wmem_default,
               values->parameters[i].tcp_fastopen,
               values->parameters[i].tcp_low_latency,
               values->parameters[i].tcp_sack, values->parameters[i].tcp_rmem0,
               values->parameters[i].tcp_rmem1, values->parameters[i].tcp_rmem2,
               values->parameters[i].tcp_wmem0, values->parameters[i].tcp_wmem1,
               values->parameters[i].tcp_wmem2,
               values->parameters[i].tcp_max_syn_backlog,
               values->parameters[i].tcp_tw_reuse,
               values->parameters[i].tcp_tw_recycle,
               values->parameters[i].tcp_timestamps,
               values->parameters[i].tcp_syn_retries,
               values->parameters[i].rx_interrupt_coalesce_usecs,
               values->parameters[i].rx_interrupt_max_coalesce_frames,
               values->parameters[i].tx_interrupt_coalesce_usecs,
               values->parameters[i].tx_interrupt_max_coalesce_frames,
               values->parameters[i].rx_checksum_offload,
               values->parameters[i].tx_checksum_offload,
               values->parameters[i].general_segmentation_offload,
               values->parameters[i].tcp_segmentation_offload,
               values->parameters[i].general_receive_offload,
               values->parameters[i].large_receive_offload,
               values->parameters[i].rx_vlan_offload,
               values->parameters[i].tx_vlan_offload,
               values->parameters[i].rx_hash);
    }
    printf("############################################## TABLE CONTENT END "
           "##############################################\n\n");
#endif
}

static inline void parseSystemSetting(tuning_params_t *systemSettings,
                                      unsigned int field) {
    FILE *fp;
    char command[MAX_COMMAND_LENGTH];
    char param[MAX_COMMAND_LENGTH];

    if (field == RX_RING_SIZE) {

    } else if (field == TX_RING_SIZE) {

    } else if (field == NET_CORE_NETDEV_BUDGET) {
        snprintf(command, MAX_COMMAND_LENGTH, "sysctl net.core.netdev_budget");
        fp = popen(command, "r");
        assert(fp != NULL);
        fgets(command, MAX_COMMAND_LENGTH, fp);
        sscanf(command, "%s = %u", param,
               &(systemSettings->net_core_netdev_budget));
        fclose(fp);
    } else if (field == NET_CORE_SOMAXCONN) {
        snprintf(command, MAX_COMMAND_LENGTH, "sysctl net.core.somaxconn");
        fp = popen(command, "r");
        assert(fp != NULL);
        fgets(command, MAX_COMMAND_LENGTH, fp);
        sscanf(command, "%s = %u", param,
               &(systemSettings->net_core_somaxconn));
        fclose(fp);
    } else if (field == NET_CORE_NETDEV_MAX_BACKLOG) {
        snprintf(command, MAX_COMMAND_LENGTH,
                 "sysctl net.core.netdev_max_backlog");
        fp = popen(command, "r");
        assert(fp != NULL);
        fgets(command, MAX_COMMAND_LENGTH, fp);
        sscanf(command, "%s = %u", param,
               &(systemSettings->net_core_netdev_max_backlog));
        fclose(fp);
    } else if (field == TCP_MAX_SYN_BACKLOG) {
        snprintf(command, MAX_COMMAND_LENGTH,
                 "sysctl net.ipv4.tcp_max_syn_backlog");
        fp = popen(command, "r");
        assert(fp != NULL);
        fgets(command, MAX_COMMAND_LENGTH, fp);
        sscanf(command, "%s = %u", param,
               &(systemSettings->tcp_max_syn_backlog));
        fclose(fp);
    } else if (field == NET_CORE_RMEM_MAX) {
        snprintf(command, MAX_COMMAND_LENGTH, "sysctl net.core.rmem_max");
        fp = popen(command, "r");
        assert(fp != NULL);
        fgets(command, MAX_COMMAND_LENGTH, fp);
        sscanf(command, "%s = %lu", param,
               &(systemSettings->net_core_rmem_max));
        fclose(fp);
    } else if (field == NET_CORE_RMEM_DEFAULT) {
        snprintf(command, MAX_COMMAND_LENGTH, "sysctl net.core.rmem_default");
        fp = popen(command, "r");
        assert(fp != NULL);
        fgets(command, MAX_COMMAND_LENGTH, fp);
        sscanf(command, "%s = %lu", param,
               &(systemSettings->net_core_rmem_default));
        fclose(fp);
    } else if (field == NET_CORE_WMEM_MAX) {
        snprintf(command, MAX_COMMAND_LENGTH, "sysctl net.core.wmem_max");
        fp = popen(command, "r");
        assert(fp != NULL);
        fgets(command, MAX_COMMAND_LENGTH, fp);
        sscanf(command, "%s = %lu", param,
               &(systemSettings->net_core_wmem_max));
        fclose(fp);
    } else if (field == NET_CORE_WMEM_DEFAULT) {
        snprintf(command, MAX_COMMAND_LENGTH, "sysctl net.core.wmem_default");
        fp = popen(command, "r");
        assert(fp != NULL);
        fgets(command, MAX_COMMAND_LENGTH, fp);
        sscanf(command, "%s = %lu", param,
               &(systemSettings->net_core_wmem_default));
        fclose(fp);
    } else if (field == TCP_RMEM_0 || field == TCP_RMEM_1 ||
               field == TCP_RMEM_2) {
        snprintf(command, MAX_COMMAND_LENGTH, "sysctl net.ipv4.tcp_rmem");
        fp = popen(command, "r");
        assert(fp != NULL);
        fgets(command, MAX_COMMAND_LENGTH, fp);
        sscanf(command, "%s = %lu\t%lu\t%lu", param,
               &(systemSettings->tcp_rmem0), &(systemSettings->tcp_rmem1),
               &(systemSettings->tcp_rmem2));
        fclose(fp);
    } else if (field == TCP_WMEM_0 || field == TCP_WMEM_1 ||
               field == TCP_WMEM_2) {
        snprintf(command, MAX_COMMAND_LENGTH, "sysctl net.ipv4.tcp_wmem");
        fp = popen(command, "r");
        assert(fp != NULL);
        fgets(command, MAX_COMMAND_LENGTH, fp);
        sscanf(command, "%s = %lu\t%lu\t%lu", param,
               &(systemSettings->tcp_wmem0), &(systemSettings->tcp_wmem1),
               &(systemSettings->tcp_wmem2));
        fclose(fp);
    }
}

extern inline void readSystemSettings(tuning_params_t *systemSettings) {
    write_log("Reading system settings...");
    fflush(stdout);

    parseSystemSetting(systemSettings, RX_RING_SIZE);
    parseSystemSetting(systemSettings, TX_RING_SIZE);
    parseSystemSetting(systemSettings, NET_CORE_NETDEV_MAX_BACKLOG);
    parseSystemSetting(systemSettings, NET_CORE_NETDEV_BUDGET);
    parseSystemSetting(systemSettings, NET_CORE_SOMAXCONN);
    parseSystemSetting(systemSettings, NET_CORE_RMEM_MAX);
    parseSystemSetting(systemSettings, NET_CORE_RMEM_DEFAULT);
    parseSystemSetting(systemSettings, NET_CORE_WMEM_MAX);
    parseSystemSetting(systemSettings, NET_CORE_WMEM_DEFAULT);
    parseSystemSetting(systemSettings, TCP_MAX_SYN_BACKLOG);
    parseSystemSetting(systemSettings, TCP_RMEM_0);
    parseSystemSetting(systemSettings, TCP_RMEM_1);
    parseSystemSetting(systemSettings, TCP_RMEM_2);
    parseSystemSetting(systemSettings, TCP_WMEM_0);
    parseSystemSetting(systemSettings, TCP_WMEM_1);
    parseSystemSetting(systemSettings, TCP_WMEM_2);

    write_log("DONE.\n");
}
