// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#ifndef _ALGORITHMIC_H_
#define _ALGORITHMIC_H_

#include "types.h"

int binarySearchWithTolerance(tuning_params_t *parameters, int l, int r,
                              double weighted_value, double tolerance,
                              unsigned int *closestIndex,
                              weights_reference_t *weights, double bias);

int addData(all_values_t *values, unsigned int origTableIndex,
            unsigned long int transferRate, double epsilon,
            unsigned short live_mode);

unsigned long generatePseudoRandomTransferRate(all_values_t *values);

#endif
