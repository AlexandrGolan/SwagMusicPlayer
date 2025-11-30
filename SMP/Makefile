CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200112L
LIBS = -lasound -lsndfile -lmpg123 -lpthread
TARGET = smp
INSTALL_DIR = /usr/local/bin

SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	sudo chmod +x $(INSTALL_DIR)/$(TARGET)

binary: $(TARGET)
	sudo cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	sudo chmod +x $(INSTALL_DIR)/$(TARGET)

deps-info:
	@echo "Required dependencies for SMP Media Player:"
	@echo "  ALSA:        libasound2-dev (Debian/Ubuntu) or alsa-lib (Arch)"
	@echo "  libsndfile:  libsndfile1-dev (Debian/Ubuntu) or libsndfile (Arch)"
	@echo "  libmpg123:   libmpg123-dev (Debian/Ubuntu) or mpg123 (Arch)"
	@echo "  tiv:         tiv (for album art display)"
	@echo ""
	@echo "Install on Arch:"
	@echo "  sudo pacman -S alsa-lib libsndfile mpg123 tiv"
	@echo ""
	@echo "Install on Debian/Ubuntu:"
	@echo "  sudo apt-get install libasound2-dev libsndfile1-dev libmpg123-dev tiv"

.PHONY: all clean install binary deps-info