#ifndef PE_EXCHANGE_H
#define PE_EXCHANGE_H

#define LOG_PREFIX "[PEX]"
#define ARG_SIZE 10
#define BUFFLEN 128
#define PRODUCT_LEN 16 // PRODUCT: string, alphanumeric, case sensitive,
                       // up to 16 characters

#include "pe_common.h"
#include "msg_cmd.h"
#include "mysignal.h"

struct fd_pool {
    int maxfd;        // largest fd in all ready descriptors (*|all fds|=maxfd+1)
    fd_set rfds;      // all ready fds
    int num_rfds;     // number of ready fds from select
    int * fds_set; // malloc this when we know num of traders
                      // IMPORTANT: select can only handle < 1024 fds; enough for the purpose of this assignment
};

extern struct fd_pool * exchange_pool;
extern struct fd_pool * trader_pool;

struct linkedList {
    int pid;
    int trader_id;
    struct linkedList * next;
};
typedef struct linkedList * signal_node;


void ini_pipes();
void connect_pipes();
signal_node enqueue(signal_node node);
int process_next_signal();
int tear_down();
void parse_products(char* filename);
void free_mem();

#endif