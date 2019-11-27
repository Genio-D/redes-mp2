TARGETS = file-receiver file-sender

CC = gcc
CFLAGS = -Wall -Werror -Wextra -pedantic -g -O3

default: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)
