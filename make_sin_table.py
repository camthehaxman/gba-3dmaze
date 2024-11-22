#!/usr/bin/env python3

import math

print('#include "fixedpoint.h"')
print('const fixed_t sineTable[] = {')
for i in range(0, 65536//4+1):
	print('TO_FIXED(' + str(math.sin(i*2*math.pi/65536)) + '),')
print('};')
