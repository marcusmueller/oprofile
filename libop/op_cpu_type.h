/**
 * @file op_cpu_type.h
 * CPU type determination
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_CPU_TYPE_H
#define OP_CPU_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

/** supported cpu type */
typedef enum {
	CPU_NO_GOOD = -1, /**< unsupported CPU type */
	CPU_PPRO, /**< Pentium Pro */
	CPU_PII, /**< Pentium II series */
	CPU_PIII, /**< Pentium III series */
	CPU_ATHLON, /**< AMD P6 series */
	CPU_RTC, /**< other CPU to use the RTC */
	MAX_CPU_TYPE
} op_cpu;

/**
 * get from /proc/sys/dev/oprofile/cpu_type the cpu type
 *
 * returns CPU_NO_GOOD if the CPU could not be identified.
 * This function can not work if the module is not loaded
 */
op_cpu op_get_cpu_type(void);

/**
 * get the cpu string.
 * @param cpu_type the cpu type identifier
 *
 * The function always return a valid char const * the core cpu denomination
 * or "invalid cpu type" if cpu_type is not valid.
 */
char const * op_get_cpu_type_str(op_cpu cpu_type);

/**
 * compute the number of counters available
 * @param cpu_type numeric processor type
 *
 * returns 0 if the CPU could not be identified
 */
int op_get_nr_counters(op_cpu cpu_type);

#ifdef __cplusplus
}
#endif

#endif /* OP_CPU_TYPE_H */
