#ifndef DEVICE_BACKPORT_H
#define DEVICE_BACKPORT_H

#include <linux/compiler.h>
#include <linux/gfp.h>

/* Pulled from linux/compiler-gcc.h */
#define __malloc		__attribute__((__malloc__))

extern __printf(3, 0)
char *devm_kvasprintf(struct device *dev, gfp_t gfp, const char *fmt, va_list ap) __malloc;
extern __printf(3, 4)
char *devm_kasprintf(struct device *dev, gfp_t gfp, const char *fmt, ...) __malloc;

#endif /* DEVICE_BACKPORT_H */
