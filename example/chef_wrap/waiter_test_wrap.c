/*
 * Copyright 2013 (c) Andreas Schneider <asn@cynapses.org>
 *                    Jakub Hrozek <jakub.hrozek@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "waiter_test_wrap.h"
#include "chef.h"

/*
 * This is a mocked Chef object. A real Chef would look if he knows
 * the dish in some kind of internal database and check his storage for
 * ingredients. This chef simply retrieves this information from the test
 * that is calling him.
 *
 * This object is also wrapped - if any code links with this file and is
 * compiled with linker option --wrap chef_cook, any calls of that code to
 * chef_cook will end up calling __wrap_chef_cook.
 *
 * If for any reason the wrapped function wanted to call the real chef_cook()
 * function, it could do so by calling the special symbol __real_chef_cook().
 *
 * Please note that when setting return codes for the chef_cook function, we
 * use this wrapper as a parameter for the will_return() macro, not the
 * real function.
 *
 * A chef object would return:
 * 0 - cooking dish went fine
 * -1 - unknown dish
 * -2 - ran out of ingredients for the dish
 * any other error code -- unexpected error while cooking
 *
 * The return codes should be consistent between the real and mocked objects.
 */
int __wrap_chef_cook(const char *order, char **dish_out)
{
    bool has_ingredients;
    bool knows_dish;
    char *dish;

    check_expected(order);

    knows_dish = (bool) mock();
    if (knows_dish == false) {
        return -1;
    }

    has_ingredients = (bool) mock();
    if (has_ingredients == false) {
        return -2;
    }

    dish = (char *) mock();
    *dish_out = strdup(dish);
    if (*dish_out == NULL) return ENOMEM;

    return mock();
}

/* Waiter return codes:
 *  0  - success
 * -1  - kitchen failed
 * -2  - kitchen succeeded, but cooked a different food
 */
static int waiter_process(const char *order, char **dish)
{
    int rv;

    rv = chef_cook(order, dish);
    if (rv != 0) {
        fprintf(stderr, "Chef couldn't cook %s: %s\n",
                order, chef_strerror(rv));
        return -1;
    }

    /* Check if we received the dish we wanted from the kitchen */
    if (strcmp(order, *dish) != 0) {
        *dish = NULL;
        return -2;
    }

    return 0;
}

static void test_order_hotdog(void **state)
{
    int rv;
    char *dish;

    (void) state; /* unused */

    /* We expect the chef to receive an order for a hotdog */
    expect_string(__wrap_chef_cook, order, "hotdog");
    /* And we tell the test chef that ke knows how to cook a hotdog
     * and has the ingredients
     */
    will_return(__wrap_chef_cook, true);
    will_return(__wrap_chef_cook, true);
    /* The result will be a hotdog and the cooking process will succeed */
    will_return(__wrap_chef_cook, "hotdog");
    will_return(__wrap_chef_cook, 0);

    /* Test the waiter */
    rv = waiter_process("hotdog", &dish);

    /* We expect the cook to succeed cooking the hotdog */
    assert_int_equal(rv, 0);
    /* And actually receive one */
    assert_string_equal(dish, "hotdog");
}

static void test_bad_dish(void **state)
{
    int rv;
    char *dish;

    (void) state; /* unused */

    /* We expect the chef to receive an order for a hotdog */
    expect_string(__wrap_chef_cook, order, "hotdog");
    /* And we tell the test chef that ke knows how to cook a hotdog
     * and has the ingredients
     */
    will_return(__wrap_chef_cook, true);
    will_return(__wrap_chef_cook, true);
    /* The result will be a burger and the cooking process will succeed.
     * We expect the waiter to handle the bad dish and return an error
     * code
     */
    will_return(__wrap_chef_cook, "burger");
    will_return(__wrap_chef_cook, 0);

    /* Test the waiter */
    rv = waiter_process("hotdog", &dish);

    /* According to the documentation the waiter should return -2 now */
    assert_int_equal(rv, -2);
    /* And do not give the bad dish to the customer */
    assert_null(dish);
}

int main(void)
{
    const UnitTest tests[] = {
        unit_test(test_order_hotdog),
        unit_test(test_bad_dish),
    };

    return run_tests(tests);
}
