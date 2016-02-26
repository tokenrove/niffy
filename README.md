# niffy - NIF testing harness

> ... an ad hoc, informally-specified, bug-ridden, slow implementation
> of 1% of Erlang

This is a very simple harness to allow running NIFs under valgrind and
other debugging tools without all the machinery of building a special
version of the runtime.

It allows you to load a NIF and invoke its functions with arbitrary
Erlang terms (or, at least, some subset of acceptable Erlang terms).

Very little of the ERTS is implemented, so it's likely that your NIF
will break in other ways if it's dependent on much of Erlang.

## Etymology

From WordNet (r) 3.0 (2006) [wn]:

> niffy
>     adj 1: (British informal) malodorous

nifty was already taken.

## Caveats

- allocates lots of memory and doesn't free it
- isn't character encoding aware (no UTF-8 support)
- much of the NIF API is still unimplemented

## Building

Requires [ragel](http://www.colm.net/open-source/ragel/),
[lemon](http://www.hwaci.com/sw/lemon/), and the header files you'd
use to compile a NIF.  Run `make all` to build everything.

## Usage

### Simple

You have a compiled NIF shared object `nif.so` and want to call some
of its functions under valgrind:

```
$ valgrind niffy nif.so <<EOF
V = nif:foo(42).
nif:bar("abc").
nif:baz(V).
EOF
```

(If your NIF fails to load because of missing symbols, try again with
the `--lazy` option.  You may still be able to do some things if you
avoid the parts of the NIF API that aren't implemented.)

niffy expects a series of period-separated function invocations of the
form `Module:Function(Arguments).` where `Module` is probably a NIF
you loaded, `Function` is some function it defines, and the
`Arguments` are constant Erlang terms of an appropriate arity.

You can assign the return value of a call to a variable (pattern
matching is not supported), and use that variable in subsequent calls.
If no variable is supplied, niffy will print the return value of each
call on stdout.

It operates on a line at a time so you can interact with it to some
extent, but keep in mind that function invocations are terminated by a
period.

Note that the NIF's `load` function will not be called unless you
explicitly call `erlang:load_nif(nif_name, [])`.  (Arbitrary BIFs are
not supported, only `erlang:load_nif/2`; you don't need to call this
if your NIF doesn't have a load callback.)

### Multiple NIFs and other libraries

You can specify several SOs on the command-line, all of which will be
loaded.  Not all of them have to be NIFs.

### Fuzzing a NIF with hypothesis

*TODO*

### Fuzzing a NIF with afl_fuzz

Build your NIF with `afl-gcc`:

```
$ cd my_nif
$ CC=afl-gcc CXX=afl-g++ rebar co
```

Build niffy and fuzz_skeleton with `afl-gcc`:
```
$ cd niffy
$ make clean
$ CC=afl-gcc make
```

You'll need a binary to run `afl-fuzz` on; you could supply `niffy`
itself, but you'd primarily be fuzzing niffy's term parser instead of
your NIF.  To make it easier to focus the fuzzing, there is a program
provided called `fuzz_skeleton`.

As is, this program reads stdin as bytes of a binary which it binds to
the variable `Input`, and then reads from a term file specified on the
command line.  If you're primarily interested in fuzzing your NIF with
binaries, this may be all you need, and you'd supply a term file like
the following:

```
_ = erlang:load_nif(jiffy, []).
Term = jiffy:nif_decode_init(Input, []).
jiffy:nif_encode_init(Term, []).
```

and then run `afl-fuzz` like this:

```
$ afl-fuzz -i input_samples -o output -- ./fuzz_skeleton ../jiffy/priv/jiffy.so jiffy_template.term
```

where `input_samples` is a directory containing some small initial
test cases, and `output` is a directory that will be created to hold
the results.

However, you probably want more control over the representation fed to
your NIF, in which case you can modify `fuzz_skeleton.c` to accept
input as appropriate for your NIF.

**Warning:** If your NIF does not load without the `--lazy` option to
niffy, you must set `LD_BIND_LAZY=1` in your environment; otherwise,
afl-fuzz sets `LD_BIND_NOW` and your fuzzer will mysteriously abort on
all your test cases if the NIF uses any unimplemented functionality.

## Alternatives

As an alternative to niffy, you could build a valgrind-enabled OTP,
[using this recipe](https://gist.github.com/gburd/4157112).
