# === Makefile for Nyx / Phobos - macspoof ===

# Root of the Nyx framework (relative to this tool's folder)
NYX_ROOT ?= ../../..

# Directories
SRC_DIR := src
CORE_DIR := $(NYX_ROOT)/core
UTILS_DIR := $(NYX_ROOT)/utils
BIN_DIR := $(NYX_ROOT)/bin
OBJ_DIR := build

# Object files by type
CORE_OBJS := $(OBJ_DIR)/nyx_logger.o $(OBJ_DIR)/nyx_cli.o $(OBJ_DIR)/nyx_error.o
UTILS_OBJS := $(OBJ_DIR)/ph_iface.o
TOOL_OBJS := $(OBJ_DIR)/ph_macspoof_impl.o $(OBJ_DIR)/ph_macspoof_cli.o

# All object files
OBJS := $(CORE_OBJS) $(UTILS_OBJS) $(TOOL_OBJS)

# Compiler + flags
CC := gcc
CFLAGS := -Wall -Wextra -O2 -std=gnu11 -I$(SRC_DIR) -I$(CORE_DIR) -I$(UTILS_DIR)
LDFLAGS := 

# Output binary name
TARGET := nyx-macspoof

# === Rules ===

all: prep $(BIN_DIR)/$(TARGET)

prep:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

# Compile tool source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile core source files
$(OBJ_DIR)/%.o: $(CORE_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile utilities source files
$(OBJ_DIR)/%.o: $(UTILS_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link final binary
$(BIN_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "[+] Built $(BIN_DIR)/$(TARGET)"

clean:
	@rm -rf $(OBJ_DIR)
	@rm -f $(BIN_DIR)/$(TARGET)
	@echo "[*] Clean complete."

.PHONY: all clean prep
