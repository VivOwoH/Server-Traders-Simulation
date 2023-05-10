#include "queue.h"

signal_node head_sig = NULL;
order_node head_order = NULL;
int buy_level = 0;
int sell_level = 0;

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

order_node create_order(int type, int pid, int trader_id, int order_id, char *product, int qty, int price) {
    order_node node = (order_node) malloc(sizeof(struct linkedList));
    node->order_type = type;
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

    if (head_order == NULL) {
        head_order = node;
    } 
    else {
        order_node tmp = head_order; 
        while(tmp->next != NULL) {
            if (node->price > tmp->price) {
                node->next = tmp;
                node->prev = tmp->prev;
                tmp->prev = node;
                tmp->next = NULL;
                return;
            }
            unique = (node->price == tmp->price) ? 0 : unique;
            tmp = tmp->next;
        }

        if (node->price > tmp->price) {
            node->next = tmp;
            node->prev = tmp->prev;
            tmp->prev = node;
            tmp->next = NULL;
        } else {
            unique = (node->price == tmp->price) ? 0 : unique;
            tmp->next = node;
            node->prev = tmp;
        }
    }
    // update levels for order book
    if (unique) {
        if (node->order_type == BUY_ORDER) 
            buy_level++;
        else if (node->order_type == SELL_ORDER)
            sell_level++;
    }
    return;
}

void remove_order(int trader_id, int order_id) {
    // TODO: check buy/sell_level; free mem
}