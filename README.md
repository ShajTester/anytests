# anytests

_Any tests for ShajTester OpenBMC_

## How to build

```
mkdir build && cd build
cmake ..
cmake --build . -- -j4
cmake --build . --target test
```


- Разбор параметров командной строки [Taywee/args](https://github.com/Taywee/args)
