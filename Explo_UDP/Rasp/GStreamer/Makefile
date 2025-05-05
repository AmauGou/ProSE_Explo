# ---- CONFIGURATION ----
# Charger les variables d'environnement depuis .env
ifneq (,$(wildcard .env))
	include .env
	export
endif

# Dossiers
SRC_DIR = ./src
BUILD_DIR = ./build
BIN_DIR = ./bin

# Nom de l'exécutable
EXEC = $(BIN_DIR)/$(EXEC_NAME)

# Chemin du sysroot local
SYSROOT_LOCAL_DIR = ./.sysroot-$(subst .,-,$(IP))

# Commandes
RM = rm -rf
CC = aarch64-linux-gnu-gcc
MKDIR_P = mkdir -p

# Flags de compilation
CCFLAGS = -Wall -MMD -MP -O2
LDFLAGS = -lm

# Ajout du sysroot aux flags
CCFLAGS += --sysroot=$(SYSROOT_LOCAL_DIR)
LDFLAGS += --sysroot=$(SYSROOT_LOCAL_DIR)

# Ajouter les flags GStreamer via pkg-config
PKG_CONFIG_PATH=$(SYSROOT_LOCAL_DIR)/usr/lib/aarch64-linux-gnu/pkgconfig
CCFLAGS += $(shell PKG_CONFIG_SYSROOT_DIR=$(SYSROOT_LOCAL_DIR) PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0)
LDFLAGS += $(shell PKG_CONFIG_SYSROOT_DIR=$(SYSROOT_LOCAL_DIR) PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs gstreamer-1.0 gstreamer-app-1.0)

# Fichiers source et objets
SRC = $(wildcard $(shell find $(SRC_DIR) -type f -regex ".*\.c"))
OBJ = $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/%, $(SRC:.c=.o))
DEP = $(OBJ:.o=.d)

# === Compilation ===
all: $(EXEC)

debug: CCFLAGS += -g
debug: rebuild

rebuild: clean all

# Linkage
$(EXEC): $(OBJ)
	@ echo "\n==== Linker ========="
	@ $(MKDIR_P) $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)
	@ echo "==== Linker done ====\n"

# Compilation des fichiers sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@ echo "\n==== Build Object $< ========="
	@ $(MKDIR_P) $(dir $@)
	$(CC) $(CCFLAGS) -c $< -o $@
	@ echo "==== Build Object done $< ====\n"

run: 
	@ echo "\n==== Run ${EXEC_NAME} ========="
	${EXEC}
	@ echo "==== Run ${EXEC_NAME} done ====\n"

# Nettoyage
clean:
	@ echo "==== Cleaning build files ===="
	$(RM) $(BUILD_DIR)
	@ echo "==== Build files cleaned ====="

distclean: clean
	@ echo "==== Cleaning binary files ===="
	$(RM) $(BIN_DIR)
	@ echo "==== Binary files cleaned ====="

.PHONY: all clean run rebuild debug distclean

# === Synchronisation sysroot ===
SYSROOT_REMOTE_PATHS = \
    /lib \
    /usr/include \
    /usr/lib \
    /usr/local/include \
    /usr/local/lib \
    /usr/lib/aarch64-linux-gnu/gstreamer-1.0

RSYNC_OPTS = -avz --progress --delete -e "ssh -p $(PORT)"

sync-sysroot:
	@$(MKDIR_P) $(SYSROOT_LOCAL_DIR)
	@for path in $(SYSROOT_REMOTE_PATHS); do \
		mkdir -p $(SYSROOT_LOCAL_DIR)$$path; \
	    rsync $(RSYNC_OPTS) --rsync-path="sudo rsync" $(USER)@$(IP):$$path/ $(SYSROOT_LOCAL_DIR)$$path; \
	done

clear-sysroot:
	rm -rf $(SYSROOT_LOCAL_DIR)/*

# === Exécution distante ===
REMOTE_PATH_EXEC = /home/${USER}/cross-compile/${EXEC_NAME}

remote-copy:
	ssh ${USER}@${IP} "$(MKDIR_P) $(dir $(REMOTE_PATH_EXEC))"
	scp $(EXEC) ${USER}@${IP}:${REMOTE_PATH_EXEC}

remote-run:
	ssh ${USER}@${IP} "${REMOTE_PATH_EXEC}"

remote-test: remote-copy remote-run

remote-ssh:
	ssh ${USER}@${IP}
