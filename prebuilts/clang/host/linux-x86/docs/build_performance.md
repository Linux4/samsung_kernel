# Benchmarking Build Performance of Android Clang

Build performance is a vital metric as it affects development velocity, CI
resource usage and the headroom for more sophisticated optimisation techniques.
We heavily optimise Android Clang for peak performance.

The Android Clang's compiler wrapper provides a convenient way to analyse the
time spent building the Android platform. It can be produced by running:

```
rm -f /tmp/rusage.txt
TOOLCHAIN_RUSAGE_OUTPUT=/tmp/rusage.txt m -j{processor count / 4}
```

We decrease the parallelism to reduce noise caused by resource contention. Also
make sure RBE is off for the build. The resulting `rusage.txt` can be analysed
with the following Python script:

```
#! /usr/bin/env python3

import json
import sys

total_clang_time = 0
total_clang_tidy_time = 0

with open(sys.argv[1], 'r') as rusage:
  for line in rusage:
    data = json.loads(line)
    if 'clang-tidy' in data['compiler']:
      total_clang_tidy_time += data['elapsed_real_time']
    else:
      total_clang_time += data['elapsed_real_time']

print('Total clang time', total_clang_time)
print('Total clang-tidy time', total_clang_tidy_time)
```
