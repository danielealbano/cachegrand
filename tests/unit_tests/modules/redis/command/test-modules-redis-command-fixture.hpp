#include <hiredis.h>

#define PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(WORKER_CONTEXT, RUNNING) { \
    do { \
        sched_yield(); \
        usleep(1000); \
        MEMORY_FENCE_LOAD(); \
    } while((WORKER_CONTEXT)->running == !(RUNNING)); \
}

enum TestModulesRedisCommandFixtureProtocol {
    TEST_MODULES_REDIS_COMMAND_FIXTURE_PROTOCOL_UNKNOWN = 0,
    TEST_MODULES_REDIS_COMMAND_FIXTURE_PROTOCOL_INLINE = 1,
    TEST_MODULES_REDIS_COMMAND_FIXTURE_PROTOCOL_RESP_2 = 2,
    TEST_MODULES_REDIS_COMMAND_FIXTURE_PROTOCOL_RESP_3 = 3,
};

class TestModulesRedisCommandFixture {
public:
    TestModulesRedisCommandFixture();
    ~TestModulesRedisCommandFixture();
protected:
    redisContext *c;
    uint8_t protocol_version;

    size_t buffer_send_data_len{};
    char buffer_send[16 * 1024] = {0};
    char buffer_recv[16 * 1024] = {0};
    int recv_packet_size = 32 * 1024;

    config_module_network_binding_t config_module_network_binding{};
    config_module_redis_t config_module_redis{};
    config_module_network_timeout_t config_module_network_timeout{};
    config_module_network_t config_module_network{};
    config_module_t config_module{};
    config_network_t config_network{};
    config_database_limits_hard_t config_database_limits_hard{};
    config_database_limits_t config_database_limits{};
    config_database_memory_limits_hard_t config_database_memory_limits_hard{};
    config_database_memory_limits_t config_database_memory_limits{};
    config_database_memory_t config_database_memory{};
    config_database_keys_eviction_t config_database_keys_eviction{};
    config_database_snapshots_t config_database_snapshots{};
    config_database_t config_database{};
    config_t config{};

    worker_context_t *worker_context;
    uint32_t workers_count;

    storage_db_config_t *db_config;
    storage_db_t *db;

    program_context_t *program_context;

    static size_t string_replace(
            char *input,
            size_t input_len,
            char *output,
            size_t output_len,
            int count,
            ...);

    size_t build_resp_command(
            char *buffer,
            size_t buffer_size,
            const std::vector<std::string>& arguments) const;

    bool send_recv_resp_command_multi_recv(
            const std::vector<std::string>& arguments,
            char *buffer_recv_int,
            size_t *out_buffer_recv_length) const;

    bool send_recv_resp_command_multi_recv_and_validate_recv(
            const std::vector<std::string>& arguments,
            char *expected,
            size_t expected_length);

    bool send_recv_resp_command_and_validate_recv(
            const std::vector<std::string>& arguments,
            char *expected,
            size_t expected_length);

    bool send_recv_resp_command_text_and_validate_recv(
            const std::vector<std::string>& arguments,
            char *expected);
};
