/**
 * comp2017 - assignment 3
 * Vivian Ha
 * weha7612
 */

#include "pe_exchange.h"

int product_num = 0;
char ** product_ls;
int num_traders = 0;
int * order_id_ls;
int total_ex_fee = 0;
pid_t * pids;
struct fd_pool ex_fds; // exchange file descriptors
struct fd_pool tr_fds; // trader file descriptors
struct fd_pool * exchange_pool = &ex_fds;
struct fd_pool * trader_pool = &tr_fds;
int all_children_terminated = 0;
int sigusr1_received = 0;
int pid_wip[2] = {0};
int order_time = 0; // orders from earliest to latest

void sigusr1_handler(int s, siginfo_t *info, void *context) {
    printf("exchange received sigusr1 from %d\n", info->si_pid);
    sigusr1_received = 1;
    pid_wip[0] = info->si_pid;
    for (int i = 0; i < num_traders; i++) {
        if (pids[i] == pid_wip[0]) {
            pid_wip[1] = i;
            break;
        }
    }
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

    parse_products(argv[1]);
    
    num_traders = argc - 2;
    trader_pool->maxfd = 0;
    exchange_pool->maxfd = 0;
    trader_pool->fds_set = malloc(sizeof(int) * (num_traders)); 
    exchange_pool->fds_set = malloc(sizeof(int) * (num_traders)); 

    create_orderbook(num_traders, product_num, product_ls); // each product has a orderbook

    order_id_ls = calloc(num_traders, sizeof(int));
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
        char msg[] = MARKET_OPEN_MSG;
        write(exchange_pool->fds_set[i], msg, strlen(msg));
        if (kill(pids[i], SIGUSR1) == -1) {
            perror("signal: kill failed");
            exit(1);
        }
    }
    
    // register signal handler
    sigset_t mask;

    Signal(SIGUSR1, sigusr1_handler);
    // Signal(SIGCHLD, sigchld_hanlder); 

    sigemptyset(&mask); // clears all signals in mask
    sigaddset(&mask, SIGUSR1); // add sigusr1 to the set
    // sigaddset(&mask, SIGCHLD); // add sigchld to the set

    while (!all_children_terminated) {
        sigprocmask(SIG_BLOCK, &mask, NULL); // block
        
        // wait for any fds to become ready
        int tr_num = select(trader_pool->maxfd+1, &trader_pool->rfds, NULL, NULL, NULL);
        int ex_num = select(exchange_pool->maxfd+1, NULL, &exchange_pool->rfds, NULL, NULL);
        // printf("tr:%d ex:%d\n", tr_num, ex_num);
        
        if (tr_num == 0 || ex_num == 0 || 
            tr_num == -1 || ex_num == -1) {
            perror("Select failed");
            exit(4);
        } else {
            int success = 0;
            if (sigusr1_received && FD_ISSET(trader_pool->fds_set[pid_wip[1]], &trader_pool->rfds) 
                    && FD_ISSET(exchange_pool->fds_set[pid_wip[1]], &exchange_pool->rfds)) {
                success = rw_trader(pid_wip[1], trader_pool->fds_set[pid_wip[1]], exchange_pool->fds_set[pid_wip[1]]);
            }

            if (success) {
                match_order();                    
                report_order_book();
            }
        } 
        sigusr1_received = 0;
        sigprocmask(SIG_UNBLOCK, &mask, NULL); // unblock
        reset_fds();

        
        pid_t pid = 0;
        int status = -1;
        int trader_id = -1;

        // wait for all children process to exit
        while ( (pid = waitpid(-1, &status, WNOHANG)) > 0) {
            for (int i = 0; i < num_traders; i++) {
                if (pids[i] == pid) {
                    trader_id = i;
                    // close(trader_pool->fds_set[i]);
                    // close(exchange_pool->fds_set[i]);
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
            else if (errno != EINTR) {
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

    calculate_total_ex_fee();
    // free all malloced memory
    free_mem();

    printf("%s Trading completed\n", LOG_PREFIX);
    printf("%s Exchange fees collected: $%d\n", LOG_PREFIX, total_ex_fee);
    
    return 0;
}

int valid_check(int trader_id, int order_type, int order_id, char * product, int qty, int price) {
    // product is in list
    // ORDER_ID: integer, 0 - 999999 (incremental)
    // QTY, PRICE: integer, 1 - 999999
    // order_id->all; qty,price->buy,sell,amend; product->buy,sell
    if (order_id < 0 || order_id > 999999 || order_id != order_id_ls[trader_id])
        return 0;
    if (order_type == BUY || order_type == SELL || order_type == AMEND) {
        if (qty<1 || qty>999999 || price<1 || price>999999)
            return 0;
    }
    int valid = 0;
    if (order_type == BUY || order_type == SELL) {
        for (int i = 0; i < product_num; i++) {
            valid = (strcmp(product_ls[i], product)==0) ? 1 : valid;
        }
    }
    return valid;
}

int rw_trader(int id, int fd_trader, int fd_exchange) {
    char line[BUFFLEN];
    char cmd[ARG_SIZE], product[PRODUCT_LEN];
    int order_id = -1;
    int qty = -1;
    int price = -1;
    int success_order = 1;
    order_node order = NULL;

    printf("trader_id=%d, fd_trader=%d, fd_exchange=%d\n", id, fd_trader, fd_exchange);

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
            success_order = valid_check(id, BUY, order_id, product, qty, price);
            printf("%d ", success_order);

            if (success_order) {
                puts("success write");
                write(fd_exchange, write_line, strlen(write_line));
                kill(pids[id], SIGUSR1);
                order = create_order(BUY_ORDER, order_time, pids[id], id, order_id, product, qty, price);
                add_order(order, NULL);
                order_id_ls[id] = order_id + 1;
                order_time++; // increment counter
            }
        } 
        else if (strcmp(cmd, CMD_STRING[SELL]) == 0 &&
                    sscanf(line, SELL_MSG, &order_id, product, &qty, &price) != EOF) {
            snprintf(write_line, BUFFLEN, MARKET_ACPT_MSG, order_id);
            success_order = valid_check(id, SELL, order_id, product, qty, price);
            printf("%d ", success_order);

            if (success_order) {
                puts("success sell");
                write(fd_exchange, write_line, strlen(write_line));
                kill(pids[id], SIGUSR1);
                order = create_order(SELL_ORDER, order_time, pids[id], id, order_id, product, qty, price);
                add_order(order, NULL);
                order_id_ls[id] = order_id + 1;
                order_time++; // increment counter
            }
        }  
        else if (strcmp(cmd, CMD_STRING[AMEND]) == 0 &&
                    sscanf(line, AMD_MSG, &order_id, &qty, &price) != EOF) {
            snprintf(write_line, BUFFLEN, MARKET_AMD_MSG, order_id);
            success_order = valid_check(id, AMEND, order_id, product, qty, price);
            printf("%d ", success_order);

            if (success_order) {
                puts("success amend");
                write(fd_exchange, write_line, strlen(write_line));
                kill(pids[id], SIGUSR1);
                order = amend_order(id, order_id, qty, price);
            }
        }
        else if (strcmp(cmd, CMD_STRING[CANCEL]) == 0 &&
                    sscanf(line, CANCL_MSG, &order_id) != EOF) {
            snprintf(write_line, BUFFLEN, MARKET_CANCL_MSG, order_id);
            success_order = valid_check(id, CANCEL, order_id, product, qty, price);
            printf("%d ", success_order);

            if (success_order) {
                puts("success cancel");
                write(fd_exchange, write_line, strlen(write_line));
                kill(pids[id], SIGUSR1);
                order = amend_order(id, order_id, 0, 0);
            } 
        }
        else { // invalid command
            success_order = 0;
        }

        if (!success_order) {
            write(fd_exchange, MARKET_IVD_MSG, strlen(MARKET_IVD_MSG));
            kill(pids[id], SIGUSR1);
        } else 
            market_alert(pids[id], order);
    }
    return success_order;
}

void market_alert(int pid, order_node order) {
    if (order == NULL) {
        perror("null order");
        exit(6);
    }

    char market_line[BUFFLEN];
    snprintf(market_line, BUFFLEN, MARKET_MSG, CMD_STRING[order->order_type], order->product,
                    order->qty, order->price);
    for (int i = 0; i < num_traders; i++) {
        if (pid != pids[i] && FD_ISSET(exchange_pool->fds_set[i], &exchange_pool->rfds)) {
            write(exchange_pool->fds_set[i], market_line, strlen(market_line));
            if (kill(pids[i], SIGUSR1)==-1) {
                perror("signal: kill failed");
                exit(1);
            }
        }
    }
}

void match_order_report(order_node highest_buy, order_node lowest_sell) {
    // matching price is the price of the older order
    // charge last trader 1% of qty*price
    int buy_fd = exchange_pool->fds_set[highest_buy->trader_id];
    int sell_fd = exchange_pool->fds_set[lowest_sell->trader_id];
    int final_price = -1;
    int transaction_fee = -1;
    int buy_fill_qty = highest_buy->qty;
    int sell_fill_qty = lowest_sell->qty;

    // -------------------- Fill + Amend -----------------------
    if (highest_buy->qty > lowest_sell->qty) {
        // sell is fullfilled, amend buy
        buy_fill_qty = lowest_sell->qty;
        amend_order(lowest_sell->trader_id, lowest_sell->order_id, 0, 0);
        amend_order(highest_buy->trader_id, highest_buy->order_id, 
                    (highest_buy->qty - buy_fill_qty), highest_buy->price);
    } else if (highest_buy->qty < lowest_sell->qty) {
        // buy is fullfilled, amend sell
        sell_fill_qty = highest_buy->qty;
        amend_order(highest_buy->trader_id, highest_buy->order_id, 0, 0);
        amend_order(lowest_sell->trader_id, lowest_sell->order_id, 
                    (lowest_sell->qty - sell_fill_qty), lowest_sell->price);
    } else {
        amend_order(highest_buy->trader_id, highest_buy->order_id, 0, 0);
        amend_order(lowest_sell->trader_id, lowest_sell->order_id, 0, 0);
    }

    // -------------------- value + fee -----------------------
    if (highest_buy->time > lowest_sell->time) {
        // match sell, new=buy
        final_price = lowest_sell->price * sell_fill_qty;
        transaction_fee = (int) (final_price * 0.01 + 0.5); 
                                            // fast round cast; not valid for neg number
        printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n", 
            LOG_PREFIX, lowest_sell->order_id, lowest_sell->trader_id, 
            highest_buy->order_id, highest_buy->trader_id, final_price, transaction_fee);
    } else {
        // match buy, new=sell
        final_price = highest_buy->price * buy_fill_qty;
        transaction_fee = (int) (final_price * 0.01 + 0.5);
        printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n", 
            LOG_PREFIX, highest_buy->order_id, highest_buy->trader_id, 
            lowest_sell->order_id, lowest_sell->trader_id, final_price, transaction_fee);
    }
    
    // -------------------- write + sig -----------------------
    char write_line[BUFFLEN], write_line_2[BUFFLEN];
    snprintf(write_line, BUFFLEN, FILL_MSG, highest_buy->order_id, buy_fill_qty);
    write(buy_fd, write_line, strlen(write_line));
    if (kill(highest_buy->pid, SIGUSR1)==-1) {
        perror("signal: kill failed");
        exit(1);
    }

    snprintf(write_line_2, BUFFLEN, FILL_MSG, lowest_sell->order_id, sell_fill_qty);
    write(sell_fd, write_line_2, strlen(write_line_2));
    if (kill(lowest_sell->pid, SIGUSR1)==-1) {
        perror("signal: kill failed");
        exit(1);
    }
}

void match_order() {
    puts("matching order......");
    orderbook_node book = NULL;
    order_node highest_buy = NULL;
    order_node lowest_sell = NULL;
    order_node head_curr = NULL;
    
    for (int i = 0; i < product_num; i++) {
        book = orderbook[i];
        int cont = 1;
        lowest_sell = book->tail_order;
        while (cont && lowest_sell != NULL) {
            lowest_sell = book->tail_order;
        
            head_curr = book->head_order;
            while (head_curr != NULL) {
                if (head_curr->order_type == BUY_ORDER) {
                    highest_buy = head_curr;
                    break;
                }
                head_curr = head_curr->next;
            }
            
            order_node tmp = lowest_sell->prev; // find earliest lowest price
            while (tmp != NULL && tmp->order_type == SELL_ORDER 
                    && (lowest_sell->price == tmp->price)) {
                lowest_sell = tmp;
                tmp = tmp->prev;
            }
            
            // match success
            if (highest_buy != NULL && (highest_buy->price >= lowest_sell->price)) {
                match_order_report(highest_buy, lowest_sell);
            } else cont = 0;
        } 
    }
    return;
}

void report_order_book() {
    int qty = 0;
    int price = 0;
    int num_order = 0;
    int sec_qty = 0;
    int sec_price = 0;
    int sec_num_order = 0;

    printf("%s\t--ORDERBOOK--\n", LOG_PREFIX);
    // ----------- ORDERS -------------
    for (int i = 0; i < product_num; i++) {
        orderbook_node book = orderbook[i];
        printf("%s\tProduct: %s; Buy levels: %d; Sell levels: %d\n", 
                LOG_PREFIX, book->product, book->buy_level, book->sell_level);
        
        order_node curr = book->head_order;
        // if (curr == NULL) puts("null!");
        while (curr != NULL) {
            qty = curr->qty;
            price = curr->price;
            num_order = 1;
            sec_qty = 0;
            sec_price = 0;
            sec_num_order = 0;
            order_node tmp = curr->next;
            while (tmp!= NULL && (tmp->price == price)) {
                if (tmp->order_type == curr->order_type) {
                    qty += tmp->qty;
                    num_order++;
                    tmp = tmp->next;
                } else {
                    sec_qty += tmp->qty;
                    sec_num_order++;
                }
            }

            printf("%s\t\t%s %d @ $%d (%d ", 
                LOG_PREFIX, CMD_STRING[curr->order_type], qty, price, num_order);
            if (num_order > 1) puts("orders)\n");
            else puts("order)\n");

            if (sec_num_order > 0) {
                printf("%s\t\t%s %d @ $%d (%d ", 
                    LOG_PREFIX, CMD_STRING[1-curr->order_type], sec_qty, 
                    sec_price, sec_num_order); // buy=0; sell=1
                if (sec_num_order > 1) puts("orders)\n");
                else puts("order)\n");
            }
            curr = tmp;
        }
    }
    // ----------- POSITION -------------
    printf("%s\t--POSITIONS--\n", LOG_PREFIX);
    for (int i = 0; i < num_traders; i++) {
        printf("%s\tTrader %d: ", LOG_PREFIX, i);
        for (int j = 0; j < product_num; j++) {
            orderbook_node book = orderbook[j];
            if (j == product_num-1)
                printf("%s %d ($%d)\n", book->product, 
                        book->trader_qty_index[i], book->trader_fee_index[i]);
            else 
                printf("%s %d ($%d), ", book->product, 
                        book->trader_qty_index[i], book->trader_fee_index[i]);
        }  
    }
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

void calculate_total_ex_fee() {
    for (int i = 0; i < product_num; i++) {
        orderbook_node book = orderbook[i];
        int sum = 0;
        for (int j = 0; j < num_traders; j++)
            sum += book->trader_fee_index[j];
        total_ex_fee += abs(sum);
    }
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
    
    free_orderbook();
}