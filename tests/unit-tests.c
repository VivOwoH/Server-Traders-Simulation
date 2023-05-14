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
order_node order = NULL;
order_node order2 = NULL;
order_node order3 = NULL;

void ini_product_ls() {
    product_ls = malloc(product_num * sizeof(char*));
    for (int i = 0; i < product_num; i++) 
        product_ls[i] = calloc(PRODUCT_LEN+1, sizeof(char));
    
    strcpy(product_ls[0], "Water");
    strcpy(product_ls[1], "Wine");
    strcpy(product_ls[2], "Beer");
}

/** Test case: create_orderbook initialization
 * Description: Tests the create_orderbook function with valid inputs.
 * Inputs: num_traders[5], product_num[3], product_ls["Water", "Wine", "Beer"]
 * Expected output: a list of 3 orderbooks with elements initialized, each orderbook
 *                  can be identified with the product name
 */
static void test_create_orderbook(void **state) {
    (void) state; /* unused */
    ini_product_ls();

    create_orderbook(num_traders, product_num, product_ls);

    for (int i = 0; i < product_num; i++) {
        assert(strcmp(orderbook[i]->product, product_ls[i])==0);
    }
}

/** Test case: get orderbok by product name
 * Description: Tests the get_orderbook_by_product function with valid/invalid inputs.
 * Inputs: product_ls[2]->"Wine", "Random"
 * Expected output: correct orderbook specified by the product if valid
 */
static void test_get_orderbook(void **state) {
    (void) state; /* unused */
    // valid
    orderbook_node book = get_orderbook_by_product(product_ls[2]);
    assert(strcmp(book->product, product_ls[2])==0);
    
    // invalid
    orderbook_node null_book = get_orderbook_by_product("Random");
    assert_null(null_book);
}

/** Test case: add a new order to a orderbook
 * Description: Tests create_order, add_order, get_order_by_ids function with valid inputs. 
 * Dependent functions: check_unqiue_price
 * Inputs: (1) sell order; (2) sell order of same price; 
 *          (3) sell order of different pice, with no orderbook provided
 * Expected output: head/tail orders, buy/sell levels are updated correctly
 */
static void test_add_new_order(void **state) {
    (void) state; /* unused */
    order = create_order(SELL_ORDER, 0, TEST_PID, TEST_TRADER_ID, 0, product_ls[0], 10, 100);
    add_order(order, orderbook[0]);
    assert(orderbook[0]->head_order == order);
    assert(orderbook[0]->tail_order == order); // since it is the only sell order
    assert(orderbook[0]->buy_level == 0);
    assert(orderbook[0]->sell_level == 1);

    // new sell order of same price (also become new tail)
    order2 = create_order(SELL_ORDER, 1, TEST_PID, TEST_TRADER_ID, 1, product_ls[0], 10, 100);
    add_order(order2, orderbook[0]);
    assert(orderbook[0]->head_order == order);
    assert(orderbook[0]->tail_order == order2);
    assert(orderbook[0]->buy_level == 0);
    assert(orderbook[0]->sell_level == 1); // unchange

    // new sell order of higher price (new head)
    order3 = create_order(SELL_ORDER, 2, TEST_PID, TEST_TRADER_ID, 2, product_ls[0], 10, 120);
    add_order(order3, NULL); // does not specify orderbook will trigger get_orderbook_by_product
    assert(orderbook[0]->head_order == order3);
    assert(orderbook[0]->tail_order == order2);
    assert(orderbook[0]->buy_level == 0);
    assert(orderbook[0]->sell_level == 2);

    // summary of order: order3 -> order1 -> order2
    // get invalid order
    order_node invalid = get_order_by_ids(TEST_TRADER_ID, 3);
    assert_null(invalid);

    // get the last added order
    order_node chosen_order = get_order_by_ids(TEST_TRADER_ID, 2);
    assert(chosen_order == order3);

    assert_null(order3->prev);
    assert(order3->next == order);
    assert(order->prev == order3);
    assert(order->next == order2);
    assert(order2->prev == order);
    assert(order2->next == NULL);
}

/** Test case: amend an existing order in a orderbook
 * Description: Tests amend_order function with valid inputs. 
 * Dependent functions: get_order_by_ids, get_orderbook_by_product,
 *                      check_unqiue_price, update_orderbook
 * Inputs: order3 amend 0,0-> cancel, ordre2 amend 20,200 
 * Expected output: order details, head/tail orders, buy/sell levels are updated correctly
 */
static void test_amend_order(void **state) {
    (void) state; /* unused */
    // summary of order: order3 -> order1 -> order2
    // sell level = 2 (100[1,2],120[3])
                
    order_node amended = amend_order(TEST_TRADER_ID, 2, 0, 0, 5);
    orderbook_node book = get_orderbook_by_product(amended->product);
    // order1 -> order2 -> order3
    // sell level=1 (0,0 excluded)
    assert(amended == order3);
    assert(amended->price == 0);
    assert(amended->qty == 0);
    assert(amended->time == 5);
    assert(book->sell_level == 1);
    assert(book->tail_order == order2); // 0,0 not considered
    assert(book->head_order == order);

    order_node amended2 = amend_order(TEST_TRADER_ID, 1, 20, 200, 6);
    // order2 -> order1 -> order3
    // sell level=1 (0,0 excluded)
    assert(amended2 == order2);
    assert(amended2->price == 200);
    assert(amended2->qty == 20);
    assert(amended2->time == 6);
    assert(book->sell_level == 2);
    assert(book->tail_order == order); // 0,0 not considered
    assert(book->head_order == order2);
}

/** Test case: remove the cancelled order
 * Description: Tests remove_order function with valid inputs. 
 * Dependent functions: check_pointer
 * Inputs: order3 (0,0) previously cancelled
 * Expected output: order no longer in the book
 */
static void test_remove_order(void **state) {
    (void) state; /* unused */
    orderbook_node book = get_orderbook_by_product(order3->product);
    remove_order(order3, book);
    // order2 -> order1 -> (x)order3
    assert_null(order->next);
}
 
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_create_orderbook),
        cmocka_unit_test(test_get_orderbook),
        cmocka_unit_test(test_add_new_order),
        cmocka_unit_test(test_amend_order),
        cmocka_unit_test(test_remove_order)
    };
 
    return cmocka_run_group_tests(tests, NULL, NULL);
}