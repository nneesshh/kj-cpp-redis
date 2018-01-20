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
	if (NULL == needle
		|| 0 == needlelen) // Empty needle.
		return (char *)haystack;

	const char needle_first = *needle;

	// Runs memchr() on the first section of the haystack as it has a lower
	// algorithmic complexity for discarding the first non-matching characters.
	char *first_match = memchr(haystack, needle_first, haystacklen);
	if (NULL == first_match) // First character of needle is not in the haystack.
		return NULL;

	// First characters of haystack and needle are the same now. Both are
	// guaranteed to be at least one character long.
	// Now computes the sum of the first needle_len characters of haystack
	// minus the sum of characters values of needle.
	const char *i_haystack = first_match + 1
		, *i_needle = needle + 1;
	unsigned int sums_diff = *first_match;
	bool identical = true;

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

	size_t needle_len = i_needle - needle;
	size_t needle_len_1 = needle_len - 1;

	// Loops for the remaining of the haystack, updating the sum iteratively.
	const char *sub_start;
	for (sub_start = first_match; i_haystack < haystack + haystacklen; ++i_haystack) {
		sums_diff -= *sub_start++;
		sums_diff += *i_haystack;

		// Since the sum of the characters is already known to be equal at that
		// point, it is enough to check just needle_len-1 characters for
		// equality.
		if (0 == sums_diff
			&& needle_first == *sub_start // Avoids some calls to memcmp.
			&& memcmp(sub_start, needle, needle_len_1) == 0)
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
	bip_buf_t *bbuf = calloc(1, sizeof(bip_buf_t));
	bbuf->_available_capacity = capacity;
	bbuf->_buffer = malloc(bbuf->_available_capacity * 2);
	bbuf->_committed_sum = 0;

	bip_buf_reset(bbuf);
	return bbuf;
}

/**------------------------------------------------------------------------------
*
*/
void
bip_buf_destroy(bip_buf_t *bbuf) {
	free(bbuf->_buffer);
	free(bbuf);
}

/**------------------------------------------------------------------------------
*
*/
void
bip_buf_reset(bip_buf_t *bbuf) {
	bbuf->_a_start = bbuf->_a_end = 0;
	bbuf->_r_start = bbuf->_r_end = 0;
}

/**------------------------------------------------------------------------------
*
*/
int
bip_buf_is_empty(const bip_buf_t *bbuf) {
	return (0 == bip_buf_get_committed_size(bbuf));
}

/**------------------------------------------------------------------------------
*
*/
int
bip_buf_is_full(const bip_buf_t *bbuf) {
	return (bip_buf_get_capacity(bbuf) == bip_buf_get_committed_size(bbuf));
}

/**------------------------------------------------------------------------------
*
*/
size_t
bip_buf_get_free_space(const bip_buf_t *bbuf) {
	return bip_buf_get_capacity(bbuf) - bip_buf_get_committed_size(bbuf);
}

/**------------------------------------------------------------------------------
*
*/
size_t
bip_buf_get_capacity(const bip_buf_t *bbuf) {
	return bbuf->_available_capacity;
}

/**------------------------------------------------------------------------------
*
*/
size_t
bip_buf_get_committed_size(const bip_buf_t *bbuf) {
	return bbuf->_a_end - bbuf->_a_start;
}

/**------------------------------------------------------------------------------
*
*/
int
bip_buf_get_committed_sum(bip_buf_t *bbuf, int reset) {
	int committed_sum = bbuf->_committed_sum;
	bbuf->_committed_sum = (reset > 0) ? 0 : bbuf->_committed_sum;
	return committed_sum;
}

/**------------------------------------------------------------------------------
*
*/
size_t
bip_buf_get_reservation_size(const bip_buf_t *bbuf) {
	return bbuf->_r_end - bbuf->_r_start;
}

/**------------------------------------------------------------------------------
*
*/
char *
bip_buf_get_contiguous_block(const bip_buf_t *bbuf) {
	return bbuf->_buffer + bbuf->_a_start;
}

/**------------------------------------------------------------------------------
*
*/
char *
bip_buf_find_str(const bip_buf_t *bbuf, const char *str, size_t str_len) {
	char *start = bip_buf_get_contiguous_block(bbuf);
	size_t start_sz = bip_buf_get_committed_size(bbuf);
	return fast_strstr(start, start_sz, str, str_len);
}

/**------------------------------------------------------------------------------
*
*/
char *
bip_buf_reserve(bip_buf_t *bbuf, size_t *size) {
	// check already reserve
	if (bip_buf_get_reservation_size(bbuf) > 0) {
		(*size) = 0;
		return NULL;
	}
	else {
		// we always allocate reserve space after A.
		size_t freespace = bip_buf_get_free_space(bbuf);
		(*size) = ((*size) > freespace) ? freespace : (*size);
		if (0 == (*size))
			return NULL;

		// check wrap Region A
		if (bbuf->_a_start >= bbuf->_available_capacity) {
			size_t reg_a_size = bip_buf_get_committed_size(bbuf);

			// wrap A
			if (reg_a_size > 0)
				memcpy(bbuf->_buffer, bbuf->_buffer + bbuf->_a_start, reg_a_size);

			bbuf->_a_start = 0;
			bbuf->_a_end = reg_a_size;
		}

		bbuf->_r_start = bbuf->_a_end;
		bbuf->_r_end = bbuf->_r_start + (*size);
		return bbuf->_buffer + bbuf->_r_start;
	}
}

/**------------------------------------------------------------------------------
*
*/
char *
bip_buf_force_reserve(bip_buf_t *bbuf, size_t *size) {
	// check already reserve
	if (bip_buf_get_reservation_size(bbuf) > 0) {
		(*size) = 0;
		return NULL;
	}
	else {
		if (0 == (*size))
			return NULL;

		// we always allocate reserve space after A.
		size_t freespace = bip_buf_get_free_space(bbuf);
		if ((*size) > freespace) {
			size_t sz = bip_buf_get_committed_size(bbuf);
			if (0 == sz) {
				bbuf->_available_capacity = next_power_of_two(bbuf->_available_capacity + (*size));
				bbuf->_buffer = malloc(bbuf->_available_capacity * 2);

				bbuf->_a_start = bbuf->_a_end = 0;
			}
			else {
				char *tmp = bbuf->_buffer;
				bbuf->_available_capacity = next_power_of_two(bbuf->_available_capacity + (*size));
				bbuf->_buffer = malloc(bbuf->_available_capacity * 2);
				memcpy(bbuf->_buffer, tmp + bbuf->_a_start, sz);
				free(tmp);

				bbuf->_a_start = 0;
				bbuf->_a_end = sz;
			}
		}

		bbuf->_r_start = bbuf->_a_end;
		bbuf->_r_end = bbuf->_r_start + (*size);
		return bbuf->_buffer + bbuf->_r_start;
	}
}

/**------------------------------------------------------------------------------
*
*/
void
bip_buf_commit(bip_buf_t *bbuf, size_t size) {
	// check already reserve
	size_t reservation_size = bip_buf_get_reservation_size(bbuf);
	if (reservation_size > 0) {
		// if we try to commit more space than we asked for, clip to the size we asked for.
		size = (size > reservation_size) ? reservation_size : size;

		// commit
		bbuf->_a_end = bbuf->_r_start + size;
	}

	// check wrap Region A
	if (bbuf->_a_start >= bbuf->_available_capacity) {
		size_t reg_a_size = bip_buf_get_committed_size(bbuf);

		// wrap A
		if (reg_a_size > 0)
			memcpy(bbuf->_buffer, bbuf->_buffer + bbuf->_a_start, reg_a_size);

		bbuf->_a_start = 0;
		bbuf->_a_end = reg_a_size;
	}

	// release reservation
	bbuf->_r_start = bbuf->_r_end = 0;
	bbuf->_committed_sum += size;
}

/**------------------------------------------------------------------------------
*
*/
size_t
bip_buf_decommit(bip_buf_t *bbuf, size_t size) {
	size_t committed_size = bip_buf_get_committed_size(bbuf);
	size_t bytes = (size >= committed_size) ? committed_size : size;
	bbuf->_a_start += bytes;
	return bytes;
}

/** -- EOF -- **/