// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#include <assert.h>
#include <math.h>
#include <netlink/route/link.h>
#include <netlink/route/rtnl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "algorithmic.h"
#include "filehelper.h"
#include "plugins.h"
#include "stats.h"
#include "utils.h"

static pthread_mutex_t tableWriteLock;
static pthread_t netStatsThreadId;
static pthread_t cpuStatsThreadId;

static all_values_t *_all_values;
static weights_reference_t *_weights;
static app_settings_t *_network_app_settings;
static tuning_params_t *_network_settings;
static double _bias;

static stats_input_param_t stats_input_params;

static unsigned long matches, total = 0L;

void applySettings(char *interfaceName, tuning_params_t *parameters,
                   unsigned int tableIndex) {
    char netCoreCommand[MAX_COMMAND_LENGTH];
    char netIPCommand[MAX_COMMAND_LENGTH];
    char ringSizeCommand[MAX_COMMAND_LENGTH];
    char offloadCommand[MAX_COMMAND_LENGTH];

#ifdef CHECK_INITIAL_SETTINGS
    if (_network_settings->rx_ring_size > parameters->rx_ring_size ||
        _network_settings->tx_ring_size > parameters->tx_ring_size) {
        write_log("Settings not being applied: current values are better.\n");
        return;
    }

    if (_network_settings->net_core_somaxconn >
            parameters->net_core_somaxconn ||
        _network_settings->net_core_netdev_budget >
            parameters->net_core_netdev_budget ||
        _network_settings->net_core_netdev_max_backlog >
            parameters->net_core_netdev_max_backlog) {
        write_log("Settings not being applied: current values are better.\n");
        return;
    }

    memcpy(_network_settings, parameters, sizeof(tuning_params_t));
#endif

    snprintf(netCoreCommand, MAX_COMMAND_LENGTH,
             "sysctl -w net.core.netdev_max_backlog=%d "
             "net.core.netdev_budget=%d "
             "net.core.somaxconn=%d "
             "net.core.busy_poll=%hd "
             "net.core.busy_read=%hd "
             "net.core.rmem_max=%ld "
             "net.core.wmem_max=%ld "
             "net.core.rmem_default=%ld "
             "net.core.wmem_default=%ld ",
             parameters[tableIndex].net_core_netdev_max_backlog,
             parameters[tableIndex].net_core_netdev_budget,
             parameters[tableIndex].net_core_somaxconn,
             parameters[tableIndex].net_core_busy_poll,
             parameters[tableIndex].net_core_busy_read,
             parameters[tableIndex].net_core_rmem_max,
             parameters[tableIndex].net_core_wmem_max,
             parameters[tableIndex].net_core_rmem_default,
             parameters[tableIndex].net_core_wmem_default);
    snprintf(netIPCommand, MAX_COMMAND_LENGTH,
             "sysctl -w net.ipv4.tcp_fastopen=%hu "
             "net.ipv4.tcp_low_latency=%hu "
             "net.ipv4.tcp_sack=%hu "
             "net.ipv4.tcp_rmem='%lu %lu %lu' "
             "net.ipv4.tcp_wmem='%lu %lu %lu' "
             "net.ipv4.tcp_max_syn_backlog=%u "
             "net.ipv4.tcp_tw_reuse=%hu "
             /* "net.ipv4.tcp_tw_recycle=%hu " no longer available */
             "net.ipv4.tcp_timestamps=%hu "
             "net.ipv4.tcp_syn_retries=%u ",
             parameters[tableIndex].tcp_fastopen,
             parameters[tableIndex].tcp_low_latency,
             parameters[tableIndex].tcp_sack, parameters[tableIndex].tcp_rmem0,
             parameters[tableIndex].tcp_rmem1, parameters[tableIndex].tcp_rmem2,
             parameters[tableIndex].tcp_wmem0, parameters[tableIndex].tcp_wmem1,
             parameters[tableIndex].tcp_wmem2,
             parameters[tableIndex].tcp_max_syn_backlog,
             parameters[tableIndex].tcp_tw_reuse,
             /* parameters[tableIndex].tcp_tw_recycle, no longer available */
             parameters[tableIndex].tcp_timestamps,
             parameters[tableIndex].tcp_syn_retries);
    snprintf(ringSizeCommand, MAX_COMMAND_LENGTH, "ethtool -G %s rx %hu tx %hu",
             interfaceName, parameters[tableIndex].rx_ring_size,
             parameters[tableIndex].tx_ring_size);
    snprintf(offloadCommand, MAX_COMMAND_LENGTH,
             "ethtool -K %s "
             "rx %s "
             "tx %s "
             "gso %s "
             "tso %s "
             "gro %s "
             "lro %s "
             "rxvlan %s "
             "txvlan %s "
             "rxhash %s ",
             interfaceName, onOrOff(parameters[tableIndex].rx_checksum_offload),
             onOrOff(parameters[tableIndex].tx_checksum_offload),
             onOrOff(parameters[tableIndex].general_segmentation_offload),
             onOrOff(parameters[tableIndex].tcp_segmentation_offload),
             onOrOff(parameters[tableIndex].general_receive_offload),
             onOrOff(parameters[tableIndex].large_receive_offload),
             onOrOff(parameters[tableIndex].rx_vlan_offload),
             onOrOff(parameters[tableIndex].tx_vlan_offload),
             onOrOff(parameters[tableIndex].rx_hash));

    write_log("\033[1;32m"); // Set the text to the color green
    write_log("$ %s\n", netCoreCommand);
    write_log("$ %s\n", netIPCommand);
    write_log("$ %s\n", ringSizeCommand);
    write_log("$ %s\n", offloadCommand);
#ifdef APPLY_CHANGES
    system(netCoreCommand);
    system(netIPCommand);
    system(ringSizeCommand);
    system(offloadCommand);
#endif
    write_log("\033[0m"); // Resets the text to default
}

static inline void networkPrintReport() {
    write_log("\033[1;32m"); // Set the text to the color green
    printf("\n\nTotal inference loops: %ld, Matches=%ld, Success Rate=%f%%, "
           "Min Transfer Rate = %ld, Max Transfer Rate = %ld\n",
           total, matches, ((double)matches / (double)total) * 100,
           getMinTransferRate(), getMaxTransferRate());
    write_log("\033[0m"); // Resets the text to default color
}

void networkLiveTraining(char *inputFileName) {
    int origTableIndex = 0;
    while (_all_values->validValues < _all_values->totalLength) {
        unsigned long transferRate = getTransferRate();
        uint64_t dropRate = getDropRate();
        uint64_t errorsRate = getErrorsRate();
        uint64_t fifoErrorsRate = getFifoErrorsRate();

        if (transferRate == 0 && dropRate == 0 && errorsRate == 0 &&
            fifoErrorsRate == 0)
            continue;

        double weightedValue =
            calculateWeightedValue(transferRate, dropRate, errorsRate,
                                   fifoErrorsRate, _weights, _bias);
        unsigned short zeros =
            digits(weightedValue) * _network_app_settings->accuracy;
        double epsilon =
            calculateEpsilon(zeros, _network_app_settings->accuracy);
        double toleranceValue = calculateTolerance(
            weightedValue, epsilon, _network_app_settings->approx_function);

        unsigned int closestIndex = 0;
        if ((origTableIndex = binarySearchWithTolerance(
                 _all_values->parameters, 0, _all_values->validValues,
                 weightedValue, toleranceValue, &closestIndex, _weights,
                 _bias)) != -1) {

            if (addData(_all_values, origTableIndex, transferRate, epsilon,
                        TRUE) == RET_FAIL)
                write_adv_log("Duplicate value (%ld) not being inserted into "
                              "the table.\n",
                              transferRate);

            if (_all_values->validValues % _network_app_settings->saving_loop ==
                0)
                saveTrainedDataToFile(_all_values, inputFileName);

        } else {
            write_adv_log("Could not find a match for value %ld; the closest "
                          "transfer rate "
                          "value at index %u is %ld. Actual delta = %ld where "
                          "tolerance is = %lf\n",
                          transferRate, closestIndex,
                          _all_values->parameters[closestIndex].transfer_rate,
                          _all_values->parameters[closestIndex].transfer_rate -
                              transferRate,
                          toleranceValue);
        }
        usleep(USEC_IN_SEC * _network_app_settings->inference_loop_period);
    }
}

void networkRunInference() {

    int i = 0;
    double prevWeightedValue = 0L;
    unsigned long timePassedSinceLastChanges = 0;
    unsigned short printAdviseMsg = 0;

    write_log("Inference running: %f...\n",
              _network_app_settings->inference_loop_period);

    while (1) {
        usleep(USEC_IN_SEC * _network_app_settings->inference_loop_period);

        timePassedSinceLastChanges +=
            (USEC_IN_SEC * _network_app_settings->inference_loop_period);

        unsigned long transferRate = getTransferRate();
        uint64_t dropRate = getDropRate();
        uint64_t errorsRate = getErrorsRate();
        uint64_t fifoErrorsRate = getFifoErrorsRate();

        if (transferRate == 0 && dropRate == 0 && errorsRate == 0 &&
            fifoErrorsRate == 0)
            continue;

        double weightedValue =
            calculateWeightedValue(transferRate, dropRate, errorsRate,
                                   fifoErrorsRate, _weights, _bias);
        unsigned short zeros =
            digits(weightedValue) * _network_app_settings->accuracy;
        double epsilon =
            calculateEpsilon(zeros, _network_app_settings->accuracy);
        double toleranceValue = calculateTolerance(
            weightedValue, epsilon, _network_app_settings->approx_function);
        double weightedValueDiff = fabs(weightedValue - prevWeightedValue);

        printAdviseMsg++;

        if (weightedValueDiff < toleranceValue) {
            if (printAdviseMsg % 10 == 0) {
                write_log("Avoiding too little delta %lf (tolerance = %lf)\n",
                          weightedValueDiff, toleranceValue);
                printAdviseMsg = 0;
            }
            continue;
        }

        if (weightedValue < prevWeightedValue &&
            (timePassedSinceLastChanges <=
             _network_app_settings->grace_period * MINUTES_IN_USEC)) {
            if (printAdviseMsg % 10 == 0) {
                write_log(
                    "Skipping changing values: weightedValue=%lf, "
                    "prevWeightedValue=%lf, timePassedSinceLastChanges=%ld\n",
                    weightedValue, prevWeightedValue,
                    timePassedSinceLastChanges);
                printAdviseMsg = 0;
            }
            continue;
        }

        write_log("Searching for: weightedValue=%lf, prevWeightedValue=%lf "
                  "with a tolerance of=%lf\n",
                  weightedValue, prevWeightedValue, toleranceValue);

        if (total % 10 == 0) {
            networkPrintReport();
        }

        unsigned int closestIndex = 0;

        if ((i = binarySearchWithTolerance(
                 _all_values->parameters, 0, _all_values->validValues,
                 weightedValue, toleranceValue, &closestIndex, _weights,
                 _bias)) != -1) {
            matches++;

            write_adv_log("Match found for value %ld; actual delta = %ld where "
                          "tolerance is = %lf\n",
                          weightedValue,
                          _all_values->parameters[closestIndex].transfer_rate,
                          _all_values->parameters[closestIndex].transfer_rate -
                              transferRate,
                          toleranceValue);

            applySettings(stats_input_params.monitored_interface,
                          _all_values->parameters, i);

            timePassedSinceLastChanges = 0;
            printAdviseMsg = 0;
            prevWeightedValue = weightedValue;
        } else {
            write_log("Could not find a match for value %ld; the closest "
                      "transfer rate "
                      "value at index %u is %ld. Actual delta = %ld where "
                      "tolerance is = %lf\n",
                      transferRate, closestIndex,
                      _all_values->parameters[closestIndex].transfer_rate,
                      _all_values->parameters[closestIndex].transfer_rate -
                          transferRate,
                      toleranceValue);
        }

        total++;
    }
}

void networkInit(char *interfaceName, app_settings_t *network_app_settings,
                 tuning_params_t *network_settings,
                 weights_reference_t *weights, all_values_t *all_values,
                 double bias) {
    pthread_mutex_init(&tableWriteLock, NULL);

    _network_app_settings = network_app_settings;
    _network_settings = network_settings;
    _all_values = all_values;
    _weights = weights;
    _bias = bias;

    memcpy(stats_input_params.monitored_interface, interfaceName,
           MAX_INTERFACE_NAME_LENGTH);
    stats_input_params.stats_collection_period =
        _network_app_settings->stats_collection_period;

    pthread_create(&netStatsThreadId, NULL, collectStats, &stats_input_params);
    pthread_create(&cpuStatsThreadId, NULL, collectCpuStats, NULL);
}

void networkRunTraining(char *inputFileName) {
    int origTableIndex = 0;

    while (1) {
        unsigned long transferRate =
            generatePseudoRandomTransferRate(_all_values);
        uint64_t dropRate = getDropRate();
        uint64_t errorsRate = getErrorsRate();
        uint64_t fifoErrorsRate = getFifoErrorsRate();

        assert(transferRate != 0);

        double weightedValue =
            calculateWeightedValue(transferRate, dropRate, errorsRate,
                                   fifoErrorsRate, _weights, _bias);

        unsigned short zeros =
            digits(weightedValue) * _network_app_settings->accuracy;
        double epsilon =
            calculateEpsilon(zeros, _network_app_settings->accuracy);

        double toleranceValue = calculateTolerance(
            weightedValue, epsilon, _network_app_settings->approx_function);

        unsigned int closestIndex = 0;
        if (pthread_mutex_lock(&tableWriteLock) == 0) {
            if (_all_values->validValues >= _all_values->totalLength) {
                pthread_mutex_unlock(&tableWriteLock);
                break;
            } else if ((origTableIndex = binarySearchWithTolerance(
                            _all_values->parameters, 0,
                            _all_values->validValues, weightedValue,
                            toleranceValue, &closestIndex, _weights, _bias)) !=
                       -1) {
                if (addData(_all_values, origTableIndex, transferRate, epsilon,
                            FALSE) == RET_FAIL) {
                    write_adv_log("Duplicate value (%ld) not being inserted "
                                  "into the table.\n",
                                  transferRate);
                }

                if (_all_values->validValues %
                        _network_app_settings->saving_loop ==
                    0)
                    saveTrainedDataToFile(_all_values, inputFileName);

            } else {
                write_adv_log(
                    "Could not find a match for value %ld; the closest "
                    "transfer rate "
                    "value at index %u is %ld. Actual delta = %ld where "
                    "tolerance is = %lf\n",
                    transferRate, closestIndex,
                    _all_values->parameters[closestIndex].transfer_rate,
                    _all_values->parameters[closestIndex].transfer_rate -
                        transferRate,
                    toleranceValue);
            }
            pthread_mutex_unlock(&tableWriteLock);
        }
    }
    return;
}

void networkDestroy() { pthread_mutex_destroy(&tableWriteLock); }

plugin_t me = {.active = TRUE,
               .name = "NETWORK_PLUGIN",
               .version = 0.1,
               .init = networkInit,
               .destroy = networkDestroy,
               .inference = networkRunInference,
               .training = networkRunTraining,
               .livetraining = networkLiveTraining,
               .print_report = networkPrintReport};

plugin_t *registerMe() {
    /* HERE call the function to add this plugin to the list of registered
     * plugins */
    write_log("Registering plugin %s ver. %g\n", me.name, me.version);

    return &me;
}
