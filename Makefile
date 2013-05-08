CC = gcc -g

objs=server.o sha1_base64_key.o sha1.o base64.o

server:$(objs)
	$(CC) -o server $(objs)

.PHONY:clean
clean:
	-rm server $(objs)
