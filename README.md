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

Requires [ragel](http://www.colm.net/open-source/ragel/), and the
header files you'd use to compile a NIF.  Run `make all` to build
everything.

## Usage

### Simple

You have a compiled NIF shared object `nif.so` and want to call some
of its functions under valgrind:

```
$ valgrind niffy nif.so <<EOF
V = nif:foo(42).
V.
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
call on stdout.  A variable alone will print its bound value.

It operates on a line at a time so you can interact with it to some
extent, but keep in mind that function invocations are terminated by a
period.

Note that the NIF's `load` function will not be called unless you
explicitly call `niffy:load_nif(nif_name, [])`.  (You don't need to
call this if your NIF doesn't have a load callback.)

### Multiple NIFs and other libraries

You can specify several SOs on the command-line, all of which will be
loaded.  Not all of them have to be NIFs.

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
_ = niffy:load_nif(jiffy, []).
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

### Tracing eunit and running your NIF calls through niffy

You can setup a tracing process that feeds all the calls to your NIF
through niffy running under valgrind through a port, and then invoke
your eunit test suite.  For the simplest NIFs, something like this is
sufficient:

```erlang
start() ->
    Port = open_port({spawn_executable, "/usr/bin/valgrind"},
                     [{args, ["--error-exitcode=42", "--",
                              "../niffy/niffy", "./priv/my_nif.so", "-q"]},
                      binary, stream, exit_status,
                      {line, 1024}]),
    Port ! {self(), {command, <<"_ = niffy:load_nif(my_nif, []).\n">>}},
    erlang:trace_pattern({my_nif, '_', '_'}, true, []),
    erlang:trace(all, true, [call]),
    loop(Port).

loop(Port) ->
    receive
        {trace, _, call, {Module, Function, Arguments}} ->
            Port ! {self(), {command, format_mfa(Module, Function, Arguments)}},
            loop(Port);
        {_Port, {exit_status, Status}} ->
            io:format("niffy exited with code ~p~n", [Status])
    end.
```

In more complex situations, you may need to handle certain calls
(those that return a non-printable term, for example) and rewrite them
to the form `Handle = my_nif:creation_call(Args).`, and suitably
replace later arguments that containing that non-printable term with
`Handle`.  An example should soon be included here, but until then,
feel free to contact me about it.


## Alternatives

As an alternative to niffy, you could build a valgrind-enabled OTP,
[using this recipe](https://gist.github.com/gburd/4157112).

## Contact

niffy is maintained by Julian Squires <julian@cipht.net>.
