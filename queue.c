#include "queue.h"

orderbook_node * orderbook;
int orderbook_size = 0;

// Sigaction wrapper
handler_t* Signal(int signum, handler_t *handler) {
    struct sigaction action = {0}; 
    struct sigaction old_action = {0};

    action.sa_flags = SA_SIGINFO | SA_RESTART; 
    action.sa_sigaction = handler; // sa_sigaction (not sa_handler) 
                            // specifies the signal-handling function for signum 
    sigemptyset(&action.sa_mask); // block signals of type currently being processed
    sigaddset(&action.sa_mask, SIGUSR1);

    if (sigaction(signum, &action, &old_action) == -1) {
        perror("sigaction error");
        exit(1);
    }
    
    return old_action.sa_sigaction;
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

// update value if unique
void check_unique_price(orderbook_node book, order_node node, int val) {
    order_node curr = book->head_order;
    while(curr != NULL) {
        if ((curr != node) && (curr->order_type == node->order_type) 
            && (curr->price == node->price))
            return; // found same price
        curr = curr->next;
    } 
    if (node->order_type == BUY_ORDER)
        book->buy_level += val;
    else
        book->sell_level += val;
}

// sorted by price-time priority
void add_order(order_node node) {
    orderbook_node book = get_orderbook_by_product(node->product);
    if (book == NULL) {
        perror("empty orderbook");
        exit(6);
    }

    check_unique_price(book, node, 1);

    if (book->head_order == NULL) {
        book->head_order = node;
    } 
    else {
        order_node tmp = book->head_order; 
        while(tmp != NULL) {
            if (node->price > tmp->price) {
                book->head_order = (tmp==book->head_order) ? node : book->head_order;
                node->next = tmp;
                node->prev = tmp->prev;
                if (tmp->prev != NULL)
                    tmp->prev->next = node; 
                tmp->prev = node;
                break;
            }
            if (tmp->next == NULL) { // last node
                tmp->next = node;
                node->prev = tmp;
            }
            tmp = tmp->next;
        }
    }
    if (node->order_type == SELL_ORDER && 
            (book->tail_order == NULL || book->tail_order->price >= node->price))
        book->tail_order = node;

    return;
}

order_node amend_order(int trader_id, int order_id, int new_qty, int new_price) {
    order_node curr = NULL;
    for (int i = 0; i < orderbook_size; i++) {
        curr = orderbook[i]->head_order;
        while (curr != NULL) {
            if (curr->trader_id == trader_id && curr->order_id == order_id) {
                check_unique_price(orderbook[i], curr, -1);
                curr->qty = new_qty;
                curr->price = new_price;
                check_unique_price(orderbook[i], curr, 1);
                return curr;
            }
            curr = curr->next;
        }
    }
    return NULL;
}

order_node cancel_order(int trader_id, int order_id) {
    // TODO: check buy/sell_level; free mem
    return NULL;
}