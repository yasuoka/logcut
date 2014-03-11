#ifndef MIN
#define MIN(m,n)	((m) < (n))? (m) : (n)
#endif
#ifndef MAX
#define MAX(m,n)	((m) > (n))? (m) : (n)
#endif

#ifdef __cplusplus
extern "C" {
#endif
time_t                get_date (char *, struct timeb *);
#ifdef __cplusplus
}
#endif
