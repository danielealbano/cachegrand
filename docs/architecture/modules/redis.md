Module - Redis
==============

The Redis module provides Redis-like functionalities to the end-user with a compatible interface over the wire
to allow unmodified client application to take advantage of better performances, lower latencies and an improved
scalability.

The module supports both authentication via mTLS (mutual TLS) and username/password based authentication, or both,
depending on the configuration.

## Supported commands

| Command       | Notes                                                                                            |
|---------------|--------------------------------------------------------------------------------------------------|
| ✔ APPEND      |                                                                                                  |
| ✔ AUTH        |                                                                                                  |
| ✔ BGSAVE      |                                                                                                  |
| ✔ CONFIG GET  | Most of the parameters are the Redis default values as are not supported directly by cachegrand. |
| ✔ COPY        | Missing DB parameter                                                                             |
| ✔ DBSIZE      |                                                                                                  |
| ✔ DECR        |                                                                                                  |
| ✔ DECRBY      |                                                                                                  |
| ✔ DEL         |                                                                                                  |
| ✔ EXISTS      |                                                                                                  |
| ✔ EXPIRE      |                                                                                                  |
| ✔ EXPIREAT    |                                                                                                  |
| ✔ EXPIRETIME  |                                                                                                  |
| ✔ FLUSHDB     | Missing ASYNC parameter                                                                          |
| ✔ GET         |                                                                                                  |
| ✔ GETDEL      |                                                                                                  |
| ✔ GETEX       |                                                                                                  |
| ✔ GETRANGE    |                                                                                                  |
| ✔ GETSET      |                                                                                                  |
| ✔ HELLO       |                                                                                                  |
| ✔ INCR        |                                                                                                  |
| ✔ INCRBY      |                                                                                                  |
| ✔ INCRBYFLOAT |                                                                                                  |
| ✔ KEYS        |                                                                                                  |
| ✔ LCS         | Missing IDX, MINMATCHLEN and WITHMATCHLEN parameters                                             |
| ✔ MGET        |                                                                                                  |
| ✔ MSET        |                                                                                                  |
| ✔ MSETNX      |                                                                                                  |
| ✔ PERSIST     |                                                                                                  |
| ✔ PEXPIRE     |                                                                                                  |
| ✔ PEXPIREAT   |                                                                                                  |
| ✔ PEXPIRETIME |                                                                                                  |
| ✔ PING        |                                                                                                  |
| ✔ PSETEX      |                                                                                                  |
| ✔ PTTL        |                                                                                                  |
| ✔ QUIT        |                                                                                                  |
| ✔ RANDOMKEY   |                                                                                                  |
| ✔ RENAME      |                                                                                                  |
| ✔ RENAMENX    |                                                                                                  |
| ✔ SAVE        |                                                                                                  |
| ✔ SCAN        | Missing TYPE parameter                                                                           |
| ✔ SET         |                                                                                                  |
| ✔ SETEX       |                                                                                                  |
| ✔ SETNX       |                                                                                                  |
| ✔ SETRANGE    |                                                                                                  |
| ✔ SHUTDOWN    | Missing the NOW and FORCE parameters                                                             |
| ✔ STRLEN      |                                                                                                  |
| ✔ SUBSTR      |                                                                                                  |
| ✔ TOUCH       |                                                                                                  |
| ✔ TTL         |                                                                                                  |
| ✔ UNLINK      |                                                                                                  |
