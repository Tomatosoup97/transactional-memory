/* Define a proposition as likely true */
#undef likely
#ifdef __GNUC__
#define likely(prop) __builtin_expect((prop) ? 1 : 0, 1)
#else
#define likely(prop) (prop)
#endif

/* Define a proposition as likely false */
#undef unlikely
#ifdef __GNUC__
#define unlikely(prop) __builtin_expect((prop) ? 1 : 0, 0)
#else
#define unlikely(prop) (prop)
#endif

/* Define one or several attributes */
#undef as
#ifdef __GNUC__
#define as(type...) __attribute__((type))
#else
#define as(type...)
#warning This compiler has no support for GCC attributes
#endif
