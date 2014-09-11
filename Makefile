all:
	gcc src/main.c -o bin/maytrics -levhtp -ljansson -levent -levent_openssl -lpthread -lssl -lcrypto -lleveldb
