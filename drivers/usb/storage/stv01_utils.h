#ifndef _STV01_UTILS_H_
#define _STV01_UTILS_H_

#include <linux/kernel.h>

#define EXPORT
#define INTERVAL_IMPL
#define TOOLS_API

#define EXPORT_PTR(type, id) \
                type *g_global_##id;

#define EXPORT_PTR_V(type, id, v) \
                type *g_global_##id = v;

#define EXTERN_PTR(type, id) \
                extern type *g_global_##id;

#define IMPORT_PTR(id) \
                g_global_##id

#define IPTR(id) IMPORT_PTR(id)

static inline TOOLS_API void utils_device_dbg(const struct device *dev, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	dev_vprintk_emit(LOGLEVEL_DEBUG, dev, fmt, args);

	va_end(args);
}

#endif /// _STV01_UTILS_H_
