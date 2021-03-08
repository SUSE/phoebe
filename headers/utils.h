// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdarg.h>

#include "types.h"

#define RX_RING_SIZE 0
#define TX_RING_SIZE 1
#define NET_CORE_NETDEV_MAX_BACKLOG 2
#define NET_CORE_NETDEV_BUDGET 3
#define NET_CORE_SOMAXCONN 4
#define NET_CORE_RMEM_MAX 5
#define NET_CORE_RMEM_DEFAULT 6
#define NET_CORE_WMEM_MAX 7
#define NET_CORE_WMEM_DEFAULT 8
#define TCP_MAX_SYN_BACKLOG 9
#define TCP_RMEM_0 10
#define TCP_RMEM_1 11
#define TCP_RMEM_2 12
#define TCP_WMEM_0 13
#define TCP_WMEM_1 14
#define TCP_WMEM_2 15

int _positionToInserElementAt(unsigned long transferRate,
                              all_values_t *destParams, unsigned int *pivot);

void _repositionElements(all_values_t *destParams, unsigned int pivot);

double adjustValue(unsigned int pivot, double pivotValue, unsigned int refIndex,
                   double pivotPrevValue, double pivotNextValue, double epsilon,
                   unsigned int approx_function);
double calcDerivedValue(tuning_params_t *parameters, unsigned int refIndex,
                        unsigned int transferRate, unsigned int pivot,
                        unsigned int field, double epsilon,
                        unsigned int approx_function);
double calculateWeightedValue(unsigned long currentTransferRate,
                              uint64_t dropRate, uint64_t errorsRate,
                              uint64_t fifoErrorsRate,
                              weights_reference_t *weights, double bias);
double calculateEpsilon(double weightedValue, double accuracy);
double calculateTolerance(double weightedValue, double eps,
                          unsigned int approx_function);

void retrieveNumberOfCores(unsigned int *threads);

void printTable(all_values_t *values);

void readSystemSettings(tuning_params_t *systemSettings);

unsigned short digits(unsigned long int num);

char *onOrOff(unsigned int input);

static inline void write_log(const char *format, ...) {
#ifdef PRINT_MSGS
    va_list args;
    va_start(args, format);

    vprintf(format, args);
    va_end(args);
#endif
}

static inline void write_adv_log(const char *format, ...) {
#ifdef PRINT_ADV_MSGS
    va_list args;
    va_start(args, format);

    vprintf(format, args);
    va_end(args);
#endif
}

#endif
