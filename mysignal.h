#ifndef PE_MYSIGNAL_H
#define PE_MYSIGNAL_H

#include "pe_common.h"

typedef void handler_t(int, siginfo_t *, void *);
handler_t *Signal(int signum, handler_t *handler); // [1] signal to catch; 
                                    // [2] pointer to the function that will be called when the signal is received

#endif