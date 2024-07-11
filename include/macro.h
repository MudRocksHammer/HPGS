#ifndef __HPGS_MACRO_H__
#define __HPGS_MACRO_H__


#include <string.h>
#include <assert.h>
#include "log.h"
#include "util.h"

#if defined __GNUC__ || defined __llvm__
//LIKELY 宏的封装，告诉编译器优化,条件大概率成立
#define HPGS_LIKELY(x) __builtin_expect(!!(x), 1)
#define HPGS_UNLIKELY(x) __builtin_expect(!!(x), 0)

#else

#define HPGS_LIKELY(x) (x)
#define HPGS_UNLIKELY(x) (x)
#endif

//断言宏封装
#define HPGS_ASSERT(x) \
        if(HPGS_UNLIKELY(!(x))) { \
            HPGS_LOG_ERROR(HPGS_LOG_ROOT()) << "ASSERTION: " #x \
                << "\nbacktrace:\n" \
                << HPGS::BacktraceToString(100, 2, "  "); \
            assert(x); \
        }

#define HPGS_ASSERT2(x, w) \
        if(HPGS_UNLIKELY(!(x))) { \
            HPGS_LOG_ERROR(HPGS_LOG_ROOT()) << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace: \n" \
            << HPGS::BacktraceToString(100, 2, "  "); \
        assert(x); \
        }

#endif
