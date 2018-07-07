#pragma once

#ifdef _MSC_VER /* msvc */
# pragma warning(disable : 4786)
# ifndef INLINE
# define INLINE __inline
# endif
# ifndef NOINLINE
# define NOINLINE __declspec (noinline)
# endif
#else  /* gcc */
# ifndef INLINE
# define INLINE inline
# endif
# ifndef NOINLINE
# define NOINLINE __attribute__ ((noinline))
# endif
#endif

#ifdef __cplusplus 
extern "C" {
#endif 

typedef struct bip_buf_s    bip_buf_t;


extern INLINE bip_buf_t *	bip_buf_create(size_t capacity);
extern INLINE void			bip_buf_destroy(bip_buf_t *bbuf);
extern INLINE void			bip_buf_reset(bip_buf_t *bbuf);
extern INLINE int			bip_buf_is_full(const bip_buf_t *bbuf);
extern INLINE int			bip_buf_is_empty(const bip_buf_t *bbuf);
extern INLINE size_t		bip_buf_get_free_space(const bip_buf_t *bbuf);

extern INLINE size_t		bip_buf_get_capacity(const bip_buf_t *bbuf);
extern INLINE size_t		bip_buf_get_committed_size(const bip_buf_t *bbuf);
extern INLINE int			bip_buf_get_committed_sum(bip_buf_t *bbuf, int reset);
extern INLINE size_t		bip_buf_get_reservation_size(const bip_buf_t *bbuf);
extern INLINE char *		bip_buf_get_contiguous_block(const bip_buf_t *bbuf);

extern INLINE char *		bip_buf_find_str(const bip_buf_t *bbuf, const char *str, size_t str_len);

/* Reserves space in the buffer for a memory write operation. */
extern char *				bip_buf_reserve(bip_buf_t *bbuf, size_t *size);

/* Like "bip_buf_reserve", but would expand buffer if not enough free space. */
extern char *				bip_buf_force_reserve(bip_buf_t *bbuf, size_t *size);

/* Commits space that has been written to in the buffer. */
extern void					bip_buf_commit(bip_buf_t *bbuf, size_t size);

/* Decommits data from the in use block. */
extern size_t				bip_buf_decommit(bip_buf_t *bbuf, size_t size);


#ifdef __cplusplus 
}
#endif 

/*EOF*/