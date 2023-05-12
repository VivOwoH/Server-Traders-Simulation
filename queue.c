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
    sigaddset(&action.sa_mask, signum);

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
        orderbook[i]->trader_fee_index = calloc(num_traders, sizeof(long long));
        orderbook[i]->trader_qty_index = calloc(num_traders, sizeof(long long));
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

order_node get_order_by_ids(int trader_id, int order_id) {
    order_node curr = NULL;
    for (int i = 0; i < orderbook_size; i++) {
        curr = orderbook[i]->head_order;
        while (curr != NULL) {
            if (curr->trader_id == trader_id && curr->order_id == order_id) {
                if (curr->price == 0 && curr->qty == 0) {
                    remove_order(curr, orderbook[i]);
                    return NULL;
                }
                return curr;
            }
            curr = curr->next;
        }
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
    if (node->order_type == BUY_ORDER) {
        book->buy_level += val;
        // printf("check:%d buy level:%d\n", node->price, book->buy_level);
    } else if (node->order_type == SELL_ORDER) {
        book->sell_level += val;
        // printf("check:%d sell level:%d\n", node->price, book->sell_level);
    }
        
    return;
}

// sorted by price-time priority
order_node add_order(order_node node, orderbook_node book) {
    if (book == NULL) 
        book = get_orderbook_by_product(node->product);

    if (node->price != 0)
        check_unique_price(book, node, 1);

    if (book->head_order == NULL) {
        book->head_order = node;
        // puts("add head order");
    } 
    else {
        order_node tmp = book->head_order;
        while(tmp != NULL) {
            if (node->price > tmp->price) {
                // prev node same price case (we want sell above buy)
                if (tmp->prev != NULL && node->price == tmp->prev->price 
                    && node->order_type == SELL_ORDER && tmp->prev->order_type == BUY_ORDER) {
                    order_node tmp_2 = tmp;
                    while (tmp_2->prev != NULL && node->price == tmp_2->prev->price 
                                && tmp_2->prev->order_type == BUY_ORDER) {
                        tmp_2 = tmp_2->prev;
                    }
                    // now tmp_2 is the last node i want to be on top of
                    node->next = tmp_2;
                    node->prev = tmp_2->prev;
                    if (tmp_2->prev != NULL)
                        tmp_2->prev->next = node; 
                    tmp_2->prev = node;
                } 
                else {
                    // puts("i will be inserted");
                    book->head_order = (tmp==book->head_order) ? node : book->head_order;
                    node->next = tmp;
                    node->prev = tmp->prev;
                    if (tmp->prev != NULL)
                        tmp->prev->next = node; 
                    tmp->prev = node;
                }
                break;
            }
            if (tmp->next == NULL) { // last node
                tmp->next = node;
                node->prev = tmp;
                // puts("i will be last");
                break;
            }
            tmp = tmp->next;
        }
    }
    if (node->order_type == SELL_ORDER && 
            (book->tail_order == NULL || node->price <= book->tail_order->price))
        book->tail_order = node;
    
    return node;
}

void update_orderbook(orderbook_node book, order_node order) {
    if (order == book->head_order && order->next != NULL) {
        book->head_order = order->next;
        order->next->prev = NULL;
    } else if (order == book->head_order && order->next == NULL) {
        return; // dont need to do anything
    } else if (order->next == NULL) { //last node
        order->prev->next = NULL;
    } else {
        order->next->prev = order->prev;
        order->prev->next = order->next;
    }
    order->next = NULL;
    order->prev = NULL;    

    add_order(order, book);
}

order_node amend_order(int trader_id, int order_id, int new_qty, int new_price, int new_time) {
    order_node curr = get_order_by_ids(trader_id, order_id);
    if (curr == NULL) {
        perror("no such order");
        exit(6);
    }
    
    orderbook_node book = get_orderbook_by_product(curr->product);
    if (book == NULL) {
        perror("no such orderbook");
        exit(6);
    }
    check_unique_price(book, curr, -1);
    curr->qty = new_qty;
    curr->price = new_price;
    curr->time = new_time;
    update_orderbook(book, curr);
    return curr;
}

void remove_order(order_node order, orderbook_node book) {
    // tail order update
    if (order == book->tail_order) {
        book->tail_order = NULL;
        order_node tmp = order->prev;
        while (tmp != NULL) {
            if (tmp->order_type == SELL_ORDER) {
                book->tail_order = tmp;
                break;
            }
            tmp = tmp->prev;
        }
    }
    // remove this order from book
    if (order == book->head_order && order->next != NULL) {
        book->head_order = order->next;
        order->next->prev = NULL;
    } else if (order == book->head_order && order->next == NULL) {
        book->head_order = NULL;
    } else if (order->next == NULL) { // last node
        order->prev->next = NULL;
    } else {
        order->next->prev = order->prev;
        order->prev->next = order->next;
    }
    free(order);
    return;
}

void free_orderbook() {
    for (int i = 0; i < orderbook_size; i++) {
        orderbook_node book = orderbook[i];
        free(book->trader_fee_index);
        free(book->trader_qty_index);

        order_node tmp = NULL;
        while (book->head_order != NULL) {
            tmp = book->head_order;
            book->head_order = book->head_order->next;
            free(tmp);
        }
        free(orderbook[i]);
    }
    free(orderbook);
}