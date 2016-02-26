.PHONY: clean all check

CC ?= gcc
CFLAGS ?= -MMD -std=gnu99 -Wall -Wextra -ggdb -fms-extensions -rdynamic
CFLAGS := $(CFLAGS) $(INCLUDE)
PARSE_CFLAGS := $(CFLAGS) -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare
LDFLAGS ?= -ldl
RAGEL ?= ragel
RAGELFLAGS ?= -G2
LEMON ?= lemon
PROVEFLAGS ?=

NIFFY_OBJS = niffy.o nif_stubs.o lex.o parse.o atom.o str.o variable.o map.o
OBJS = $(NIFFY_OBJS)
GENERATED = lex.c parse.c parse.h

all: niffy lex_test parse_test

main.c $(NIFFY_OBJS): parse.h

niffy: main.o $(NIFFY_OBJS) | parse.h
	$(CC) $(CFLAGS) -o niffy $^ $(LDFLAGS)

lex_test: lex.o atom.o str.o | parse.h

parse_test: parse_test.o atom.o str.o map.o variable.o nif_stubs.o lex.o parse.o | parse.h

lex.c: lex.rl
	$(RAGEL) $(RAGELFLAGS) -o $@ $^

parse.o: parse.c
	$(CC) $(PARSE_CFLAGS) -c $< -o $@

%.c %.h: %.y
	$(LEMON) $<

clean:
	$(RM) $(OBJS) $(OBJS:.o=.d) $(GENERATED) lex.t niffy

check: lex_test parse_test
	prove $(PROVEFLAGS)

-include $(OBJS:.o=.d)
