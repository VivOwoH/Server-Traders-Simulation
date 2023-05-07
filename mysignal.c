#include "mysignal.h"

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