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

#include "filehelper.h"
#include "phoebe.h"
#include "stats.h"
#include "utils.h"

int addData(all_values_t *values, unsigned int origTableIndex,
            unsigned long int transferRate, double epsilon,
            unsigned int approx_function, unsigned short liveMode) {
    unsigned int pivot;

    if (_positionToInserElementAt(transferRate, values, &pivot) == RET_FAIL)
        return RET_FAIL;

    _repositionElements(values, pivot);

    values->parameters[pivot].transfer_rate = transferRate;

    /* The following parameters can be read from the system - when running
     * liveTraining - instead of being inferred.
     */
    if (liveMode)
        values->parameters[pivot].drop_rate = getDropRate();
    else {
        values->parameters[pivot].drop_rate =
            values->parameters[origTableIndex].drop_rate;

        adjustValue(pivot, origTableIndex, values->parameters[pivot].drop_rate,
                    values->parameters[pivot - 1].drop_rate,
                    values->parameters[pivot + 1].drop_rate, epsilon,
                    approx_function);
    }

    if (liveMode)
        values->parameters[pivot].errors_rate = getErrorsRate();
    else {
        values->parameters[pivot].errors_rate =
            values->parameters[origTableIndex].errors_rate;

        adjustValue(pivot, origTableIndex,
                    values->parameters[pivot].errors_rate,
                    values->parameters[pivot - 1].errors_rate,
                    values->parameters[pivot + 1].errors_rate, epsilon,
                    approx_function);
    }

    if (liveMode)
        values->parameters[pivot].fifo_errors_rate = getFifoErrorsRate();
    else {
        values->parameters[pivot].fifo_errors_rate =
            values->parameters[origTableIndex].fifo_errors_rate;

        adjustValue(pivot, origTableIndex,
                    values->parameters[pivot].fifo_errors_rate,
                    values->parameters[pivot - 1].fifo_errors_rate,
                    values->parameters[pivot + 1].fifo_errors_rate, epsilon,
                    approx_function);
    }

    if (liveMode)
        values->parameters[pivot].cpu_usage_percentage = getCpuBusyTime();
    else {
        values->parameters[pivot].cpu_usage_percentage =
            values->parameters[origTableIndex].cpu_usage_percentage;

        adjustValue(pivot, origTableIndex,
                    values->parameters[pivot].cpu_usage_percentage,
                    values->parameters[pivot - 1].cpu_usage_percentage,
                    values->parameters[pivot + 1].cpu_usage_percentage, epsilon,
                    approx_function);
    }

    /* The following parameters can be assigned directly from the corresponding
     * matching item */
    values->parameters[pivot].cpu_speed =
        values->parameters[origTableIndex].cpu_speed;
    adjustValue(pivot, origTableIndex, values->parameters[pivot].cpu_speed,
                values->parameters[pivot - 1].cpu_speed,
                values->parameters[pivot + 1].cpu_speed, epsilon,
                approx_function);

    values->parameters[pivot].cores = values->parameters[origTableIndex].cores;
    values->parameters[pivot].governor =
        values->parameters[origTableIndex].governor;
    values->parameters[pivot].io_scheduler =
        values->parameters[origTableIndex].io_scheduler;
    values->parameters[pivot].task_scheduler =
        values->parameters[origTableIndex].task_scheduler;

    values->parameters[pivot].kernel_sched_min_granularity_ns =
        values->parameters[origTableIndex].kernel_sched_min_granularity_ns;
    adjustValue(pivot, origTableIndex,
                values->parameters[pivot].kernel_sched_min_granularity_ns,
                values->parameters[pivot - 1].kernel_sched_min_granularity_ns,
                values->parameters[pivot + 1].kernel_sched_min_granularity_ns,
                epsilon, approx_function);

    values->parameters[pivot].kernel_sched_wakeup_granularity_ns =
        values->parameters[origTableIndex].kernel_sched_wakeup_granularity_ns;
    adjustValue(
        pivot, origTableIndex,
        values->parameters[pivot].kernel_sched_wakeup_granularity_ns,
        values->parameters[pivot - 1].kernel_sched_wakeup_granularity_ns,
        values->parameters[pivot + 1].kernel_sched_wakeup_granularity_ns,
        epsilon, approx_function);

    values->parameters[pivot].kernel_sched_migration_cost_ns =
        values->parameters[origTableIndex].kernel_sched_migration_cost_ns;
    adjustValue(pivot, origTableIndex,
                values->parameters[pivot].kernel_sched_migration_cost_ns,
                values->parameters[pivot - 1].kernel_sched_migration_cost_ns,
                values->parameters[pivot + 1].kernel_sched_migration_cost_ns,
                epsilon, approx_function);

    values->parameters[pivot].kernel_numa_balancing =
        values->parameters[origTableIndex].kernel_numa_balancing;

    values->parameters[pivot].kernel_pid_max =
        values->parameters[origTableIndex].kernel_pid_max;
    adjustValue(pivot, origTableIndex, values->parameters[pivot].kernel_pid_max,
                values->parameters[pivot - 1].kernel_pid_max,
                values->parameters[pivot + 1].kernel_pid_max, epsilon,
                approx_function);

    /* All other parameters' value are derived from the corresponding matching
     * item. */
    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     RX_RING_SIZE, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     TX_RING_SIZE, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     NET_CORE_NETDEV_MAX_BACKLOG, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     NET_CORE_NETDEV_BUDGET, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     NET_CORE_SOMAXCONN, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     TCP_MAX_SYN_BACKLOG, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     NET_CORE_RMEM_MAX, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     NET_CORE_RMEM_DEFAULT, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     NET_CORE_WMEM_MAX, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     NET_CORE_WMEM_DEFAULT, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     TCP_RMEM_0, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     TCP_RMEM_1, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     TCP_RMEM_2, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     TCP_WMEM_0, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     TCP_WMEM_1, epsilon, approx_function);

    calcDerivedValue(values->parameters, origTableIndex,
                     values->parameters[origTableIndex].transfer_rate, pivot,
                     TCP_WMEM_2, epsilon, approx_function);

    values->validValues++;

    if (values->validValues % 10 == 0)
        write_log("Total entries in table = %ld\n", values->validValues);

    return RET_OK;
}

extern inline int binarySearchWithTolerance(tuning_params_t *parameters, int l,
                                            int r, double weighted_value,
                                            double tolerance,
                                            unsigned int *closestIndex,
                                            weights_reference_t *weights,
                                            double bias) {
    if (r >= l) {
        unsigned int mid = l + (r - l) / 2;

        double ref_weighted_value = calculateWeightedValue(
            parameters[mid].transfer_rate, parameters[mid].drop_rate,
            parameters[mid].errors_rate, parameters[mid].fifo_errors_rate,
            weights, bias);

        double diff = fabs(weighted_value - ref_weighted_value);

        *closestIndex = mid;

        if (diff <= tolerance)
            return mid;

        if (parameters[mid].transfer_rate > weighted_value)
            return binarySearchWithTolerance(parameters, l, mid - 1,
                                             weighted_value, tolerance,
                                             closestIndex, weights, bias);

        return binarySearchWithTolerance(parameters, mid + 1, r, weighted_value,
                                         tolerance, closestIndex, weights,
                                         bias);
    }
    return -1;
}

extern inline unsigned long
generatePseudoRandomTransferRate(all_values_t *values) {
    unsigned long transferRate =
        rand() % values->parameters[values->validValues - 1].transfer_rate;

    /* Ensure we do NOT produce a 0 transferRate value */
    if (transferRate == 0)
        transferRate = values->parameters[0].transfer_rate;

    return transferRate;
}
