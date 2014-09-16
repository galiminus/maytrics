all:
	gcc src/*.c -ggdb -W -Wall -o bin/maytrics -l:libevent_core-2.0.so.5 -levhtp -ljansson -lpthread -lssl -lcrypto -lhiredis
