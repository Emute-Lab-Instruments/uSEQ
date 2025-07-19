# fast_any
A faster alternative to boost:any / std::any

# Benchmarks

From my benchmarks in <a href=https://github.com/cross-platform/dspatch>DSPatch</a>, `fast_any` is over 25% faster than `std::any`

`std::any`:
```
0x Buffers, 0x Threads, 10000x Components: 0.125533ms
```

`fast_any`:
```
0x Buffers, 0x Threads, 10000x Components: 0.093862ms
```

- Test code: https://github.com/cross-platform/dspatch/blob/master/tests/main.cpp
- Test branch using `std::any`: https://github.com/cross-platform/dspatch/pull/53
