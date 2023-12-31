#include "pe_trader.h"

int trader_id = -1; // unique trader id
int order_id = 0; // unique order id sets for each trader
int fd_read = -1; // read file descriptor
int fd_write = -1; // write file descriptor
pid_t pid = -1;
pid_t parent_pid = -1;
int msg_sent = 0;
int signaled = 0;
int retry = 0;
char write_line[BUFFLEN];

// ----------------------------------------------------------
// ------------------------- SIGNAL -------------------------
// ----------------------------------------------------------
void sigusr1_handler(int s, siginfo_t *info, void *context) {
    puts("trader received sigusr1");
    if (msg_sent) signaled = 1;
    return;
}

void timeout_handler(int s, siginfo_t *info, void *context) {
    // Timeout occurred
    if (!msg_sent) return;

    if (msg_sent && retry == MAX_RETRY) {
        perror("time out");
        exit(1);
    }
    else if (msg_sent && !signaled) {
        // resent
        write(fd_write, write_line, strlen(write_line));
        kill(parent_pid, SIGUSR1);
        retry++;
    } else {
        msg_sent = 0;
        signaled = 0;
        retry = 0;
    }
}

// ----------------------------------------------------------
// -------------------------- MAIN --------------------------
// ----------------------------------------------------------
int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }

    trader_id = atoi(argv[1]);
    order_id = 0;

    pid = getpid();
    parent_pid = getppid();

    // register signal handler
    sigset_t mask, prev;

    Signal(SIGUSR1, sigusr1_handler);
    Signal(SIGALRM, timeout_handler);
    sigemptyset(&mask); // clears all signals in mask
    sigaddset(&mask, SIGUSR1); // add sigusr1 to the set

    // connect to named pipes
    char buffer_exchange[BUFFLEN], buffer_trader[BUFFLEN];
    snprintf(buffer_exchange, BUFFLEN, FIFO_EXCHANGE, trader_id);
    snprintf(buffer_trader, BUFFLEN, FIFO_TRADER, trader_id);

    // for the exchange write to each trader (read)
    fd_read = open(buffer_exchange, O_RDONLY);
    // for each trader to write to the exchange (write)
    fd_write = open(buffer_trader, O_WRONLY);

    if (fd_read == -1 || fd_write == -1) {
        printf("r_fd=%d w_fd=%d\n", fd_read, fd_write);
        perror("open fifo failed");
        exit(4);
    }

    int cont = 1;
    // event loop:
    while (cont) {
        sigprocmask(SIG_BLOCK, &mask, &prev); // block
        // wait for sigusr1 to be received
        alarm(3);
        sigsuspend(&prev);

        cont = event();
        
        sigprocmask(SIG_SETMASK, &prev, NULL); // unblock
    }
    close(fd_read); // disconnect with exchange
    close(fd_write); // shutdown trader client
    return 0;
}

int event() {
    char line[BUFFLEN];
    char arg0[ARG_SIZE], cmd[ARG_SIZE], product[PRODUCT_LEN];
    int qty = -1;
    int price = -1;

    // initial scan
    if (read(fd_read, line, sizeof(line)) > 0) {
        if (strcmp(arg0, MARKET_OPEN_MSG) == 0) {
            // indicate market open and finishes this event
            return 1;
        }

        for (int i = 0; i < strlen(line); i++){
            if (line[i] == ';') {
                line[i] = '\0'; // terminate the message
                break;
            } 
        }
        if (sscanf(line, "%s %s %s %d %d", arg0, cmd, product, &qty, &price) == EOF) {
            perror("Invalid line");
            exit(5);
        }
        // printf("%s %s %s %d %d\n", arg0, cmd, product, qty, price);
    }

    // arg0 = "MARKET" => market msg
    // arg0 != "MARKET" => must be 1 of the responses
    if (strcmp(arg0, "MARKET") == 0) {
        if (qty >= 1000) { 
            return 0; // disconnect and shut down if BUY with qty>=1000
        }

        if (strcmp(cmd, CMD_STRING[SELL]) == 0) {
            snprintf(write_line, BUFFLEN, BUY_MSG, order_id, product, qty, price);
            msg_sent = 1;
            write(fd_write, write_line, strlen(write_line));
            kill(parent_pid, SIGUSR1);
        }
    } 
    else if (strcmp(arg0, "ACCEPTED") == 0) {
        int given_order_id = -1;
        sscanf(line, MARKET_ACPT_MSG, &given_order_id); // rescan
        assert(given_order_id == order_id && "order id inconsistency in market and trader");
        order_id++;
    }
    return 1;
}
