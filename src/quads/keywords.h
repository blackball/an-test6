// borrowed from http://rlove.org/log/2005102601.

#if __GNUC__ >= 3

//# define Inline		inline __attribute__ ((always_inline))
# define Inline		inline
# define Pure		__attribute__ ((pure))
# define Const	__attribute__ ((const))
# define Noreturn	__attribute__ ((noreturn))
# define Malloc	__attribute__ ((malloc))
# define Must_check	__attribute__ ((warn_unused_result))
# define Deprecated	__attribute__ ((deprecated))
# define Used		__attribute__ ((used))
# define Unused	__attribute__ ((unused))
# define Packed	__attribute__ ((packed))
# define likely(x)	__builtin_expect (!!(x), 1)
# define unlikely(x)	__builtin_expect (!!(x), 0)

#else

# define Inline		/* no inline */
# define Pure		/* no pure */
# define Const	/* no const */
# define Noreturn	/* no noreturn */
# define Malloc	/* no malloc */
# define Must_check	/* no warn_unused_result */
# define Deprecated	/* no deprecated */
# define Used		/* no used */
# define Unused	/* no unused */
# define Packed	/* no packed */
# define likely(x)	(x)
# define unlikely(x)	(x)

#endif
