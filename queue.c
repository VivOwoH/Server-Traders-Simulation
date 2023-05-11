#include "queue.h"

signal_node head_sig = NULL;
orderbook_node * orderbook;
int orderbook_size = 0;

// Sigaction wrapper
handler_t* Signal(int signum, handler_t *handler) {
    struct sigaction action, old_action;

    old_action.sa_flags = SA_SIGINFO;
    action.sa_flags = SA_SIGINFO; 
    action.sa_sigaction = handler; // sa_sigaction (not sa_handler) 
                            // specifies the signal-handling function for signum 
    sigemptyset(&action.sa_mask); // block signals of type currently being processed

    if (sigaction(signum, &action, &old_action) == -1) {
        perror("sigaction error");
        exit(1);
    }
    
    return old_action.sa_sigaction;
}

void enqueue(signal_node node) {
    if (head_sig == NULL) {
        head_sig = node;
    } else {
        signal_node tmp = head_sig; 
        while(tmp->next != NULL){
            tmp = tmp->next; 
        }
        tmp->next = node; 
    }
    return;
}

void dequeue() {
    if (head_sig == NULL)
        return;

    signal_node tmp = head_sig;
    head_sig = head_sig->next;
    
    free(tmp);
}

void create_orderbook(int num_traders, int product_num, char ** product_ls) {
    orderbook_size = product_num;
    orderbook = malloc(orderbook_size * sizeof(orderbook_node));
    for (int i = 0; i < orderbook_size; i++) {
        orderbook[i] = (orderbook_node) malloc(sizeof(struct product_orders));
        orderbook[i]->trader_fee_index = calloc(num_traders, sizeof(int));
        orderbook[i]->trader_qty_index = calloc(num_traders, sizeof(int));
        orderbook[i]->product = product_ls[i];
        orderbook[i]->buy_level = 0;
        orderbook[i]->sell_level = 0;
        orderbook[i]->head_order = NULL;
        orderbook[i]->tail_order = NULL;
    }
}

orderbook_node get_orderbook_by_product(char * product) {
    for (int i = 0; i < orderbook_size; i++) {
        if (strcmp(orderbook[i]->product, product)==0)
            return orderbook[i];
    }
    return NULL;
}

order_node create_order(int type, int time, int pid, int trader_id, int order_id, char *product, int qty, int price) {
    order_node node = (order_node) malloc(sizeof(struct linkedList));
    node->order_type = type;
    node->time = time;
    node->pid = pid;
    node->trader_id = trader_id;
    node->order_id = order_id;
    node->product = product;
    node->qty = qty;
    node->price = price;
    node->prev = NULL;
    node->next = NULL;

    return node;
}

// sorted by price-time priority
void add_order(order_node node) {
    int unique = 1;
    orderbook_node book = get_orderbook_by_product(node->product);

    if (book->head_order == NULL) {
        book->head_order = node;
        book->tail_order = node;
    } 
    else {
        order_node tmp = book->head_order; 
        while(tmp->next != NULL) {
            if (node->price > tmp->price) {
                node->next = tmp;
                node->prev = tmp->prev;
                tmp->prev = node;
                return;
            }
            unique = (node->price == tmp->price) ? 0 : unique;
            tmp = tmp->next;
        }

        if (node->price > tmp->price) {
            book->head_order = (tmp==book->head_order) ? node : book->head_order;
            node->next = tmp;
            node->prev = tmp->prev;
            tmp->prev = node;
        } else {
            book->tail_order = (tmp==book->tail_order) ? node : book->tail_order;
            unique = (node->price == tmp->price) ? 0 : unique;
            tmp->next = node;
            node->prev = tmp;
        }
    }
    // update levels for order book
    if (unique) {
        if (node->order_type == BUY_ORDER) 
            book->buy_level++;
        else if (node->order_type == SELL_ORDER)
            book->sell_level++;
    }
    return;
}

order_node amend_order(int trader_id, int order_id, int new_qty, int new_price) {
    // TODO: check buy/sell_level, amend order
    return NULL;
}

order_node cancel_order(int trader_id, int order_id) {
    // TODO: check buy/sell_level; free mem
    return NULL;
}