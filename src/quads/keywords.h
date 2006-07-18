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
# define Noinline __attribute__ ((noinline))
#else

# define Inline
# define Pure
# define Const
# define Noreturn
# define Malloc
# define Must_check
# define Deprecated
# define Used
# define Unused
# define Packed
# define likely(x)	(x)
# define unlikely(x)	(x)
# define Noinline

#endif
