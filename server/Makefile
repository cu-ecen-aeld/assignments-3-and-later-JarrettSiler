CC=aarch64-none-linux-gnu-gcc
CFLAGS=-Wall
TARGET=aesdsocket


all: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c -lpthread

clean:
	rm -f $(TARGET)

default: all