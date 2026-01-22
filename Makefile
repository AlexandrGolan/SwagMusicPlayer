CC = gcc
SRC_DIR = src
INCLUDE_DIR = include
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200112L -I$(INCLUDE_DIR)
LIBS = -lasound -lsndfile -lmpg123 -lpthread -ltag_c
TARGET = smp
INSTALL_DIR = /usr/local/bin

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/ui.c $(SRC_DIR)/audio.c $(SRC_DIR)/playlist.c $(SRC_DIR)/file_utils.c $(SRC_DIR)/config.c
OBJS = $(SRCS:.c=.o)

RED=\033[0;31m
GREEN=\033[0;32m
YELLOW=\033[0;33m
BLUE=\033[0;34m
CYAN=\033[0;36m
RESET=\033[0m

all: $(TARGET)

$(TARGET): $(OBJS)
	@printf "$(CYAN)[CCO] Compiling %+57s$(RESET)\n" "-> $@"
	@$(CC) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.c
	@printf "$(CYAN)[CC]  Compiling %-50s -> %s$(RESET)\n" "$<" "$@"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -f $(OBJS)

remove:
	@rm -f $(TARGET) $(OBJS)
	@if [ -f "$(INSTALL_DIR)/$(TARGET)" ]; then \
		echo "Removing $(INSTALL_DIR)/$(TARGET)"; \
		sudo rm -f "$(INSTALL_DIR)/$(TARGET)"; \
	fi

install: $(TARGET)
	@sudo cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	@sudo chmod +x $(INSTALL_DIR)/$(TARGET)
	@echo "Installed to $(INSTALL_DIR)/$(TARGET)"

.PHONY: all clean install
