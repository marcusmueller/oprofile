#ifndef OP_POPT_H
#define OP_POPT_H

#include <popt.h>

#ifdef __cplusplus
extern "C" {
#endif

/* wrapper around popt library: handle all non recognized options. all error
 * are fatal */
/* TODO: add version/help automatically, handle it here then remove it else
 * else where ? (need to put opd_malloc and related in the util directory) */
poptContext opd_poptGetContext(const char * name,
                int argc, const char ** argv,
                const struct poptOption * options, int flags);

#ifdef __cplusplus
}
#endif

#endif /* OP_POPT_H */
