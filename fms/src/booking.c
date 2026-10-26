#define _POSIX_C_SOURCE 200809L
#include "fms.h"
#include <stdlib.h>
#include <string.h>

/* ---------- money rules ---------- */
double fare_calculate(double base_fare)
{
    return round_money(base_fare * (1.0 + TAX_RATE));
}

/* Refund only when cancellation is at least 75 minutes before departure. */
double refund_calculate(double fare, time_t departure, time_t now)
{
    double minutes = difftime(departure, now) / 60.0;
    if (minutes < 75.0) return 0.0;
    return round_money(fare);
}

/* ---------- helpers (caller holds sys->lock) ---------- */
static Booking *find_booking_locked(const FmsSystem *sys, unsigned id)
{
    char key[KEY_MAX];
    snprintf(key, sizeof key, "%u", id);
    return (Booking *)ht_get(sys->booking_index, key);      /* hash table lookup */
}

/* Passengers see only their own bookings, staff see all. */
static bool can_view(const User *actor, const Booking *b)
{
    if (auth_can(actor, ACT_VIEW_ALL_BOOKINGS)) return true;
    return b->user_id == actor->id;
}

static bool is_active(const Booking *b)
{
    return b->status == BK_PENDING || b->status == BK_CONFIRMED;
}

/* ---------- reserve a seat (passenger) ---------- */
static FmsStatus request_locked(FmsSystem *sys, const User *actor, const BookingForm *f,
                                unsigned *out_id)
{
    Flight   *fl;
    Booking  *b;
    char      key[KEY_MAX];
    FmsStatus st;

    fl = flight_lookup_locked(sys, f->flight_id);
    if (fl == NULL) return FMS_ERR_NOT_FOUND;
    if (fl->status == FL_CANCELLED) return FMS_ERR_STATE;
    if (dt_to_time(&fl->departure) <= time(NULL)) return FMS_ERR_STATE;   /* already left */
    if (strcmp(f->boarding, fl->source) != 0 ||
        strcmp(f->destination, fl->destination) != 0 ||
        !dt_same_day(&f->journey, &fl->departure)) {
        return FMS_ERR_INVALID;                /* form does not match the flight */
    }
    if (fl->seat_map.available <= 0) return FMS_ERR_NO_SEATS;   /* passenger is informed */

    b = calloc(1, sizeof *b);
    if (b == NULL) return FMS_ERR_MEM;
    b->id      = sys->next_booking_id;
    b->user_id = actor->id;
    snprintf(b->flight_id, sizeof b->flight_id, "%s", f->flight_id);
    b->passenger = f->passenger;               /* copy of the nested structure */
    b->journey   = f->journey;
    snprintf(b->boarding, sizeof b->boarding, "%s", f->boarding);
    snprintf(b->destination, sizeof b->destination, "%s", f->destination);
    snprintf(b->purpose, sizeof b->purpose, "%s", f->purpose);
    snprintf(b->payment, sizeof b->payment, "%s", f->payment);
    b->fare   = fare_calculate(fl->base_fare);
    b->status = BK_PENDING;
    b->booked_at = time(NULL);
    b->seat_no = flight_take_seat(fl, b->id);
    if (b->seat_no == 0) {
        free(b);
        return FMS_ERR_NO_SEATS;
    }

    snprintf(key, sizeof key, "%u", b->id);
    st = ht_put(sys->booking_index, key, b);
    if (st != FMS_OK) {
        flight_release_seat(fl, b->seat_no);
        free(b);
        return st;
    }
    sys->next_booking_id++;
    b->next        = sys->bookings;
    sys->bookings  = b;
    *out_id        = b->id;
    return FMS_OK;
}

FmsStatus booking_request(FmsSystem *sys, const User *actor, const BookingForm *f,
                          unsigned *out_id)
{
    FmsStatus st;

    if (sys == NULL || actor == NULL || f == NULL || out_id == NULL) return FMS_ERR_NULL;
    if (!auth_can(actor, ACT_REQUEST_BOOKING)) return FMS_ERR_PERM;
    if (!valid_flight_id(f->flight_id) || !valid_person_name(f->passenger.name) ||
        !valid_address(f->passenger.address) || !valid_id_number(f->passenger.id_number) ||
        !valid_choice(f->passenger.id_type, ID_TYPES, ID_TYPE_COUNT) ||
        !valid_datetime(&f->journey) || !valid_airport(f->boarding) ||
        !valid_airport(f->destination) ||
        !valid_choice(f->purpose, PURPOSES, PURPOSE_COUNT) ||
        !valid_choice(f->payment, PAYMENT_MODES, PAYMENT_COUNT)) {
        return FMS_ERR_INVALID;
    }

    pthread_mutex_lock(&sys->lock);
    st = request_locked(sys, actor, f, out_id);
    pthread_mutex_unlock(&sys->lock);

    if (st == FMS_OK) {
        log_write("INFO", "booking", "booking %u requested flight=%s user=%u", *out_id,
                  f->flight_id, actor->id);
    } else {
        log_write("WARN", "booking", "request failed flight=%s user=%u reason=%s",
                  f->flight_id, actor->id, fms_strerror(st));
    }
    return st;
}

/* ---------- clerk: confirm or reject ---------- */
FmsStatus booking_confirm(FmsSystem *sys, const User *actor, unsigned id)
{
    Booking  *b;
    FmsStatus st = FMS_OK;

    if (sys == NULL || actor == NULL) return FMS_ERR_NULL;
    if (!auth_can(actor, ACT_CONFIRM_BOOKING)) return FMS_ERR_PERM;

    pthread_mutex_lock(&sys->lock);
    b = find_booking_locked(sys, id);
    if (b == NULL) {
        st = FMS_ERR_NOT_FOUND;
    } else if (b->status != BK_PENDING) {
        st = FMS_ERR_STATE;
    } else {
        b->status = BK_CONFIRMED;              /* payment taken, ticket issued */
        b->confirmed_at = time(NULL);
    }
    pthread_mutex_unlock(&sys->lock);

    if (st == FMS_OK) log_write("INFO", "booking", "booking %u confirmed by user=%u", id, actor->id);
    return st;
}

FmsStatus booking_reject(FmsSystem *sys, const User *actor, unsigned id)
{
    Booking  *b;
    Flight   *fl;
    FmsStatus st = FMS_OK;

    if (sys == NULL || actor == NULL) return FMS_ERR_NULL;
    if (!auth_can(actor, ACT_CONFIRM_BOOKING)) return FMS_ERR_PERM;

    pthread_mutex_lock(&sys->lock);
    b = find_booking_locked(sys, id);
    if (b == NULL) {
        st = FMS_ERR_NOT_FOUND;
    } else if (b->status != BK_PENDING) {
        st = FMS_ERR_STATE;
    } else {
        fl = flight_lookup_locked(sys, b->flight_id);
        flight_release_seat(fl, b->seat_no);
        b->status = BK_REJECTED;
    }
    pthread_mutex_unlock(&sys->lock);

    if (st == FMS_OK) log_write("INFO", "booking", "booking %u rejected by user=%u", id, actor->id);
    return st;
}

/* ---------- cancel a seat ---------- */
FmsStatus booking_cancel(FmsSystem *sys, const User *actor, const CancelForm *f,
                         double *refund_out)
{
    Booking  *b;
    Flight   *fl;
    FmsStatus st = FMS_OK;
    double    refund = 0.0;

    if (sys == NULL || actor == NULL || f == NULL || refund_out == NULL) return FMS_ERR_NULL;
    if (!auth_can(actor, ACT_CANCEL_ANY_BOOKING) && !auth_can(actor, ACT_CANCEL_OWN_BOOKING)) {
        return FMS_ERR_PERM;
    }
    if (!valid_person_name(f->name) || !valid_flight_id(f->flight_id) ||
        !valid_datetime(&f->journey)) {
        return FMS_ERR_INVALID;
    }

    pthread_mutex_lock(&sys->lock);
    b = find_booking_locked(sys, f->id);
    if (b == NULL || !can_view(actor, b)) {
        st = FMS_ERR_NOT_FOUND;                /* do not reveal other people's bookings */
    } else if (!is_active(b)) {
        st = FMS_ERR_STATE;
    } else if (strcmp(b->passenger.name, f->name) != 0 ||
               strcmp(b->flight_id, f->flight_id) != 0 ||
               !dt_same_day(&b->journey, &f->journey)) {
        st = FMS_ERR_INVALID;                  /* form details do not match the booking */
    } else {
        fl = flight_lookup_locked(sys, b->flight_id);
        if (fl == NULL) {
            st = FMS_ERR_NOT_FOUND;
        } else {
            if (b->status == BK_CONFIRMED) {
                refund = refund_calculate(b->fare, dt_to_time(&fl->departure), time(NULL));
            }                                  /* pending bookings were never paid: refund 0 */
            /* Seat reuse is allowed only when cancellation is >= 5 hours before departure. */
            if (difftime(dt_to_time(&fl->departure), time(NULL)) >= 5.0 * 3600.0) {
                flight_release_seat(fl, b->seat_no);
            }
            b->status = BK_CANCELLED;
            b->cancelled_at = time(NULL);
            b->refund = refund;
        }
    }
    pthread_mutex_unlock(&sys->lock);

    if (st == FMS_OK) {
        *refund_out = refund;
        log_write("INFO", "booking", "booking %u cancelled refund=%.2f", f->id, refund);
    }
    return st;
}

/* Called by flight_update when the AIRLINE cancels a flight: full refund for everyone. */
void booking_cancel_for_flight_locked(FmsSystem *sys, Flight *f)
{
    Booking *b;

    for (b = sys->bookings; b != NULL; b = b->next) {
        if (strcmp(b->flight_id, f->id) == 0 && is_active(b)) {
            b->refund = (b->status == BK_CONFIRMED) ? b->fare : 0.0;
            b->status = BK_CANCELLED_BY_AIRLINE;
            b->cancelled_at = time(NULL);
            flight_release_seat(f, b->seat_no);
        }
    }
}

/* ---------- view status ---------- */
FmsStatus booking_get_status(FmsSystem *sys, const User *actor, unsigned id, BookingStatus *out)
{
    const Booking *b;
    FmsStatus st = FMS_OK;

    if (sys == NULL || actor == NULL || out == NULL) return FMS_ERR_NULL;
    pthread_mutex_lock(&sys->lock);
    b = find_booking_locked(sys, id);
    if (b == NULL || !can_view(actor, b)) {
        st = FMS_ERR_NOT_FOUND;
    } else {
        *out = b->status;
    }
    pthread_mutex_unlock(&sys->lock);
    return st;
}

static void print_booking_locked(const FmsSystem *sys, const Booking *b, FILE *out)
{
    char          when[24];
    const Flight *fl = flight_lookup_locked(sys, b->flight_id);

    dt_format(&b->journey, when, sizeof when);
    fprintf(out, "Booking ID : %u\n", b->id);
    fprintf(out, "Status     : %s\n", booking_status_name(b->status));
    if (b->booked_at != (time_t)0) fprintf(out, "Booked at  : %s", ctime(&b->booked_at));
    if (b->confirmed_at != (time_t)0) fprintf(out, "Confirmed  : %s", ctime(&b->confirmed_at));
    if (b->cancelled_at != (time_t)0) fprintf(out, "Cancelled  : %s", ctime(&b->cancelled_at));
    fprintf(out, "Passenger  : %s\n", b->passenger.name);
    fprintf(out, "Flight     : %s  %s -> %s  on %.10s\n", b->flight_id, b->boarding,
            b->destination, when);
    fprintf(out, "Seat       : %d\n", b->seat_no);
    fprintf(out, "Fare       : Rs.%.2f (%s)\n", b->fare, b->payment);
    if (b->status == BK_CANCELLED || b->status == BK_CANCELLED_BY_AIRLINE) {
        fprintf(out, "Refund     : Rs.%.2f\n", b->refund);
    }
    if (fl != NULL && is_active(b)) {          /* airline updates shown to the passenger */
        if (fl->status == FL_DELAYED) {
            fprintf(out, "NOTICE     : flight delayed by %d minutes\n", fl->delay_min);
        }
    }
    if (b->status == BK_CANCELLED_BY_AIRLINE) {
        fprintf(out, "NOTICE     : the airline cancelled this flight, full refund given\n");
    }
}

FmsStatus booking_print_status(FmsSystem *sys, const User *actor, unsigned id, FILE *out)
{
    const Booking *b;
    FmsStatus st = FMS_OK;

    if (sys == NULL || actor == NULL || out == NULL) return FMS_ERR_NULL;
    pthread_mutex_lock(&sys->lock);
    b = find_booking_locked(sys, id);
    if (b == NULL || !can_view(actor, b)) {
        st = FMS_ERR_NOT_FOUND;
    } else {
        print_booking_locked(sys, b, out);
    }
    pthread_mutex_unlock(&sys->lock);
    return st;
}

FmsStatus booking_print_list(FmsSystem *sys, const User *actor, bool pending_only, FILE *out)
{
    Booking *b;
    int      shown = 0;

    if (sys == NULL || actor == NULL || out == NULL) return FMS_ERR_NULL;
    pthread_mutex_lock(&sys->lock);
    fprintf(out, "%-8s %-8s %-20s %-6s %-22s %s\n", "BOOKING", "FLIGHT", "PASSENGER", "SEAT",
            "STATUS", "FARE");
    for (b = sys->bookings; b != NULL; b = b->next) {
        if (!can_view(actor, b)) continue;
        if (pending_only && b->status != BK_PENDING) continue;
        fprintf(out, "%-8u %-8s %-20s %-6d %-22s %.2f\n", b->id, b->flight_id,
                b->passenger.name, b->seat_no, booking_status_name(b->status), b->fare);
        shown++;
    }
    pthread_mutex_unlock(&sys->lock);
    if (shown == 0) fprintf(out, "(none)\n");
    return FMS_OK;
}

/* ---------- the air ticket (only for CONFIRMED bookings) ---------- */
FmsStatus booking_print_ticket(FmsSystem *sys, const User *actor, unsigned id, FILE *out)
{
    const Booking *b;
    const Flight *fl;
    char      when[24];
    FmsStatus st = FMS_OK;

    if (sys == NULL || actor == NULL || out == NULL) return FMS_ERR_NULL;
    pthread_mutex_lock(&sys->lock);
    b = find_booking_locked(sys, id);
    if (b == NULL || !can_view(actor, b)) {
        st = FMS_ERR_NOT_FOUND;
    } else if (b->status != BK_CONFIRMED) {
        st = FMS_ERR_STATE;
    } else {
        fl = flight_lookup_locked(sys, b->flight_id);
        if (fl == NULL) {
            st = FMS_ERR_NOT_FOUND;
        } else {
            dt_format(&fl->departure, when, sizeof when);
            fprintf(out, "\n========== AIR TICKET ==========\n");
            fprintf(out, "Booking ID : %u\n", b->id);
            fprintf(out, "Passenger  : %s\n", b->passenger.name);
            fprintf(out, "ID shown   : %s\n", b->passenger.id_type);
            fprintf(out, "Flight     : %s (%s)\n", fl->id, fl->name);
            fprintf(out, "Route      : %s -> %s\n", b->boarding, b->destination);
            fprintf(out, "Departure  : %s\n", when);
            fprintf(out, "Seat       : %d\n", b->seat_no);
            fprintf(out, "Purpose    : %s\n", b->purpose);
            fprintf(out, "Amount paid: Rs.%.2f by %s\n", b->fare, b->payment);
            fprintf(out, "================================\n");
        }
    }
    pthread_mutex_unlock(&sys->lock);
    return st;
}
