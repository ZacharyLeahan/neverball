/*
 * Copyright (C) 2003-2010 Neverball authors
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

#ifdef NXDK
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dir.h"
#include "common.h"

/*
 * HACK: MinGW provides numerous POSIX extensions to MSVCRT, including
 * dirent.h, so parasti ever so lazily has not bothered to port the
 * code below to FindFirstFile et al.
 */

/*
 * Enumerate files in a system directory. Returns a List of allocated filenames.
 */
List dir_list_files(const char *path)
{
#ifdef NXDK
    WIN32_FIND_DATA data;
    char *pattern = path_to_system(path_join(path, "*"));
    if (!pattern) return NULL;
    HANDLE search = FindFirstFileA(pattern, &data);
    List files = NULL;
    free(pattern);
    if (search != INVALID_HANDLE_VALUE)
    {
        do {
            if (strcmp(data.cFileName, ".") && strcmp(data.cFileName, ".."))
            {
                char *name = strdup(data.cFileName);
                if (name && !list_push(&files, name)) free(name);
            }
        } while (FindNextFileA(search, &data));
        FindClose(search);
    }
#else
    DIR *dir;
    List files = NULL;

    if ((dir = opendir(path)))
    {
        struct dirent *ent;

        while ((ent = readdir(dir)))
        {
            char *name;

            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            if (!(name = strdup(ent->d_name)))
                continue;

            if (!list_push(&files, name))
                free(name);
        }

        closedir(dir);
    }

#endif
    return files;
}

/*
 * Free the allocated filenames and the List cells.
 */
void dir_list_free(List files)
{
    while (files)
    {
        free(files->data);
        files = list_rest(files);
    }
}

/*
 * Add a struct dir_item to the given Array.
 */
static struct dir_item *add_item(Array items, const char *dir, const char *name)
{
    struct dir_item *item;

    if (!items || !name)
        return NULL;

    item = array_add(items);

    if (!item)
        return NULL;

    item->path = path_join(dir ? dir : "", name);
    item->data = NULL;

    return item;
}

/*
 * Remove a struct dir_item from the given array.
 */
static void del_item(Array items)
{
    struct dir_item *item;

    if (!items || array_len(items) <= 0)
        return;

    item = array_get(items, array_len(items) - 1);

    if (item)
    {
        if (item->path)
        {
            free((void *) item->path);
            item->path = NULL;
        }
        item->data = NULL;
    }

    array_del(items);
}

/*
 * Enumerate files in a directory. Returns an Array of struct dir_item.
 *
 * By passing `list_files`, an arbitrary list may be turned into an Array
 * of struct dir_item. By passing `filter`, any struct dir_item may be
 * excluded from the final Array.
 */
Array dir_scan(const char *path,
               int  (*filter)    (struct dir_item *),
               List (*list_files)(const char *),
               void (*free_files)(List))
{
    List files, file;
    Array items = NULL;

    assert((list_files && free_files) || (!list_files && !free_files));

    if (!list_files) list_files = dir_list_files;
    if (!free_files) free_files = dir_list_free;

    items = array_new(sizeof (struct dir_item));

    if (items && (files = list_files(path)))
    {
        for (file = files; file; file = file->next)
        {
            struct dir_item *item;

            item = add_item(items, path, file->data);

            if (item)
            {
                if (filter && !filter(item))
                    del_item(items);
            }
        }

        free_files(files);
    }

    return items;
}

/*
 * Free the Array of struct dir_item.
 */
void dir_free(Array items)
{
    if (!items)
        return;

    while (array_len(items))
        del_item(items);

    array_free(items);
}

/*
 * Test existence of a system directory.
 */
int dir_exists(const char *path)
{
#ifdef NXDK
    char *native = path_to_system(strdup(path));
    DWORD attr;
    if (!native) return 0;
    attr = GetFileAttributesA(native);
    free(native);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    DIR *dir;

    if ((dir = opendir(path)))
    {
        closedir(dir);
        return 1;
    }
    return 0;
#endif
}
