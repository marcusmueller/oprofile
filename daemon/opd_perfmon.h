/**
 * @file opd_perfmon.h
 * perfmonctl() handling
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 */

#ifndef OPD_PERFMON_H
#define OPD_PERFMON_H

#ifdef __ia64__

void perfmon_init(void);
void perfmon_exit(void);
void perfmon_start(void);
void perfmon_stop(void);

#else

void perfmon_init(void)
{
}


void perfmon_exit(void)
{
}


void perfmon_start(void)
{
}


void perfmon_stop(void)
{
}

#endif /* __ia64__ */

#endif /* OPD_PERFMON_H */
