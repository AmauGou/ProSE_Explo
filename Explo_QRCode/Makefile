# ---- CONFIGURATION ----
# Retrieve the environment variables from the .env file
ifneq (,$(wildcard .env))
	include .env
	export
endif

# Folder structure
SRC_DIR = ./src
BUILD_DIR = ./build
BIN_DIR = ./bin

# ----- END OF CONFIGURATION -----
# Executable path
EXEC = $(BIN_DIR)/$(EXEC_NAME)

# Sysroot path localy
SYSROOT_LOCAL_DIR = ./.sysroot-$(subst .,-,$(IP))

# Commands
RM = rm -f
CC = aarch64-linux-gnu-gcc
MKDIR_P = mkdir -p

PKG_CONFIG_PATH := $(SYSROOT_LOCAL_DIR)/usr/lib/aarch64-linux-gnu/pkgconfig:$(SYSROOT_LOCAL_DIR)/usr/share/pkgconfig
PKG_CONFIG_SYSROOT_DIR := $(SYSROOT_LOCAL_DIR)

GST_CFLAGS := $(shell PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0)
GST_LIBS   := $(shell PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs gstreamer-1.0 gstreamer-app-1.0)

ZBAR_CFLAGS := $(shell PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags zbar-1)
ZBAR_LIBS   := $(shell PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs zbar-1)

# Compilation flags
CCFLAGS = -Wall -MMD -MP
CCFLAGS += -lrt
CCFLAGS += -O0
# Linker flags
CCFLAGS += $(GST_CFLAGS) $(ZBAR_CFLAGS)
LDFLAGS += $(GST_LIBS) $(ZBAR_LIBS) -lm -lpthread -lrt -lprotobuf-c -lzbar

# Error handling 
CCFLAGS += -Werror=uninitialized

# Add sysroot to the compilation flags
CCFLAGS += --sysroot=$(SYSROOT_LOCAL_DIR)
LDFLAGS += --sysroot=$(SYSROOT_LOCAL_DIR)

# Source files
SRC = $(wildcard $(shell find $(SRC_DIR) -type f -regex ".*\.c"))

# Object files
OBJ = $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/%, $(SRC:.c=.o))
DEP = $(OBJ:.o=.d)

# === Compilation rules === 
all: $(EXEC)

debug: CCFLAGS += -g
debug: rebuild

rebuild: distclean all

# Link the object files to create the executable
$(EXEC): $(OBJ)
	@ echo "\n==== Linker ========="
	@ $(MKDIR_P) $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)
	@ echo "==== Linker done ====\n"

# Dependencies
-include $(DEP)

# Compile the source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@ echo "\n==== Build Object $< ========="
	@ $(MKDIR_P) $(dir $@)
	$(CC) $(CCFLAGS) -c $< -o $@
	@ echo "==== Build Object done $< ====\n"

run: 
	@ echo "\n==== Run ${EXEC_NAME} ========="
	${EXEC}
	@ echo "==== Run ${EXEC_NAME} done ====\n"

# Clean the project
clean:
	@ echo "==== Cleaning build files ===="
	$(RM) -r $(BUILD_DIR)
	@ echo "==== Build files cleaned ====="

distclean: clean
	@ echo "==== Cleaning binary files ===="
	$(RM) -r $(BIN_DIR)
	@ echo "==== Binary files cleaned ====="


# Phony targets
.PHONY: all clean test run rebuild debug distclean


# === Distant Run ===
REMOTE_PATH_EXEC = /home/${USER}/cross-compile/${EXEC_NAME}

SYSROOT_REMOTE_PATHS = \
    /lib \
    /usr/include \
    /usr/lib \
    /usr/local/include \
    /usr/local/lib

# Rsync options
RSYNC_OPTS = -avz --progress --delete -e "ssh -p $(PORT)"

sync-sysroot:
	@ echo "==== Syncing sysroot to $(SYSROOT_LOCAL_DIR) ===="
	@$(MKDIR_P) $(SYSROOT_LOCAL_DIR)  # Ensure base local directory exists
	@for path in $(SYSROOT_REMOTE_PATHS); do \
		mkdir -p $(SYSROOT_LOCAL_DIR)$$path; \
	    rsync $(RSYNC_OPTS) $(USER)@$(IP):$$path/ $(SYSROOT_LOCAL_DIR)$$path; \
	done
	@ echo "==== Sysroot synced to $(SYSROOT_LOCAL_DIR) ===="

clear-sysroot:
	@ echo "==== Clearing sysroot from $(SYSROOT_LOCAL_DIR) ===="
	rm -rf $(SYSROOT_LOCAL_DIR)/*
	@ echo "==== $(SYSROOT_LOCAL_DIR) cleared ===="

# Copy the executable to the remote machine
remote-copy:
	@ echo "==== Copying executable to remote machine at $(IP) ===="
	ssh ${USER}@${IP} "$(MKDIR_P) $(dir /home/${USER}/cross-compile/${EXEC_NAME})"
	scp $(EXEC) ${USER}@${IP}:${REMOTE_PATH_EXEC}
	@ echo "==== Executable copied to remote machine at $(IP) ===="

# Run the executable on the remote machine
remote-run:
	@ echo "==== Running executable on remote machine at $(IP) ===="
	ssh ${USER}@${IP} "${REMOTE_PATH_EXEC}"
	@ echo "==== Executable ran on remote machine at $(IP) ===="

remote-test: remote-copy remote-run

remote-ssh:
	@ echo "==== SSH to remote machine at $(IP) ===="
	ssh ${USER}@${IP}
	@ echo "==== Disconnected from remote machine at $(IP) ===="

testremote: copy remote sync-sysroot
