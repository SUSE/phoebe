// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#include <assert.h>
#include <errno.h>
#include <getopt.h>
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

#include <dirent.h>
#include <sys/types.h>

#include <dlfcn.h>

#include "algorithmic.h"
#include "filehelper.h"
#include "phoebe.h"
#include "plugins.h"
#include "stats.h"
#include "utils.h"

char inputFileName[MAX_FILENAME_LENGTH];
char settingsFileName[MAX_FILENAME_LENGTH];

static int fileRows = 0;

static all_values_t reference_values;
static weights_reference_t weights;
static double bias;
static app_settings_t app_settings;
static tuning_params_t system_settings;

static char operationalMode[MAX_COMMAND_LENGTH];
static char interfaceName[MAX_INTERFACE_NAME_LENGTH];

static plugin_t *plugins[MAX_PLUGINS];

void *runStdTraining(void *arg __attribute__((unused))) {
    unsigned short i;

    for (i = 0; i < MAX_PLUGINS; i++)
        plugins[i]->training(inputFileName);

    return NULL;
}

void runLiveTraining() {
    unsigned short i;

    for (i = 0; i < MAX_PLUGINS; i++)
        plugins[i]->livetraining(inputFileName);
}

void runInference() {
    unsigned short i;

    for (i = 0; i < MAX_PLUGINS; i++)
        plugins[i]->inference();
}

void handleSigint(int sig __attribute__((unused))) {
    unsigned short i;

    for (i = 0; i < MAX_PLUGINS; i++)
        plugins[i]->print_report();

    free(reference_values.parameters);
    fflush(stdout);
}

void handleSighup(int sig __attribute__((unused))) {
    app_settings_t tmpAppSettings;
    weights_reference_t tmpWeights;
    double tmpBias;

    if (readSettingsFromJsonFile(settingsFileName, &tmpAppSettings, &tmpWeights,
                                 &tmpBias) == RET_OK) {
        memcpy(&app_settings, &tmpAppSettings, sizeof(app_settings_t));
        memcpy(&weights, &tmpWeights, sizeof(weights_reference_t));
        bias = tmpBias;
    }
}

void printHelp(char *argv0) {
    printf("Usage: %s [options]\n\n", argv0);
    printf("\t-f, --csvfile\t\tinput-file path\n");
    printf("\t-i, --interface\t\tinterface to monitor\n");
    printf("\t-m, --mode\t\ttraining | live-training | inference\n");
    printf("\t-s, --settings\t\tJSON file for app-settings\n");
    printf("\t-?\t\t\tprints this help and exit\n");
    printf("\n\n");
}

int handleCommandLineArguments(int argc, char **argv) {
    int c;

    if (argc == 1) {
        printHelp(argv[0]);
        return RET_FAIL;
    }

    memset(operationalMode, 0, sizeof(operationalMode));
    memset(interfaceName, 0, sizeof(interfaceName));

    while (1) {
        static struct option _longOptions[] = {
            /* These options don’t set a flag.
               We distinguish them by their indices. */
            {"csvfile", required_argument, 0, 'f'},
            {"interface", required_argument, 0, 'i'},
            {"mode", required_argument, 0, 'm'},
            {"settings", optional_argument, 0, 's'},
            {0, 0, 0, 0}};
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long(argc, argv, "f:i:m:s:", _longOptions, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 0:
            /* If this option set a flag, do nothing else now. */
            if (_longOptions[option_index].flag != 0)
                break;
            printf("option %s", _longOptions[option_index].name);
            if (optarg)
                printf(" with arg %s", optarg);
            printf("\n");
            break;

        case 'i':
            /* TODO: validate interface; good enough now for POC */
            memcpy(interfaceName, optarg, MAX_INTERFACE_NAME_LENGTH);
            break;

        case 'm': {
            memcpy(operationalMode, optarg, strlen(optarg));

            if (strncmp(operationalMode, "training", strlen("training")) != 0 &&
                strncmp(operationalMode, "live-training",
                        strlen("live-training")) != 0 &&
                strncmp(operationalMode, "inference", strlen("inference")) !=
                    0) {
                printHelp(argv[0]);
                return RET_FAIL;
            }
        } break;

        case 'f': {
            memset(inputFileName, 0, MAX_FILENAME_LENGTH);
            memcpy(inputFileName, optarg, strlen(optarg));
        } break;

        case 's': {
            memset(settingsFileName, 0, MAX_FILENAME_LENGTH);
            if (optarg != NULL)
                memcpy(settingsFileName, optarg, strlen(optarg));
        } break;

        case '?':
            printHelp(argv[0]);
            return RET_FAIL;

        default:
            printHelp(argv[0]);
            return RET_FAIL;
        }
    }
    /* Print any remaining command line arguments (not options). */
    if (optind < argc) {
        printHelp(argv[0]);
        return RET_FAIL;
    }

    return RET_OK;
}

int endsWith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

/**
 * @brief Registers all plugins found in the plugins_path folder.
 *
 * @return The number of plugins that were loaded.
 */
int registerAllPlugins() {
    int registered_plugin_count = 0;
    void *handle;
    char *error;

    char resolved_path[MAX_FILENAME_LENGTH * 2 + 1];
    DIR *d;
    struct dirent *dir;

    d = opendir(app_settings.plugins_path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (endsWith(dir->d_name, ".so")) {
                sprintf(resolved_path, "%s/%s", app_settings.plugins_path,
                        dir->d_name);
                printf("Loading plugin %s\n", resolved_path);

                handle = dlopen(resolved_path, RTLD_NOW);
                if (!handle) {
                    write_log("%s\n", dlerror());
                    exit(EXIT_FAILURE);
                }

                dlerror(); /* Clear any existing error */

                plugin_t *(*getPluginInstance)();
                *(void **)(&getPluginInstance) = dlsym(handle, "registerMe");

                error = dlerror();

                if (error != NULL) {
                    write_log("%s\n", error);
                    exit(EXIT_FAILURE);
                }

                plugins[registered_plugin_count] = getPluginInstance();
                plugins[registered_plugin_count]->init(
                    interfaceName, &app_settings, &system_settings, &weights,
                    &reference_values, bias);

                registered_plugin_count++;
            }
        }
        closedir(d);
    }
    return registered_plugin_count;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    FILE *inputDataFile;

    unsigned int n_threads;

    signal(SIGINT, handleSigint);
    signal(SIGHUP, handleSighup);

    if (handleCommandLineArguments(argc, argv) == RET_FAIL)
        exit(RET_FAIL);

#ifdef M_THREADS
    retrieveNumberOfCores(&n_threads);
#else
    n_threads = 1;
#endif

    if (strncmp(settingsFileName, "0", strlen(settingsFileName)) == 0)
        snprintf(settingsFileName, MAX_FILENAME_LENGTH, "%s",
                 SETTINGS_DEFAULT_PATH);

    write_log("Loading file (%s)...", settingsFileName);
    fflush(stdout);
    if (readSettingsFromJsonFile(settingsFileName, &app_settings, &weights,
                                 &bias) == RET_FAIL)
        exit(1);
    write_log("DONE.\n");

    write_log("Loading file (%s)...", inputFileName);
    fflush(stdout);

    inputDataFile = fopen(inputFileName, "r");
    if (inputDataFile == NULL) {
        printf("Error (%d) opening input CSV file.\n\n", errno);
        printHelp(argv[0]);
        return (RET_FAIL);
    }

    if ((fileRows = allocateMemoryBasedOnInputAndMaxLearningValues(
             inputDataFile, &app_settings, &reference_values)) == RET_FAIL)
        printf("allocateMemoryBasedOnInputAndMaxLearningValues(...) error\n");

    rewind(inputDataFile);

    if (loadFile(inputDataFile, fileRows, &reference_values) == RET_FAIL)
        printf("loadFile(...) error\n");

    fclose(inputDataFile);

    write_log("DONE.\n");
    fflush(stdout);

    write_log("Memory footprint to hold data: %ld bytes\n",
              sizeof(tuning_params_t) * reference_values.totalLength +
                  sizeof(all_values_t));

    if (registerAllPlugins() == 0) {
        write_log("No plugins were registered! Cannot run any training");
        return EXIT_FAILURE;
    }

    /*
        printTable(&allValues);
    */
    if (strncmp(operationalMode, "training", strlen("training")) == 0) {

        srand(time(NULL));

        printf("Augmenting data...\n");

        pthread_t threads[n_threads];
        memset(threads, 0, n_threads * sizeof(pthread_t));
        for (unsigned int i = 0; i < n_threads; i++)
            pthread_create(&threads[i], NULL, runStdTraining, NULL);

        for (unsigned int i = 0; i < n_threads; i++)
            pthread_join(threads[i], NULL);

        printf("DONE.\n");

        // printTable(&allValues);

        printf("Final table length: %d\n", reference_values.validValues);

        unsigned int totalFileEntries =
            saveTrainedDataToFile(&reference_values, inputFileName);

        printf("Total entries in file: %d\n", totalFileEntries);

    } else if (strncmp(operationalMode, "live-training",
                       strlen("live-training")) == 0) {

        srand(time(NULL));

        printf("Augmenting data...\n");

        runLiveTraining();

        printf("DONE.\n");

        // printTable(&allValues);

        printf("Final table length: %d\n", reference_values.validValues);

        unsigned int totalFileEntries =
            saveTrainedDataToFile(&reference_values, inputFileName);

        printf("Total entries in file: %d\n", totalFileEntries);

    } else if (strncmp(operationalMode, "inference", strlen("inference")) ==
               0) {

        readSystemSettings(&system_settings);

        printTable(&reference_values);

        runInference();
    }

    free(reference_values.parameters);

    fflush(stdout);

    return (RET_OK);
}
