#ifndef _TOOLS_
#define _TOOLS_

#include <string.h>

#define ARR_SZ(arr) (sizeof(arr) / sizeof(arr[0]))
#define match(s1, s2) (strncmp(s1, s2, strlen(s2)) == 0)

#endif
