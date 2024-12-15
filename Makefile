CC = gcc
CFLAGS = -Wall -pthread

TARGET = wordcount
OBJS = threadpool.o mapreduce.o distwc.o

all: $(TARGET)

threadpool.o: threadpool.c threadpool.h
	$(CC) $(CFLAGS) -c threadpool.c -o threadpool.o

mapreduce.o: mapreduce.c mapreduce.h
	$(CC) $(CFLAGS) -c mapreduce.c -o mapreduce.o

distwc.o: distwc.c mapreduce.h
	$(CC) $(CFLAGS) -c distwc.c -o distwc.o

$(TARGET): threadpool.o mapreduce.o distwc.o
	$(CC) $(CFLAGS) threadpool.o mapreduce.o distwc.o -o $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
	rm *.txt