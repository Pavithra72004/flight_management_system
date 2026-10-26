#define _POSIX_C_SOURCE 200809L
#include "fms.h"
#include <stdlib.h>
#include <string.h>

/* Flight report: details + seat map (array) + passenger list (linked list) */
FmsStatus report_flight(FmsSystem *sys, const User *actor, const char *flight_id, FILE *out)
{
    const Flight  *f;
    const Booking *b;
    int            i;

    if (sys == NULL || flight_id == NULL || out == NULL) return FMS_ERR_NULL;
    if (!auth_can(actor, ACT_VIEW_REPORTS)) return FMS_ERR_PERM;
    if (!valid_flight_id(flight_id)) return FMS_ERR_INVALID;

    pthread_mutex_lock(&sys->lock);
    f = flight_lookup_locked(sys, flight_id);
    if (f == NULL) {
        pthread_mutex_unlock(&sys->lock);
        return FMS_ERR_NOT_FOUND;
    }
    fprintf(out, "\n=== FLIGHT REPORT: %s ===\n", f->id);
    flight_print_header(out);
    flight_print_row(out, f);

    fprintf(out, "\nSeat map (X = booked, . = free):\n");
    for (i = 1; i <= f->seat_map.total; i++) {
        fprintf(out, "%3d%c ", i, (f->seat_map.seats[i - 1] != 0U) ? 'X' : '.');
        if (i % 10 == 0 || i == f->seat_map.total) fputc('\n', out);
    }

    fprintf(out, "\nPassengers:\n");
    fprintf(out, "%-8s %-22s %-5s %s\n", "BOOKING", "NAME", "SEAT", "STATUS");
    for (b = sys->bookings; b != NULL; b = b->next) {
        if (strcmp(b->flight_id, f->id) == 0 &&
            (b->status == BK_PENDING || b->status == BK_CONFIRMED)) {
            fprintf(out, "%-8u %-22s %-5d %s\n", b->id, b->passenger.name, b->seat_no,
                    booking_status_name(b->status));
        }
    }
    pthread_mutex_unlock(&sys->lock);
    return FMS_OK;
}

/* Reservation chart: one line per flight with a bar showing how full it is */
FmsStatus report_reservation_chart(FmsSystem *sys, const User *actor, FILE *out)
{
    Flight **arr;
    size_t   n = 0;
    size_t   i;

    if (sys == NULL || out == NULL) return FMS_ERR_NULL;
    if (!auth_can(actor, ACT_VIEW_REPORTS)) return FMS_ERR_PERM;

    pthread_mutex_lock(&sys->lock);
    arr = flight_collect_sorted_locked(sys, &n);
    if (arr == NULL) {
        pthread_mutex_unlock(&sys->lock);
        return FMS_ERR_MEM;
    }
    fprintf(out, "\n=== RESERVATION CHART ===\n");
    fprintf(out, "%-6s %-9s %6s %6s %6s %6s %6s  %s\n", "FLIGHT", "ROUTE", "TOTAL", "CONF",
            "PEND", "BOOKED", "FREE", "OCCUPANCY");
    for (i = 0; i < n; i++) {
        const Flight  *f = arr[i];
        const Booking *b;
        int confirmed = 0;
        int pending   = 0;
        int booked    = f->seat_map.total - f->seat_map.available;
        int bar       = (f->seat_map.total > 0) ? (booked * 20) / f->seat_map.total : 0;
        int k;
        char route[12];

        for (b = sys->bookings; b != NULL; b = b->next) {
            if (strcmp(b->flight_id, f->id) != 0) continue;
            if (b->status == BK_CONFIRMED) confirmed++;
            if (b->status == BK_PENDING) pending++;
        }
        snprintf(route, sizeof route, "%s->%s", f->source, f->destination);
        fprintf(out, "%-6s %-9s %6d %6d %6d %6d %6d  [", f->id, route, f->seat_map.total,
                confirmed, pending, booked, f->seat_map.available);
        for (k = 0; k < 20; k++) fputc(k < bar ? '#' : '-', out);
        fprintf(out, "]\n");
    }
    pthread_mutex_unlock(&sys->lock);
    free(arr);
    arr = NULL;
    return FMS_OK;
}

/* Schedule report: days = 7 (weekly) or 30 (monthly) */
FmsStatus report_schedule(FmsSystem *sys, const User *actor, int days, FILE *out)
{
    Flight **arr;
    size_t   n = 0;
    size_t   i;
    size_t   shown = 0;
    time_t   now = time(NULL);
    time_t   end;

    if (sys == NULL || out == NULL) return FMS_ERR_NULL;
    if (!auth_can(actor, ACT_VIEW_REPORTS)) return FMS_ERR_PERM;
    if (days < 1 || days > 366) return FMS_ERR_INVALID;
    end = now + (time_t)days * 24 * 3600;

    pthread_mutex_lock(&sys->lock);
    arr = flight_collect_sorted_locked(sys, &n);
    if (arr == NULL) {
        pthread_mutex_unlock(&sys->lock);
        return FMS_ERR_MEM;
    }
    fprintf(out, "\n=== %s FLIGHT SCHEDULE (next %d days) ===\n",
            (days == 7) ? "WEEKLY" : (days == 30) ? "MONTHLY" : "CUSTOM", days);
    flight_print_header(out);
    for (i = 0; i < n; i++) {
        time_t dep = dt_to_time(&arr[i]->departure);
        if (dep >= now && dep < end) {
            flight_print_row(out, arr[i]);
            shown++;
        }
    }
    pthread_mutex_unlock(&sys->lock);
    if (shown == 0) fprintf(out, "(no flights in this period)\n");
    free(arr);
    arr = NULL;
    return FMS_OK;
}
