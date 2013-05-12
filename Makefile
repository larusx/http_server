CC = gcc -g

objs=server.o sha1_base64_key.o sha1.o base64.o list.o

server:$(objs)
	$(CC) -o server $(objs) -lpthread

.PHONY:clean
clean:
	-rm server $(objs)
