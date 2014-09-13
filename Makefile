all:
	gcc src/*.c -ggdb -W -Wall -o bin/maytrics -l:libevent-2.1.so.4 -l:libevent_openssl-2.1.so.4 -levhtp -ljansson -lpthread -lssl -lcrypto -lhiredis
