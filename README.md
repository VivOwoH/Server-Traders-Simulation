1. Describe how your exchange works.

The flow of exchange is as follows:
[Initialization]
- parse product file -> list of products, number of products
- initalize variables and structs
- create orderbook for each product
- create pipes
- fork processes and launch traders
- connect to pipes (open and put them in corresponding exchange/trader file descriptors pool)
- 

2. Describe your design decisions for the trader and how it's fault-tolerant.



3. Describe your tests and how to run them.
