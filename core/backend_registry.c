#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>

#define MAX_BACKENDS 32

int backend_fds[MAX_BACKENDS];
atomic_int backend_count = 0;
atomic_int rr = 0;

static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;

/* ========================================= */

void add_backend(int fd) {
    pthread_mutex_lock(&registry_lock);

    int n = atomic_load(&backend_count);

    if (n < MAX_BACKENDS) {
        backend_fds[n] = fd;
        atomic_store(&backend_count, n + 1);
    }

    pthread_mutex_unlock(&registry_lock);
}

/* ========================================= */

void remove_backend(int fd) {
    pthread_mutex_lock(&registry_lock);

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

    pthread_mutex_unlock(&registry_lock);
    close(fd);
}
