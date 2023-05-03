#include "mysignal.h"

/* USYD CODE CITATION ACKNOWLEDGEMENT
* I declare that following code structure have been adapted from textbook
* Computer Systems: A Programmer's Perspective 3rd edition (2016) p811, but it is mostly my own work. 
*/
// Sigaction wrapper
handler_t* Signal(int signum, handler_t *handler) {
    struct sigaction action, old_action;

    old_action.sa_flags = SA_SIGINFO;
    action.sa_flags = SA_SIGINFO; // sa_sigaction (not sa_handler) 
                            // specifies the signal-handling function for signum 
    action.sa_sigaction = handler;
    sigemptyset(&action.sa_mask); // block signals of type currently being processed by handler

    if (sigaction(signum, &action, &old_action) == -1) {
        perror("sigaction error");
        exit(1);
    }
    
    return old_action.sa_sigaction;
}