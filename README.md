# shmcache

re-write libshmcache for practice. The data structure is quite different from libshmcache, see the pdf for more info.

TODO

0. replace mutex with rwlock? 
1. move read_date() out of the lock and add check algo(crc32?) to ensure that the data is complete?
2. add compress algorithm for value?
