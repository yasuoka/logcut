#include <sys/cdefs.h>

#ifdef	LOGCUT_DEBUG
#define	LOGCUT_DBG(x)	fprintf x
#else
#define	LOGCUT_DBG(x)
#endif

#ifndef MIN
#define MIN(m,n)	((m) < (n))? (m) : (n)
#endif
#ifndef MAX
#define MAX(m,n)	((m) > (n))? (m) : (n)
#endif

/* "struct timeb" is absolute, fake it */
struct timeb {
	time_t	 time;
	int	 timezone;
};

__BEGIN_DECLS
int	 yyparse(void);
time_t	 get_date(char *, struct timeb *);
__END_DECLS
