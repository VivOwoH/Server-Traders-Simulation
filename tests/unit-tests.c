#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <signal.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>

#include "cmocka.h"
#include "../queue.h"

#define TEST_TRADER_ID 0
#define TEST_PID 87

static int num_traders = 5;
static int product_num = 3;
static char ** product_ls;
orderbook_node * orderbook;

void ini_product_ls() {
    product_ls = malloc(product_num * sizeof(char*));
    for (int i = 0; i < product_num; i++) 
        product_ls[i] = calloc(PRODUCT_LEN+1, sizeof(char));
    
    strcpy(product_ls[0], "Water");
    strcpy(product_ls[1], "Wine");
    strcpy(product_ls[2], "Beer");
}

static void test_create_orderbook(void **state) {
    (void) state; /* unused */
    ini_product_ls();

    create_orderbook(num_traders, product_num, product_ls);

    for (int i = 0; i < product_num; i++) {
        assert(strcmp(orderbook[i]->product, product_ls[i])==0);
    }
}

static void test_get_orderbook(void **state) {
    (void) state; /* unused */
    // valid
    orderbook_node book = get_orderbook_by_product(product_ls[2]);
    assert(strcmp(book->product, product_ls[2])==0);
    
    // invalid
    orderbook_node null_book = get_orderbook_by_product("Random");
    assert_null(null_book);
}

static void test_add_new_order(void **state) {
    (void) state; /* unused */
    order_node order = create_order(SELL_ORDER, 0, TEST_PID, TEST_TRADER_ID, 0, product_ls[0], 10, 100);
    add_order(order, orderbook[0]);
    assert(orderbook[0]->head_order == order);
    assert(orderbook[0]->tail_order == order); // since it is the only sell order
    assert(orderbook[0]->buy_level == 0);
    assert(orderbook[0]->sell_level == 1);

    // new sell order of same price (also become new tail)
    order_node order2 = create_order(SELL_ORDER, 1, TEST_PID, TEST_TRADER_ID, 1, product_ls[0], 10, 100);
    add_order(order2, orderbook[0]);
    assert(orderbook[0]->head_order == order);
    assert(orderbook[0]->tail_order == order2);
    assert(orderbook[0]->buy_level == 0);
    assert(orderbook[0]->sell_level == 1); // unchange

    // new sell order of higher price (new head)
    order_node order3 = create_order(SELL_ORDER, 2, TEST_PID, TEST_TRADER_ID, 2, product_ls[0], 10, 120);
    add_order(order3, NULL); // does not specify orderbook will trigger get_orderbook_by_product
    assert(orderbook[0]->head_order == order3);
    assert(orderbook[0]->tail_order == order2);
    assert(orderbook[0]->buy_level == 0);
    assert(orderbook[0]->sell_level == 2);
}
 
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_create_orderbook),
        cmocka_unit_test(test_get_orderbook),
        cmocka_unit_test(test_add_new_order)
    };
 
    return cmocka_run_group_tests(tests, NULL, NULL);
}