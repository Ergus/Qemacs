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

#include <assert.h>
#include "container.h"

int main ()
{
    int i;
    list_int lst;

    list_int_init(&lst, 2);
    int a[] = {1, 2, 3, 4, 5};

    printf("Test insert with 2 reallocs\n");
    for (i = 0; i < (sizeof(a) / sizeof(a[0])); ++i) {
        int **ptr = list_int_insert(&lst, &a[i], i);
        printf ("%d %d %p %d\n", i, a[i], *ptr, **ptr);
        assert(a[i] == **ptr);
    }

    printf("Array size %d == %d\n", list_int_size(&lst), 5);
    assert(5 == list_int_size(&lst));

    printf("Array max size %d\n", list_int_max_size(&lst));
    assert(list_int_size(&lst) <= list_int_max_size(&lst));

    printf("Test get\n");
    for (i = 0; i < (sizeof(a) / sizeof(a[0])); ++i) {
        int *ptr = list_int_get(&lst, i);
        printf ("%d %d %p %d\n", i, a[i], ptr, *ptr);
        assert(a[i] == *ptr);
    }

    printf("Test iterator\n");
    i = 0;
    for (int **it = list_int_begin(&lst); it != list_int_end(&lst); it++, i++ ) {
        printf ("%d %p %d\n", a[i], *it, **it);
        assert(a[i] == **it);
    }


    int *ptr = qe_malloc(int);
    qe_free(&ptr);

    return 0;
}
