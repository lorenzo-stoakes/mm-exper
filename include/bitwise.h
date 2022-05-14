#pragma once

#include <stdint.h>

#define ALIGN_UP_IMPL_2

/*
 * Align `value` to power-of-2 `align_to`, i.e. clear bits [0, align_to).
 *
 * This aligns _downwards_ so e.g.:
 *   align64(0b01`011, 0b01`000) == 0b01`000.
 */
static inline uint64_t align64(uint64_t value, uint64_t align_to)
{
	// A power-of-2 subtracted by 1 sets all lower bits. Complement
	// generates a mask of all other bits.
	return value & ~(align_to - 1);
}

/*
 * Align `value` to power-of-2 `align_to` rounding up to the next aligned value
 * (leaving the value unchanged if already aligned).
 *
 * This aligns _upwards_ so e.g.:
 *   align64_up(0b01`011, 0b01`000) == 0b10`000.
 */
static inline uint64_t align64_up(uint64_t value, uint64_t align_to)
{
	// By adding the lower bits of `align_to` we will be either simply go on
	// to clear these lower bits or we will clear the lower bits of `value +
	// align_to`.
#if defined(ALIGN_UP_IMPL_1)
	return align64(value + align_to - 1, align_to);
#elif defined(ALIGN_UP_IMPL_2)
	// We could alternatively achieve this by invoking two's complement
	// (complement then add 1) which will add `align_to - (value & (align_to
	// - 1))` except if the lower bits are zeroed, achieving the same thing.
	return value + (-value & (align_to - 1));
#else
#error align64_up() implementation not specified.
#endif
}

#undef ALIGN_UP_IMPL_1
#undef ALIGN_UP_IMPL_2
