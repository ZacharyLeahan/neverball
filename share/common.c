/*
 *  Copyright (C) 2007  Neverball authors
 *
 *  This  program is  free software;  you can  redistribute  it and/or
 *  modify it  under the  terms of the  GNU General Public  License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program  is distributed in the  hope that it  will be useful,
 *  but  WITHOUT ANY WARRANTY;  without even  the implied  warranty of
 *  MERCHANTABILITY or FITNESS FOR  A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a  copy of the GNU General Public License
 *  along  with this  program;  if  not, write  to  the Free  Software
 *  Foundation,  Inc.,   59  Temple  Place,  Suite   330,  Boston,  MA
 *  02111-1307 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

/*
 * No platform checking, relying on MinGW to provide.
 */
#include <sys/stat.h> /* stat() */
#ifdef NXDK
#include <windows.h>
#else
#include <unistd.h>   /* access() */
#endif

#include "common.h"
#include "fs.h"

#define MAXSTR 256

/*---------------------------------------------------------------------------*/

int read_line(char **dst, fs_file fin)
{
    char buff[MAXSTR];

    char *line, *new;
    size_t len0, len1;

    if (!dst || !fin)
        return 0;

    line = NULL;

    while (fs_gets(buff, sizeof (buff), fin))
    {
        /* Append to data read so far. */

        if (line)
        {
            new  = concat_string(line, buff, NULL);
            free(line);
            line = new;
        }
        else
        {
            line = strdup(buff);
        }

        if (!line)
            break;

        /* Strip newline, if any. */

        len0 = strlen(line);
        strip_newline(line);
        len1 = strlen(line);

        if (len1 != len0)
        {
            /* We hit a newline, clean up and break. */
            new = realloc(line, len1 + 1);
            if (new)
                line = new;
            break;
        }
    }

    *dst = line;
    return line ? 1 : 0;
}

char *strip_newline(char *str)
{
    if (str && *str)
    {
        char *p = str + strlen(str) - 1;

        while (p >= str && (*p == '\n' || *p =='\r'))
            *p-- = '\0';
    }

    return str;
}

char *strip_spaces(char *str)
{
    if (str && *str)
    {
        char *p = str + strlen(str) - 1;

        while (p >= str && isspace(*p))
            *p-- = 0;
    }

    return str;
}

char *dupe_string(const char *src)
{
    char *dst = NULL;

    if (src && (dst = malloc(strlen(src) + 1)))
        strcpy(dst, src);

    return dst;
}

char *concat_string(const char *first, ...)
{
    char *full;

    if ((full = strdup(first)))
    {
        const char *part;
        va_list ap;

        va_start(ap, first);

        while ((part = va_arg(ap, const char *)))
        {
            char *new;

            if ((new = realloc(full, strlen(full) + strlen(part) + 1)))
            {
                full = new;
                strcat(full, part);
            }
            else
            {
                free(full);
                full = NULL;
                break;
            }
        }

        va_end(ap);
    }

    return full;
}

/* UTC conversion adapted from neverball-nxdk commit 458301d1 (Zachary Leahan). */
time_t make_time_from_utc(struct tm *tm)
{
#ifdef NXDK
    /* PDCLib's mktime/localtime are stubs. Replay timestamps are UTC,
     * so convert directly through the Xbox kernel's calendar routines. */
    SYSTEMTIME calendar = { 0 };
    FILETIME file_time;
    ULARGE_INTEGER ticks;

    if (tm->tm_year < -299 || tm->tm_year > 30867 ||
        tm->tm_mon < 0 || tm->tm_mon > 11 ||
        tm->tm_mday < 1 || tm->tm_mday > 31 ||
        tm->tm_hour < 0 || tm->tm_hour > 23 ||
        tm->tm_min < 0 || tm->tm_min > 59 ||
        tm->tm_sec < 0 || tm->tm_sec > 59)
        return (time_t) -1;

    calendar.wYear = tm->tm_year + 1900;
    calendar.wMonth = tm->tm_mon + 1;
    calendar.wDay = tm->tm_mday;
    calendar.wHour = tm->tm_hour;
    calendar.wMinute = tm->tm_min;
    calendar.wSecond = tm->tm_sec;
    if (!SystemTimeToFileTime(&calendar, &file_time))
        return (time_t) -1;
    ticks.LowPart = file_time.dwLowDateTime;
    ticks.HighPart = file_time.dwHighDateTime;
    return (time_t) (((long long) ticks.QuadPart - 116444736000000000LL) / 10000000LL);
#else
    struct tm local, *utc;
    time_t t;

    t = mktime(tm);

    local = *localtime(&t);
    utc   =  gmtime(&t);

    local.tm_year += local.tm_year - utc->tm_year;
    local.tm_mon  += local.tm_mon  - utc->tm_mon ;
    local.tm_mday += local.tm_mday - utc->tm_mday;
    local.tm_hour += local.tm_hour - utc->tm_hour;
    local.tm_min  += local.tm_min  - utc->tm_min ;
    local.tm_sec  += local.tm_sec  - utc->tm_sec ;

    return mktime(&local);
#endif
}

const char *date_to_str(time_t i)
{
    static char str[sizeof ("YYYY-mm-dd HH:MM:SS")];
#ifdef NXDK
    /* Display UTC until local timezone conversion is available in PDCLib. */
    strftime(str, sizeof (str), "%Y-%m-%d %H:%M:%S", gmtime(&i));
#else
    strftime(str, sizeof (str), "%Y-%m-%d %H:%M:%S", localtime(&i));
#endif
    return str;
}

#include <assert.h>
#ifdef NXDK
#include <windows.h>
#endif
int file_exists(const char *path)
{
#ifdef NXDK
    char *native = path_to_system(strdup(path));
    DWORD attributes;
    if (!native) return 0;
    attributes = GetFileAttributesA(native);
    free(native);
    return attributes != INVALID_FILE_ATTRIBUTES;
#else
    return (access(path, F_OK) == 0);
#endif
}

int file_rename(const char *src, const char *dst)
{
#ifdef _WIN32
    if (file_exists(dst))
        remove(dst);
#endif
    return rename(src, dst);
}

int file_size(const char *path)
{
#ifdef NXDK
    WIN32_FILE_ATTRIBUTE_DATA data;
    char *native = path_to_system(strdup(path));
    int found;
    if (!native) return 0;
    found = GetFileAttributesExA(native, GetFileExInfoStandard, &data);
    free(native);
    if (found) return (int) data.nFileSizeLow;
#else
    struct stat buf;
    if (stat(path, &buf) == 0)
        return (int) buf.st_size;
#endif
    return 0;
}

void file_copy(FILE *fin, FILE *fout)
{
    char   buff[MAXSTR];
    size_t size;

    while ((size = fread(buff, 1, sizeof (buff), fin)) > 0)
        fwrite(buff, 1, size, fout);
}

/*---------------------------------------------------------------------------*/

int path_is_sep(int c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

int path_is_abs(const char *path)
{
    if (path_is_sep(path[0]))
        return 1;

#ifdef _WIN32
    if (isalpha(path[0]) && path[1] == ':' && path_is_sep(path[2]))
        return 1;
#endif

    return 0;
}

/* Generated by Codex CLI 0.144.6 (GPT-6).
 * Convert owned system paths at the filesystem boundary; keep ZIP/virtual
 * paths in upstream's canonical forward-slash form.
 */
char *path_to_system(char *path)
{
#ifdef NXDK
    char *p;
    if (path)
        for (p = path; *p; p++)
            if (*p == '/') *p = '\\';
#endif
    return path;
}

char *path_join(const char *head, const char *tail)
{
    if (!head || !*head)
        return tail ? strdup(tail) : NULL;
    if (!tail || !*tail)
        return strdup(head);
    return concat_string(head, "/", tail, NULL);
}

const char *path_last_sep(const char *path)
{
    const char *sep;

    sep = strrchr(path, '/');

#ifdef _WIN32
    if (!sep)
    {
        sep = strrchr(path, '\\');
    }
    else
    {
        const char *tmp;

        if ((tmp = strrchr(path, '\\')) && sep < tmp)
            sep = tmp;
    }
#endif

    return sep;
}

const char *path_next_sep(const char *path)
{
    size_t skip;

#ifdef _WIN32
    skip = strcspn(path, "/\\");
#else
    skip = strcspn(path, "/");
#endif

    return *(path + skip) ? path + skip : NULL;
}

char *path_normalize(char *path)
{
    char *sep = path;

    while ((sep = (char *) path_next_sep(sep)))
        *sep++ = '/';

    return path;
}

const char *base_name_sans(const char *name, const char *suffix)
{
    static char base[MAXSTR];
    size_t blen, slen;

    if (!name)
        return NULL;
    if (!suffix)
        return base_name(name);

    /* Remove the directory part. */

    SAFECPY(base, base_name(name));

    /* Remove the suffix. */

    blen = strlen(base);
    slen = strlen(suffix);

    if (blen >= slen && strcmp(base + blen - slen, suffix) == 0)
        base[blen - slen] = '\0';

    return base;
}

const char *base_name(const char *name)
{
    static char buff[MAXSTR];

    char *sep;

    if (!name) {
        return name;
    }

    SAFECPY(buff, name);

    // Remove trailing slashes.
    while ((sep = (char *) path_last_sep(buff)) && !sep[1]) {
        *sep = 0;
    }

    return (sep = (char *) path_last_sep(buff)) ? sep + 1 : buff;
}

const char *dir_name(const char *name)
{
    if (name && *name)
    {
        static char buff[MAXSTR];

        char *sep;

        SAFECPY(buff, name);

        // Remove trailing slashes.
        while ((sep = (char *) path_last_sep(buff)) && !sep[1]) {
            *sep = 0;
        }

        if ((sep = (char *) path_last_sep(buff)))
        {
            if (sep == buff)
                return "/";

            *sep = '\0';

            return buff;
        }
    }

    return ".";
}

/*---------------------------------------------------------------------------*/

int rand_between(int low, int high)
{
    return low + rand() / (RAND_MAX / (high - low + 1) + 1);
}

/*---------------------------------------------------------------------------*/

#if defined(_WIN32) && !defined(NXDK)

/* MinGW hides this from ANSI C. MinGW-w64 doesn't. */
_CRTIMP int _putenv(const char *envstring);

int set_env_var(const char *name, const char *value)
{
    if (name)
    {
        char str[MAXSTR];

        if (value)
            sprintf(str, "%s=%s", name, value);
        else
            sprintf(str, "%s=", name);

        return (_putenv(str) == 0);
    }
    return 0;
}

#elif defined(NXDK)
int set_env_var(const char *name, const char *value)
{
    (void) name;
    (void) value;
    return 0; /* NXDK has no process environment mutation API. */
}
#else

extern int setenv(const char *name, const char *value, int overwrite);
extern int unsetenv(const char *name);

int set_env_var(const char *name, const char *value)
{
    if (name)
    {
        if (value)
            return (setenv(name, value, 1) == 0);
        else
            return (unsetenv(name) == 0);
    }
    return 0;
}

#endif

/*---------------------------------------------------------------------------*/
