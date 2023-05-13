#ifndef PE_QUEUE_H
#define PE_QUEUE_H

#include "pe_common.h"

#define BUY_ORDER 0
#define SELL_ORDER 1
#define PRODUCT_LEN 16

typedef void handler_t(int, siginfo_t *, void *);

struct linkedList
{
    int order_type;
    int time;
    int pid;
    int trader_id;
    int order_id;
    char *product;
    int qty;
    int price;
    struct linkedList * prev;
    struct linkedList *next;
};
typedef struct linkedList *order_node;

struct product_orders {
    char *product;
    order_node head_order;
    order_node tail_order;
    long long * trader_qty_index; // number of this product held per trader
    long long * trader_fee_index; // exchange fee put in for this product per trader
    int buy_level;
    int sell_level;
};
typedef struct product_orders *orderbook_node;
extern orderbook_node * orderbook;


handler_t *Signal(int signum, handler_t *handler); // [1] signal to catch;
                                                   // [2] pointer to the function that will be called when the signal is received
void create_orderbook(int num_traders, int product_num, char ** product_ls);
orderbook_node get_orderbook_by_product(char * product);
order_node get_order_by_ids(int trader_id, int order_id);
order_node create_order(int type, int time, int pid, int trader_id, int order_id, char *product, int qty, int price);
void check_unique_price(orderbook_node book, order_node node, int val);
order_node add_order(order_node node, orderbook_node book);
void update_orderbook(orderbook_node book, order_node order);
order_node amend_order(int trader_id, int order_id, int new_qty, int new_price, int new_time);
void remove_order(order_node order, orderbook_node book);
void free_orderbook();
void check_pointer(orderbook_node book);

#endif