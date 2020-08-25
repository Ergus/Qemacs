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

#include "qe.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// List containers
#define list_for(T)                                                     \
    /* Init empty list */                                               \
    void list_##T##_init (list_##T *in, int max_size)                   \
    {                                                                   \
        in->list = qe_mallocz_array(T*, max_size);                      \
        in->count = 0;                                                  \
        in->max_size = max_size;                                        \
    }                                                                   \
                                                                        \
    /* Insert and copy element at position pos */                       \
    /* If pos is out of range inserts at the end */                     \
    /* If value is set in the position or null */                       \
    /* A pointer to the pos element is always returned */               \
    /* The element is not initialized */                                \
    T** list_##T##_insert(list_##T *in, T *value, size_t pos)           \
    {                                                                   \
        size_t idx = in->count;                                         \
                                                                        \
        /* extend and reallocate if full */                             \
        if (in->count + 1 >= in->max_size) {                            \
            assert(in->max_size * 2 > in->max_size);                    \
            in->max_size *= 2;                                          \
            in->list = qe_realloc(&(in->list), in->max_size * sizeof(T*)); \
            if (!in->list)                                              \
                return NULL;                                            \
        }                                                               \
                                                                        \
        /* open a hole */                                               \
        if (pos < in->count) {                                          \
            memmove(&in->list[pos + 1], &in->list[pos],                 \
			        (in->count - pos) * sizeof (T*));                   \
            idx = pos;                                                  \
        }                                                               \
                                                                        \
        /* assign value */                                              \
        in->list[idx] = value;                                          \
        in->count++;                                                    \
                                                                        \
        return &in->list[idx];                                          \
    }                                                                   \
                                                                        \
    /* Remove element at position pos */                                \
    T* list_##T##_remove_(list_##T *in, size_t pos, bool free)          \
    {                                                                   \
        if (pos < in->count) {                                          \
            T* ret = in->list[pos];                                     \
            if (pos < in->count - 1)                                    \
                memmove(&in->list[pos], &in->list[pos + 1],             \
				        (in->count - pos - 1) * sizeof (T*));           \
                                                                        \
            in->list[in->count--] = NULL;                               \
            return ret;                                                 \
        }                                                               \
                                                                        \
        perror("Array out of range");                                   \
        abort();                                                        \
    }                                                                   \
                                                                        \
    int list_##T##_size(list_##T *in) {return in->count;}               \
    int list_##T##_max_size(list_##T *in) {return in->max_size;}        \
                                                                        \
    void list_##T##_free(list_##T *in)                                  \
    {                                                                   \
        qe_free(in->list);                                              \
    }                                                                   \
                                                                        \
    T* list_##T##_get(list_##T *in, size_t pos)                         \
    {                                                                   \
        if (pos < in->count)                                            \
            return in->list[pos];                                       \
        return NULL;                                                    \
    }                                                                   \
                                                                        \
    T* list_##T##_extract(list_##T *in, size_t pos)                     \
    {                                                                   \
        return list_##T##_remove_(in, pos, false);                      \
    }                                                                   \
                                                                        \
    T* list_##T##_drop(list_##T *in, size_t pos)                        \
    {                                                                   \
        return list_##T##_remove_(in, pos, true);                       \
    }                                                                   \
                                                                        \
    /* First and last element in the list */                            \
    T** list_##T##_begin(list_##T *in)                                  \
    {                                                                   \
        return in->list;                                                \
    }                                                                   \
                                                                        \
    T** list_##T##_end (list_##T *in)                                   \
    {                                                                   \
        return &in->list[in->count];                                    \
    }                                                                   \
                                                                        \
    T** list_##T##_push(list_##T *in, T *value)                         \
    {                                                                   \
        const size_t last = list_##T##_size(in);                        \
        return list_##T##_insert(in, value, last);                      \
    }                                                                   \
                                                                        \
    T** list_##T##_next(list_##T *in, T **it) {                         \
        T **begin = list_##T##_begin (in);                              \
        T **end = list_##T##_end (in);                                  \
                                                                        \
        if (it >= end || it < begin)                                    \
            return NULL;                                                \
                                                                        \
        return it + 1;                                                  \
    }                                                                   \
                                                                        \
    T** list##T##_prev(list_##T *in, T **it) {                          \
        T **begin = list_##T##_begin (in);                              \
        T **end = list_##T##_end (in);                                  \
                                                                        \
        if (it <= begin || it > end )                                   \
            return NULL;                                                \
                                                                        \
        return it - 1;                                                  \
    }                                                                   \

list_for(int);
list_for(EditBuffer);
#undef list_for
