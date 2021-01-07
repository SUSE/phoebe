// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#ifndef _FILEHELPER_H_
#define _FILEHELPER_H_

#include "types.h"

#include <stdio.h>

int addDataFromFile(tuning_params_t *srcParams, all_values_t *destParams);

int loadFile(FILE *pFile, unsigned int fileRows,
             all_values_t *reference_values);
void loadValues(char *line, long lineno, all_values_t *reference_values);
void loadValuesFromUnordedFile(char *line, all_values_t *reference_values);

/**
 * @brief Allocates enough memory in reference_values->parameters to hold all
 *     lines from the file stored in pFile.
 *
 * @warning This function calls exit if a call to `malloc(3)` fails.
 *
 * @return The number of lines in pFile or @ref RET_FAIL if the file cannot be
 *     read or no file is open.
 */
int allocateMemoryBasedOnInputAndMaxLearningValues(
    FILE *pFile, const app_settings_t *app_settings,
    all_values_t *reference_values);

int readSettingsFromJsonFile(char *settingsFileName,
                             app_settings_t *app_settings,
                             weights_reference_t *weights, double *bias);

unsigned int saveTrainedDataToFile(all_values_t *values,
                                   char *path_with_filename);

int writeHeader(FILE *fp);
int writeRow(FILE *fp, tuning_params_t *row);

#endif
