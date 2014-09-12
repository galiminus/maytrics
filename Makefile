all:
	gcc src/main.c -ggdb -W -Wall -o bin/maytrics -l:libevent-2.0.so.5 -levhtp -ljansson -lpthread -lssl -lcrypto -lhiredis
