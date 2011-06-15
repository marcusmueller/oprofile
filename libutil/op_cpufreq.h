/**
 * @file op_cpufreq.h
 * get cpu frequency declaration
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 * @author Suravee Suthikulpanit
 */

#ifndef OP_CPUFREQ_H
#define OP_CPUFREQ_H

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * return the estimated cpu frequency in Mhz,
 * return 0 if this information
 * is not avalaible e.g. sparc64 with a non SMP kernel
 */
double op_cpu_frequency(void);

#if defined(__cplusplus)
}
#endif

#endif /* !OP_CPUFREQ_H */
