/**
 * @file opd_cookie.h
 * cookie -> name cache
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 */

#ifndef OPD_COOKIE_H
#define OPD_COOKIE_H

typedef unsigned long long cookie_t;

#define INVALID_COOKIE ~0LLU

/**
 * Shift value to remove trailing zero on a dcookie value, 7 is sufficient
 * for most architecture
 */
#define DCOOKIE_SHIFT	7

/**
 * Return the name of the given dcookie. May return
 * NULL on failure.
 */
char const * find_cookie(cookie_t cookie);

/** return non zero if we must record sample for this cookie */
int is_cookie_filtered(cookie_t cookie);

/** give a textual description of the cookie */
char const * verbose_cookie(cookie_t cookie);

void cookie_init(void);

#endif /* OPD_COOKIE_H */
