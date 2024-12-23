CC = gcc
CFLAGS = -Wall -g -ggdb

SRC = lib/ult.c lib/mutex.c lib/utils.c lib/cond.c
OBJ = $(patsubst lib/%.c,bin/%.o,$(SRC))

bin/%.o: lib/%.c | bin
	$(CC) -c $(CFLAGS) $< -o $@

bin/main.o: main.c | bin
	$(CC) -c $(CFLAGS) $< -o $@

main: bin/main.o $(OBJ)
	$(CC) -o bin/main bin/main.o $(OBJ)

bin/main_mutex.o: main_mutex.c | bin
	$(CC) -c $(CFLAGS) $< -o $@

mutex: bin/main_mutex.o $(OBJ)
	$(CC) -o bin/main_mutex bin/main_mutex.o $(OBJ)

bin/deadlocks.o: deadlocks.c | bin
	$(CC) -c $(CFLAGS) $< -o $@

deadlocks: bin/deadlocks.o $(OBJ)
	$(CC) -o bin/deadlocks bin/deadlocks.o $(OBJ)

bin/cond_var.o: cond_var.c | bin
	$(CC) -c $(CFLAGS) $< -o $@

cond: bin/cond_var.o $(OBJ)
	$(CC) -o bin/cond bin/cond_var.o $(OBJ)

bin/cond_all.o: cond_all.c | bin
	$(CC) -c $(CFLAGS) $< -o $@

condall: bin/cond_all.o $(OBJ)
	$(CC) -o bin/condall bin/cond_all.o $(OBJ)
	
bin:
	mkdir -p bin

clean:
	$(RM) -rf bin/