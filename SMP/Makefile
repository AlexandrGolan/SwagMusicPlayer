CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200112L -Iheader-files
LIBS = -lasound -lsndfile -lmpg123 -lpthread
TARGET = smp
INSTALL_DIR = /usr/local/bin

SRCS = C-files/main.c C-files/audio.c C-files/playlist.c C-files/ui.c C-files/file_utils.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
	@if [ -f "$(INSTALL_DIR)/$(TARGET)" ]; then \
		echo "Removing $(INSTALL_DIR)/$(TARGET)"; \
		sudo rm -f "$(INSTALL_DIR)/$(TARGET)"; \
	fi

install: $(TARGET)
	sudo cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	sudo chmod +x $(INSTALL_DIR)/$(TARGET)
	@echo "Installed to $(INSTALL_DIR)/$(TARGET)"

binary: $(TARGET)
	sudo cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	sudo chmod +x $(INSTALL_DIR)/$(TARGET)
	@echo "Binary moved to $(INSTALL_DIR)/$(TARGET)"

deps-info:
	@echo "Required dependencies for SMP Media Player:"
	@echo "  ALSA:        libasound2-dev (Debian/Ubuntu) or alsa-lib (Arch)"
	@echo "  libsndfile:  libsndfile1-dev (Deban/Ubuntu) or libsndfile (Arch)"
	@echo "  libmpg123:   libmpg123-dev (Debian/Ubuntu) or mpg123 (Arch)"
	@echo "  tiv:         tiv (for album art display)"
	@echo ""
	@echo "Install on Arch:"
	@echo "  sudo pacman -S alsa-lib libsndfile mpg123 tiv"
	@echo ""
	@echo "Install on Debian/Ubuntu:"
	@echo "  sudo apt-get install libasound2-dev libsndfile1-dev libmpg123-dev tiv"

depence-info: deps-info
	@echo "(Note: 'depence-info' is an alias for 'deps-info')"

.PHONY: all clean install binary deps-info depence-info
