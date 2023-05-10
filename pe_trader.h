#ifndef PE_TRADER_H
#define PE_TRADER_H

#define BUFFLEN 128
#define ARG_SIZE 10
#define PRODUCT_LEN 16 // PRODUCT: string, alphanumeric, case sensitive,
                       // up to 16 characters

#include "pe_common.h"
#include "msg_cmd.h"
#include "mysignal.h"

int event();

#endif
