#define _POSIX_C_SOURCE 200809L
/* Demo: 8 workers handle 60 booking requests for a 20-seat flight. Run: make pool_demo */
#include "fms.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void)
{
    enum { REQUESTS = 60, SEATS = 20, WORKERS = 8 };
    FmsSystem   sys;
    WorkerPool  pool;
    static Job  jobs[REQUESTS];
    const User *admin = NULL, *pass = NULL;
    FlightParams fp;
    int         i, ok = 0, full = 0, other = 0;
    time_t      dep = time(NULL) + 72 * 3600;
    struct timespec t0, t1;

    if (fms_init(&sys) != FMS_OK) return 1;
    (void)auth_create_first_admin(&sys, "admin", "Admin@123");
    (void)auth_login(&sys, "admin", "Admin@123", &admin);
    (void)auth_register(&sys, NULL, "pass1", "Pass@123", ROLE_PASSENGER);
    (void)auth_login(&sys, "pass1", "Pass@123", &pass);

    memset(&fp, 0, sizeof fp);
    snprintf(fp.id, sizeof fp.id, "AI101");
    snprintf(fp.name, sizeof fp.name, "Pool Air");
    snprintf(fp.source, sizeof fp.source, "MAA");
    snprintf(fp.destination, sizeof fp.destination, "DEL");
    fp.departure = dt_from_time(dep);
    fp.seats = SEATS; fp.fare = 1000.0;
    (void)flight_add(&sys, admin, &fp);

    if (pool_init(&pool, &sys, WORKERS) != FMS_OK) return 1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < REQUESTS; i++) {
        BookingForm *f = &jobs[i].form;
        memset(&jobs[i], 0, sizeof jobs[i]);
        jobs[i].type  = JOB_BOOK;
        jobs[i].actor = pass;
        snprintf(f->flight_id, sizeof f->flight_id, "AI101");
        snprintf(f->passenger.name, sizeof f->passenger.name, "Asha Rao");
        snprintf(f->passenger.address, sizeof f->passenger.address, "5 Main Street Chennai");
        snprintf(f->passenger.id_type, sizeof f->passenger.id_type, "PASSPORT");
        snprintf(f->passenger.id_number, sizeof f->passenger.id_number, "P1234567");
        f->journey = dt_from_time(dep);
        snprintf(f->boarding, sizeof f->boarding, "MAA");
        snprintf(f->destination, sizeof f->destination, "DEL");
        snprintf(f->purpose, sizeof f->purpose, "LEISURE");
        snprintf(f->payment, sizeof f->payment, "CARD");
        (void)pool_submit(&pool, &jobs[i]);
    }
    pool_wait(&pool);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (i = 0; i < REQUESTS; i++) {
        if (jobs[i].result == FMS_OK) ok++;
        else if (jobs[i].result == FMS_ERR_NO_SEATS) full++;
        else other++;
    }
    printf("%d requests, %d workers, %d seats\n", REQUESTS, WORKERS, SEATS);
    printf("booked: %d   told 'no seats': %d   other: %d\n", ok, full, other);
    printf("time  : %.4f s\n", (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9);
    pool_destroy(&pool);
    fms_shutdown(&sys);
    return (ok == SEATS && full == REQUESTS - SEATS && other == 0) ? 0 : 1;
}
