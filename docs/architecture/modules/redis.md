Module - Redis
==============

The Redis module exposes the redis-like functionalities to the end-user providing a compatible interface over the wire
to allow unmodified client application to take advantage of better performances, lower latencies and an improved
scalability.

The module supports both authentication via mTLS (mutual TLS) and username/password based authentication, or both,
depending on the configuration.

## Supported commands

### Basic commands

Only a subset of commands are supported, mostly string and keyspace related ones:

| Command       | Notes                                                |
|---------------|------------------------------------------------------|
| ✔ APPEND      |                                                      |
| ✔ AUTH        |                                                      |
| ✔ BGSAVE      |                                                      |
| ✔ COPY        | Missing DB parameter                                 |
| ✔ DBSIZE      |                                                      |
| ✔ DECR        |                                                      |
| ✔ DECRBY      |                                                      |
| ✔ DEL         |                                                      |
| ✔ EXISTS      |                                                      |
| ✔ EXPIRE      |                                                      |
| ✔ EXPIREAT    |                                                      |
| ✔ EXPIRETIME  |                                                      |
| ✔ FLUSHDB     | Missing ASYNC parameter                              |
| ✔ GET         |                                                      |
| ✔ GETDEL      |                                                      |
| ✔ GETEX       |                                                      |
| ✔ GETRANGE    |                                                      |
| ✔ GETSET      |                                                      |
| ✔ HELLO       |                                                      |
| ✔ INCR        |                                                      |
| ✔ INCRBY      |                                                      |
| ✔ INCRBYFLOAT |                                                      |
| ✔ KEYS        |                                                      |
| ✔ LCS         | Missing IDX, MINMATCHLEN and WITHMATCHLEN parameters |
| ✔ MGET        |                                                      |
| ✔ MSET        |                                                      |
| ✔ MSETNX      |                                                      |
| ✔ PERSIST     |                                                      |
| ✔ PEXPIRE     |                                                      |
| ✔ PEXPIREAT   |                                                      |
| ✔ PEXPIRETIME |                                                      |
| ✔ PING        |                                                      |
| ✔ PSETEX      |                                                      |
| ✔ PTTL        |                                                      |
| ✔ QUIT        |                                                      |
| ✔ RANDOMKEY   |                                                      |
| ✔ RENAME      |                                                      |
| ✔ RENAMENX    |                                                      |
| ✔ SAVE        |                                                      |
| ✔ SCAN        | Missing TYPE parameter                               |
| ✔ SET         |                                                      |
| ✔ SETEX       |                                                      |
| ✔ SETNX       |                                                      |
| ✔ SETRANGE    |                                                      |
| ✔ SHUTDOWN    | Missing the NOW and FORCE parameters                 |
| ✔ STRLEN      |                                                      |
| ✔ SUBSTR      |                                                      |
| ✔ TOUCH       |                                                      |
| ✔ TTL         |                                                      |
| ✔ UNLINK      |                                                      |
