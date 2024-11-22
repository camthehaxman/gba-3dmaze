#!/usr/bin/env python

FRACT_BITS = 15  # needs to be kept in sync with fixedpoint.h

print('#include "assert.h"')
print('#include "fixedpoint.h"')
print('static_assert(FRACT_BITS == %i);' % FRACT_BITS)
print('const fixed_t recipTable[] = {')
for i in range(0, 1 << FRACT_BITS):
	print('TO_FIXED(1.0/%.8f),' % (i / (1 << FRACT_BITS)))
print('};')
