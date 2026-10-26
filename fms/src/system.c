#include "fms.h"
#include <stdlib.h>
#include <string.h>

/* initialization function */
FmsStatus fms_init(FmsSystem *sys)
{
    if (sys == NULL) return FMS_ERR_NULL;
    memset(sys, 0, sizeof *sys);
    sys->next_user_id    = 1;
    sys->next_booking_id = 1000;
    sys->flight_index    = ht_create(64);
    sys->booking_index   = ht_create(256);
    if (sys->flight_index == NULL || sys->booking_index == NULL) {
        ht_destroy(sys->flight_index);
        ht_destroy(sys->booking_index);
        memset(sys, 0, sizeof *sys);
        return FMS_ERR_MEM;
    }
    if (pthread_mutex_init(&sys->lock, NULL) != 0) {
        ht_destroy(sys->flight_index);
        ht_destroy(sys->booking_index);
        memset(sys, 0, sizeof *sys);
        return FMS_ERR_MEM;
    }
    log_write("INFO", "system", "system initialised");
    return FMS_OK;
}

/* explicit cleanup function: frees EVERY allocation (Valgrind must report 0 leaks) */
void fms_shutdown(FmsSystem *sys)
{
    User    *u;
    Flight  *f;
    Booking *b;

    if (sys == NULL) return;

    u = sys->users;
    while (u != NULL) {
        User *next = u->next;
        free(u);
        u = next;
    }
    sys->users = NULL;

    f = sys->flights;
    while (f != NULL) {
        Flight *next = f->next;
        free(f->seat_map.seats);               /* the seat ARRAY */
        f->seat_map.seats = NULL;
        free(f);
        f = next;
    }
    sys->flights = NULL;

    b = sys->bookings;
    while (b != NULL) {
        Booking *next = b->next;
        free(b);
        b = next;
    }
    sys->bookings = NULL;

    ht_destroy(sys->flight_index);
    sys->flight_index = NULL;
    ht_destroy(sys->booking_index);
    sys->booking_index = NULL;

    (void)pthread_mutex_destroy(&sys->lock);
    log_write("INFO", "system", "system shut down");
}
