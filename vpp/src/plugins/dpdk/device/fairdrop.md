node.c has all the functions related to fairdrop thread at the beginning of the file untill the commnet "end of thread" 

fairdrop.h is a header file with the required structures.

flow_table_cpu.h and flow_table_var.h as still included in node.c and are same as before.

To specify the core number for fairdrop thread, use corelist-fairdrop N in cpu section of startup conf. Ex: cpu {main-core 28 corelist-fairdrop 29}

Todo:
understand the locking mechanism and do some optimization.
