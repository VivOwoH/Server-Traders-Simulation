// maintain commands and responses here with X-Macros
#define _COMMAND(_CMD) \
        _CMD(BUY)   \
        _CMD(SELL)  \
        _CMD(AMEND)   \
        _CMD(CANCEL)  \
        _CMD(FILL)  \

#define _TO_ENUM(ENUM) ENUM, // comma to separate elements
#define _TO_STRING(STRING) #STRING,  // #=stringification 

enum COMMANDS {
    _COMMAND(_TO_ENUM)
};

static const char * CMD_STRING[] = {
    _COMMAND(_TO_STRING)
};


// Response msg
#define MARKET_ACPT_MSG "ACCEPTED %d;" // <ORDER_ID>
#define MARKET_AMD_MSG "AMENDED %d;" // <ORDER_ID>
#define MARKET_CANCL_MSG "CANCELLED %d;" // <ORDER_ID>
#define MARKET_IVD_MSG "INVALID;"

#define MARKET_OPEN_MSG "MARKET OPEN;"
#define MARKET_MSG "MARKET %s %s %d %d;" //<ORDER TYPE> <PRODUCT> <QTY> <PRICE>

// cmd msg
#define CHK_BUY_MSG "BUY %d %s %d %d; %c" // <ORDER_ID> <PRODUCT> <QTY> <PRICE>
#define CHK_SELL_MSG "SELL %d %s %d %d; %c" // <ORDER_ID> <PRODUCT> <QTY> <PRICE>
#define CHK_AMD_MSG "AMEND %d %d %d; %c" // <ORDER_ID> <QTY> <PRICE>
#define CHK_CANCL_MSG "CANCEL %d; %c" // <ORDER_ID>

#define BUY_MSG "BUY %d %s %d %d;" // <ORDER_ID> <PRODUCT> <QTY> <PRICE>
#define SELL_MSG "SELL %d %s %d %d;" // <ORDER_ID> <PRODUCT> <QTY> <PRICE>
#define AMD_MSG "AMEND %d %d %d;" // <ORDER_ID> <QTY> <PRICE>
#define CANCL_MSG "CANCEL %d;" // <ORDER_ID>

#define FILL_MSG "FILL %d %lld;" // <ORDER_ID> <QTY>