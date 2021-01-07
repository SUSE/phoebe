// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#ifndef _STATS_H_
#define _STATS_H_

#include <stdint.h>

#include "types.h"

typedef struct stats_input_params_s {
    char monitored_interface[MAX_INTERFACE_NAME_LENGTH];
    double stats_collection_period;
} stats_input_param_t;

double calculateCpuBusyPercentage(cpu_stats_t *prev, cpu_stats_t *cur);
void readCpuStats(cpu_stats_t *stats);
void *collectCpuStats(void *arg);
int cpuGovernorIndex(const char *query);
void calculateInterfaceRatesPerSecond(if_stats_t *prev, if_stats_t *cur,
                                      if_rates_t *rates,
                                      double stats_collection_period);
void readStats(struct rtnl_link *link, if_stats_t *stats);
void *collectStats(void *stats_input_params);
struct ifreq *allocRingSizeRequest(const char *ifname);
void freeRingSizeRequest(struct ifreq *ifr);
void readRingSize(int sock, struct ifreq *ifr, if_ring_size_t *stats);
struct ifreq *allocGetCoalesceRequest(const char *ifname);
void freeGetCoalesceRequest(struct ifreq *ifr);
void readCoalesce(int sock, struct ifreq *ifr, if_coalesce_t *stats);
struct ifreq *allocGetOffloadsRequest(const char *ifname);
void freeGetOffloadsRequest(struct ifreq *ifr);
void readOffloads(int sock, struct ifreq *ifr, if_offloads_t *stats);

uint64_t getTransferRate();
uint64_t getDropRate();
uint64_t getErrorsRate();
uint64_t getFifoErrorsRate();

double getCpuBusyTime();

uint64_t getMinTransferRate();
uint64_t getMaxTransferRate();

#endif
