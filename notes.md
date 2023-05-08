
------------------------ IGNORE -----------------------------
# NOTES 
- fluctuated testcases:
    - sigaction oldact.sa_flag = 0 (maybe?)
    - unitialized local variables qty and price (more likely)

exit codes:
1 - sigaction
2 - fork
3 - product num & provided num inconsistent
4 - file descriptor errors
5 - invalid message (reach max number of arguments)