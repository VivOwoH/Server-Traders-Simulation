#include "queue.h"

signal_node head_sig = NULL;
order_node head_order = NULL;

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

// sorted by price-time priority
void add_order(order_node node) {
    if (head_order == NULL) {
        head_order = node;
    } else {
        order_node tmp = head_order; 
        while(tmp->next != NULL){
            tmp = tmp->next; 
        }
        tmp->next = node; 
    }
    return;
}

void remove_order(int trader_id, int order_id) {
    
}