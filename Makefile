TARGET = pe_exchange
FILE = products.txt
TEST_TARGET = autotest.sh

CC = gcc
CFLAGS     = -Wall -Wuninitialized -Wmaybe-uninitialized -Werror -Wvla -O0 -std=c11 -g -fsanitize=address,leak,undefined
LDFLAGS    = -lm -fsanitize=address,leak,undefined
# CFLAGS     = -Wall -Wuninitialized -Wmaybe-uninitialized -Werror -Wvla -O0 -std=c11 -g
# LDFLAGS    = -lm 
BINARIES   = pe_exchange pe_trader 
EX_SRC        = pe_exchange.c queue.c
TR_SRC        = pe_trader.c queue.c 
EX_OBJ        = $(EX_SRC:.c=.o)
TR_OBJ		= $(TR_SRC:.c=.o)

all: $(BINARIES)

pe_exchange: $(EX_OBJ)
	$(CC) -o $@ $(EX_OBJ) $(LDFLAGS)

pe_trader: $(TR_OBJ)
	$(CC) -o $@ $(TR_OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@ 

run:
	./$(TARGET) $(FILE) ./trader_a ./trader_b ./trader_c

tests:
	echo "additional c file for tests":

# compile rule different in tests
run_tests:
	./$(TEST_TARGET)

.PHONY: clean
clean:
	rm -f $(BINARIES) *.o 

