# NAME: 	Brian Be, 				Leslie Liang
# EMAIL: 	bebrian458@gmail.com, 	lliang9838@gmail.com
# UID: 		204612203, 				204625818

CC = gcc
LDLIBS =
DIST = lab3a.c ext2_fs.h Makefile README

default: lab3a

lab3a: lab3a.c
	$(CC) -o $@ lab3a.c 

clean:
	rm -f lab3a

dist:
	tar -czf lab3a-204612203.tar.gz $(DIST)