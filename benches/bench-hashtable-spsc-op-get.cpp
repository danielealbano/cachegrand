/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstring>
#include <benchmark/benchmark.h>

#include "benchmark-program.hpp"

#include "data_structures/hashtable/spsc/hashtable_spsc.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wpointer-arith"

char *global_keys[] = {
        "SET", "ACL", "ACL CAT", "ACL DELUSER", "ACL DRYRUN", "ACL GENPASS", "ACL GETUSER", "ACL HELP", "ACL LIST", "ACL LOAD",
        "ACL LOG", "ACL SAVE", "ACL SETUSER", "ACL USERS", "ACL WHOAMI", "APPEND", "ASKING", "AUTH", "BGREWRITEAOF",
        "BGSAVE", "BITCOUNT", "BITFIELD", "BITFIELD_RO", "BITOP", "BITPOS", "BLMOVE", "BLMPOP", "BLPOP", "BRPOP",
        "BRPOPLPUSH", "BZMPOP", "BZPOPMAX", "BZPOPMIN", "CLIENT", "CLIENT CACHING", "CLIENT GETNAME", "CLIENT GETREDIR",
        "CLIENT HELP", "CLIENT ID", "CLIENT INFO", "CLIENT KILL", "CLIENT LIST", "CLIENT NO-EVICT", "CLIENT PAUSE",
        "CLIENT REPLY", "CLIENT SETNAME", "CLIENT TRACKING", "CLIENT TRACKINGINFO", "CLIENT UNBLOCK", "CLIENT UNPAUSE",
        "CLUSTER", "CLUSTER ADDSLOTS", "CLUSTER ADDSLOTSRANGE", "CLUSTER BUMPEPOCH", "CLUSTER COUNT-FAILURE-REPORTS",
        "CLUSTER COUNTKEYSINSLOT", "CLUSTER DELSLOTS", "CLUSTER DELSLOTSRANGE", "CLUSTER FAILOVER", "CLUSTER FLUSHSLOTS",
        "CLUSTER FORGET", "CLUSTER GETKEYSINSLOT", "CLUSTER HELP", "CLUSTER INFO", "CLUSTER KEYSLOT", "CLUSTER LINKS",
        "CLUSTER MEET", "CLUSTER MYID", "CLUSTER NODES", "CLUSTER REPLICAS", "CLUSTER REPLICATE", "CLUSTER RESET",
        "CLUSTER SAVECONFIG", "CLUSTER SET-CONFIG-EPOCH", "CLUSTER SETSLOT", "CLUSTER SHARDS", "CLUSTER SLAVES",
        "CLUSTER SLOTS", "COMMAND", "COMMAND COUNT", "COMMAND DOCS", "COMMAND GETKEYS", "COMMAND GETKEYSANDFLAGS",
        "COMMAND HELP", "COMMAND INFO", "COMMAND LIST", "CONFIG", "CONFIG GET", "CONFIG HELP", "CONFIG RESETSTAT",
        "CONFIG REWRITE", "CONFIG SET", "COPY", "DBSIZE", "DEBUG", "DECR", "DECRBY", "DEL", "DISCARD", "DUMP", "ECHO",
        "EVAL", "EVALSHA", "EVALSHA_RO", "EVAL_RO", "EXEC", "EXISTS", "EXPIRE", "EXPIREAT", "EXPIRETIME", "FAILOVER",
        "FCALL", "FCALL_RO", "FLUSHALL", "FLUSHDB", "FUNCTION", "FUNCTION DELETE", "FUNCTION DUMP", "FUNCTION FLUSH",
        "FUNCTION HELP", "FUNCTION KILL", "FUNCTION LIST", "FUNCTION LOAD", "FUNCTION RESTORE", "FUNCTION STATS", "GEOADD",
        "GEODIST", "GEOHASH", "GEOPOS", "GEORADIUS", "GEORADIUSBYMEMBER", "GEORADIUSBYMEMBER_RO", "GEORADIUS_RO",
        "GEOSEARCH", "GEOSEARCHSTORE", "GET", "GETBIT", "GETDEL", "GETEX", "GETRANGE", "GETSET", "HDEL", "HELLO",
        "HEXISTS", "HGET", "HGETALL", "HINCRBY", "HINCRBYFLOAT", "HKEYS", "HLEN", "HMGET", "HMSET", "HRANDFIELD",
        "HSCAN", "HSET", "HSETNX", "HSTRLEN", "HVALS", "INCR", "INCRBY", "INCRBYFLOAT", "INFO", "KEYS", "LASTSAVE",
        "LATENCY", "LATENCY DOCTOR", "LATENCY GRAPH", "LATENCY HELP", "LATENCY HISTOGRAM", "LATENCY HISTORY",
        "LATENCY LATEST", "LATENCY RESET", "LCS", "LINDEX", "LINSERT", "LLEN", "LMOVE", "LMPOP", "LOLWUT", "LPOP",
        "LPOS", "LPUSH", "LPUSHX", "LRANGE", "LREM", "LSET", "LTRIM", "MEMORY", "MEMORY DOCTOR", "MEMORY HELP",
        "MEMORY MALLOC-STATS", "MEMORY PURGE", "MEMORY STATS", "MEMORY USAGE", "MGET", "MIGRATE", "MODULE", "MODULE HELP",
        "MODULE LIST", "MODULE LOAD", "MODULE LOADEX", "MODULE UNLOAD", "MONITOR", "MOVE", "MSET", "MSETNX", "MULTI",
        "OBJECT", "OBJECT ENCODING", "OBJECT FREQ", "OBJECT HELP", "OBJECT IDLETIME", "OBJECT REFCOUNT", "PERSIST",
        "PEXPIRE", "PEXPIREAT", "PEXPIRETIME", "PFADD", "PFCOUNT", "PFDEBUG", "PFMERGE", "PFSELFTEST", "PING", "PSETEX",
        "PSUBSCRIBE", "PSYNC", "PTTL", "PUBLISH", "PUBSUB", "PUBSUB CHANNELS", "PUBSUB HELP", "PUBSUB NUMPAT", "PUBSUB NUMSUB",
        "PUBSUB SHARDCHANNELS", "PUBSUB SHARDNUMSUB", "PUNSUBSCRIBE", "QUIT", "RANDOMKEY", "READONLY", "READWRITE", "RENAME",
        "RENAMENX", "REPLCONF", "REPLICAOF", "RESET", "RESTORE", "RESTORE-ASKING", "ROLE", "RPOP", "RPOPLPUSH", "RPUSH",
        "RPUSHX", "SADD", "SAVE", "SCAN", "SCARD", "SCRIPT", "SCRIPT DEBUG", "SCRIPT EXISTS", "SCRIPT FLUSH", "SCRIPT HELP",
        "SCRIPT KILL", "SCRIPT LOAD", "SDIFF", "SDIFFSTORE", "SELECT", "SETBIT", "SETEX", "SETNX", "SETRANGE", "SHUTDOWN",
        "SINTER", "SINTERCARD", "SINTERSTORE", "SISMEMBER", "SLAVEOF", "SLOWLOG", "SLOWLOG GET", "SLOWLOG HELP", "SLOWLOG LEN",
        "SLOWLOG RESET", "SMEMBERS", "SMISMEMBER", "SMOVE", "SORT", "SORT_RO", "SPOP", "SPUBLISH", "SRANDMEMBER", "SREM",
        "SSCAN", "SSUBSCRIBE", "STRLEN", "SUBSCRIBE", "SUBSTR", "SUNION", "SUNIONSTORE", "SUNSUBSCRIBE", "SWAPDB", "SYNC",
        "TIME", "TOUCH", "TTL", "TYPE", "UNLINK", "UNSUBSCRIBE", "UNWATCH", "WAIT", "WATCH", "XACK", "XADD", "XAUTOCLAIM",
        "XCLAIM", "XDEL", "XGROUP", "XGROUP CREATE", "XGROUP CREATECONSUMER", "XGROUP DELCONSUMER", "XGROUP DESTROY",
        "XGROUP HELP", "XGROUP SETID", "XINFO", "XINFO CONSUMERS", "XINFO GROUPS", "XINFO HELP", "XINFO STREAM", "XLEN",
        "XPENDING", "XRANGE", "XREAD", "XREADGROUP", "XREVRANGE", "XSETID", "XTRIM", "ZADD", "ZCARD", "ZCOUNT", "ZDIFF",
        "ZDIFFSTORE", "ZINCRBY", "ZINTER", "ZINTERCARD", "ZINTERSTORE", "ZLEXCOUNT", "ZMPOP", "ZMSCORE", "ZPOPMAX", "ZPOPMIN",
        "ZRANDMEMBER", "ZRANGE", "ZRANGEBYLEX", "ZRANGEBYSCORE", "ZRANGESTORE", "ZRANK", "ZREM", "ZREMRANGEBYLEX",
        "ZREMRANGEBYRANK", "ZREMRANGEBYSCORE", "ZREVRANGE", "ZREVRANGEBYLEX", "ZREVRANGEBYSCORE", "ZREVRANK", "ZSCAN",
        "ZSCORE", "ZUNION", "ZUNIONSTORE",
};
int global_keys_count = ARRAY_SIZE(global_keys);


class HashtableSpscOpGetFixture : public benchmark::Fixture {
private:
    hashtable_spsc_t *_hashtable = nullptr;
    char **_keys = (char**)global_keys;
    int _keys_count = global_keys_count;
public:
    hashtable_spsc_t *GetHashtable() {
        return this->_hashtable;
    }

    void SetUp(benchmark::State& state) override {
        this->_hashtable = hashtable_spsc_new(
                HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                false,
                this->_keys_count);

        for (int index = 0; index < this->_keys_count; index++) {
            if (!hashtable_spsc_op_set(this->_hashtable, _keys[index], strlen(_keys[index]), _keys[index])) {
                char message[250] = { 0 };
                snprintf(message, sizeof(message) - 1, "Unable to insert the token '%s'", _keys[index]);
                state.SkipWithError(message);
                return;
            }
        }
    }

    void TearDown(const ::benchmark::State &state) override {
        hashtable_spsc_free(this->_hashtable);
    }
};

BENCHMARK_DEFINE_F(HashtableSpscOpGetFixture, FindTokenInHashtableWorstCase1Benchmark)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get(
                this->GetHashtable(),
                "non-existing",
                strlen("non-existing")));
    }
}

BENCHMARK_DEFINE_F(HashtableSpscOpGetFixture, FindTokenInHashtableWorstCase2Benchmark)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get(
                this->GetHashtable(),
                "non-existing",
                strlen("non-existing")));
    }
}

BENCHMARK_DEFINE_F(HashtableSpscOpGetFixture, FindTokenInHashtableAvgCase1Benchmark)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get(
                this->GetHashtable(),
                "set",
                strlen("set")));
    }
}

BENCHMARK_DEFINE_F(HashtableSpscOpGetFixture, FindTokenInHashtableAvgCase2Benchmark)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get(
                this->GetHashtable(),
                "append",
                strlen("append")));
    }
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    b->Iterations(100000)->UseRealTime();
}

BENCHMARK_REGISTER_F(HashtableSpscOpGetFixture, FindTokenInHashtableWorstCase1Benchmark)
    ->Apply(BenchArguments);
BENCHMARK_REGISTER_F(HashtableSpscOpGetFixture, FindTokenInHashtableWorstCase2Benchmark)
    ->Apply(BenchArguments);
BENCHMARK_REGISTER_F(HashtableSpscOpGetFixture, FindTokenInHashtableAvgCase1Benchmark)
    ->Apply(BenchArguments);
BENCHMARK_REGISTER_F(HashtableSpscOpGetFixture, FindTokenInHashtableAvgCase2Benchmark)
    ->Apply(BenchArguments);
