TARGET = ssdc
Objects = blk_stat_tb.o blk_tb.o delete.o gar_col.o hash_func.o la2lpn.o lpn2ppn.o main.o read.o write.o stream.o
CC = gcc
CFLAGS = -Wall -g

all : $(TARGET)

ssdc : $(Objects)
	$(CC) -o $@ $^ 

.c.o : 
	$(CC) -c -o $@ $< $(CFLAGS)

clean : 
	rm -f *.o
