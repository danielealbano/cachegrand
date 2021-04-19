#if DEBUG == 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "misc.h"
#include "log.h"
#include "fatal.h"
#include "log/sink/log_sink_support.h"
#include "log/sink/log_sink_console.h"

#include "log_debug.h"

extern thread_local char* log_early_prefix_thread;

bool log_message_debug_check_rules(const char* str, const char* rules_include[], const char* rules_exclude[]) {
    bool has_includes = rules_include[0] != NULL;

    const char** rulesset[] = {
            rules_include,
            rules_exclude
    };

    for(uint32_t rulesset_index = 0; rulesset_index <= 1; rulesset_index++) {
        const char** rules = rulesset[rulesset_index];

        // Very wrong but the memory is not being accessed, it's used only to reduce code duplication & checks
        rules--;
        while(*(++rules) != NULL) {
            if (strcmp(*rules, str) == 0) {
                return rulesset_index == 0 ? true : false;
            }
        }

        if (rulesset_index == 0 && has_includes) {
            return false;
        }
    }

    return true;
}

void log_message_debug(const char* src_path, const char* src_func, const int src_line, const char* message, ...) {
    static log_sink_settings_t log_sink_console_settings = {
            .console = {
                    .use_stdout_for_errors = true
            }
    };

    char tag[256] = { 0 };
    size_t tag_len = sizeof(tag);

    LOG_MESSAGE_DEBUG_RULES(rules_src_path_include, SRC_PATH, INCLUDE);
    LOG_MESSAGE_DEBUG_RULES(rules_src_path_exclude, SRC_PATH, EXCLUDE);
    LOG_MESSAGE_DEBUG_RULES(rules_src_func_include, SRC_FUNC, INCLUDE);
    LOG_MESSAGE_DEBUG_RULES(rules_src_func_exclude, SRC_FUNC, EXCLUDE);

    if (log_message_debug_check_rules(src_path, rules_src_path_include, rules_src_path_exclude) == false) {
        return;
    }

    if (log_message_debug_check_rules(src_func, rules_src_func_include, rules_src_func_exclude) == false) {
        return;
    }

    snprintf(tag, tag_len, "%s][%s():%d", src_path, src_func, src_line);

    va_list args;
    va_start(args, message);

    log_sink_console_printer(
            &log_sink_console_settings,
            tag,
            log_message_timestamp(),
            LOG_LEVEL_DEBUG_INTERNALS,
            log_early_prefix_thread,
            message,
            args);

    va_end(args);
}

#endif // DEBUG == 1
