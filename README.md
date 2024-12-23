# Thread library
Implementation of user lever threads and 2 synchronization primitives.
- mutexes
- conditional variables

## User level threads
Are an implementation of the *pthreads* library. Each process has to have an allocated
context. Each **ult** is given a context and a quota *(a unit of time that tells us
how much time each thread is allowed to have CPU before a scheduler has to give it
to another thread)*. The scheduler then checks for the next available thread that is
ready to continue it's execution in a *round-robin* fashion.

Functions
```C
init()
create()
join()
exit()
```

## Build
### Requirements
- Make toolchain
- GCC for compiling **C** programs
### Build commands
```sh
make main       # run a simple example that creates 10 custom threads and let's you see their interactions
make mutex      # usage of the mutex primitive (a shared counter, should avoid race condition and display the correct counter)
make deadlocks  # a deadclock example (detects cycles in the dependency graph using a DFS aproach)
make cond       # producer consumer example, uses the signal function of the conditional variable
make condall    # use the conditional variable broadcast to wake up a bunch of worker threads
make clean      # cleans the bin of all executables
```

### Running each example
```sh
bin/main
bin/main_mutex
bin/deadlocks
bin/cond
bin/condall
```
