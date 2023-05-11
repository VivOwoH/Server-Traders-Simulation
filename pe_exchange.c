/**
 * comp2017 - assignment 3
 * Vivian Ha
 * weha7612
 */

#include "pe_exchange.h"

int product_num = 0;
char ** product_ls;
int num_traders = 0;
int ** trader_productQTY_index;
int * trader_fee_index;
int total_ex_fee = 0;
pid_t * pids;
struct fd_pool ex_fds;
struct fd_pool tr_fds;
struct fd_pool * exchange_pool = &ex_fds;
struct fd_pool * trader_pool = &tr_fds;
int all_children_terminated = 0;
// int sigchld_received = 0;
int sigusr1_received = 0;


void sigusr1_handler(int s, siginfo_t *info, void *context) {
    // puts("exchange received sigusr1");
    sigusr1_received = 1;

    signal_node node = (signal_node) malloc(sizeof(struct queue));
    node->trader_id = info->si_value.sival_int;
    node->next = NULL;
    enqueue(node);

    return;
}

void reset_fds() {
    FD_ZERO(&trader_pool->rfds); // clear all
    FD_ZERO(&exchange_pool->rfds);

    for (int i = 0; i < num_traders; i++) {
        FD_SET(exchange_pool->fds_set[i], &exchange_pool->rfds); // add fds to rfds
        FD_SET(trader_pool->fds_set[i], &trader_pool->rfds);
        
        if (trader_pool->fds_set[i] > trader_pool->maxfd)
            trader_pool->maxfd = trader_pool->fds_set[i];
        if (exchange_pool->fds_set[i] > exchange_pool->maxfd)
            exchange_pool->maxfd = exchange_pool->fds_set[i];  
        // printf("%d %d\n", exchange_pool->fds_set[i], trader_pool->fds_set[i]);
        // printf("%d %d\n", exchange_pool->maxfd, trader_pool->maxfd);
    }
}

// ----------------------------------------------------------
// -------------------------- MAIN --------------------------
// ----------------------------------------------------------

int main(int argc, char ** argv) {
    if (argc < 3) {
        printf("usage: ./pe_exchange [product file] [trader 0] [trader 1] ... [trader n]\n");
        return 1;
    }

    printf("%s Starting\n", LOG_PREFIX);

    // e.g. ./pe_exchange products.txt ./trader_a ./trader_b
    parse_products(argv[1]);
    create_orderbook(product_num, product_ls); // each product has a orderbook

    num_traders = argc - 2;
    trader_pool->maxfd = 0;
    exchange_pool->maxfd = 0;
    trader_pool->fds_set = malloc(sizeof(int) * (num_traders)); 
    exchange_pool->fds_set = malloc(sizeof(int) * (num_traders)); 

    // launch binaries (fork + exec)
    // each child exchange has a ex_fd that connects to the corresponding tr_fd
    // i.e. exchange_child_0 [exchange_fd_0] <-> [trader_fd_0] trader 0
    pids = malloc(sizeof(pid_t) * (num_traders));

    for (int i = 0; i < num_traders; i++) {
        // initialization
        ini_pipes(i);

        pid_t pid = fork();
        pids[i] = pid;

        if (pid < 0) {
            perror("Fork error");
            exit(2);
        }
        else if (pid == 0) { // child process: exec binary
            char buffer[BUFFLEN];
            snprintf(buffer, BUFFLEN,"%d",i);
            printf("%s Starting trader %d (%s)\n", LOG_PREFIX, i, argv[i+2]);
            execl(argv[i+2], argv[i+2], buffer, (char*) NULL); 
            perror("execv"); // If exec success it will never return
            exit(3);
        }
        usleep(50000); // give some time for trader to connect and suspend
        connect_pipes(i);
    }

    // -------------------------- MARKET OPEN --------------------------
    reset_fds();
    for (int i = 0; i < num_traders; i++) {
        if (FD_ISSET(exchange_pool->fds_set[i], &exchange_pool->rfds)) {
            char msg[] = MARKET_OPEN_MSG;
            write(exchange_pool->fds_set[i], msg, strlen(msg));
            kill(pids[i], SIGUSR1);
        } else {
            perror("file descriptor not ready");
            exit(4);
        }
    }
    
    // register signal handler
    sigset_t mask;

    Signal(SIGUSR1, sigusr1_handler);
    // Signal(SIGCHLD, sigchld_handler); 

    sigemptyset(&mask); // clears all signals in mask
    sigaddset(&mask, SIGUSR1); // add sigusr1 to the set

    while (!all_children_terminated) {
        if (sigusr1_received) {
            sigprocmask(SIG_BLOCK, &mask, NULL); // block
            
            struct timeval timeout;
            // Set the timeout value to 5 seconds
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;
            
            // wait for any fds to become ready
            int tr_num = select(trader_pool->maxfd+1, &trader_pool->rfds, NULL, NULL, &timeout);
            int ex_num = select(exchange_pool->maxfd+1, NULL, &exchange_pool->rfds, NULL, &timeout);
            
            if (tr_num == 0 || ex_num == 0) {
                perror("Select timed out");
                exit(4);
            } else if ((tr_num == -1 || ex_num == -1)) {
                perror("Select failed");
                exit(4);
            } else {
                while (head_sig != NULL) {
                    if (FD_ISSET(trader_pool->fds_set[head_sig->trader_id], &trader_pool->rfds) 
                            && FD_ISSET(exchange_pool->fds_set[head_sig->trader_id], &exchange_pool->rfds)) {
                        rw_trader(head_sig->trader_id, trader_pool->fds_set[head_sig->trader_id], 
                                    exchange_pool->fds_set[head_sig->trader_id]);
                    }
                    dequeue();

                    match_order();                    
                    report_order_book();
                }
            } 
            sigusr1_received = 0;
            sigprocmask(SIG_UNBLOCK, &mask, NULL); // unblock
        }

        pid_t pid = 0;
        int status = -1;
        int trader_id = -1;
        
        // wait for all children process to exit
        while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            for (int i = 0; i < num_traders; i++) {
                if (pids[i] == pid) {
                    trader_id = i;
                    FD_CLR(trader_pool->fds_set[i], &trader_pool->rfds);
                    FD_CLR(exchange_pool->fds_set[i], &exchange_pool->rfds);
                    break;
                }
            }
            printf("%s Trader %d disconnected\n", LOG_PREFIX, trader_id);
        }

        if (pid == -1) {
            if (errno == ECHILD) {
                all_children_terminated = 1;
            }
            else {
                perror("waitpid error");
                exit(6);
            }
        }
    }

    // disconnect
    for (int i = 0; i < num_traders; i++){
        char fifo_buffer_ex[BUFFLEN], fifo_buffer_tr[BUFFLEN];
        snprintf(fifo_buffer_ex, BUFFLEN, FIFO_EXCHANGE, i); // i = assigned trader id (from 0)  
        snprintf(fifo_buffer_tr, BUFFLEN, FIFO_TRADER, i);  
        close(exchange_pool->fds_set[i]);
        close(trader_pool->fds_set[i]);
        unlink(fifo_buffer_ex);
        unlink(fifo_buffer_tr);
    }

    // free all malloced memory
    free_mem();

    printf("%s Trading completed\n", LOG_PREFIX);
    printf("%s Exchange fees collected: $%d\n", LOG_PREFIX, total_ex_fee);
    
    return 0;
}

int rw_trader(int id, int fd_trader, int fd_exchange) {
    char line[BUFFLEN];
    char cmd[ARG_SIZE], product[PRODUCT_LEN];
    int order_id = -1;
    int qty = -1;
    int price = -1;

    // printf("trader_id=%d, fd_trader=%d\n", id, trader_pool->fds_set[id]);

    // check the read descriptor is ready
    int num_bytes = read(fd_trader, line, sizeof(line));

    if (num_bytes == -1) {
        perror("Read failure");
        exit(4);
    } 
    else if (num_bytes == 0) {
        perror("Read end of pipe closed");
        exit(4);
    } 
    else {
        for (int i = 0; i < strlen(line); i++){
            if (line[i] == ';') {
                line[i] = '\0'; // terminate the message
                break;
            } 
        }
        printf("%s [T%d] Parsing command: <%s>\n", LOG_PREFIX, id, line);

        sscanf(line, "%s", cmd); // read until first space
        char write_line[BUFFLEN];
        
        if (strcmp(cmd, CMD_STRING[BUY]) == 0 &&
            sscanf(line, BUY_MSG, &order_id, product, &qty, &price) != EOF) {
                snprintf(write_line, BUFFLEN, MARKET_ACPT_MSG, order_id);
                // printf("%s\n", write_line);
                add_order(create_order(BUY_ORDER, pids[id], id, order_id, product, qty, price));
                write(fd_exchange, write_line, strlen(write_line));
        } 
        else if (strcmp(cmd, CMD_STRING[SELL]) == 0 &&
            sscanf(line, SELL_MSG, &order_id, product, &qty, &price) != EOF) {
                snprintf(write_line, BUFFLEN, MARKET_ACPT_MSG, order_id);
                // printf("%s\n", write_line);
                add_order(create_order(SELL_ORDER, pids[id], id, order_id, product, qty, price));
                write(fd_exchange, write_line, strlen(write_line));
        }  
        else if (strcmp(cmd, CMD_STRING[AMEND]) == 0 &&
            sscanf(line, AMD_MSG, &order_id, &qty, &price) != EOF) {
                snprintf(write_line, BUFFLEN, MARKET_AMD_MSG, order_id);
                // printf("%s\n", write_line);
                // TODO: amend
                write(fd_exchange, write_line, strlen(write_line));
        }
        else if (strcmp(cmd, CMD_STRING[CANCEL]) == 0 &&
            sscanf(line, CANCL_MSG, &order_id) != EOF) {
                snprintf(write_line, BUFFLEN, MARKET_CANCL_MSG, order_id);
                // printf("%s\n", write_line);
                // TODO: cancel
                write(fd_exchange, write_line, strlen(write_line));
        }
        else { // invalid command
            write(fd_exchange, MARKET_IVD_MSG, strlen(MARKET_IVD_MSG));
        }
        kill(pids[id], SIGUSR1);
    }
    return 0;
}

void match_order() {
    // TODO: match order
    return;
}

void report_order_book() {
    printf("%s\t--ORDERBOOK--\n", LOG_PREFIX);
    // ----------- ORDERS -------------
    for (int i = 0; i < product_num; i++) {
        orderbook_node book = orderbook[i];
        printf("%s\tProduct: %s; Buy levels: %d; Sell levels: %d\n", 
                LOG_PREFIX, book->product, book->buy_level, book->sell_level);
        
        order_node curr = book->head_order;
        if (curr == NULL) puts("null!");
        while (curr != NULL) {
            int qty = curr->qty;
            int price = curr->price;
            int num_order = 1;
            int sec_qty = 0;
            int sec_price = 0;
            int sec_num_order = 0;
            order_node tmp = curr->next;
            while (tmp->price == price) {
                if (tmp->order_type == curr->order_type) {
                    qty += tmp->qty;
                    price += tmp->price;
                    num_order++;
                    tmp = tmp->next;
                } else {
                    sec_qty += tmp->qty;
                    sec_price += tmp->price;
                    sec_num_order++;
                }
            }

            printf("%s\t\t%s %d @ $%d (%d order)\n", 
                LOG_PREFIX, CMD_STRING[curr->order_type], qty, price, num_order);
            if (sec_num_order > 0) {
                printf("%s\t\t%s %d @ $%d (%d order)\n", 
                    LOG_PREFIX, CMD_STRING[1-curr->order_type], sec_qty, 
                    sec_price, sec_num_order); // buy=0; sell=1
            }
            curr = tmp;
        }
    }
    // ----------- POSITION -------------
    printf("%s\t--POSITIONS--\n", LOG_PREFIX);
    //TODO: positions
    return;
}

void parse_products(char* filename) {
    char buffer[BUFFLEN];
    FILE * fp = fopen(filename, "r");
    
    product_num = atoi(fgets(buffer, BUFFLEN, fp));
    product_ls = malloc(product_num * sizeof(char*));

    for (int i = 0; i < product_num; i++) {
        if (fgets(buffer, BUFFLEN, fp) == NULL) {
            perror("product num inconsistent");
            exit(3);
        }
        buffer[strlen(buffer)-1] = '\0';
        product_ls[i] = calloc(PRODUCT_LEN+1, sizeof(char)); // (+1 null terminator)
        strcpy(product_ls[i], buffer);
    }
    
    printf("%s Trading %d products: ", LOG_PREFIX, product_num);
    for (int i = 0; i < product_num; i++) {
        // printf("%s", product_ls[i]);
        if (i == product_num-1)
            printf("%s\n", product_ls[i]);
        else
            printf("%s ", product_ls[i]);
    }

    fclose(fp);
}

// create pipes for 1 trader
void ini_pipes(int i) {
    char fifo_buffer_ex[BUFFLEN], fifo_buffer_tr[BUFFLEN];
    snprintf(fifo_buffer_ex, BUFFLEN, FIFO_EXCHANGE, i); // i = assigned trader id (from 0)  
    snprintf(fifo_buffer_tr, BUFFLEN, FIFO_TRADER, i);  
    unlink(fifo_buffer_ex); // remove if exist
    unlink(fifo_buffer_tr);

    // create two named pipes
    // 0777 = read, write, & execute for owner, group and others
    if (mkfifo(fifo_buffer_tr, 0777) == -1) {
        perror("make fifo_trader failed");
        exit(4);
    }
    if (mkfifo(fifo_buffer_ex, 0777) == -1) {
        perror("make fifo_exchange failed");
        exit(4);
    }

    // successfully created 2 fifos
    printf("%s Created FIFO /tmp/pe_exchange_%d\n", LOG_PREFIX, i);
    printf("%s Created FIFO /tmp/pe_trader_%d\n", LOG_PREFIX, i);
    return;
}

// create pipes for 1 trader
void connect_pipes(int i) {
    char fifo_buffer_ex[BUFFLEN], fifo_buffer_tr[BUFFLEN];
    snprintf(fifo_buffer_ex, BUFFLEN, FIFO_EXCHANGE, i); // i = assigned trader id (from 0)  
    snprintf(fifo_buffer_tr, BUFFLEN, FIFO_TRADER, i);  

    // exchange write to fifo_ex; read from fifo_tr
    int fd_tr = open(fifo_buffer_tr, O_RDONLY | O_NONBLOCK);
    trader_pool->fds_set[i] = fd_tr;
    int fd_ex = open(fifo_buffer_ex, O_RDWR | O_NONBLOCK); // must read to open write in non-block
    exchange_pool->fds_set[i] = fd_ex;
    
    if (fd_tr == -1 || fd_ex == -1) {
        printf("r_fd=%d w_fd=%d\n", fd_tr, fd_ex);
        perror("open fifo failed");
        exit(4);
    }

    printf("%s Connected to /tmp/pe_exchange_%d\n", LOG_PREFIX, i);
    printf("%s Connected to /tmp/pe_trader_%d\n", LOG_PREFIX, i);
    
    return;
}

void free_mem() {
    for (int i = 0; i < product_num; i++) {
        free(product_ls[i]);
    }
    free(product_ls);
    free(pids);
    free(trader_pool->fds_set);
    free(exchange_pool->fds_set);

    signal_node tmp = NULL;
    while (head_sig != NULL) {
       tmp = head_sig;
       head_sig = head_sig->next;
       free(tmp);
    }

    for (int i = 0; i < product_num; i++) {
        orderbook_node book = orderbook[i];
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