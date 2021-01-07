// SPDX-License-Identifier: BSD-3-Clause
// Copyright SUSE LLC

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <math.h>
#include <net/if.h>
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
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "stats.h"
#include "utils.h"

// Based on https://www.kernel.org/doc/Documentation/cpu-freq/governors.txt
static const char *GOVERNORS[] = {
    "performance", "powersave",    "userspace",
    "ondemand",    "conservative", "schedutil",
};

volatile double cpuBusyTime = 0.0;

volatile if_rates_t rates;

double getCpuBusyTime() { return cpuBusyTime; }

inline double calculateCpuBusyPercentage(cpu_stats_t *prev, cpu_stats_t *cur) {
    // differentiate: actual value minus the previous one
    double idled = cur->idleTotal - prev->idleTotal;
    double totald = idled + cur->nonIdleTotal - prev->nonIdleTotal;

    if (totald != 0) // Avoid any potential division-by-zero
    {
        return ((totald - idled) / totald) * 100;
    }
    return NAN;
}

inline void *collectCpuStats(void *arg) {
    cpu_stats_t stats, prev = {0};
    while (1) {
        readCpuStats(&stats);

        cpuBusyTime = calculateCpuBusyPercentage(&prev, &stats);
        write_adv_log("Busy for : %lf %% of the time.\n", cpuBusyTime);

        prev = stats;

        usleep(USEC_IN_SEC);
    }

    return NULL;
}

inline void readCpuStats(cpu_stats_t *stats) {
    char str[MAX_PROC_STRING_LENGTH];
    cpu_raw_stats_t raw;

    /*  cpu user nice system idle iowait irq softirq steal guest guest_nice
     */
    FILE *fp = fopen("/proc/stat", "r");
    fgets(str, MAX_PROC_STRING_LENGTH, fp);
    sscanf(str, "%s %u %u %u %u %u %u %u %u %u %u", raw.cpu, &raw.user,
           &raw.nice, &raw.system, &raw.idle, &raw.iowait, &raw.irq,
           &raw.softirq, &raw.steal, &raw.guest, &raw.guest_nice);
    fclose(fp);

    stats->idleTotal = raw.idle + raw.iowait;
    stats->nonIdleTotal =
        raw.user + raw.nice + raw.system + raw.irq + raw.softirq + raw.steal;
}

inline int cpuGovernorIndex(const char *query) {
    for (int i = 0; i < sizeof(GOVERNORS); i++) {
        const char *governor = GOVERNORS[i];
        if (strcmp(query, governor) == 0)
            // Begin from 1
            return i + 1;
    }
    return -1;
}

void calculateInterfaceRatesPerSecond(if_stats_t *prev, if_stats_t *cur,
                                      if_rates_t *rates,
                                      double stats_collection_period) {
    rates->transfer_rate =
        (cur->bytes_total - prev->bytes_total) / stats_collection_period;
    rates->errors_rate =
        (cur->errors_total - prev->errors_total) / stats_collection_period;
    rates->drop_rate =
        (cur->dropped_total - prev->dropped_total) / stats_collection_period;
    rates->fifo_err_rate =
        (cur->fifo_err_total - prev->fifo_err_total) / stats_collection_period;

    if (rates->transfer_rate < rates->min_transfer_rate)
        rates->min_transfer_rate = rates->transfer_rate;
    if (rates->transfer_rate > rates->max_transfer_rate)
        rates->max_transfer_rate = rates->transfer_rate;
}

void *collectStats(void *stats_input_params) {
    struct rtnl_link *link;
    struct nl_sock *socket;
    if_stats_t stats, prevStats;
    if_rates_t tmpRates;

    char monitored_interface[MAX_INTERFACE_NAME_LENGTH];

    memcpy(monitored_interface,
           ((stats_input_param_t *)stats_input_params)->monitored_interface,
           MAX_INTERFACE_NAME_LENGTH);

    socket = nl_socket_alloc();
    nl_connect(socket, NETLINK_ROUTE);

    if (rtnl_link_get_kernel(socket, 0, monitored_interface, &link) >= 0) {
        readStats(link, &prevStats);
        rtnl_link_put(link);
    }

    while (1) {
        if (rtnl_link_get_kernel(socket, 0, monitored_interface, &link) >= 0) {
            readStats(link, &stats);
            rtnl_link_put(link);

            tmpRates.min_transfer_rate = rates.min_transfer_rate;
            tmpRates.max_transfer_rate = rates.max_transfer_rate;
            calculateInterfaceRatesPerSecond(
                &prevStats, &stats, &tmpRates,
                ((stats_input_param_t *)stats_input_params)
                    ->stats_collection_period);
            rates = tmpRates;
            write_adv_log(
                "transfer_rate(in+out)=%ld B/s, error_rate(rx+tx)=%ld/s, "
                "drop_rate(rx+tx)=%ld, fifo_err_rate(rx+tx)=%ld/s\n",
                rates.transfer_rate, rates.errors_rate, rates.drop_rate,
                rates.fifo_err_rate);

            prevStats = stats;

            usleep(USEC_IN_SEC * ((stats_input_param_t *)stats_input_params)
                                     ->stats_collection_period);
        }
    }

    nl_socket_free(socket);

    write_log("Stats Collection exiting...\n");

    return NULL;
}

inline void readStats(struct rtnl_link *link, if_stats_t *stats) {
    if_raw_stats_t raw;

    raw.rx_errors = rtnl_link_get_stat(link, RTNL_LINK_RX_ERRORS);
    raw.tx_errors = rtnl_link_get_stat(link, RTNL_LINK_TX_ERRORS);
    raw.rx_dropped = rtnl_link_get_stat(link, RTNL_LINK_RX_DROPPED);
    raw.tx_dropped = rtnl_link_get_stat(link, RTNL_LINK_TX_DROPPED);
    raw.rx_fifo_err = rtnl_link_get_stat(link, RTNL_LINK_RX_FIFO_ERR);
    raw.tx_fifo_err = rtnl_link_get_stat(link, RTNL_LINK_TX_FIFO_ERR);
    raw.bytes_in = rtnl_link_get_stat(link, RTNL_LINK_RX_BYTES);
    raw.bytes_out = rtnl_link_get_stat(link, RTNL_LINK_TX_BYTES);

    stats->bytes_total = raw.bytes_in + raw.bytes_out;
    stats->errors_total = raw.rx_errors + raw.tx_errors;
    stats->dropped_total = raw.rx_dropped + raw.tx_dropped;
    stats->fifo_err_total = raw.rx_fifo_err + raw.tx_fifo_err;

    write_adv_log("rx_errors=%ld, tx_errors=%ld, rx_dropped=%ld, "
                  "tx_dropped=%ld, rx_fifo_err=%ld, tx_fifo_err=%ld, ",
                  raw.rx_errors, raw.tx_errors, raw.rx_dropped, raw.tx_dropped,
                  raw.rx_fifo_err, raw.tx_fifo_err);
}

struct ifreq *allocInterfaceRequest(const char *ifname) {
    struct ifreq *ifr;

    ifr = (struct ifreq *)malloc(sizeof(struct ifreq));
    if (!ifr) {
        return NULL;
    }
    strncpy(ifr->ifr_name, ifname, sizeof(ifr->ifr_name) - 1);

    return ifr;
}

static inline void freeInterfaceRequest(struct ifreq *ifr) { free(ifr); }

struct ifreq *allocRingSizeRequest(const char *ifname) {
    struct ifreq *ifr;
    struct ethtool_ringparam *ering;

    ifr = allocInterfaceRequest(ifname);
    if (!ifr)
        return NULL;

    ering =
        (struct ethtool_ringparam *)malloc(sizeof(struct ethtool_ringparam));
    if (!ering) {
        freeInterfaceRequest(ifr);
        return NULL;
    }
    ifr->ifr_data = (char *)ering;

    return ifr;
}

inline void freeRingSizeRequest(struct ifreq *ifr) {
    free(ifr->ifr_data);
    freeInterfaceRequest(ifr);
}

inline void readRingSize(int sock, struct ifreq *ifr, if_ring_size_t *stats) {
    struct ethtool_ringparam *ering = (struct ethtool_ringparam *)ifr->ifr_data;

    ering->cmd = ETHTOOL_GRINGPARAM;
    if (ioctl(sock, SIOCETHTOOL, ifr) == -1) {
        printf("ETHTOOL_GRINGPARAM failed: %s\n", strerror(errno));
        return;
    }

    stats->rx = ering->rx_pending;
    stats->tx = ering->tx_pending;
    return;
}

struct ifreq *allocGetCoalesceRequest(const char *ifname) {
    struct ifreq *ifr;
    struct ethtool_coalesce *ecoalesce;

    ifr = allocInterfaceRequest(ifname);
    if (!ifr)
        return NULL;

    ecoalesce =
        (struct ethtool_coalesce *)malloc(sizeof(struct ethtool_coalesce));
    if (!ecoalesce) {
        freeInterfaceRequest(ifr);
        return NULL;
    }
    ecoalesce->cmd = ETHTOOL_GCOALESCE;
    ifr->ifr_data = (char *)ecoalesce;

    return ifr;
}

inline void freeGetCoalesceRequest(struct ifreq *ifr) {
    free(ifr->ifr_data);
    freeInterfaceRequest(ifr);
}

inline void readCoalesce(int sock, struct ifreq *ifr, if_coalesce_t *stats) {
    struct ethtool_coalesce *ecoalesce;

    if (ioctl(sock, SIOCETHTOOL, ifr) == -1) {
        printf("ETHTOOL_GCOALESCE failed: %s\n", strerror(errno));
        return;
    }

    ecoalesce = (struct ethtool_coalesce *)ifr->ifr_data;
    stats->rx_coalesce_usecs = ecoalesce->rx_coalesce_usecs;
    stats->rx_max_coalesced_frames = ecoalesce->rx_max_coalesced_frames;
    stats->tx_coalesce_usecs = ecoalesce->tx_coalesce_usecs;
    stats->tx_max_coalesced_frames = ecoalesce->tx_max_coalesced_frames;

    return;
}

struct ifreq *allocGetOffloadsRequest(const char *ifname) {
    struct ifreq *ifr;
    struct ethtool_value *evalue;

    ifr = allocInterfaceRequest(ifname);
    if (!ifr)
        return NULL;

    evalue = (struct ethtool_value *)malloc(sizeof(struct ethtool_value));
    if (!evalue) {
        freeInterfaceRequest(ifr);
        return NULL;
    }
    ifr->ifr_data = (char *)evalue;

    return ifr;
}

inline void freeGetOffloadsRequest(struct ifreq *ifr) {
    free(ifr->ifr_data);
    freeInterfaceRequest(ifr);
}

void readOffloads(int sock, struct ifreq *ifr, if_offloads_t *stats) {
    struct ethtool_value *evalue = (struct ethtool_value *)ifr->ifr_data;

    evalue->cmd = ETHTOOL_GRXCSUM;
    if (ioctl(sock, SIOCETHTOOL, ifr) == -1) {
        printf("ETHTOOL_GRXCSUM failed: %s\n", strerror(errno));
        return;
    }
    stats->rx_chksum_offload = !!evalue->data;

    evalue->cmd = ETHTOOL_GTXCSUM;
    if (ioctl(sock, SIOCETHTOOL, ifr) == -1) {
        printf("ETHTOOL_GTXCSUM failed: %s\n", strerror(errno));
        return;
    }
    stats->tx_chksum_offload = !!evalue->data;

    evalue->cmd = ETHTOOL_GGSO;
    if (ioctl(sock, SIOCETHTOOL, ifr) == -1) {
        printf("ETHTOOL_GGSO failed: %s\n", strerror(errno));
        return;
    }
    stats->general_segmentation_offload = !!evalue->data;

    evalue->cmd = ETHTOOL_GTSO;
    if (ioctl(sock, SIOCETHTOOL, ifr) == -1) {
        printf("ETHTOOL_GTSO failed: %s\n", strerror(errno));
        return;
    }
    stats->tcp_segmentation_offload = !!evalue->data;

    evalue->cmd = ETHTOOL_GGRO;
    if (ioctl(sock, SIOCETHTOOL, ifr) == -1) {
        printf("ETHTOOL_GGRO failed: %s\n", strerror(errno));
        return;
    }
    stats->general_receive_offload = !!evalue->data;

    evalue->cmd = ETHTOOL_GFLAGS;
    if (ioctl(sock, SIOCETHTOOL, ifr) == -1) {
        printf("ETHTOOL_GFLAGS failed: %s\n", strerror(errno));
        return;
    }
    stats->large_receive_offload = ETH_FLAG_LRO & evalue->data;
    stats->rx_vlan_offload = ETH_FLAG_RXVLAN & evalue->data;
    stats->tx_vlan_offload = ETH_FLAG_TXVLAN & evalue->data;
    stats->rx_hash = ETH_FLAG_RXHASH & evalue->data;

    return;
}

inline uint64_t getTransferRate() { return rates.transfer_rate; }

inline uint64_t getDropRate() { return rates.drop_rate; }

inline uint64_t getErrorsRate() { return rates.errors_rate; }

inline uint64_t getFifoErrorsRate() { return rates.fifo_err_rate; }

inline uint64_t getMinTransferRate() { return rates.min_transfer_rate; }

inline uint64_t getMaxTransferRate() { return rates.max_transfer_rate; }
