# multiproxy
Multiproxy written in C. Tested on Ubuntu 18.04.
# usage
Arguments should be passed like that: numport:address:rmtport where:\n
numport is a number of the port which we listen on for upcoming connections\n
address is an address on which we want to redirect connection (it can be for example 127.0.0.1 or localhost)\n
rmtport is a port on remote machine we want to redirect connection to.
