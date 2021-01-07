// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#ifndef _PHOEBE_H_
#define _PHOEBE_H_

#include <stdbool.h>
#include <stdio.h>

#include "types.h"

all_values_t *getAllValues();
weights_reference_t *getWeights();
app_settings_t *getAppSettings();
tuning_params_t *getSystemSettings();
double getBias();
char *getInputFileName();
char *getSettingsFileName();
unsigned short isInterrupted();

#endif
