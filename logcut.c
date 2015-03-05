/*-
 * Copyright (c) 2006 Yasuoka Masahiko <yasuoka@yasuoka.net>
 * All rights reserved.
 */
/* $Id: logcut.c,v 1.10 2006/02/13 05:33:53 yasuoka Exp $ */
/*
 * Select lines of a log file with given timestamp using binary search.
 *
 * example:
 *	logcut -f '2 hours ago' -a /var/log/messages
 *	logcut -f '5:55' -t '8:30' /usr/local/tomcat5/logs/localhost_log.txt
 *	logcut -f '2/1' -t '2/8' -w /var/log/httpd/access_log
 *
 * Log records of syslog has no 'year' information.  'logcut' fills up the
 * year using current time's year.  If month of the log is less than or
 * equals to the current month, it fills the current your, otherwise it fills
 * (current year - 1) as the year of the log record.
 * 
 * As 'logcut' uses binary search, logs must be sorted.  Log records without
 * year, years will be filled up but they must be sorted.
 *
#define	LOGCUT_DEBUG	1
 */
#include <sys/types.h>
#if defined(__GLIBC__) && (__GLIBC__ >= 2 )
#define _XOPEN_SOURCE
#define __USE_XOPEN
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <unistd.h>

#include "defs.h"

static time_t curr_time;
static struct tm curr_tm;

static void adjust_tm (struct tm *);
static int read_time (int, const char *, time_t *);
static off_t line_next (int, off_t);
static off_t line_head (int, off_t);
static off_t search_by_time (int, const char *, time_t, off_t, off_t);
static int cut_file (int, int, const char *, time_t, time_t);
static void usage (void);

static const char *ISO_FMT		= "%Y-%m-%d %T";
static const char *ANSI_FMT		= "%b %d %T";
static const char *APACHE_FMT		= "%d/%b/%Y:%T";

#ifdef	LOGCUT_DEBUG	
#define	LOGCUT_DBG(x)	fprintf x
#else
#define	LOGCUT_DBG(x)	
#endif

static void
usage(void)
{
	fprintf(stderr,
	    "usage: logcut [-iawh] [-F format] -f date_spec [-t date_spec] "
		"file...\n"
	    "\t-F: Specify timestamp field format in strptime(3)\n"
	    "\t-a: Use ANSI/syslog timestamp format (%%b %%d %%T)\n"
	    "\t-i: Use ISO timestamp format (%%Y-%%m-%%d %%T)\n"
	    "\t-w: Use apache timestamp format ([%%d/%%b/%%Y:%%T)\n");
}

/** program entry point */
int
main(int argc, char *argv[])
{
	int ch, fd;
	const char *fmt = NULL;
	time_t f, t;
	struct timeb tb;
	extern int optind;
	extern char *optarg;

	tzset();

	time(&curr_time);
	localtime_r(&curr_time, &curr_tm);

	memset(&tb, 0, sizeof(tb));
	tb.time = curr_time;
#if defined(__GLIBC__) || defined(__CYGWIN__)
	{
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	tb.timezone = tz.tz_minuteswest;
	}
#else
	/*
	 * as localtime_r(3) 
	 *	tm_gmtoff is the offset (in seconds) of the time represented
	 *	from UTC, with positive values indicating east of the Prime
	 *	Meridian.
	 * on the other hands tb.tb_timezone is minutes *west* of CUT.
	 */
	tb.timezone = -1 * (curr_tm.tm_gmtoff / 60);
#endif
	f = -1;
	t = curr_time;

	while ((ch = getopt(argc, argv, "iawhF:f:t:")) != -1) {
		switch (ch) {
		case 'i':
			if (fmt != NULL)
				goto fmt_redef;
			fmt = ISO_FMT;
			break;
		case 'a':
			if (fmt != NULL)
				goto fmt_redef;
			fmt = ANSI_FMT;
			break;
		case 'w':
			if (fmt != NULL)
				goto fmt_redef;
			fmt = APACHE_FMT;
			break;
		case 't':
			if ((t = get_date(optarg, &tb)) < 0) {
				fprintf(stderr, "parse error: %s\n", optarg);
				exit(1);
			}
			break;
		case 'f':
			if ((f = get_date(optarg, &tb)) < 0) {
				fprintf(stderr, "parse error: %s\n", optarg);
				exit(1);
			}
			break;
		case 'F':
			if (fmt != NULL) {
		fmt_redef:
				fprintf(stderr,
				    "Format is already specified: %s\n", fmt);
				exit(1);
			}
			fmt = optarg;
			break;
		default:
		case 'h':
			usage();
			exit(1);
		}
	}
	argc -= optind;
	if (f == -1) {
		usage();
		exit(1);
	}
	if (fmt == NULL)
		fmt = ANSI_FMT;
	argv += optind;

	if (argc <= 0) {
		usage();
		exit(1);
	}
	while (argc > 0) {
		if ((fd = open(argv[0], O_RDONLY)) < 0) {
			perror(argv[0]);
			exit(1);
		}
		if (cut_file(fd, STDOUT_FILENO, fmt, f, t) != 0) {
			perror(argv[0]);
			exit(1);
		}
		close(fd);
		argv++;
		argc--;
	}

	exit(0);
}

static void
adjust_tm(struct tm * tm)
{
	if (tm->tm_year == 0) {
		if (curr_tm.tm_mon < tm->tm_mon)
			tm->tm_year = curr_tm.tm_year - 1;
		else
			tm->tm_year = curr_tm.tm_year;
	}
}

static int
read_time(int fd, const char *fmt, time_t * t)
{
	int sz, web_log;
	struct tm tm;
	char *bufp, buf[40];

	web_log = (fmt == APACHE_FMT)? 1 : 0;
	memset(&tm, 0, sizeof(tm));
	tm.tm_mon = -1;
	if ((sz = read(fd, buf, sizeof(buf) - 1)) < 0) {
		perror("read");
		exit(1);
	}
	if (sz == 0)
		return -1;
	buf[sz] = '\0';
	if (web_log && (bufp = strchr(buf, '[')) != NULL)
		bufp++;
	else
		bufp = buf;
	if (strptime(bufp, fmt, &tm) == NULL)
		return 0;
	adjust_tm(&tm);

	*t = mktime(&tm);

	return 1;
}

static off_t
line_next(int fd, off_t off)
{
	int sz;
	char *nl, buf[BUFSIZ];
	int sz0;

	sz0 = sizeof(buf) - 1;
	lseek(fd, off, SEEK_SET);
	while ((sz = read(fd, buf, sz0)) > 0) {
		buf[sz] = '\0';
		if ((nl = strchr(buf, '\n')) != NULL) {
			off += (nl - buf) + 1;
			break;
		}
		off += sz;
	}
	lseek(fd, off, SEEK_SET);
	return off;
}

static off_t
line_head(int fd, off_t off)
{
	off_t r;
	size_t sz;
	char *nl, buf[BUFSIZ];
	int sz0;

	sz0 = sizeof(buf) - 1;

	while ((r = lseek(fd, MAX(off - sz0, 0), SEEK_SET)) >= 0) {
		sz0 = off - r;
		off = r;
		if (sz0 <= 0)
			break;
		if ((sz = read(fd, buf, sz0)) < 0) {
			perror("read");
			exit(1);
		}
		buf[sz] = '\0';
		if ((nl = strrchr(buf, '\n')) != NULL) {
			off += (nl - buf) + 1;
			break;
		}
	}
	if (r < 0)
		off = 0;
	lseek(fd, off, SEEK_SET);

	return off;
}

#ifdef LOGCUT_DEBUG
static char *my_ctime(time_t *t, char *buf, int lbuf)
{
	struct tm tm;

	memset(&tm, 0, sizeof(tm));

	localtime_r(t, &tm);

	snprintf(buf, lbuf, "%04d-%02d-%02d %02d:%02d:%02d",
	    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	    tm.tm_hour, tm.tm_min, tm.tm_sec);

	return buf;
}
#endif

static off_t
search_by_time(int fd, const char *fmt, time_t t, off_t off_m, off_t off_n)
{
	int rval;
	time_t val_p;
	off_t off_p;
#ifdef	LOGCUT_DEBUG
	char buf0[256], buf1[256];
#endif

	while (off_m < off_n) {
		off_p = line_head(fd, (off_m + off_n) / 2);
		do {
			rval = read_time(fd, fmt, &val_p);
			if (off_p > 1 && rval == 0) {
				off_p = line_head(fd, off_p - 1);
				continue;
			}
			break;
		} while (1);
		if (t <= val_p) {
			LOGCUT_DBG((stderr, "n %10lld => %10lld %s <= %s\n",
			    off_n, off_p, my_ctime(&t, buf0, sizeof(buf0)),
			    my_ctime(&val_p, buf1, sizeof(buf1))));
			off_n = off_p;
		} else {
			off_p = line_next(fd, (off_m + off_n) / 2);
			do {
				rval = read_time(fd, fmt, &val_p);
				if (rval == 0) {
					off_p = line_next(fd, off_p);
					continue;
				}
				break;
			} while (off_p < off_n);
			LOGCUT_DBG((stderr,
			    "m %10lld => %10lld %s >  %s\n",
			    off_n, off_p, my_ctime(&t, buf0, sizeof(buf0)),
			    my_ctime(&val_p, buf1, sizeof(buf1))));
			off_m = off_p;
		}

	}

	return off_m;
}

static int
cut_file(int fd, int outfd, const char *fmt, time_t f, time_t t)
{
	int sz;
	char buf[BUFSIZ];
	struct stat st;
	off_t off_b, off_e;

	if (fstat(fd, &st) != 0)
		return 1;

	LOGCUT_DBG((stderr, "Searching %s", ctime(&f)));
	off_b = search_by_time(fd, fmt, f, 0, st.st_size);
	LOGCUT_DBG((stderr, "Searching %s", ctime(&t)));
	off_e = search_by_time(fd, fmt, t, off_b, st.st_size);
	lseek(fd, off_b, SEEK_SET);

	while (off_b < off_e &&
	    (sz = read(fd, buf, MIN(sizeof(buf) - 1, off_e - off_b))) > 0) {
		off_b += sz;
		write(outfd, buf, sz);
	}

	return 0;
}
