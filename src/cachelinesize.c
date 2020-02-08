// Author: Nick Strupat
// Date: October 29, 2010
// Returns the cache line size (in bytes) of the processor, or 0 on failure

#include <stddef.h>

#include "cachelinesize.h"

static size_t _cacheline_size = 0;

#if defined(__APPLE__)

#include <sys/sysctl.h>
size_t cacheline_size() {
    if (_cacheline_size == 0) {
        size_t line_size = 0;
        size_t line_size_size = sizeof(line_size);
        sysctlbyname("hw.cachelinesize", &line_size, &line_size_size, 0, 0);
    	_cacheline_size = line_size;
    }

    return _cacheline_size;
}

#elif defined(_WIN32)

#include <stdlib.h>
#include <windows.h>
size_t cacheline_size() {
    if (_cacheline_size == 0) {
        size_t line_size = 0;
        DWORD buffer_size = 0;
        DWORD i = 0;
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION * buffer = 0;

        GetLogicalProcessorInformation(0, &buffer_size);
        buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *) malloc(buffer_size);
        GetLogicalProcessorInformation(&buffer[0], &buffer_size);

        for (i = 0; i != buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
            if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1) {
                line_size = buffer[i].Cache.LineSize;
                break;
            }
        }

        free(buffer);

        _cacheline_size = line_size;
	}

    return _cacheline_size;
}

#elif defined(__linux__)

// #include <stdio.h>
#include <unistd.h>
size_t cacheline_size() {
//	unsigned int line_size = 0;
//	FILE * p = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
//	if (p) {
//		fscanf(p, "%d", &line_size);
//		fclose(p);
//	}
//	return line_size;
    if (_cacheline_size == 0) {
        _cacheline_size = sysconf (_SC_LEVEL1_DCACHE_LINESIZE);
    }

    return _cacheline_size;
}

#else
#error Unrecognized platform
#endif