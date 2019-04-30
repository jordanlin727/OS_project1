CC=gcc
CFLAGS=-Wall -g -pthread
SRCS=project1.c
OBJS=project1.o

.c.o:
	$(CC) $(CFLAGS) -c $<

all: project1

project1: $(OBJS)
	$(CC) $(CFLAGS) project1.o -o project1

clean:
	rm -f $(OBJS) project1 project.o
