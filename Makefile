CFLAGS := -Wall -Wextra -Wpedantic -std=gnu99

all: release

debug : CFLAGS += -D_BENCHMARK -Werror -Og -g
release : CFLAGS += -Os -s -no-pie

debug: dump-archived-history
release: dump-archived-history

dump-archived-history: main.o enumerate-histfiles.o decompressor.o spawn-child.o
	$(CC) $(CFLAGS) main.o enumerate-histfiles.o decompressor.o spawn-child.o -o dump-archived-history

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: dump-archived-history
	cp dump-archived-history $(HOME)/.local/bin/dump-archived-history

remove:
	$(RM) $(HOME)/.local/bin/dump-archived-history

clean:
	$(RM) main.o enumerate-histfiles.o decompressor.o spawn-child.o dump-archived-history

