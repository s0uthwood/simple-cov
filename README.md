# simple-cov

build myclang

```
cmake .
make
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
