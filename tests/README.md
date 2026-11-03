SPI effect queue tests
======================

Run from the repository root (Clang or GCC, no serial device required):

```sh
cc -fsanitize=address,undefined -g -c third-party/include/miniz/miniz.c -o /tmp/effect-miniz.o
c++ -std=c++17 -D__ANDROID__ -fsanitize=address,undefined -g -Isrc -Ithird-party/include tests/effect_queue_tests.cpp src/ZeDMDComm.cpp /tmp/effect-miniz.o -o /tmp/effect-queue-tests
/tmp/effect-queue-tests
```

The Android define selects the existing serial-I/O stubs; the real queue and
worker are exercised. The test checks an independently computed CRC vector,
corruption at every byte, slot/channel/flag validation, the merge rules of
resets, defaults and live effects, buffer ownership, and 1,000 pairs sent to a
slow link: pictures go out in order, waiting ones are replaced, and no light
change is lost on the way.
