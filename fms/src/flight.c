#define _POSIX_C_SOURCE 200809L
#include "fms.h"
#include <stdlib.h>
#include <string.h>

Flight *flight_lookup_locked(const FmsSystem *sys, const char *id)
{
    return (Flight *)ht_get(sys->flight_index, id);     /* hash table: O(1) */
}

/* ---------- seat array helpers ---------- */
int flight_take_seat(Flight *f, unsigned booking_id)
{
    int i;
    if (f == NULL || f->seat_map.seats == NULL || f->seat_map.available <= 0) return 0;
    for (i = 0; i < f->seat_map.total; i++) {
        if (f->seat_map.seats[i] == 0U) {
            f->seat_map.seats[i] = booking_id;
            f->seat_map.available--;
            return i + 1;                      /* seat numbers start at 1 */
        }
    }
    return 0;
}

void flight_release_seat(Flight *f, int seat_no)
{
    if (f == NULL || f->seat_map.seats == NULL) return;
    if (seat_no < 1 || seat_no > f->seat_map.total) return;
    if (f->seat_map.seats[seat_no - 1] != 0U) {
        f->seat_map.seats[seat_no - 1] = 0U;
        f->seat_map.available++;
    }
}

/* ---------- sorted view of the linked list (uses a temporary array) ---------- */
static int cmp_departure(const void *a, const void *b)
{
    const Flight *fa = *(Flight *const *)a;
    const Flight *fb = *(Flight *const *)b;
    time_t ta = dt_to_time(&fa->departure);
    time_t tb = dt_to_time(&fb->departure);
    return (ta > tb) - (ta < tb);
}

Flight **flight_collect_sorted_locked(FmsSystem *sys, size_t *n_out)
{
    size_t   n = 0;
    size_t   i = 0;
    Flight  *f;
    Flight **arr;

    for (f = sys->flights; f != NULL; f = f->next) n++;
    arr = malloc((n + 1) * sizeof *arr);
    if (arr == NULL) {
        *n_out = 0;
        return NULL;
    }
    for (f = sys->flights; f != NULL; f = f->next) arr[i++] = f;
    qsort(arr, n, sizeof *arr, cmp_departure);
    *n_out = n;
    return arr;
}

/* ---------- printing ---------- */
void flight_print_header(FILE *out)
{
    fprintf(out, "%-6s %-20s %-9s %-16s %-13s %10s %9s\n", "FLIGHT", "NAME", "ROUTE",
            "DEPARTURE", "STATUS", "FARE(Rs)", "SEATS");
    fprintf(out, "------------------------------------------------------------------------------------\n");
}

void flight_print_row(FILE *out, const Flight *f)
{
    char when[24];
    char status[24];
    char route[12];
    char seats[16];

    dt_format(&f->departure, when, sizeof when);
    if (f->status == FL_DELAYED) {
        snprintf(status, sizeof status, "DELAYED+%dm", f->delay_min);
    } else {
        snprintf(status, sizeof status, "%s", flight_status_name(f->status));
    }
    snprintf(route, sizeof route, "%s->%s", f->source, f->destination);
    snprintf(seats, sizeof seats, "%d/%d", f->seat_map.available, f->seat_map.total);
    fprintf(out, "%-6s %-20s %-9s %-16s %-13s %10.2f %9s\n", f->id, f->name, route, when,
            status, f->base_fare, seats);
}

FmsStatus flight_print_schedule(FmsSystem *sys, FILE *out, const char *dest_filter)
{
    Flight **arr;
    size_t   n = 0;
    size_t   i;
    size_t   shown = 0;

    if (sys == NULL || out == NULL) return FMS_ERR_NULL;
    if (dest_filter != NULL && dest_filter[0] != '\0' && !valid_airport(dest_filter)) {
        return FMS_ERR_INVALID;
    }

    pthread_mutex_lock(&sys->lock);
    arr = flight_collect_sorted_locked(sys, &n);
    if (arr == NULL) {
        pthread_mutex_unlock(&sys->lock);
        return FMS_ERR_MEM;
    }
    flight_print_header(out);
    for (i = 0; i < n; i++) {
        if (dest_filter != NULL && dest_filter[0] != '\0' &&
            strcmp(arr[i]->destination, dest_filter) != 0) {
            continue;
        }
        flight_print_row(out, arr[i]);
        shown++;
    }
    pthread_mutex_unlock(&sys->lock);

    if (shown == 0) fprintf(out, "(no flights found)\n");
    free(arr);
    arr = NULL;
    return FMS_OK;
}

FmsStatus flight_seat_info(FmsSystem *sys, const char *flight_id, int *available, int *total)
{
    Flight *f;

    if (sys == NULL || flight_id == NULL || available == NULL || total == NULL) return FMS_ERR_NULL;
    pthread_mutex_lock(&sys->lock);
    f = flight_lookup_locked(sys, flight_id);
    if (f != NULL) {
        *available = f->seat_map.available;
        *total     = f->seat_map.total;
    }
    pthread_mutex_unlock(&sys->lock);
    return (f != NULL) ? FMS_OK : FMS_ERR_NOT_FOUND;
}

/* ---------- admin: add a flight ---------- */
FmsStatus flight_add(FmsSystem *sys, const User *actor, const FlightParams *p)
{
    Flight   *f;
    FmsStatus st;

    if (sys == NULL || p == NULL) return FMS_ERR_NULL;
    if (!auth_can(actor, ACT_MANAGE_FLIGHTS)) return FMS_ERR_PERM;
    if (!valid_flight_id(p->id) || !valid_flight_name(p->name) ||
        !valid_airport(p->source) || !valid_airport(p->destination) ||
        strcmp(p->source, p->destination) == 0 || !valid_datetime(&p->departure) ||
        p->seats < 1 || p->seats > MAX_SEATS || p->fare <= 0.0 || p->fare > 1000000.0) {
        return FMS_ERR_INVALID;
    }

    pthread_mutex_lock(&sys->lock);
    if (flight_lookup_locked(sys, p->id) != NULL) {
        pthread_mutex_unlock(&sys->lock);
        return FMS_ERR_EXISTS;
    }
    f = calloc(1, sizeof *f);
    if (f == NULL) {
        pthread_mutex_unlock(&sys->lock);
        return FMS_ERR_MEM;
    }
    f->seat_map.seats = calloc((size_t)p->seats, sizeof *f->seat_map.seats);
    if (f->seat_map.seats == NULL) {
        free(f);
        pthread_mutex_unlock(&sys->lock);
        return FMS_ERR_MEM;
    }
    snprintf(f->id, sizeof f->id, "%s", p->id);
    snprintf(f->name, sizeof f->name, "%s", p->name);
    snprintf(f->source, sizeof f->source, "%s", p->source);
    snprintf(f->destination, sizeof f->destination, "%s", p->destination);
    f->departure           = p->departure;
    f->status              = FL_SCHEDULED;
    f->base_fare           = p->fare;
    f->seat_map.total      = p->seats;
    f->seat_map.available  = p->seats;

    st = ht_put(sys->flight_index, f->id, f);
    if (st != FMS_OK) {
        free(f->seat_map.seats);
        free(f);
        pthread_mutex_unlock(&sys->lock);
        return st;
    }
    f->next      = sys->flights;
    sys->flights = f;
    pthread_mutex_unlock(&sys->lock);

    log_write("INFO", "flight", "flight %s added (%s->%s, %d seats)", p->id, p->source,
              p->destination, p->seats);
    return FMS_OK;
}

/* ---------- admin: change name / time, delay or cancel ---------- */
FmsStatus flight_update(FmsSystem *sys, const User *actor, const char *flight_id,
                        const FlightUpdate *u)
{
    Flight   *f;
    FmsStatus st = FMS_OK;

    if (sys == NULL || flight_id == NULL || u == NULL) return FMS_ERR_NULL;
    if (!auth_can(actor, ACT_MANAGE_FLIGHTS)) return FMS_ERR_PERM;
    if (!valid_flight_id(flight_id)) return FMS_ERR_INVALID;

    pthread_mutex_lock(&sys->lock);
    f = flight_lookup_locked(sys, flight_id);
    if (f == NULL) {
        st = FMS_ERR_NOT_FOUND;
    } else if (f->status == FL_CANCELLED) {
        st = FMS_ERR_STATE;                    /* a cancelled flight cannot change */
    } else {
        switch (u->kind) {
        case UPD_NAME:
            if (valid_flight_name(u->name)) {
                snprintf(f->name, sizeof f->name, "%s", u->name);
            } else {
                st = FMS_ERR_INVALID;
            }
            break;
        case UPD_DEPARTURE:
            if (valid_datetime(&u->departure) && dt_to_time(&u->departure) > time(NULL)) {
                f->departure = u->departure;
                f->delay_min = 0;
                f->status    = FL_SCHEDULED;
            } else {
                st = FMS_ERR_INVALID;
            }
            break;
        case UPD_DELAY:
            if (u->delay_min >= 1 && u->delay_min <= 1440) {
                f->delay_min = u->delay_min;
                f->status    = FL_DELAYED;
            } else {
                st = FMS_ERR_INVALID;
            }
            break;
        case UPD_CANCEL:
            f->status = FL_CANCELLED;
            booking_cancel_for_flight_locked(sys, f);   /* refund everyone */
            break;
        default:
            st = FMS_ERR_INVALID;
            break;
        }
    }
    pthread_mutex_unlock(&sys->lock);

    if (st == FMS_OK) {
        log_write("INFO", "flight", "flight %s updated (kind=%d)", flight_id, (int)u->kind);
    }
    return st;
}
