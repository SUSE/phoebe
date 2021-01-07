# SPDX-License-Identifier: BSD-3-Clause
# Copyright SUSE LLC

CC ?= gcc

CFLAGS  += -g -Wall -Wpedantic -std=gnu99
ifeq ($(BUILD),debug)
	CFLAGS  += -O0 -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer
endif

# PRINT_MSGS = prints to screen only the most important messages
#
# PRINT_ADV_MSGS = very verbose printing to screen (useful for debugging
# purposes)
#
# PRINT_TABLE = prints to screen all data stored in the different tables
# maintained by the application
#
# APPLY_CHANGES = it enables the application to actually apply the kernel
# settings via sysctl command
#
# CHECK_INITIAL_SETTINGS = when enabled it will prevent the application to
# apply lower settings than the ones already applied on the system at bootstrap
#
# M_THREADS = when enabled will run training using as many threads as cores are
# available on the machine
EXTRA_CFLAGS  = -DPRINT_MSGS
INCLUDES = -I/usr/include/libnl3 -I./headers/
LFLAGS =
LIBS = -lc -lm -ldl -lnl-3 -lnl-nf-3 -lnl-genl-3 -lnl-route-3 -lpthread -ljson-c

SCRIPTS = scripts
SRCS = src
BIN = bin

OBJS = $(BIN)/filehelper.o $(BIN)/utils.o

PLUGINS = $(BIN)/network_plugin.so

# the build target executable:
TARGET = phoebe

all: $(TARGET) $(PLUGINS)

python: $(SCRIPTS)/_$(TARGET).abi3.so

plugins: $(PLUGINS)

$(TARGET): $(SRCS)/$(TARGET).c $(OBJS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(INCLUDES) -o $(BIN)/$@ $(LFLAGS) $(LIBS) $^

$(SCRIPTS)/_$(TARGET).abi3.so: $(OBJS) $(BIN)/stats.o $(BIN)/_$(TARGET).c
	$(CC) $(CFLAGS) -Wno-pedantic $(EXTRA_CFLAGS) $(INCLUDES) $(shell python3-config --includes) $(LFLAGS) $(shell python3-config --ldflags) $(LIBS) $(shell python3-config --libs) -fPIC -shared $^ -o $@

$(BIN)/_$(TARGET).c: $(SCRIPTS)/collect_stats.py
	$(SCRIPTS)/collect_stats.py generate $@

$(BIN)/$(TARGET).o: $(SRCS)/$(TARGET).c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -c -fPIC $^ -o $@

$(BIN)/filehelper.o: $(SRCS)/filehelper.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -c -fPIC $^ -o $@

$(BIN)/algorithmic.o: $(SRCS)/algorithmic.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -c -fPIC $^ -o $@

$(BIN)/stats.o: $(SRCS)/stats.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -c -fPIC $^ -o $@

$(BIN)/utils.o: $(SRCS)/utils.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -c -fPIC $^ -o $@

$(BIN)/network_plugin.o: $(SRCS)/network_plugin.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -c -fPIC $^ -o $@

$(BIN)/network_plugin.so: $(BIN)/network_plugin.o $(BIN)/stats.o $(BIN)/algorithmic.o $(BIN)/utils.o $(BIN)/filehelper.o
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(INCLUDES) -shared -o $@ $(LFLAGS) $(LIBS) $^

test: $(TARGET) $(PLUGINS) python
	./bin/phoebe -s ./tests/settings.json -f ./csv_files/rates.csv -m training
	$(SCRIPTS)/collect_stats.py lo 1

format:
	clang-format --sort-includes -style="{BasedOnStyle: llvm, IndentWidth: 4}" -i ./src/*.c
	clang-format --sort-includes -style="{BasedOnStyle: llvm, IndentWidth: 4}" -i ./headers/*.h

clean:
	$(RM) $(BIN)/* $(SCRIPTS)/_$(TARGET).abi3.so _$(TARGET).* $(OBJS)
