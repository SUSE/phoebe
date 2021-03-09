// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#ifndef _NETWORK_PLUGIN_H_
#define _NETWORK_PLUGIN_H_

#include "types.h"

#define PLUGIN_NAME_LEN 32

typedef struct plugin_s {
    bool active;
    char name[PLUGIN_NAME_LEN];
    double version;
    void (*init)(char *, app_settings_t *, tuning_params_t *,
                 weights_reference_t *, all_values_t *, double);
    void (*inference)();
    void (*training)(char *inputFileName);
    void (*livetraining)(char *inputFileName);
    void (*destroy)();
    void (*print_report)();

} plugin_t;

#endif
