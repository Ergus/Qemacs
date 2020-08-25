/*
 * Copyright (C) 2019  Jimmy Aguilar Mena
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CONTAINER_H
#define CONTAINER_H

#include "qe.h"

#include <stdbool.h>
#include <stddef.h>

// list container (this is very general)
#define list_for(T)                                                 \
    typedef struct list_##T {                                       \
        T **list;                                                   \
        size_t count;                                               \
        size_t max_size;                                            \
    } list_##T;                                                     \
                                                                    \
    void list_##T##_init(list_##T *in, int max);                    \
    T** list_##T##_insert(list_##T *in, T *value, size_t pos);      \
    T* list_##T##_remove_(list_##T *in, size_t pos, bool free);     \
    int list_##T##_size(list_##T *in);                              \
    int list_##T##_max_size(list_##T *in);                          \
    void list_##T##_free(list_##T *in);                             \
    T* list_##T##_get(list_##T *in, size_t pos);                    \
    T* list_##T##_extract(list_##T *in, size_t pos);                \
    T* list_##T##_drop(list_##T *in, size_t pos);                   \
    T** list_##T##_begin(list_##T *in);                             \
    T** list_##T##_end(list_##T *in);                               \
    T** list_##T##_push(list_##T *in, T *value);                    \
    T** list_##T##_next(list_##T *in, T **it);                      \
    T** list_##T##_prev(list_##T *in, T **it);                      \

list_for(int);
list_for(EditBuffer);
#undef list_for

#endif
