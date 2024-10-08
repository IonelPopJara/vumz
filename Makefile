CC = gcc
CFLAGS = -Wall -g -I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2
LDFLAGS = -lncursesw -lpipewire-0.3 -lm

SRC = src/main.c src/audio-out.c src/audio-cap.c
OBJ = $(SRC:.c=.o)
TARGET = vumz
INSTALL_PATH = /usr/local/bin

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -Dm755 $(TARGET) $(INSTALL_PATH)/$(TARGET)

uninstall:
	rm -f $(INSTALL_PATH)/$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean install uninstall
