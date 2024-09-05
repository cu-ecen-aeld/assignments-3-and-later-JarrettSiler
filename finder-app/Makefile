TARGET = writer
SRC = writer.c

ifeq ($(CROSS_COMPILE),)
	# Native build
	CC = gcc
else
	# Cross-compilation
	CC = $(CROSS_COMPILE)gcc
endif

#build the executable
writer: writer.c
	$(CC) -o writer writer.c

#remove the build files
clean:
	rm -f writer
