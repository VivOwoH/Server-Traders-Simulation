#include "pe_trader.h"

void sigusr1_handler(int s, siginfo_t *info, void *context) {
    puts("received signal");
    return;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }

    puts("running");

    pid_t parent_pid = getppid();

    int trader_id = atoi(argv[1]);
    int order_id = 0;

    // register signal handler
    sigset_t mask, prev;

    Signal(SIGUSR1, sigusr1_handler);
    sigemptyset(&mask); // clears all signals in mask
    sigaddset(&mask, SIGUSR1); // add sigusr1 to the set

    // connect to named pipes
    char buffer_exchange[BUFFLEN], buffer_trader[BUFFLEN];
    snprintf(buffer_exchange, BUFFLEN, FIFO_EXCHANGE, trader_id);
    snprintf(buffer_trader, BUFFLEN, FIFO_TRADER, trader_id);

    // for the exchange write to each trader (read)
    int fd_read = open(buffer_exchange, O_RDONLY);
    // for each trader to write to the exchange (write)
    int fd_write = open(buffer_trader, O_WRONLY);

    if (fd_read == -1 || fd_write == -1) {
        printf("r_fd=%d w_fd=%d\n", fd_read, fd_write);
        perror("open fifo failed");
        exit(4);
    }
    
    // event loop:
    while (1) {
        sigprocmask(SIG_BLOCK, &mask, &prev); // block
        // wait for sigusr1 to be received
        sigsuspend(&prev);
        
        // wait for exchange update (MARKET message)
        // send order   
        // wait for exchange confirmation (ACCEPTED message)
        char line[BUFFLEN];
        char arg0[ARG_SIZE], cmd[ARG_SIZE], product[ARG_SIZE];
        int qty = 0;
        int price = 0;

        if (read(fd_read, line, sizeof(line)) > 0) {
            for (int i = 0; i < strlen(line); i++){
                if (line[i] == ';') {
                    line[i] = '\0'; // terminate the message
                    break;
                } 
            }
            sscanf(line, "%s %s %s %d %d", arg0, cmd, product, &qty, &price);
        }
        // printf("%s %s %s %d %d\n", arg0, cmd, product, qty, price);

        if (qty >= 1000) {// disconnect and shut down if BUY with qty>=1000
            break;
        }

        if (strcmp(cmd, "SELL") == 0) {
            char write_line[BUFFLEN];
            snprintf(write_line, BUFFLEN, "BUY %d %s %d %d;", order_id, product, qty, price);
            write(fd_write, write_line, strlen(write_line));
            kill(parent_pid, SIGUSR1);
        } 
        else if (strcmp(arg0, "ACCEPTED") == 0) {
            order_id++;
        }
        sigprocmask(SIG_SETMASK, &prev, NULL); // unblock
    }
    close(fd_read); // disconnect with exchange
    close(fd_write); // shutdown trader client
    return 0;
}


