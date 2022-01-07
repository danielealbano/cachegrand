#include <fcntl.h>
#include <unistd.h>
#include <catch2/catch.hpp>

#include "support/simple_file_io.h"
#include "pidfile.h"

extern bool pidfile_owned;
extern int pidfile_fd;

bool test_pidfile_get_fnctl_lock(
        const char* path,
        struct flock *lock) {
    int pipe_fd[2];
    pid_t pid;

    // The locks set via fnctl can't be read by the same process as F_GETLK returns if the current process can apply
    // the lock requested in the struct passed as data and not "get" an applied lock.
    // This, though, doesn't apply to forked processes as forks don't inherit locks applied via F_SETLK/F_SETLKW.
    // Therefore, the function takes care of spawning up a pipe, forking the current process, with the child trying to
    // read the lock applied and writing it to the pipe and with the parent (the test itself) waiting for the data on
    // the pipe to copy it into the lock variable passed to the func.

    if (pipe(pipe_fd) == -1) {
        return false;
    }

    if ((pid = fork()) < 0) {
        return false;
    }

    if (pid == 0) {
        // Child process
        int fd;
        struct flock child_lock = { 0 };

        if ((fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) > -1) {
            if (fcntl(fd, F_GETLK, &child_lock) > -1) {
                write(pipe_fd[1], &child_lock, sizeof(struct flock));
                fsync(pipe_fd[1]);
            }
        }

        close(fd);
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        exit(0);
    } else {
        // Parent process
        size_t read_len = read(pipe_fd[0], lock, sizeof(struct flock));

        close(pipe_fd[1]);
        close(pipe_fd[0]);

        if (read_len != sizeof(struct flock)) {
            return false;
        }

        return true;
    }
}

TEST_CASE("pidfile.c", "[pidfile]") {
    char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
    int fixture_temp_path_suffix_len = 4;
    close(mkstemps(fixture_temp_path, fixture_temp_path_suffix_len));
    int fd = -1;

    SECTION("pidfile_open") {
        SECTION("Existing path") {
            fd = pidfile_open(fixture_temp_path);
            REQUIRE(fd > 0);
        }

        SECTION("Non-existing path") {
            fd = pidfile_open("/non/existing/path/leading/nowhere");
            REQUIRE(fd == -1);
        }
    }

    SECTION("pidfile_request_close_on_exec") {
        SECTION("valid fd") {
            fd = pidfile_open(fixture_temp_path);
            REQUIRE(fd > 0);
            REQUIRE(pidfile_request_close_on_exec(fd));

            int flags = fcntl(fd, F_GETFD);
            REQUIRE(flags != -1);
            REQUIRE((flags & FD_CLOEXEC) == FD_CLOEXEC);
        }

        SECTION("invalid fd") {
            REQUIRE(!pidfile_request_close_on_exec(-1));
        }
    }

    SECTION("pidfile_request_lock") {
        SECTION("valid fd") {
            struct flock lock = { 0 };

            fd = pidfile_open(fixture_temp_path);
            REQUIRE(fd > 0);

            REQUIRE(pidfile_request_lock(fd));

            REQUIRE(test_pidfile_get_fnctl_lock(fixture_temp_path, &lock));
            REQUIRE(lock.l_start == 0);
            REQUIRE(lock.l_len == 0);
            REQUIRE(lock.l_pid == getpid());
            REQUIRE(lock.l_type == F_WRLCK);
        }

        SECTION("invalid fd") {
            REQUIRE(!pidfile_request_lock(-1));
        }
    }

    SECTION("pidfile_write_pid") {
        SECTION("valid fd") {
            fd = pidfile_open(fixture_temp_path);
            REQUIRE(fd > 0);

            REQUIRE(pidfile_write_pid(fd, 12345));
            REQUIRE(simple_file_io_read_uint32_return(fixture_temp_path) == 12345);
        }

        SECTION("invalid fd") {
            REQUIRE(!pidfile_write_pid(-1, 12345));
        }
    }

    SECTION("pidfile_close") {
        SECTION("valid fd") {
            fd = pidfile_open(fixture_temp_path);
            REQUIRE(fd > 0);

            REQUIRE(pidfile_close(fd));
            REQUIRE(fcntl(fd, F_GETFD) == -1);
            REQUIRE(access(fixture_temp_path, F_OK) != 0);

            // Set fd to -1 to avoid the testing code trying to close it
            fd = -1;
        }

        SECTION("invalid fd") {
            REQUIRE(!pidfile_close(-1));
        }
    }

    SECTION("pidfile_create") {
        struct flock lock = { 0 };

        REQUIRE(pidfile_create(fixture_temp_path));
        REQUIRE(pidfile_fd > -1);
        REQUIRE(pidfile_owned == true);

        REQUIRE(test_pidfile_get_fnctl_lock(fixture_temp_path, &lock));
        REQUIRE(lock.l_start == 0);
        REQUIRE(lock.l_len == 0);
        REQUIRE(lock.l_pid == getpid());
        REQUIRE(lock.l_type == F_WRLCK);

        int flags = fcntl(pidfile_fd, F_GETFD);
        REQUIRE(flags != -1);
        REQUIRE((flags & FD_CLOEXEC) == FD_CLOEXEC);

        REQUIRE(simple_file_io_read_uint32_return(fixture_temp_path) == (long)getpid());
    }

    SECTION("pidfile_is_owned") {
        pidfile_owned = false;
        REQUIRE(pidfile_is_owned() == false);

        pidfile_owned = true;
        REQUIRE(pidfile_is_owned() == true);
    }

    SECTION("pidfile_get_fd") {
        pidfile_fd = -1;
        REQUIRE(pidfile_get_fd() == -1);

        pidfile_fd = 99;
        REQUIRE(pidfile_get_fd() == 99);
    }

    if (fd > 0) {
        close(fd);
    }

    pidfile_fd = -1;
    pidfile_owned = false;
    unlink(fixture_temp_path);
}
