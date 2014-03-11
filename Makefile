#
# Makefile for BSD
#
# $Id: Makefile,v 1.3 2006/02/11 04:40:18 yasuoka Exp $
PROG=		logcut
CFLAGS+=	-g -Wall
YPREFIX=	getdate_
SRCS=		logcut.c getdate.y
NOMAN=		#

.include <bsd.prog.mk>
