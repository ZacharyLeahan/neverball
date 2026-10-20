#ifndef ZIP_H
#define ZIP_H 1

/*
 * Neverball defaults for miniz.
 */
#define MINIZ_NO_ZLIB_APIS 1
#define MINIZ_NO_ARCHIVE_WRITING_APIS 1
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES 1

#ifdef NXDK
#define MINIZ_NO_TIME 1
#endif
#include "miniz.h"

#endif
