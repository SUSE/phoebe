// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#include <assert.h>
#include <errno.h>
#include <libgen.h>
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

#include <json-c/json.h>

#include "algorithmic.h"
#include "filehelper.h"
#include "phoebe.h"
#include "utils.h"

int addDataFromFile(tuning_params_t *srcParams, all_values_t *destParams) {
    unsigned int pivot;

    if (_positionToInserElementAt(srcParams->transfer_rate, destParams,
                                  &pivot) == RET_FAIL)
        return RET_FAIL;

    _repositionElements(destParams, pivot);

    memcpy(&destParams->parameters[pivot], srcParams, sizeof(tuning_params_t));

    destParams->validValues++;

    return RET_OK;
}

unsigned int saveTrainedDataToFile(all_values_t *values,
                                   char *path_with_filename) {

    FILE *fp;
    char outputFileName[MAX_FILENAME_LENGTH];
    char file[MAX_FILENAME_LENGTH];
    unsigned int i;

    memset(outputFileName, 0, MAX_FILENAME_LENGTH);
    memset(file, 0, MAX_FILENAME_LENGTH);

    memcpy(file, path_with_filename, MAX_FILENAME_LENGTH);

    char *filename = basename(file);
    char *directory = dirname(file);

    snprintf(outputFileName, MAX_FILENAME_LENGTH, "%s/%s_%s", directory,
             strtok(filename, "."), APPEND_TO_FILE_NAME);

    write_adv_log("Output filename: %s\n", outputFileName);
    if ((fp = fopen(outputFileName, "w")) == NULL)
        return -1;

    writeHeader(fp);

    unsigned long prevTransferRate = 0;
    unsigned int totalFileEntries = 0;

    /* 46 parameters */
    for (i = 0; i < values->validValues; i++) {
        /* Do not write any potential duplicates to the final file */
        if (values->parameters[i].transfer_rate == prevTransferRate)
            continue;

        ++totalFileEntries;

        writeRow(fp, &values->parameters[i]);

        prevTransferRate = values->parameters[i].transfer_rate;
    }

    fclose(fp);
    return totalFileEntries;
}

int writeHeader(FILE *fp) {
    return fprintf(
        fp,
        "transfer_rate,drop_rate,errors_rate,fifo_errors_rate,"
        "cpu_usage_percentage,rx_ring_size,tx_ring_size,cores,governor,"
        "cpu_freq,io_scheduler,task_scheduler,kernel.sched_min_granularity_ns,"
        "kernel.sched_wakeup_granularity_ns,kernel.sched_migration_cost_ns,"
        "kernel.numa_balancing,kernel.pid_max,"
        "net.core.netdev_max_backlog,net.core.netdev_budget,"
        "net.core.somaxconn,net.core.busy_poll,net.core.busy_read,"
        "net.core.rmem_max,net.core.wmem_max,net.core.rmem_default,"
        "net.core.wmem_default,tcp_fastopen,tcp_lowlatency,tcp_sack,"
        "tcp_rmem[0],tcp_rmem[1],tcp_rmem[2],"
        "tcp_wmem[0],tcp_wmem[1],tcp_wmem[2],"
        "tcp_max_syn_backlog,tcp_tw_reuse,tcp_tw_recycle,"
        "tcp_timestamps,tcp_syn_retries,"
        "rx_interrupt_coalesce_usecs,rx_interrupt_max_coalesce_frames,"
        "tx_interrupt_coalesce_usecs,tx_interrupt_max_coalesce_frames,"
        "rx_checksum_offload,tx_checksum_offload,"
        "general_segmentation_offload,tcp_segmentation_offload,"
        "general_receive_offload,large_receive_offload,"
        "rx_vlan_offload,tx_vlan_offload,rx_hash\n");
}

int writeRow(FILE *fp, tuning_params_t *row) {
    return fprintf(
        fp,
        "%lu,%lu,%lu,%lu,%lf,%hu,%hu,%hu,%hu,%u,%hu,%hu,%u,"
        "%u,%u,%hu,%u,"
        "%u,%u,%u,%hu,%hu,%lu,%lu,%lu,%lu,%hu,%hu,%hu,%lu,"
        "%lu,%lu,%lu,%lu,%lu,"
        "%u,%hu,%hu,%hu,%u,%hu,%hu,%hu,%hu,"
        "%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu\n",
        row->transfer_rate, row->drop_rate, row->errors_rate,
        row->fifo_errors_rate, row->cpu_usage_percentage, row->rx_ring_size,
        row->tx_ring_size, row->cores, row->governor, row->cpu_speed,
        row->io_scheduler, row->task_scheduler,
        row->kernel_sched_min_granularity_ns,
        row->kernel_sched_wakeup_granularity_ns,
        row->kernel_sched_migration_cost_ns, row->kernel_numa_balancing,
        row->kernel_pid_max, row->net_core_netdev_max_backlog,
        row->net_core_netdev_budget, row->net_core_somaxconn,
        row->net_core_busy_poll, row->net_core_busy_read,
        row->net_core_rmem_max, row->net_core_wmem_max,
        row->net_core_rmem_default, row->net_core_wmem_default,
        row->tcp_fastopen, row->tcp_low_latency, row->tcp_sack, row->tcp_rmem0,
        row->tcp_rmem1, row->tcp_rmem2, row->tcp_wmem0, row->tcp_wmem1,
        row->tcp_wmem2, row->tcp_max_syn_backlog, row->tcp_tw_reuse,
        row->tcp_tw_recycle, row->tcp_timestamps, row->tcp_syn_retries,
        row->rx_interrupt_coalesce_usecs, row->rx_interrupt_max_coalesce_frames,
        row->tx_interrupt_coalesce_usecs, row->tx_interrupt_max_coalesce_frames,
        row->rx_checksum_offload, row->tx_checksum_offload,
        row->general_segmentation_offload, row->tcp_segmentation_offload,
        row->general_receive_offload, row->large_receive_offload,
        row->rx_vlan_offload, row->tx_vlan_offload, row->rx_hash);
}
int readSettingsFromJsonFile(char *settingsFileName, app_settings_t *settings,
                             weights_reference_t *weights, double *bias) {
    FILE *fp;
    size_t read_size;
    char buffer[BUFFER_SIZE + 1];

    struct json_object *parsed_json;
    struct json_object *transfer_rate_weight;
    struct json_object *drop_rate_weight;
    struct json_object *errors_rate_weight;
    struct json_object *fifo_errors_rate_weight;
    struct json_object *max_learning_values;
    struct json_object *saving_loop;
    struct json_object *accuracy;
    struct json_object *approx_function;
    struct json_object *grace_period;
    struct json_object *stats_collection_period;
    struct json_object *inference_loop_period;
    struct json_object *plugins_path;

    struct json_object *app_settings;
    struct json_object *weights_settings;
    struct json_object *bias_settings;

    if ((fp = fopen(settingsFileName, "r")) == NULL)
        return RET_FAIL;

    read_size = fread(buffer, 1, BUFFER_SIZE, fp);
    buffer[read_size] = '\0';
    fclose(fp);

    parsed_json = json_tokener_parse(buffer);

    json_object_object_get_ex(parsed_json, "app_settings", &app_settings);
    json_object_object_get_ex(app_settings, "max_learning_values",
                              &max_learning_values);
    json_object_object_get_ex(app_settings, "saving_loop", &saving_loop);
    json_object_object_get_ex(app_settings, "accuracy", &accuracy);
    json_object_object_get_ex(app_settings, "approx_function",
                              &approx_function);
    json_object_object_get_ex(app_settings, "grace_period", &grace_period);
    json_object_object_get_ex(app_settings, "stats_collection_period",
                              &stats_collection_period);
    json_object_object_get_ex(app_settings, "inference_loop_period",
                              &inference_loop_period);
    json_object_object_get_ex(app_settings, "plugins_path", &plugins_path);

    settings->max_learning_values = json_object_get_int(max_learning_values);
    settings->saving_loop = json_object_get_int(saving_loop);
    settings->accuracy = json_object_get_double(accuracy);
    settings->approx_function = json_object_get_int(approx_function);
    settings->grace_period = json_object_get_int(grace_period);
    settings->stats_collection_period =
        json_object_get_double(stats_collection_period);
    settings->inference_loop_period =
        json_object_get_double(inference_loop_period);

    assert(sizeof(settings->plugins_path) >
           (long unsigned int)json_object_get_string_len(plugins_path));
    memcpy(settings->plugins_path, json_object_get_string(plugins_path),
           json_object_get_string_len(plugins_path) + 1);

    write_adv_log("settings->plugins_path: %s\n", settings->plugins_path);

    write_adv_log("settings->max_learning_values: %d\n",
                  settings->max_learning_values);
    if (settings->saving_loop < 1000) {
        write_log(
            "Saving loop too little; changing it to the default value of 1000");
        settings->saving_loop = 1000;
    } else
        write_adv_log("settings->saving_loop: %d\n", settings->saving_loop);

    if (settings->stats_collection_period <= 0.0) {
        write_log("Stats colletion loop value is too small; changing to the "
                  "default value of 1");
        settings->stats_collection_period = 1;
    } else
        write_adv_log("settings->stats_collection_period: %u",
                      settings->stats_collection_period);

    write_adv_log("settings->accuracy: %f\n", settings->accuracy);
    write_adv_log("settings->approx_function: %d\n", settings->approx_function);
    write_adv_log("settings->grace_period: %d\n", settings->grace_period);

    json_object_object_get_ex(parsed_json, "weights", &weights_settings);
    json_object_object_get_ex(weights_settings, "transfer_rate_weight",
                              &transfer_rate_weight);
    json_object_object_get_ex(weights_settings, "drop_rate_weight",
                              &drop_rate_weight);
    json_object_object_get_ex(weights_settings, "errors_rate_weight",
                              &errors_rate_weight);
    json_object_object_get_ex(weights_settings, "fifo_errors_rate_weight",
                              &fifo_errors_rate_weight);

    weights->transfer_rate_weight =
        json_object_get_double(transfer_rate_weight);
    weights->drop_rate_weight = json_object_get_double(drop_rate_weight);
    weights->errors_rate_weight = json_object_get_double(errors_rate_weight);
    weights->fifo_errors_rate_weight =
        json_object_get_double(fifo_errors_rate_weight);

    json_object_object_get_ex(parsed_json, "bias", &bias_settings);
    *bias = json_object_get_double(bias_settings);

    write_adv_log("transfer_rate_weight: %lf\n", weights->transfer_rate_weight);
    write_adv_log("drop_rate_weight: %lf\n", weights->drop_rate_weight);
    write_adv_log("errors_rate_weight: %lf\n", weights->errors_rate_weight);
    write_adv_log("fifo_errors_rate_weight: %lf\n",
                  weights->fifo_errors_rate_weight);

    json_object_put(parsed_json);

    if ((weights->transfer_rate_weight + weights->drop_rate_weight +
         weights->errors_rate_weight + weights->fifo_errors_rate_weight) > 1) {
        write_log("The sum of all weights is bigger than 1.\n");
        return RET_FAIL;
    }

    if (settings->approx_function > 4) {
        write_log("The settings->approx_function is invalid.\n");
        return RET_FAIL;
    }

    return RET_OK;
}

int allocateMemoryBasedOnInputAndMaxLearningValues(
    FILE *pFile, const app_settings_t *app_settings,
    all_values_t *reference_values) {
    char sInputBuf[BUFFER_SIZE];
    long lineno = 0L;

    if (pFile == NULL) {
        return RET_FAIL;
    }

    // load line into static buffer
    if (fgets(sInputBuf, BUFFER_SIZE - 1, pFile) == NULL) {
        return RET_FAIL;
    }

    while (!feof(pFile)) {
        // load line into static buffer
        if (fgets(sInputBuf, BUFFER_SIZE - 1, pFile) == NULL) {
            break;
        }

        ++lineno;
    }

    reference_values->totalLength = app_settings->max_learning_values + lineno;
    reference_values->validValues = 0;
    reference_values->parameters =
        calloc(reference_values->totalLength, sizeof(tuning_params_t));

    if (reference_values->parameters == NULL) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }

    return lineno;
}

inline void loadValuesFromUnordedFile(char *line, all_values_t *allValues) {
    tuning_params_t param;
    int valueCount = 0;

    if (line == NULL)
        return;

    valueCount = sscanf(
        line,
        "%lu, %lu, %lu, %lu, %lf, %hu, %hu, %hu, %hu, %u, %hu, %hu, %u, "
        "%u, %u, %hu, %u, "
        "%u, %u, %u, %hu, %hu, %lu, %lu, %lu, %lu, %hu, %hu, %hu, %lu, "
        "%lu, %lu, %lu, %lu, %lu, "
        "%u, %hu, %hu, %hu, %u, %hu, %hu, %hu, %hu, %hu, %hu, %hu, %hu, "
        "%hu, %hu, %hu, %hu, %hu",
        &param.transfer_rate, &param.drop_rate, &param.errors_rate,
        &param.fifo_errors_rate, &param.cpu_usage_percentage,
        &param.rx_ring_size, &param.tx_ring_size, &param.cores, &param.governor,
        &param.cpu_speed, &param.io_scheduler, &param.task_scheduler,
        &param.kernel_sched_min_granularity_ns,
        &param.kernel_sched_wakeup_granularity_ns,
        &param.kernel_sched_migration_cost_ns, &param.kernel_numa_balancing,
        &param.kernel_pid_max, &param.net_core_netdev_max_backlog,
        &param.net_core_netdev_budget, &param.net_core_somaxconn,
        &param.net_core_busy_poll, &param.net_core_busy_read,
        &param.net_core_rmem_max, &param.net_core_wmem_max,
        &param.net_core_rmem_default, &param.net_core_wmem_default,
        &param.tcp_fastopen, &param.tcp_low_latency, &param.tcp_sack,
        &param.tcp_rmem0, &param.tcp_rmem1, &param.tcp_rmem2, &param.tcp_wmem0,
        &param.tcp_wmem1, &param.tcp_wmem2, &param.tcp_max_syn_backlog,
        &param.tcp_tw_reuse, &param.tcp_tw_recycle, &param.tcp_timestamps,
        &param.tcp_syn_retries, &param.rx_interrupt_coalesce_usecs,
        &param.rx_interrupt_max_coalesce_frames,
        &param.tx_interrupt_coalesce_usecs,
        &param.tx_interrupt_max_coalesce_frames, &param.rx_checksum_offload,
        &param.tx_checksum_offload, &param.general_segmentation_offload,
        &param.tcp_segmentation_offload, &param.general_receive_offload,
        &param.large_receive_offload, &param.rx_vlan_offload,
        &param.tx_vlan_offload, &param.rx_hash);
    if (valueCount != NUM_TUNING_PARAMS) {
        fprintf(stderr, "Expecting %d values but only got %d in line \"%s\"",
                NUM_TUNING_PARAMS, valueCount, line);
        exit(RET_FAIL);
    }

    addDataFromFile(&param, allValues);

    return;
}

inline void loadValues(char *line, long lineno, all_values_t *allValues) {
    int valueCount = 0;

    if (line == NULL)
        return;

    valueCount = sscanf(
        line,
        "%lu, %lu, %lu, %lu, %lf, %hu, %hu, %hu, %hu, %u, %hu, %hu, %u, "
        "%u, %u, %hu, %u, "
        "%u, %u, %u, %hu, %hu, %lu, %lu, %lu, %lu, %hu, %hu, %hu, %lu, "
        "%lu, %lu, %lu, %lu, %lu, "
        "%u, %hu, %hu, %hu, %u, %hu, %hu, %hu, %hu, %hu, %hu, %hu, %hu, "
        "%hu, %hu, %hu, %hu, %hu",
        &allValues->parameters[lineno].transfer_rate,
        &allValues->parameters[lineno].drop_rate,
        &allValues->parameters[lineno].errors_rate,
        &allValues->parameters[lineno].fifo_errors_rate,
        &allValues->parameters[lineno].cpu_usage_percentage,
        &allValues->parameters[lineno].rx_ring_size,
        &allValues->parameters[lineno].tx_ring_size,
        &allValues->parameters[lineno].cores,
        &allValues->parameters[lineno].governor,
        &allValues->parameters[lineno].cpu_speed,
        &allValues->parameters[lineno].io_scheduler,
        &allValues->parameters[lineno].task_scheduler,
        &allValues->parameters[lineno].kernel_sched_min_granularity_ns,
        &allValues->parameters[lineno].kernel_sched_wakeup_granularity_ns,
        &allValues->parameters[lineno].kernel_sched_migration_cost_ns,
        &allValues->parameters[lineno].kernel_numa_balancing,
        &allValues->parameters[lineno].kernel_pid_max,
        &allValues->parameters[lineno].net_core_netdev_max_backlog,
        &allValues->parameters[lineno].net_core_netdev_budget,
        &allValues->parameters[lineno].net_core_somaxconn,
        &allValues->parameters[lineno].net_core_busy_poll,
        &allValues->parameters[lineno].net_core_busy_read,
        &allValues->parameters[lineno].net_core_rmem_max,
        &allValues->parameters[lineno].net_core_wmem_max,
        &allValues->parameters[lineno].net_core_rmem_default,
        &allValues->parameters[lineno].net_core_wmem_default,
        &allValues->parameters[lineno].tcp_fastopen,
        &allValues->parameters[lineno].tcp_low_latency,
        &allValues->parameters[lineno].tcp_sack,
        &allValues->parameters[lineno].tcp_rmem0,
        &allValues->parameters[lineno].tcp_rmem1,
        &allValues->parameters[lineno].tcp_rmem2,
        &allValues->parameters[lineno].tcp_wmem0,
        &allValues->parameters[lineno].tcp_wmem1,
        &allValues->parameters[lineno].tcp_wmem2,
        &allValues->parameters[lineno].tcp_max_syn_backlog,
        &allValues->parameters[lineno].tcp_tw_reuse,
        &allValues->parameters[lineno].tcp_tw_recycle,
        &allValues->parameters[lineno].tcp_timestamps,
        &allValues->parameters[lineno].tcp_syn_retries,
        &allValues->parameters[lineno].rx_interrupt_coalesce_usecs,
        &allValues->parameters[lineno].rx_interrupt_max_coalesce_frames,
        &allValues->parameters[lineno].tx_interrupt_coalesce_usecs,
        &allValues->parameters[lineno].tx_interrupt_max_coalesce_frames,
        &allValues->parameters[lineno].rx_checksum_offload,
        &allValues->parameters[lineno].tx_checksum_offload,
        &allValues->parameters[lineno].general_segmentation_offload,
        &allValues->parameters[lineno].tcp_segmentation_offload,
        &allValues->parameters[lineno].general_receive_offload,
        &allValues->parameters[lineno].large_receive_offload,
        &allValues->parameters[lineno].rx_vlan_offload,
        &allValues->parameters[lineno].tx_vlan_offload,
        &allValues->parameters[lineno].rx_hash);
    if (valueCount != NUM_TUNING_PARAMS) {
        fprintf(stderr,
                "Expecting %d values but only got %d in line number %ld",
                NUM_TUNING_PARAMS, valueCount, lineno);
        exit(RET_FAIL);
    }

    allValues->validValues++;

    return;
}

int loadFile(FILE *pFile, unsigned int fileRows,
             all_values_t *reference_values) {

    char sInputBuf[BUFFER_SIZE];
    long lineno = 0L;

    if (pFile == NULL)
        return RET_FAIL;

    // skip first spreadsheet row
    if (fgets(sInputBuf, BUFFER_SIZE - 1, pFile) == NULL)
        return RET_FAIL;

    while (!feof(pFile)) {
        // load line into static buffer
        if (fgets(sInputBuf, BUFFER_SIZE - 1, pFile) == NULL) {
            if (lineno == fileRows)
                return lineno;
            return RET_FAIL;
        }

        // jump over empty lines
        if (strlen(sInputBuf) == 0)
            continue;

        loadValues(sInputBuf, lineno, reference_values);

        ++lineno;

        if (lineno % 1000 == 0) {
            write_log(".");
            fflush(stdout);
        }
    }

    // all_values_t *allValues = getAllValues();
    // printTable(allValues);
    return RET_OK;
}
