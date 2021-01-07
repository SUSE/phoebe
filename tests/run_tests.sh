#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright SUSE LLC

set -euox pipefail

SRC_DIR=$1
BUILD_DIR=$2

cat <<EOF > settings.json
{
  "app_settings": {
    "plugins_path": "${BUILD_DIR}/src",
    "max_learning_values": 1000,
    "saving_loop": 1000,
    "accuracy": 0.5,
    "approx_function": 0,
    "grace_period": 10,
    "stats_collection_period": 0.5,
    "inference_loop_period": 1
  },
  "weights": {
    "transfer_rate_weight": 0.8,
    "drop_rate_weight": 0.1,
    "errors_rate_weight": 0.05,
    "fifo_errors_rate_weight": 0.05
  },
  "bias": 10
}
EOF

cp ${SRC_DIR}/csv_files/rates.csv .

${BUILD_DIR}/src/phoebe -s settings.json -f rates.csv -m training
PYTHONPATH=${BUILD_DIR}/scripts/ ${SRC_DIR}/scripts/collect_stats.py lo 1
