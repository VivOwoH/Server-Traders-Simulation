#ifndef PE_EXCHANGE_H
#define PE_EXCHANGE_H

#define LOG_PREFIX "[PEX]"
#define ARG_SIZE 10
#define BUFFLEN 128
#define PRODUCT_LEN 16 // PRODUCT: string, alphanumeric, case sensitive,
                       // up to 16 characters

#include "pe_common.h"
#include "msg_cmd.h"
#include "queue.h"

struct fd_pool {
    int maxfd;        // largest fd in all ready descriptors (*|all fds|=maxfd+1)
    fd_set rfds;      // all ready fds
    int * fds_set; // malloc this when we know num of traders
                      // IMPORTANT: select can only handle < 1024 fds; enough for the purpose of this assignment
};

extern struct fd_pool * exchange_pool;
extern struct fd_pool * trader_pool;

void ini_pipes(int);
void connect_pipes(int);
int rw_trader(int, int, int);
void market_alert(int pid, order_node order);
void parse_products(char* filename);
void match_order();
void report_order_book();
void calculate_total_ex_fee();
void free_mem();

#endif