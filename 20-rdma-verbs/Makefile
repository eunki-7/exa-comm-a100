# Simple Makefile for RDMA ping-pong example
CC=gcc
CFLAGS=-O2 -Wall
LIBS=-libverbs -lrdmacm
all: rdma_pingpong
rdma_pingpong: rdma_pingpong.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)
clean:
	rm -f rdma_pingpong
