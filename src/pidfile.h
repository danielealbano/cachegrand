#ifndef CACHEGRAND_PIDFILE_H
#define CACHEGRAND_PIDFILE_H

#ifdef __cplusplus
extern "C" {
#endif

bool pidfile_request_close_on_exec(
        int fd);

bool pidfile_request_lock(
        int fd);

int pidfile_open(
        const char* pidfile_path);

bool pidfile_write_pid(
        int fd,
        long pid);

bool pidfile_create(
        const char* pidfile_path);

void pidfile_close(
        int fd);

bool pidfile_is_owned();

int pidfile_get_fd();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PIDFILE_H
