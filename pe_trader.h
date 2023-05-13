#ifndef PE_TRADER_H
#define PE_TRADER_H

#define BUFFLEN 128
#define ARG_SIZE 10
#define PRODUCT_LEN 16 // PRODUCT: string, alphanumeric, case sensitive,
                       // up to 16 characters
#define MAX_RETRY 3

#include "pe_common.h"
#include "msg_cmd.h"
#include "queue.h"

int event();

#endif
