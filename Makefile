all:
	gcc src/main.c -o bin/maytrics -levhtp -ljansson -levent -lpthread -lssl -lcrypto -lleveldb
