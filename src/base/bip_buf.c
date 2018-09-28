#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "bip_buf.h"

// Round up to the next power of two
static uint32_t
next_power_of_two(uint32_t x) {
	--x;
	x |= x >> 1; // handle 2 bit numbers
	x |= x >> 2; // handle 4 bit numbers
	x |= x >> 4; // handle 8 bit numbers
	x |= x >> 8; // handle 16 bit numbers
	x |= x >> 16; // handle 32 bit numbers
	return ++x;
}

/**
* Finds the first occurrence of the sub-string needle in the string haystack.
* Returns NULL if needle was not found.
*/
static char *
fast_strstr(const char *haystack, size_t haystacklen, const char *needle, size_t needlelen) {
	size_t needle_len, needle_len_1;
	char needle_first;
	const char *first_match, *i_haystack, *i_needle, *sub_start;
	unsigned int sums_diff;
	bool identical;

	if (NULL == needle
		|| 0 == needlelen) // Empty needle.
		return (char *)haystack;

	needle_first = *needle;

	// Runs memchr() on the first section of the haystack as it has a lower
	// algorithmic complexity for discarding the first non-matching characters.
	first_match = memchr(haystack, needle_first, haystacklen);
	if (NULL == first_match) // First character of needle is not in the haystack.
		return NULL;

	// First characters of haystack and needle are the same now. Both are
	// guaranteed to be at least one character long.
	// Now computes the sum of the first needle_len characters of haystack
	// minus the sum of characters values of needle.
	i_haystack = first_match + 1;
	i_needle = needle + 1;
	sums_diff = *first_match;
	identical = true;

	while (i_haystack < haystack + haystacklen
		&& i_needle < needle + needlelen) {
		sums_diff += *i_haystack;
		sums_diff -= *i_needle;
		identical &= (*i_haystack++ == *i_needle++);
	}

	// i_haystack now references the (needle_len + 1)-th character.
	if (i_needle < needle + needlelen) // haystack is smaller than needle.
		return NULL;
	else if (identical)
		return (char *)first_match;

	needle_len = i_needle - needle;
	needle_len_1 = needle_len - 1;

	// Loops for the remaining of the haystack, updating the sum iteratively.
	for (sub_start = first_match; i_haystack < haystack + haystacklen; ++i_haystack) {
		sums_diff -= *sub_start++;
		sums_diff += *i_haystack;

		// Since the sum of the characters is already known to be equal at that
		// point, it is enough to check just needle_len-1 characters for
		// equality.
		if (0 == sums_diff
			&& needle_first == *sub_start // Avoids some calls to memcmp.
			&& 0 == memcmp(sub_start, needle, needle_len_1))
			return (char *)sub_start;
	}
	return NULL;
}

/************************************************************************/
/* struct bip_buf_s                                                     */
/************************************************************************/
struct bip_buf_s {
	/* the mirror buffer, it's real size is the double size of available capacity */
	char *_buffer;

	/* user usable capacity ( = half size of the full mirror buffer) */
	size_t _available_capacity;

	/* stats for commit */
	int _committed_sum;

	/* region A only, we won't use region B because we must keep region A as the unique contiguous block */
	size_t _a_start, _a_end;

	/* region Reserve */
	size_t _r_start, _r_end;
};

/**------------------------------------------------------------------------------
*
*/
bip_buf_t *
bip_buf_create(size_t capacity) {
	bip_buf_t *bb = calloc(1, sizeof(bip_buf_t));

	bb->_available_capacity = capacity;
	bb->_buffer = malloc(bb->_available_capacity * 2);
	bb->_committed_sum = 0;

	bip_buf_reset(bb);
	return bb;
}

/**------------------------------------------------------------------------------
*
*/
void
bip_buf_destroy(bip_buf_t *bb) {
	free(bb->_buffer);
	free(bb);
}

/**------------------------------------------------------------------------------
*
*/
void
bip_buf_reset(bip_buf_t *bb) {
	bb->_a_start = bb->_a_end = 0;
	bb->_r_start = bb->_r_end = 0;
}

/**------------------------------------------------------------------------------
*
*/
int
bip_buf_is_empty(const bip_buf_t *bb) {
	return (0 == bip_buf_get_committed_size(bb));
}

/**------------------------------------------------------------------------------
*
*/
int
bip_buf_is_full(const bip_buf_t *bb) {
	return (bip_buf_get_capacity(bb) == bip_buf_get_committed_size(bb));
}

/**------------------------------------------------------------------------------
*
*/
size_t
bip_buf_get_capacity(const bip_buf_t *bb) {
	return bb->_available_capacity;
}

/**------------------------------------------------------------------------------
*
*/
size_t
bip_buf_get_committed_size(const bip_buf_t *bb) {
	return bb->_a_end - bb->_a_start;
}

/**------------------------------------------------------------------------------
*
*/
int
bip_buf_get_committed_sum(bip_buf_t *bb, int reset) {
	int committed_sum = bb->_committed_sum;

	bb->_committed_sum = (reset > 0) ? 0 : bb->_committed_sum;
	return committed_sum;
}

/**------------------------------------------------------------------------------
*
*/
size_t
bip_buf_get_reservation_size(const bip_buf_t *bb) {
	return bb->_r_end - bb->_r_start;
}

/**------------------------------------------------------------------------------
*
*/
char *
bip_buf_get_contiguous_block(const bip_buf_t *bb) {
	return bb->_buffer + bb->_a_start;
}

/**------------------------------------------------------------------------------
*
*/
char *
bip_buf_find_str(const bip_buf_t *bb, const char *str, size_t str_len) {
	char *buf_start = bip_buf_get_contiguous_block(bb);
	size_t buf_size = bip_buf_get_committed_size(bb);

	return fast_strstr(buf_start, buf_size, str, str_len);
}

/**------------------------------------------------------------------------------
*
*/
char *
bip_buf_reserve(bip_buf_t *bb, size_t *size) {
	size_t region_a_size, freespace;

	// check already reserve
	if (bip_buf_get_reservation_size(bb) > 0) {
		(*size) = 0;
		return NULL;
	}
	else {
		region_a_size = bip_buf_get_committed_size(bb);
		freespace = bb->_available_capacity - region_a_size;

		if (0 == freespace) {
			(*size) = 0;
			return NULL;
		}

		(*size) = freespace; /* all freespace would be reserved */

		// check Region A
		if (0 == region_a_size) {
			bb->_a_start = bb->_a_end = 0;
		}
		else if (bb->_available_capacity + bb->_available_capacity - bb->_a_end < (*size)) {
			// wrap A
			memcpy(bb->_buffer, bb->_buffer + bb->_a_start, region_a_size);

			bb->_a_start = 0;
			bb->_a_end = region_a_size;
		}

		bb->_r_start = bb->_a_end;
		bb->_r_end = bb->_r_start + (*size);
		return bb->_buffer + bb->_r_start;
	}
}

/**------------------------------------------------------------------------------
*
*/
char *
bip_buf_force_reserve(bip_buf_t *bb, const size_t size) {
	size_t region_a_size, freespace, buf_size;

	// check already reserve
	if (bip_buf_get_reservation_size(bb) > 0
		|| 0 == size) {
		return NULL;
	}
	else {
		region_a_size = bip_buf_get_committed_size(bb);
		freespace = bb->_available_capacity - region_a_size;

		if (0 == freespace) {
			return NULL;
		}

		// is freespace enough ?
		if (size > freespace) {
			buf_size = bip_buf_get_committed_size(bb);
			if (0 == buf_size) {
				bb->_available_capacity = next_power_of_two(bb->_available_capacity + size);
				bb->_buffer = malloc(bb->_available_capacity + bb->_available_capacity); /* bb->_available_capacity * 2 */

				bb->_a_start = bb->_a_end = 0;
			}
			else {
				char *tmp = bb->_buffer;
				bb->_available_capacity = next_power_of_two(bb->_available_capacity + size);
				bb->_buffer = malloc(bb->_available_capacity + bb->_available_capacity); /* bb->_available_capacity * 2 */
				memcpy(bb->_buffer, tmp + bb->_a_start, buf_size);
				free(tmp);

				bb->_a_start = 0;
				bb->_a_end = buf_size;
			}
		}
		else {
			region_a_size = bip_buf_get_committed_size(bb);

			// check Region A
			if (0 == region_a_size) {
				bb->_a_start = bb->_a_end = 0;
			}
			else if (bb->_available_capacity + bb->_available_capacity - bb->_a_end < size) {
				// wrap A
				memcpy(bb->_buffer, bb->_buffer + bb->_a_start, region_a_size);

				bb->_a_start = 0;
				bb->_a_end = region_a_size;
			}
		}

		bb->_r_start = bb->_a_end;
		bb->_r_end = bb->_r_start + size;
		return bb->_buffer + bb->_r_start;
	}
}

/**------------------------------------------------------------------------------
*
*/
void
bip_buf_commit(bip_buf_t *bb, size_t size) {
	// check already reserve
	size_t reservation_size = bip_buf_get_reservation_size(bb);
	if (reservation_size > 0) {
		// if we try to commit more space than we asked for, clip to the size we asked for.
		size = (size > reservation_size) ? reservation_size : size;

		// commit
		bb->_a_end = bb->_r_start + size;
	}

	// release reservation
	bb->_r_start = bb->_r_end = 0;
	bb->_committed_sum += size;
}

/**------------------------------------------------------------------------------
*
*/
size_t
bip_buf_decommit(bip_buf_t *bb, size_t size) {
	size_t committed_size = bip_buf_get_committed_size(bb);
	size_t bytes = (size >= committed_size) ? committed_size : size;

	bb->_a_start += bytes;
	return bytes;
}

/** -- EOF -- **/