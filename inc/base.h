#ifndef BASE_H
#define BASE_H

#ifdef PLATFORM_WIN32
#define _USE_MATH_DEFINES
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define SQLITE_INT64_TYPE int64_t
#define SQLITE_UINT64_TYPE uint64_t
#include <sqlite/sqlite3.h>

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif
