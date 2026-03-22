#include <stdatomic.h>
#include <string.h>
#include <unistd.h>

#define MAX_BACKENDS 32

int backend_fds[MAX_BACKENDS];
atomic_int backend_count = 0;
atomic_int rr = 0;

/* ========================================= */

void add_backend(int fd) {
    int n = atomic_load(&backend_count);

    if (n >= MAX_BACKENDS) return;

    backend_fds[n] = fd;
    atomic_store(&backend_count, n + 1);
}

/* ========================================= */

void remove_backend(int fd) {
    int n = atomic_load(&backend_count);

    for (int i = 0; i < n; i++) {
        if (backend_fds[i] == fd) {

            /* shift left */
            for (int j = i; j < n - 1; j++) {
                backend_fds[j] = backend_fds[j + 1];
            }

            atomic_store(&backend_count, n - 1);
            break;
        }
    }

    close(fd);
}
