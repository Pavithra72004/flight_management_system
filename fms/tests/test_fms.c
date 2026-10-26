#define _POSIX_C_SOURCE 200809L
#include "fms.h"
#include <CUnit/Basic.h>
#include <pthread.h>
#include <string.h>

/* ---------- helper: a fresh system with 1 admin, 1 clerk, 2 passengers ---------- */
typedef struct {
    FmsSystem   sys;
    const User *admin;
    const User *clerk;
    const User *pass;
    const User *pass2;
} Env;

static void env_up(Env *e)
{
    memset(e, 0, sizeof *e);
    CU_ASSERT_EQUAL(fms_init(&e->sys), FMS_OK);
    CU_ASSERT_EQUAL(auth_create_first_admin(&e->sys, "admin", "Admin@123"), FMS_OK);
    CU_ASSERT_EQUAL(auth_login(&e->sys, "admin", "Admin@123", &e->admin), FMS_OK);
    CU_ASSERT_EQUAL(auth_register(&e->sys, e->admin, "clerk1", "Clerk@123", ROLE_CLERK), FMS_OK);
    CU_ASSERT_EQUAL(auth_register(&e->sys, NULL, "pass1", "Pass@123", ROLE_PASSENGER), FMS_OK);
    CU_ASSERT_EQUAL(auth_register(&e->sys, NULL, "pass2", "Pass@456", ROLE_PASSENGER), FMS_OK);
    CU_ASSERT_EQUAL(auth_login(&e->sys, "clerk1", "Clerk@123", &e->clerk), FMS_OK);
    CU_ASSERT_EQUAL(auth_login(&e->sys, "pass1", "Pass@123", &e->pass), FMS_OK);
    CU_ASSERT_EQUAL(auth_login(&e->sys, "pass2", "Pass@456", &e->pass2), FMS_OK);
}

static void env_down(Env *e) { fms_shutdown(&e->sys); }

static FmsStatus add_flight(Env *e, const char *id, int seats, int hours_ahead)
{
    FlightParams p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.name, sizeof p.name, "Test Air");
    snprintf(p.source, sizeof p.source, "MAA");
    snprintf(p.destination, sizeof p.destination, "DEL");
    p.departure = dt_from_time(time(NULL) + (time_t)hours_ahead * 3600);
    p.seats = seats;
    p.fare  = 1000.0;
    return flight_add(&e->sys, e->admin, &p);
}

static BookingForm make_form(Env *e, const char *id, int hours_ahead)
{
    BookingForm f;
    (void)e;
    memset(&f, 0, sizeof f);
    snprintf(f.flight_id, sizeof f.flight_id, "%s", id);
    snprintf(f.passenger.name, sizeof f.passenger.name, "Asha Rao");
    snprintf(f.passenger.address, sizeof f.passenger.address, "5 Main Street Chennai");
    snprintf(f.passenger.id_type, sizeof f.passenger.id_type, "PASSPORT");
    snprintf(f.passenger.id_number, sizeof f.passenger.id_number, "P1234567");
    f.journey = dt_from_time(time(NULL) + (time_t)hours_ahead * 3600);
    snprintf(f.boarding, sizeof f.boarding, "MAA");
    snprintf(f.destination, sizeof f.destination, "DEL");
    snprintf(f.purpose, sizeof f.purpose, "LEISURE");
    snprintf(f.payment, sizeof f.payment, "CARD");
    return f;
}

static CancelForm make_cancel(unsigned id, const char *fid, int hours_ahead)
{
    CancelForm c;
    memset(&c, 0, sizeof c);
    c.id = id;
    snprintf(c.name, sizeof c.name, "Asha Rao");
    snprintf(c.flight_id, sizeof c.flight_id, "%s", fid);
    c.journey = dt_from_time(time(NULL) + (time_t)hours_ahead * 3600);
    return c;
}

/* ======================= validation ======================= */
static void test_validation(void)
{
    DateTime d;
    CU_ASSERT_TRUE(valid_username("ravi_01"));
    CU_ASSERT_FALSE(valid_username("ab"));
    CU_ASSERT_FALSE(valid_username("bad name"));
    CU_ASSERT_FALSE(valid_username("this_username_is_way_too_long"));
    CU_ASSERT_TRUE(valid_password("Pass@123"));
    CU_ASSERT_FALSE(valid_password("ab1"));            /* too short */
    CU_ASSERT_FALSE(valid_password("onlyletters"));
    CU_ASSERT_FALSE(valid_password("12345678"));
    CU_ASSERT_FALSE(valid_password("has space1"));
    CU_ASSERT_TRUE(valid_flight_id("AI101"));
    CU_ASSERT_FALSE(valid_flight_id("ai101"));
    CU_ASSERT_FALSE(valid_flight_id("A101"));
    CU_ASSERT_FALSE(valid_flight_id("AI12345"));
    CU_ASSERT_TRUE(valid_airport("MAA"));
    CU_ASSERT_FALSE(valid_airport("maa"));
    CU_ASSERT_FALSE(valid_airport("MAAA"));
    CU_ASSERT_TRUE(valid_person_name("Asha Rao"));
    CU_ASSERT_FALSE(valid_person_name("R2D2"));
    CU_ASSERT_FALSE(valid_person_name(NULL));
    CU_ASSERT_TRUE(parse_datetime("2026-02-28", &d, false));
    CU_ASSERT_FALSE(parse_datetime("2026-02-30", &d, false));
    CU_ASSERT_TRUE(parse_datetime("2028-02-29 23:59", &d, true));
    CU_ASSERT_FALSE(parse_datetime("2026-13-01", &d, false));
    CU_ASSERT_FALSE(parse_datetime("2026-10-10 25:00", &d, true));
    CU_ASSERT_FALSE(parse_datetime("2026-10-10 junk", &d, false));
}

/* ======================= hash table ======================= */
static void test_hashtable(void)
{
    int        a = 1, b = 2;
    char       key[KEY_MAX];
    int        vals[200];
    int        i;
    HashTable *h = ht_create(4);                      /* tiny table => many collisions */

    CU_ASSERT_PTR_NOT_NULL_FATAL(h);
    CU_ASSERT_EQUAL(ht_put(h, "AI101", &a), FMS_OK);
    CU_ASSERT_EQUAL(ht_put(h, "BA202", &b), FMS_OK);
    CU_ASSERT_EQUAL(ht_put(h, "AI101", &b), FMS_ERR_EXISTS);
    CU_ASSERT_PTR_EQUAL(ht_get(h, "AI101"), &a);
    CU_ASSERT_PTR_EQUAL(ht_get(h, "BA202"), &b);
    CU_ASSERT_PTR_NULL(ht_get(h, "ZZ999"));
    CU_ASSERT_EQUAL(ht_remove(h, "AI101"), FMS_OK);
    CU_ASSERT_PTR_NULL(ht_get(h, "AI101"));
    CU_ASSERT_EQUAL(ht_remove(h, "AI101"), FMS_ERR_NOT_FOUND);
    CU_ASSERT_EQUAL(ht_put(NULL, "x", &a), FMS_ERR_NULL);
    for (i = 0; i < 200; i++) {
        vals[i] = i;
        snprintf(key, sizeof key, "K%d", i);
        CU_ASSERT_EQUAL(ht_put(h, key, &vals[i]), FMS_OK);
    }
    for (i = 0; i < 200; i++) {
        snprintf(key, sizeof key, "K%d", i);
        CU_ASSERT_PTR_EQUAL(ht_get(h, key), &vals[i]);
    }
    ht_destroy(h);
}

/* ======================= auth / registration ======================= */
static void test_auth(void)
{
    Env         e;
    const User *u = NULL;

    env_up(&e);
    CU_ASSERT_EQUAL(auth_register(&e.sys, NULL, "pass1", "Pass@123", ROLE_PASSENGER), FMS_ERR_EXISTS);
    CU_ASSERT_EQUAL(auth_register(&e.sys, NULL, "newuser", "weak", ROLE_PASSENGER), FMS_ERR_INVALID);
    CU_ASSERT_EQUAL(auth_register(&e.sys, NULL, "x", "Pass@123", ROLE_PASSENGER), FMS_ERR_INVALID);
    /* only an admin may create staff */
    CU_ASSERT_EQUAL(auth_register(&e.sys, NULL, "evil", "Pass@123", ROLE_ADMIN), FMS_ERR_PERM);
    CU_ASSERT_EQUAL(auth_register(&e.sys, e.pass, "evil", "Pass@123", ROLE_CLERK), FMS_ERR_PERM);
    CU_ASSERT_EQUAL(auth_register(&e.sys, e.admin, "clerk2", "Clerk@456", ROLE_CLERK), FMS_OK);
    /* login */
    CU_ASSERT_EQUAL(auth_login(&e.sys, "pass1", "Pass@123", &u), FMS_OK);
    CU_ASSERT_PTR_NOT_NULL(u);
    CU_ASSERT_EQUAL(auth_login(&e.sys, "pass1", "wrong", &u), FMS_ERR_AUTH);
    CU_ASSERT_PTR_NULL(u);
    CU_ASSERT_EQUAL(auth_login(&e.sys, "nobody", "Pass@123", &u), FMS_ERR_AUTH);
    CU_ASSERT_EQUAL(auth_create_first_admin(&e.sys, "admin2", "Admin@456"), FMS_ERR_STATE);
    /* RBAC table */
    CU_ASSERT_TRUE(auth_can(e.admin, ACT_MANAGE_FLIGHTS));
    CU_ASSERT_FALSE(auth_can(e.pass, ACT_MANAGE_FLIGHTS));
    CU_ASSERT_TRUE(auth_can(e.clerk, ACT_CONFIRM_BOOKING));
    CU_ASSERT_FALSE(auth_can(NULL, ACT_VIEW_REPORTS));
    env_down(&e);
}

/* ======================= flight creation / search ======================= */
static void test_flight_creation(void)
{
    Env          e;
    FlightParams p;
    int          avail = 0, total = 0;
    FILE        *tmp = tmpfile();

    env_up(&e);
    CU_ASSERT_EQUAL(add_flight(&e, "AI101", 10, 48), FMS_OK);
    CU_ASSERT_EQUAL(add_flight(&e, "AI101", 10, 48), FMS_ERR_EXISTS);
    CU_ASSERT_EQUAL(flight_seat_info(&e.sys, "AI101", &avail, &total), FMS_OK);
    CU_ASSERT_EQUAL(avail, 10);
    CU_ASSERT_EQUAL(total, 10);
    CU_ASSERT_EQUAL(flight_seat_info(&e.sys, "ZZ999", &avail, &total), FMS_ERR_NOT_FOUND);

    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "AI102");
    snprintf(p.name, sizeof p.name, "Test Air");
    snprintf(p.source, sizeof p.source, "MAA");
    snprintf(p.destination, sizeof p.destination, "MAA");      /* same as source */
    p.departure = dt_from_time(time(NULL) + 3600);
    p.seats = 10; p.fare = 100.0;
    CU_ASSERT_EQUAL(flight_add(&e.sys, e.admin, &p), FMS_ERR_INVALID);
    snprintf(p.destination, sizeof p.destination, "DEL");
    p.seats = 0;
    CU_ASSERT_EQUAL(flight_add(&e.sys, e.admin, &p), FMS_ERR_INVALID);
    p.seats = 10;
    CU_ASSERT_EQUAL(flight_add(&e.sys, e.pass, &p), FMS_ERR_PERM);   /* passenger cannot */
    CU_ASSERT_EQUAL(flight_add(&e.sys, e.clerk, &p), FMS_ERR_PERM);
    CU_ASSERT_EQUAL(flight_add(&e.sys, e.admin, &p), FMS_OK);

    /* search by destination */
    CU_ASSERT_PTR_NOT_NULL_FATAL(tmp);
    CU_ASSERT_EQUAL(flight_print_schedule(&e.sys, tmp, "DEL"), FMS_OK);
    CU_ASSERT_EQUAL(flight_print_schedule(&e.sys, tmp, "xx"), FMS_ERR_INVALID);
    fclose(tmp);
    env_down(&e);
}

/* ======================= booking ======================= */
static void test_booking_flow(void)
{
    Env           e;
    BookingForm   f;
    unsigned      id = 0, id2 = 0;
    BookingStatus st;
    int           avail = 0, total = 0;

    env_up(&e);
    CU_ASSERT_EQUAL(add_flight(&e, "AI101", 2, 72), FMS_OK);
    f = make_form(&e, "AI101", 72);

    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass, &f, &id), FMS_OK);
    CU_ASSERT_EQUAL(booking_get_status(&e.sys, e.pass, id, &st), FMS_OK);
    CU_ASSERT_EQUAL(st, BK_PENDING);
    CU_ASSERT_EQUAL(booking_get_status(&e.sys, e.pass2, id, &st), FMS_ERR_NOT_FOUND);  /* not theirs */
    CU_ASSERT_EQUAL(booking_get_status(&e.sys, e.clerk, id, &st), FMS_OK);              /* staff sees all */

    CU_ASSERT_EQUAL(booking_confirm(&e.sys, e.pass, id), FMS_ERR_PERM);                 /* passenger cannot */
    CU_ASSERT_EQUAL(booking_confirm(&e.sys, e.clerk, id), FMS_OK);
    CU_ASSERT_EQUAL(booking_confirm(&e.sys, e.clerk, id), FMS_ERR_STATE);               /* twice */
    CU_ASSERT_EQUAL(booking_confirm(&e.sys, e.clerk, 99999), FMS_ERR_NOT_FOUND);

    /* second passenger takes the last seat, third request finds the flight full */
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass2, &f, &id2), FMS_OK);
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass, &f, &id), FMS_ERR_NO_SEATS);
    /* clerk rejects the pending one -> the seat becomes free again */
    CU_ASSERT_EQUAL(booking_reject(&e.sys, e.clerk, id2), FMS_OK);
    CU_ASSERT_EQUAL(flight_seat_info(&e.sys, "AI101", &avail, &total), FMS_OK);
    CU_ASSERT_EQUAL(avail, 1);
    CU_ASSERT_EQUAL(booking_get_status(&e.sys, e.pass2, id2, &st), FMS_OK);
    CU_ASSERT_EQUAL(st, BK_REJECTED);

    /* bad forms */
    f = make_form(&e, "AI101", 72);
    snprintf(f.boarding, sizeof f.boarding, "BLR");              /* not this flight's source */
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass, &f, &id), FMS_ERR_INVALID);
    f = make_form(&e, "AI101", 72);
    snprintf(f.payment, sizeof f.payment, "GOLD");
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass, &f, &id), FMS_ERR_INVALID);
    f = make_form(&e, "AI101", 72);
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.clerk, &f, &id), FMS_ERR_PERM);   /* clerk cannot request */
    f = make_form(&e, "ZZ999", 72);
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass, &f, &id), FMS_ERR_NOT_FOUND);
    CU_ASSERT_EQUAL(booking_request(&e.sys, NULL, &f, &id), FMS_ERR_NULL);
    env_down(&e);
}

/* ======================= fare / refund / cancellation ======================= */
static void test_fare_and_refund(void)
{
    time_t now = 1000000;
    CU_ASSERT_DOUBLE_EQUAL(fare_calculate(1000.0), 1050.0, 0.001);
    CU_ASSERT_DOUBLE_EQUAL(fare_calculate(5200.0), 5460.0, 0.001);
    CU_ASSERT_DOUBLE_EQUAL(refund_calculate(1000.0, now + 72 * 3600, now), 1000.0, 0.001);
    CU_ASSERT_DOUBLE_EQUAL(refund_calculate(1000.0, now + 75 * 60, now), 1000.0, 0.001);
    CU_ASSERT_DOUBLE_EQUAL(refund_calculate(1000.0, now + 74 * 60, now), 0.0, 0.001);
    CU_ASSERT_DOUBLE_EQUAL(refund_calculate(1000.0, now + 30 * 60, now), 0.0, 0.001);
    CU_ASSERT_DOUBLE_EQUAL(refund_calculate(1000.0, now - 3600, now), 0.0, 0.001);
}

static void test_cancellation(void)
{
    Env         e;
    BookingForm f;
    CancelForm  c;
    unsigned    id = 0;
    double      refund = -1.0;
    int         avail = 0, total = 0;

    env_up(&e);
    CU_ASSERT_EQUAL(add_flight(&e, "AI101", 5, 72), FMS_OK);
    f = make_form(&e, "AI101", 72);
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass, &f, &id), FMS_OK);
    CU_ASSERT_EQUAL(booking_confirm(&e.sys, e.clerk, id), FMS_OK);

    c = make_cancel(id, "AI101", 72);
    CU_ASSERT_EQUAL(booking_cancel(&e.sys, e.pass2, &c, &refund), FMS_ERR_NOT_FOUND);  /* someone else's */
    snprintf(c.name, sizeof c.name, "Wrong Name");
    CU_ASSERT_EQUAL(booking_cancel(&e.sys, e.pass, &c, &refund), FMS_ERR_INVALID);     /* details mismatch */
    c = make_cancel(id, "AI101", 72);
    CU_ASSERT_EQUAL(booking_cancel(&e.sys, e.pass, &c, &refund), FMS_OK);
    CU_ASSERT_DOUBLE_EQUAL(refund, 1050.0 * 0.90, 0.01);                              /* 72h -> 90% */
    CU_ASSERT_EQUAL(booking_cancel(&e.sys, e.pass, &c, &refund), FMS_ERR_STATE);       /* already cancelled */
    CU_ASSERT_EQUAL(flight_seat_info(&e.sys, "AI101", &avail, &total), FMS_OK);
    CU_ASSERT_EQUAL(avail, 5);                                                          /* seat restored */

    /* a pending booking cancels with no refund; the clerk may cancel any booking */
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass, &f, &id), FMS_OK);
    c = make_cancel(id, "AI101", 72);
    CU_ASSERT_EQUAL(booking_cancel(&e.sys, e.clerk, &c, &refund), FMS_OK);
    CU_ASSERT_DOUBLE_EQUAL(refund, 0.0, 0.001);
    env_down(&e);
}

/* ======================= admin updates a flight ======================= */
static void test_flight_update(void)
{
    Env           e;
    BookingForm   f;
    FlightUpdate  u;
    unsigned      id = 0, id2 = 0;
    BookingStatus st;
    int           avail = 0, total = 0;
    FILE         *tmp = tmpfile();

    env_up(&e);
    CU_ASSERT_EQUAL(add_flight(&e, "AI101", 5, 72), FMS_OK);
    f = make_form(&e, "AI101", 72);
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass, &f, &id), FMS_OK);
    CU_ASSERT_EQUAL(booking_confirm(&e.sys, e.clerk, id), FMS_OK);
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass2, &f, &id2), FMS_OK);   /* stays pending */

    memset(&u, 0, sizeof u);
    u.kind = UPD_DELAY; u.delay_min = 45;
    CU_ASSERT_EQUAL(flight_update(&e.sys, e.pass, "AI101", &u), FMS_ERR_PERM);
    CU_ASSERT_EQUAL(flight_update(&e.sys, e.admin, "AI101", &u), FMS_OK);
    u.delay_min = 0;
    CU_ASSERT_EQUAL(flight_update(&e.sys, e.admin, "AI101", &u), FMS_ERR_INVALID);
    u.kind = UPD_NAME; snprintf(u.name, sizeof u.name, "Renamed Air");
    CU_ASSERT_EQUAL(flight_update(&e.sys, e.admin, "AI101", &u), FMS_OK);
    CU_ASSERT_EQUAL(flight_update(&e.sys, e.admin, "ZZ999", &u), FMS_ERR_NOT_FOUND);
    /* the passenger sees the delay notice in the status view */
    CU_ASSERT_PTR_NOT_NULL_FATAL(tmp);
    CU_ASSERT_EQUAL(booking_print_status(&e.sys, e.pass, id, tmp), FMS_OK);
    rewind(tmp);
    {
        char line[200];
        int  seen = 0;
        while (fgets(line, sizeof line, tmp) != NULL) {
            if (strstr(line, "delayed by 45") != NULL) seen = 1;
        }
        CU_ASSERT_EQUAL(seen, 1);
    }
    fclose(tmp);

    /* airline cancels the flight: everybody is cancelled, seats freed */
    u.kind = UPD_CANCEL;
    CU_ASSERT_EQUAL(flight_update(&e.sys, e.admin, "AI101", &u), FMS_OK);
    CU_ASSERT_EQUAL(booking_get_status(&e.sys, e.pass, id, &st), FMS_OK);
    CU_ASSERT_EQUAL(st, BK_CANCELLED_BY_AIRLINE);
    CU_ASSERT_EQUAL(booking_get_status(&e.sys, e.pass2, id2, &st), FMS_OK);
    CU_ASSERT_EQUAL(st, BK_CANCELLED_BY_AIRLINE);
    CU_ASSERT_EQUAL(flight_seat_info(&e.sys, "AI101", &avail, &total), FMS_OK);
    CU_ASSERT_EQUAL(avail, 5);
    CU_ASSERT_EQUAL(flight_update(&e.sys, e.admin, "AI101", &u), FMS_ERR_STATE);
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass, &f, &id), FMS_ERR_STATE);
    env_down(&e);
}

/* ======================= reports ======================= */
static void test_reports(void)
{
    Env         e;
    BookingForm f;
    unsigned    id = 0;
    FILE       *tmp = tmpfile();

    env_up(&e);
    CU_ASSERT_PTR_NOT_NULL_FATAL(tmp);
    CU_ASSERT_EQUAL(add_flight(&e, "AI101", 5, 72), FMS_OK);
    CU_ASSERT_EQUAL(add_flight(&e, "AI102", 5, 24 * 20), FMS_OK);
    f = make_form(&e, "AI101", 72);
    CU_ASSERT_EQUAL(booking_request(&e.sys, e.pass, &f, &id), FMS_OK);
    CU_ASSERT_EQUAL(report_flight(&e.sys, e.admin, "AI101", tmp), FMS_OK);
    CU_ASSERT_EQUAL(report_flight(&e.sys, e.admin, "ZZ999", tmp), FMS_ERR_NOT_FOUND);
    CU_ASSERT_EQUAL(report_flight(&e.sys, e.pass, "AI101", tmp), FMS_ERR_PERM);
    CU_ASSERT_EQUAL(report_reservation_chart(&e.sys, e.clerk, tmp), FMS_OK);
    CU_ASSERT_EQUAL(report_reservation_chart(&e.sys, e.pass, tmp), FMS_ERR_PERM);
    CU_ASSERT_EQUAL(report_schedule(&e.sys, e.admin, 7, tmp), FMS_OK);
    CU_ASSERT_EQUAL(report_schedule(&e.sys, e.admin, 30, tmp), FMS_OK);
    CU_ASSERT_EQUAL(report_schedule(&e.sys, e.admin, 0, tmp), FMS_ERR_INVALID);
    CU_ASSERT_EQUAL(booking_print_list(&e.sys, e.pass, false, tmp), FMS_OK);
    CU_ASSERT_EQUAL(booking_print_ticket(&e.sys, e.pass, id, tmp), FMS_ERR_STATE);   /* not confirmed yet */
    CU_ASSERT_EQUAL(booking_confirm(&e.sys, e.clerk, id), FMS_OK);
    CU_ASSERT_EQUAL(booking_print_ticket(&e.sys, e.pass, id, tmp), FMS_OK);
    fclose(tmp);
    env_down(&e);
}

/* ======================= concurrency: 30 threads fight for 10 seats ======================= */
typedef struct {
    FmsSystem  *sys;
    const User *user;
    BookingForm form;
    FmsStatus   result;
    unsigned    id;
} RaceJob;

static void *worker(void *arg)
{
    RaceJob *j = (RaceJob *)arg;
    j->result = booking_request(j->sys, j->user, &j->form, &j->id);
    return NULL;
}

static void test_concurrent_booking(void)
{
    enum { THREADS = 30, SEATS = 10 };
    Env       e;
    RaceJob   jobs[THREADS];
    pthread_t th[THREADS];
    int       ok = 0, full = 0, avail = -1, total = 0, i;

    env_up(&e);
    CU_ASSERT_EQUAL(add_flight(&e, "AI101", SEATS, 72), FMS_OK);
    for (i = 0; i < THREADS; i++) {
        jobs[i].sys    = &e.sys;
        jobs[i].user   = (i % 2 == 0) ? e.pass : e.pass2;
        jobs[i].form   = make_form(&e, "AI101", 72);
        jobs[i].result = FMS_ERR_NULL;
        jobs[i].id     = 0;
        CU_ASSERT_EQUAL(pthread_create(&th[i], NULL, worker, &jobs[i]), 0);
    }
    for (i = 0; i < THREADS; i++) {
        CU_ASSERT_EQUAL(pthread_join(th[i], NULL), 0);
        if (jobs[i].result == FMS_OK) ok++;
        if (jobs[i].result == FMS_ERR_NO_SEATS) full++;
    }
    CU_ASSERT_EQUAL(ok, SEATS);                    /* exactly 10 succeed, never more */
    CU_ASSERT_EQUAL(full, THREADS - SEATS);
    CU_ASSERT_EQUAL(flight_seat_info(&e.sys, "AI101", &avail, &total), FMS_OK);
    CU_ASSERT_EQUAL(avail, 0);
    env_down(&e);
}


/* ======================= keyboard input (stdin is replaced by a temp file) ======================= */
static void test_input(void)
{
    char   buf[16];
    int    n = 0;
    double d = 0.0;
    FILE  *in = tmpfile();

    CU_ASSERT_PTR_NOT_NULL_FATAL(in);
    fputs("hello\nabc\n99\n7\n2.5\nthis line is far too long for the buffer\n", in);
    rewind(in);
    {
        FILE *old = stdin;
        stdin = in;                                   /* read from our temp file */
        CU_ASSERT_TRUE(input_line(NULL, buf, sizeof buf));
        CU_ASSERT_STRING_EQUAL(buf, "hello");
        CU_ASSERT_TRUE(input_int(NULL, 1, 10, &n));   /* skips "abc" and 99, accepts 7 */
        CU_ASSERT_EQUAL(n, 7);
        CU_ASSERT_TRUE(input_double(NULL, 0.0, 10.0, &d));
        CU_ASSERT_DOUBLE_EQUAL(d, 2.5, 0.001);
        CU_ASSERT_TRUE(input_line(NULL, buf, sizeof buf));   /* too long -> empty string */
        CU_ASSERT_STRING_EQUAL(buf, "");
        CU_ASSERT_FALSE(input_line(NULL, buf, sizeof buf));  /* end of file */
        stdin = old;
    }
    fclose(in);
}

static void test_misc_names(void)
{
    CU_ASSERT_STRING_EQUAL(fms_strerror(FMS_ERR_NO_SEATS), "No seats available");
    CU_ASSERT_STRING_EQUAL(role_name(ROLE_CLERK), "CLERK");
    CU_ASSERT_STRING_EQUAL(flight_status_name(FL_DELAYED), "DELAYED");
    CU_ASSERT_STRING_EQUAL(booking_status_name(BK_CONFIRMED), "CONFIRMED");
    CU_ASSERT_STRING_EQUAL(booking_status_name((BookingStatus)99), "UNKNOWN");
    CU_ASSERT_TRUE(valid_choice("CARD", PAYMENT_MODES, PAYMENT_COUNT));
    CU_ASSERT_FALSE(valid_choice("GOLD", PAYMENT_MODES, PAYMENT_COUNT));
}


/* ======================= worker pool: booking + cancel jobs on 4 threads ======================= */
static void test_worker_pool(void)
{
    enum { N = 25, SEATS = 10 };
    Env        e;
    WorkerPool pool;
    Job        jobs[N];
    Job        cjobs[SEATS];
    int        i, ok = 0, full = 0, k = 0, cancelled = 0, avail = -1, total = 0;

    env_up(&e);
    CU_ASSERT_EQUAL(add_flight(&e, "AI101", SEATS, 72), FMS_OK);
    CU_ASSERT_EQUAL(pool_init(&pool, &e.sys, 0), FMS_ERR_INVALID);
    CU_ASSERT_EQUAL(pool_init(&pool, &e.sys, 4), FMS_OK);
    for (i = 0; i < N; i++) {
        memset(&jobs[i], 0, sizeof jobs[i]);
        jobs[i].type  = JOB_BOOK;
        jobs[i].actor = e.pass;
        jobs[i].form  = make_form(&e, "AI101", 72);
        CU_ASSERT_EQUAL(pool_submit(&pool, &jobs[i]), FMS_OK);
    }
    pool_wait(&pool);
    for (i = 0; i < N; i++) {
        if (jobs[i].result == FMS_OK) ok++;
        if (jobs[i].result == FMS_ERR_NO_SEATS) full++;
    }
    CU_ASSERT_EQUAL(ok, SEATS);
    CU_ASSERT_EQUAL(full, N - SEATS);

    /* now cancel every successful booking through the pool as well */
    for (i = 0; i < N; i++) {
        if (jobs[i].result != FMS_OK) continue;
        memset(&cjobs[k], 0, sizeof cjobs[k]);
        cjobs[k].type   = JOB_CANCEL;
        cjobs[k].actor  = e.pass;
        cjobs[k].cancel = make_cancel(jobs[i].booking_id, "AI101", 72);
        CU_ASSERT_EQUAL(pool_submit(&pool, &cjobs[k]), FMS_OK);
        k++;
    }
    pool_wait(&pool);
    for (i = 0; i < k; i++) if (cjobs[i].result == FMS_OK) cancelled++;
    CU_ASSERT_EQUAL(cancelled, SEATS);
    CU_ASSERT_EQUAL(flight_seat_info(&e.sys, "AI101", &avail, &total), FMS_OK);
    CU_ASSERT_EQUAL(avail, SEATS);                     /* every seat is free again */
    pool_destroy(&pool);
    env_down(&e);
}

/* ======================= runner ======================= */
int main(void)
{
    CU_pSuite s;
    unsigned  failures;

    log_init("build/test.log");                    /* exercise the logger too */
    if (CU_initialize_registry() != CUE_SUCCESS) return CU_get_error();
    s = CU_add_suite("Flight Management System", NULL, NULL);
    if (s == NULL) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    if (!CU_add_test(s, "validation",              test_validation) ||
        !CU_add_test(s, "hash table",              test_hashtable) ||
        !CU_add_test(s, "registration and login",  test_auth) ||
        !CU_add_test(s, "flight creation/search",  test_flight_creation) ||
        !CU_add_test(s, "ticket booking flow",     test_booking_flow) ||
        !CU_add_test(s, "fare and refund tiers",   test_fare_and_refund) ||
        !CU_add_test(s, "ticket cancellation",     test_cancellation) ||
        !CU_add_test(s, "seat availability update",test_flight_update) ||
        !CU_add_test(s, "reports and tickets",     test_reports) ||
        !CU_add_test(s, "keyboard input helpers",  test_input) ||
        !CU_add_test(s, "names and choices",       test_misc_names) ||
        !CU_add_test(s, "concurrent booking",      test_concurrent_booking) ||
        !CU_add_test(s, "worker pool jobs",        test_worker_pool)) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    log_close();
    return (failures == 0) ? 0 : 1;
}
