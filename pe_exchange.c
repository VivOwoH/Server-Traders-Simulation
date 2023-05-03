/**
 * comp2017 - assignment 3
 * Vivian Ha
 * weha7612
 */

#include "pe_exchange.h"

int product_num = 0;
char ** product_ls;
int num_traders = 0;
signal_node head_sig = NULL;
pid_t * pids;
struct fd_pool ex_fds;
struct fd_pool tr_fds;
struct fd_pool * exchange_pool = &ex_fds;
struct fd_pool * trader_pool = &tr_fds;

void sigusr1_handler(int s, siginfo_t *info, void *context) {
    puts("exchange received sigusr1");
    signal_node node = (signal_node) malloc(sizeof(struct linkedList));
    node->pid = info->si_pid;
    node->next = NULL;
    enqueue(node);
    return;
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        printf("usage: ./pe_exchange [product file] [trader 0] [trader 1] ... [trader n]\n");
        return 1;
    }

    // e.g. ./pe_exchange products.txt ./trader_a ./trader_b
    parse_products(argv[1]);

    num_traders = argc - 2;
    trader_pool->maxfd = 0;
    exchange_pool->maxfd = 0;
    trader_pool->fds_set = malloc(sizeof(int) * (num_traders)); 
    exchange_pool->fds_set = malloc(sizeof(int) * (num_traders)); 

    // initialization
    ini_pipes();

    // launch binaries (fork + exec)
    // each child exchange has a ex_fd that connects to the corresponding tr_fd
    // i.e. exchange_child_0 [exchange_fd_0] <-> [trader_fd_0] trader 0
    pids = malloc(sizeof(pid_t) * (num_traders));
    for (int i = 0; i < num_traders; i++) {
        pid_t pid = fork();
        pids[i] = pid;

        if (pid < 0) {
            perror("Fork error");
            exit(2);
        }
        else if (pid == 0) { // child process: exec binary
            char buffer[BUFFLEN];
            snprintf(buffer, BUFFLEN,"%d",i);
            execl(argv[i+2], argv[i+2], buffer, (char*) NULL); 
            perror("execv"); // If exec success it will never return
            exit(3);
        } 
        // printf("parent%d\n", getpid());
        // printf("pids[i]%d\n", pids[i]);
        if (FD_ISSET(exchange_pool->fds_set[i], &exchange_pool->rfds)) {
            char msg[] = "MARKET OPEN;";
            write(exchange_pool->fds_set[i], msg, strlen(msg));
            usleep(50000);
            kill(pid, SIGUSR1);
        }
    }
    
    // register signal handler
    sigset_t mask;
    Signal(SIGUSR1, sigusr1_handler);
    sigemptyset(&mask); // clears all signals in mask
    sigaddset(&mask, SIGUSR1); // add sigusr1 to the set

    while (1) {
        // wait for fds to become "ready"
        trader_pool->num_rfds = select(trader_pool->maxfd+1, &trader_pool->rfds, NULL, NULL, NULL);
        exchange_pool->num_rfds = select(exchange_pool->maxfd+1, &exchange_pool->rfds, NULL, NULL, NULL);

        process_next_signal();
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

    puts("close market"); // TODO: modify this
    free_mem();
    return 0;
}

signal_node enqueue(signal_node node) {
    if (head_sig == NULL) {
        head_sig = node;
    } else {
        signal_node tmp = head_sig; 
        while(tmp->next != NULL){
            tmp = tmp->next;//traverse the list until p is the last node.The last node always points to NULL.
        }
        tmp->next = node;//Point the previous last node to the new node created.
    }
    return head_sig;
}

void process_next_signal() {
    if (head_sig != NULL) {
        // Process the signal here
        // ...
        puts("next signal is being processed");
    }
}

void parse_products(char* filename) {
    char buffer[BUFFLEN];
    FILE * fp = fopen(filename, "r");
    
    product_num = atoi(fgets(buffer, BUFFLEN, fp));
    product_ls = malloc(product_num * sizeof(char *));
    
    int i = 0;
    while(fgets(buffer, BUFFLEN, fp)) {
        if (i + 1 > product_num) {
            perror("product num inconsistent");
            exit(3);
        }
        buffer[strlen(buffer)-1] = '\0';
        product_ls[i] = calloc(PRODUCT_LEN, sizeof(char));
        strcpy(buffer, product_ls[i]);
        i++;
    }

    fclose(fp);
}

// create pipes for traders
void ini_pipes() {
    for (int i = 0; i < num_traders; i++) {
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
        
        if (fd_tr > trader_pool->maxfd)
            trader_pool->maxfd = fd_tr;
        if (fd_ex > exchange_pool->maxfd)
            exchange_pool->maxfd = fd_ex;
        
        FD_ZERO(&trader_pool->rfds); // clear all
        FD_ZERO(&exchange_pool->rfds);
        FD_SET(fd_tr, &trader_pool->rfds); // add created fds to rfds
        FD_SET(fd_ex, &exchange_pool->rfds);
    }
    return;
}

void free_mem() {
    for (int i = 0; i < product_num; i++) {
        free(product_ls[i]);
    }
    free(product_ls);
}