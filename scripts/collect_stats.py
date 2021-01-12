#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright SUSE LLC

import contextlib
import json
import sys
import time
from os import path
from pathlib import Path

import cffi

SYSFS_CPU_PATH = '/sys/devices/system/cpu/'
SYSCTL_NAMES = [
    'kernel.sched_min_granularity_ns',
    'kernel.sched_wakeup_granularity_ns',
    'kernel.sched_migration_cost_ns',
    'kernel.numa_balancing',
    'kernel.pid_max',
    'net.core.netdev_max_backlog',
    'net.core.netdev_budget',
    'net.core.somaxconn',
    'net.core.busy_poll',
    'net.core.busy_read',
    'net.core.rmem_max',
    'net.core.wmem_max',
    'net.core.rmem_default',
    'net.core.wmem_default',
    'net.ipv4.tcp_fastopen',
    'net.ipv4.tcp_low_latency',
    'net.ipv4.tcp_sack',
    'net.ipv4.tcp_rmem',
    'net.ipv4.tcp_wmem',
    'net.ipv4.tcp_max_syn_backlog',
    'net.ipv4.tcp_tw_reuse',
    #'net.ipv4.tcp_tw_recycle', No longer available
    'net.ipv4.tcp_timestamps',
    'net.ipv4.tcp_syn_retries',
]
SYSCTL_NOT_IN_CONTAINER = set([
    'net.core.netdev_max_backlog',
    'net.core.netdev_budget',
    'net.core.busy_poll',
    'net.core.busy_read',
    'net.core.rmem_max',
    'net.core.wmem_max',
    'net.core.rmem_default',
    'net.core.wmem_default',
    'net.ipv4.tcp_fastopen',
    'net.ipv4.tcp_low_latency',
    'net.ipv4.tcp_sack',
    'net.ipv4.tcp_rmem',
    'net.ipv4.tcp_wmem',
    'net.ipv4.tcp_max_syn_backlog',
    'net.ipv4.tcp_tw_reuse',
    'net.ipv4.tcp_timestamps',
    'net.ipv4.tcp_syn_retries',
])
SYSCTL_ENTRIES = {
    sysctl_name: (sysctl_name.replace('net.ipv4.', '').replace('.', '_'), '/proc/sys/' + sysctl_name.replace('.', '/'))
    for sysctl_name in SYSCTL_NAMES
}

def header_file_content(file_path):
    with open(file_path) as f:
        for line in f:
            if line.startswith('#'):
                continue
            yield line

def compile_module(output_filename):
    src_root = Path(__file__).parent.parent.absolute()
    ffibuilder = cffi.FFI()

    # libc and libnl
    definitions = '''\
    int get_nprocs(void);
    struct nl_sock *nl_socket_alloc(void);
    int nl_connect(struct nl_sock *, int);
    void nl_socket_free(struct nl_sock *);
    int nl_socket_get_fd(const struct nl_sock *);
    int rtnl_link_get_kernel(struct nl_sock *, int, const char *, struct rtnl_link **);
    void rtnl_link_put(struct rtnl_link *);
    '''

    # There's no easy way to include all #define yet
    # Doing it manually for now
    definitions += '''\
    #define MAX_CPU_NAME_LENGTH 7
    #define MAX_FILENAME_LENGTH 255
    #define MAX_INTERFACE_NAME_LENGTH 255
    '''

    for line in header_file_content(path.join(src_root, 'headers', 'types.h')):
        definitions += line

    for line in header_file_content(path.join(src_root, 'headers', 'stats.h')):
        definitions += line

    for line in header_file_content(path.join(src_root, 'headers', 'filehelper.h')):
        definitions += line

    ffibuilder.cdef(definitions)

    ffibuilder.set_source(
        '_phoebe',
        '''
        #include <netlink/netlink.h>
        #include <netlink/route/link.h>
        #include <netlink/route/rtnl.h>
        #include <sys/sysinfo.h>
        #include "phoebe.h"
        #include "filehelper.h"
        #include "stats.h"
        ''',
        # XXX: This mean we cannot move libphoebe.so to another path. We can
        # omit the pwd path, but then we'll need to modify LD_LIBRARY_PATH.
        extra_objects=[path.join(src_root, 'bin/libphoebe.so')],
        include_dirs=['/usr/include/libnl3','./headers/'],
        libraries=['nl-3', 'nl-route-3'],
    )

    ffibuilder.emit_c_code(output_filename)

@contextlib.contextmanager
def nl_socket():
    socket = phoebe.nl_socket_alloc()
    phoebe.nl_connect(socket, 0)
    yield socket
    phoebe.nl_socket_free(socket)

@contextlib.contextmanager
def rtnl_link(socket, interface_name):
    link_p = ffi.new('struct rtnl_link **')
    failed = phoebe.rtnl_link_get_kernel(socket, 0, interface_name, link_p)
    if failed:
        raise RuntimeError('Failed to open link')

    yield link_p[0]

    phoebe.rtnl_link_put(link_p[0])

def collect_stats(
        interface,
        interval,
        socket,
        socket_fd,
        row,
        stats_ptr,
        prev_stats_ptr,
        rates_ptr,
        cpu_stats_ptr,
        prev_cpu_stats_ptr,
        ring_size_ptr,
        ring_size_request,
        coalesce_ptr,
        coalesce_request,
        offloads_ptr,
        offloads_request,
):
    with rtnl_link(socket, interface) as link:
        phoebe.readStats(link, stats_ptr)
    phoebe.calculateInterfaceRatesPerSecond(
        prev_stats_ptr,
        stats_ptr,
        rates_ptr,
        interval,
    )
    rates = rates_ptr[0]
    row.transfer_rate = rates.transfer_rate
    row.errors_rate = rates.errors_rate
    row.drop_rate = rates.drop_rate
    row.fifo_errors_rate = rates.fifo_err_rate
    prev_stats_ptr[0] = stats_ptr[0]

    phoebe.readRingSize(socket_fd, ring_size_request, ring_size_ptr)
    ring_size = ring_size_ptr[0]
    row.rx_ring_size = ring_size.rx
    row.tx_ring_size = ring_size.tx

    # TODO: only count physical cores (i.e. don't include HyperThread)
    nproc = phoebe.get_nprocs()
    row.cores = nproc

    # TODO: bring back reading of scaling_governor and scaling_cur_freq

    phoebe.readCpuStats(cpu_stats_ptr)
    busy_percentage = phoebe.calculateCpuBusyPercentage(prev_cpu_stats_ptr, cpu_stats_ptr)
    row.cpu_usage_percentage = busy_percentage
    prev_cpu_stats_ptr[0] = cpu_stats_ptr[0]

    for sysctl_name, (field_name, path) in SYSCTL_ENTRIES.items():
        try:
            with open(path) as f:
                value = f.read().strip()
        except FileNotFoundError:
            if sysctl_name in SYSCTL_NOT_IN_CONTAINER:
                value = '0'
            else:
                raise

        if 'tcp_rmem' == field_name or 'tcp_wmem' == field_name:
            values = value.split('\t')
            for idx, value in enumerate(values):
                setattr(row, field_name + str(idx), int(value))
        else:
            setattr(row, field_name, int(value))

    phoebe.readCoalesce(socket_fd, coalesce_request, coalesce_ptr)
    coalesce = coalesce_ptr[0]
    row.rx_interrupt_coalesce_usecs = coalesce.rx_coalesce_usecs
    row.rx_interrupt_max_coalesce_frames = coalesce.rx_max_coalesced_frames
    row.tx_interrupt_coalesce_usecs = coalesce.tx_coalesce_usecs
    row.tx_interrupt_max_coalesce_frames = coalesce.tx_max_coalesced_frames

    phoebe.readOffloads(socket_fd, offloads_request, offloads_ptr)
    offloads = offloads_ptr[0]
    row.rx_checksum_offload = offloads.rx_chksum_offload
    row.tx_checksum_offload = offloads.tx_chksum_offload
    row.rx_vlan_offload = offloads.rx_vlan_offload
    row.tx_vlan_offload = offloads.tx_vlan_offload
    row.general_segmentation_offload = offloads.general_segmentation_offload
    row.tcp_segmentation_offload = offloads.tcp_segmentation_offload
    row.general_receive_offload = offloads.general_receive_offload
    row.large_receive_offload = offloads.large_receive_offload
    row.rx_hash = offloads.rx_hash

def main(ifname, settings, count=None):
    ifname = ifname.encode('ascii')
    interval = settings['app_settings']['stats_collection_period']
    row_data_ptr = ffi.new('tuning_params_t *')
    row_data = row_data_ptr[0]
    stats_ptr = ffi.new('if_stats_t *')
    prev_stats_ptr = ffi.new('if_stats_t *')
    rates_ptr = ffi.new('if_rates_t *')
    cpu_stats_ptr = ffi.new('cpu_stats_t *')
    prev_cpu_stats_ptr = ffi.new('cpu_stats_t *')
    ring_size_ptr = ffi.new('if_ring_size_t *')
    ring_size_request = ffi.gc(
            phoebe.allocRingSizeRequest(ifname),
            phoebe.freeRingSizeRequest,
    )
    coalesce_ptr = ffi.new('if_coalesce_t *')
    coalesce_request = ffi.gc(
            phoebe.allocGetCoalesceRequest(ifname),
            phoebe.freeGetCoalesceRequest,
    )
    offloads_ptr = ffi.new('if_offloads_t *')
    offloads_request = ffi.gc(
            phoebe.allocGetOffloadsRequest(ifname),
            phoebe.freeGetOffloadsRequest,
    )
    with nl_socket() as socket:
        fp = ffi.cast('FILE *', sys.stdout)
        phoebe.writeHeader(fp)
        socket_fd = phoebe.nl_socket_get_fd(socket)
        with rtnl_link(socket, ifname) as link:
            phoebe.readStats(link, prev_stats_ptr)
        phoebe.readCpuStats(prev_cpu_stats_ptr)
        while count is None or count > 0:
            time.sleep(interval)
            collect_stats(
                    ifname,
                    interval,
                    socket,
                    socket_fd,
                    row_data,
                    stats_ptr,
                    prev_stats_ptr,
                    rates_ptr,
                    cpu_stats_ptr,
                    prev_cpu_stats_ptr,
                    ring_size_ptr,
                    ring_size_request,
                    coalesce_ptr,
                    coalesce_request,
                    offloads_ptr,
                    offloads_request,
            )
            # Omit entries where transfer_rate is 0
            if row_data.transfer_rate != 0:
                phoebe.writeRow(fp, row_data_ptr)
            if count is not None:
                count -= 1
    print('Done!', file=sys.stderr)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'generate':
        compile_module(sys.argv[2])
    else:
        from _phoebe import lib as phoebe
        from _phoebe import ffi as ffi
        count = None if len(sys.argv) < 3 else int(sys.argv[2])
        with open('settings.json') as f:
            settings = json.load(f)
        main(sys.argv[1], settings, count)
