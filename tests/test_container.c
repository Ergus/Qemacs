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

#include "container.h"

int main ()
{
    int i;
    list_int lst;

    list_int_init(&lst, 10);
    int a[] = {1, 2, 3, 4, 5};

    for (i = 0; i < (sizeof(a) / sizeof(a[0])); ++i) {
        int **ptr = list_int_insert (&lst, &a[i], i);
        printf ("%d %d %d\n", i, a[i], *ptr);
    }

    int *ptr = qe_malloc(int);
    qe_free(&ptr);

    return 0;
}
