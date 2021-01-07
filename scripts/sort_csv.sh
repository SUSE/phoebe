#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright SUSE LLC

# This is a hacked up script that sort the output(s) of collect_stats.py so
# that
#   1) The file is sorted by transfer_rate, from smallest to largest
#   2) There should be no rows with the exact same value of transfer_rate
set -euo pipefail
IFS=$'\n'

if [[ "$#" -lt 1 ]]; then
    echo "Usage: sort_csv.sh FILE [FILE]..."
    echo "At least one file is required"
    exit 1
fi

# Print header
cat "$1" | head -n 1

for file in "$@"; do
    cat "$file" | \
        tail -n +2 # Strip header
done | \
    sort --numeric-sort --unique --field-separator=',' --key=1,1
