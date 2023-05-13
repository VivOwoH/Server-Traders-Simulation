# 1. Describe how your exchange works.

##### The flow of exchange is as follows:

[INITIALIZATION]
- parse product file -> list of products, number of products
- initalize variables and structs
- create orderbook for each product
- create pipes
- fork processes and launch traders
- usleep(80000) for children process to connect
- connect to pipes (open and put them in corresponding exchange/trader file descriptors pool)
- MARKET OPEN to all exchange pipes
- register signal handlers

[EVENT]
- block SIGUSR1 so that it does not interrupt select
- select() for both read/write fds to wait for any fds to become ready
- SIGUSR1 handler will capture the pid of trader,along with trader_id. Both are used to determine which trader pipe to read from
- if it is a success order (buy,sell,cancel,amend) from pipe, proceed to match orders, then report the orderbook
- unblock SIGUSR1 and reset fds as select() may change their behaviour

[TERMINATE]
- the event loop break after all children processes terminate (kept track of with waitpid)
- close all pipes and remove them 
- caluclate total exchange feee
- free all memory allocated
- report complete and total exchange fee


##### Significant data structures used:

[ORDERBOOK]
The orders are stored as a linkedlist, with 2 pointers (head_order, tail_order) maintained in the orderbook. When we add a order_node to the list, we make sure it is sorted in price-time priority. Head being highest price and earliest. 

Note that the tail_order is not the last node in the list, but the sell order with lowest price. This is to make the process of matching orders easier. When matching orders, we start from head_order to find the first buy order, which would be the highest buy order. The tail_order is the lowest sell order. 

There will be individual order book for each product. Each orderbook include unique sell and buy level, a list of numbers of this product held per trader, and a list of exchange fee put in this product per trader. 


## 2. Describe your design decisions for the trader and how it's fault-tolerant.

In addition to the specification of the auto-trader, the trader has a alarm for 3 seconds in its loop. The SIGALRM handler does not do anything if the trader has not sent an order. If it has sent an order, the SIGALRM handler will check if a SIGUSR1 has been received since the order has been sent. If not, the trader will attempt to send the last order again, and this action increment the retry attempt. The same check will be repeated until max attempt (3) is reached, or a SIGUSR1 has been picked up. If the return message is "accepted", the trader will increment its unique order id. 


## 3. Describe your tests and how to run them.

To run the tests, run "make run_tests".

Due to the lack of time, I have only produced unit-tests for the queue.c source file, which contains the main orderbook data structures. The unit-tests tests for individual functions, and in each test contains both valid/invalid cases. 
The list of functions tested can be found in queue.h file, and the comments above testcases briefly explains what each of them does.

