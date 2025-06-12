# simple-cov

build myclang

```
cmake .
make
```

compile runtime

```
clang -fPIC -c mycov_runtime.c -o mycov_runtime.o
```

run myclang

```
./myclang -fsanitize=address -g test.c -o test
```

build monitor

```
g++ monitor.cpp -o monitor
```

run test with monitor

```
./monitor ./test
```
