/* ustd.h common macros and includes */
#ifndef LIBRHASH_USTD_H
#define LIBRHASH_USTD_H

#if _MSC_VER > 1000
# include <stddef.h> /* size_t for vc6.0 */

# if _MSC_VER >= 1600
/* Visual Studio >= 2010 has stdint.h */
#  include <stdint.h>
# else
  /* vc6.0 has bug with __int8, so using char instead */
  typedef signed char       int8_t;
  typedef signed __int16    int16_t;
  typedef signed __int32    int32_t;
  typedef signed __int64    int64_t;
  typedef unsigned char     uint8_t;
  typedef unsigned __int16  uint16_t;
  typedef unsigned __int32  uint32_t;
  typedef unsigned __int64  uint64_t;
# endif /* _MSC_VER >= 1600 */

/* disable warnings: The POSIX name for this item is deprecated. Use the ISO C++ conformant name. */
# pragma warning(disable : 4996)

#else /* _MSC_VER > 1000 */

# include <stdint.h>
# include <unistd.h>

#endif /* _MSC_VER > 1000 */

#endif /* LIBRHASH_USTD_H */

