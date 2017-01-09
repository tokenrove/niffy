.PHONY: clean all check test_programs

ERTS_INCLUDE_DIR ?= $(shell erl -smp disable -noshell -s init stop -eval "io:format(\"~s/erts-~s/include/\", [code:root_dir(), erlang:system_info(version)]), halt(0).")

PREFIX ?= /usr/local
INSTALL ?= install

CC ?= gcc
CFLAGS ?= -MMD -std=gnu99 -Wall -Wextra -ggdb -fms-extensions -rdynamic -Wno-missing-field-initializers
CFLAGS := $(CFLAGS) -I$(ERTS_INCLUDE_DIR) $(DEFINES)
PARSE_CFLAGS := $(CFLAGS) -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare
LDFLAGS ?= -ldl
RAGEL ?= ragel
RAGELFLAGS ?= -G2
PROVEFLAGS ?=

NIFFY_OBJS = niffy.o nif_stubs.o lex.o parse.o atom.o str.o variable.o map.o
OBJS = $(NIFFY_OBJS)
GENERATED = lex.c parse.c parse.h parse.out
BINARIES = niffy fuzz_skeleton lex_test parse_test t/leaky_nif.so t/clean_nif.so vendor/lemon/lemon

all: niffy fuzz_skeleton test_programs

test_programs: lex_test parse_test t/leaky_nif.so t/clean_nif.so

main.c fuzz_skeleton.c $(NIFFY_OBJS): parse.h

niffy: main.o $(NIFFY_OBJS) | parse.h
	$(CC) $(CFLAGS) -o niffy $^ $(LDFLAGS)

fuzz_skeleton: fuzz_skeleton.o $(NIFFY_OBJS) | parse.h
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

lex_test: lex.o atom.o str.o | parse.h

parse_test: parse_test.o atom.o str.o map.o variable.o nif_stubs.o lex.o parse.o | parse.h

vendor/lemon/lemon: vendor/lemon/lemon.c
	$(CC) -o $@ $<

t/%.so: t/%.c
	$(CC) $(CFLAGS) -fPIC -shared $^ -o $@

lex.c: lex.rl
	$(RAGEL) $(RAGELFLAGS) -o $@ $^

parse.o: parse.c
	$(CC) $(PARSE_CFLAGS) -c $< -o $@

%.c %.h: %.y | vendor/lemon/lemon
	vendor/lemon/lemon $<

clean:
	$(RM) $(OBJS) $(OBJS:.o=.d) $(GENERATED) $(BINARIES)

check: niffy test_programs
	prove $(PROVEFLAGS)

install: niffy
	$(INSTALL) -d $(PREFIX)/bin
	$(INSTALL) ./niffy $(PREFIX)/bin/niffy

-include $(OBJS:.o=.d)
