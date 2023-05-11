#ifndef PE_QUEUE_H
#define PE_QUEUE_H

#include "pe_common.h"

#define BUY_ORDER 0
#define SELL_ORDER 1

typedef void handler_t(int, siginfo_t *, void *);
handler_t *Signal(int signum, handler_t *handler); // [1] signal to catch;
                                                   // [2] pointer to the function that will be called when the signal is received

struct queue
{
    int trader_id;
    struct queue *next;
};
typedef struct queue * signal_node;
extern signal_node head_sig;

void enqueue(signal_node node);
void dequeue();

struct linkedList
{
    int order_type;
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
    int buy_level;
    int sell_level;
};
typedef struct product_orders *orderbook_node;
extern orderbook_node * orderbook;

void create_orderbook(int product_num, char ** product_ls);
order_node create_order(int type, int pid, int trader_id, int order_id, char *product, int qty, int price);
void add_order(order_node node);
void remove_order(int trader_id, int order_id);

#endif