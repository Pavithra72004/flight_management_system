#define _POSIX_C_SOURCE 200809L
/* Benchmark: hash table lookup vs walking the linked list. Run with: make bench */
#include "fms.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);          /* monotonic clock for timing */
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void)
{
    enum { N = 5000, LOOKUPS = 20000 };
    FmsSystem   sys;
    const User *admin = NULL;
    char        id[FLIGHT_ID_MAX];
    int         i;
    int         found_hash = 0, found_list = 0;
    double      t0, t_hash, t_list;

    if (fms_init(&sys) != FMS_OK) return 1;
    (void)auth_create_first_admin(&sys, "admin", "Admin@123");
    (void)auth_login(&sys, "admin", "Admin@123", &admin);
    for (i = 0; i < N; i++) {
        FlightParams p;
        memset(&p, 0, sizeof p);
        snprintf(p.id, sizeof p.id, "%c%c%04d", 'A' + (i / 10000) % 26, 'A', i % 10000);
        snprintf(p.name, sizeof p.name, "Bench Air");
        snprintf(p.source, sizeof p.source, "MAA");
        snprintf(p.destination, sizeof p.destination, "DEL");
        p.departure = dt_from_time(time(NULL) + 86400);
        p.seats = 10; p.fare = 100.0;
        (void)flight_add(&sys, admin, &p);
    }

    t0 = now_sec();
    for (i = 0; i < LOOKUPS; i++) {
        snprintf(id, sizeof id, "AA%04d", (i * 7) % N);
        if (flight_lookup_locked(&sys, id) != NULL) found_hash++;
    }
    t_hash = now_sec() - t0;

    t0 = now_sec();
    for (i = 0; i < LOOKUPS; i++) {
        const Flight *f;
        snprintf(id, sizeof id, "AA%04d", (i * 7) % N);
        for (f = sys.flights; f != NULL; f = f->next) {
            if (strcmp(f->id, id) == 0) { found_list++; break; }
        }
    }
    t_list = now_sec() - t0;

    printf("%d flights, %d lookups\n", N, LOOKUPS);
    printf("hash table  : %8.4f s (found %d)\n", t_hash, found_hash);
    printf("linked list : %8.4f s (found %d)\n", t_list, found_list);
    if (t_hash > 0.0) printf("speed-up    : %.1fx\n", t_list / t_hash);
    fms_shutdown(&sys);
    return 0;
}
