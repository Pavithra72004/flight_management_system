#define _POSIX_C_SOURCE 200809L
#include "fms.h"
#include <stdlib.h>
#include <string.h>

/* ======================= small input helpers ======================= */

typedef bool (*Validator)(const char *);

/* keep asking until the validator accepts the text */
static bool ask_valid(const char *prompt, char *dst, size_t dst_size, Validator ok,
                      const char *hint)
{
    char buf[128];

    for (;;) {
        if (!input_line(prompt, buf, sizeof buf)) return false;
        if (ok(buf) && strlen(buf) < dst_size) {
            snprintf(dst, dst_size, "%s", buf);
            return true;
        }
        printf("  Invalid input. %s\n", hint);
    }
}

static bool ask_choice(const char *title, const char *const *options, int count, char *dst,
                       size_t dst_size)
{
    int i;
    int pick = 0;

    printf("%s:\n", title);
    for (i = 0; i < count; i++) printf("  %d. %s\n", i + 1, options[i]);
    if (!input_int("  Choose: ", 1, count, &pick)) return false;
    snprintf(dst, dst_size, "%s", options[pick - 1]);
    return true;
}

static bool ask_date(const char *prompt, DateTime *out, bool with_time)
{
    char buf[64];

    for (;;) {
        if (!input_line(prompt, buf, sizeof buf)) return false;
        if (parse_datetime(buf, out, with_time)) return true;
        printf("  Invalid date. Use %s\n", with_time ? "YYYY-MM-DD HH:MM" : "YYYY-MM-DD");
    }
}

static void show_error(FmsStatus st)
{
    printf("\n  >> Failed: %s\n", fms_strerror(st));
}

/* ======================= passenger screens ======================= */

static void ui_reserve(FmsSystem *sys, const User *user)
{
    BookingForm f;
    unsigned    id = 0;
    FmsStatus   st;

    memset(&f, 0, sizeof f);
    flight_print_schedule(sys, stdout, NULL);
    printf("\n--- Reservation form ---\n");
    if (!ask_valid("Flight ID (e.g. AI101): ", f.flight_id, sizeof f.flight_id,
                   valid_flight_id, "Two capital letters + 1-4 digits.")) return;
    if (!ask_valid("Passenger name: ", f.passenger.name, sizeof f.passenger.name,
                   valid_person_name, "Letters, spaces, '.' and '-' only.")) return;
    if (!ask_valid("Address: ", f.passenger.address, sizeof f.passenger.address,
                   valid_address, "5-63 characters (letters, digits, space , . - / #).")) return;
    if (!ask_choice("Identification to be produced", ID_TYPES, ID_TYPE_COUNT,
                    f.passenger.id_type, sizeof f.passenger.id_type)) return;
    if (!ask_valid("ID number: ", f.passenger.id_number, sizeof f.passenger.id_number,
                   valid_id_number, "4-23 letters/digits.")) return;
    if (!ask_date("Date of journey (YYYY-MM-DD): ", &f.journey, false)) return;
    if (!ask_valid("Boarding airport (e.g. MAA): ", f.boarding, sizeof f.boarding,
                   valid_airport, "Three capital letters.")) return;
    if (!ask_valid("Destination (e.g. DEL): ", f.destination, sizeof f.destination,
                   valid_airport, "Three capital letters.")) return;
    if (!ask_choice("Purpose of journey", PURPOSES, PURPOSE_COUNT, f.purpose,
                    sizeof f.purpose)) return;
    if (!ask_choice("Mode of payment", PAYMENT_MODES, PAYMENT_COUNT, f.payment,
                    sizeof f.payment)) return;

    st = booking_request(sys, user, &f, &id);
    if (st == FMS_OK) {
        printf("\n  >> Request submitted. Your booking ID is %u.\n", id);
        printf("     Status is PENDING until the reservation clerk confirms it.\n");
    } else if (st == FMS_ERR_NO_SEATS) {
        printf("\n  >> Sorry, no seats are available on flight %s.\n", f.flight_id);
    } else {
        show_error(st);
    }
}

static void ui_status_by_id(FmsSystem *sys, const User *user)
{
    int       id = 0;
    FmsStatus st;

    if (!input_int("Booking ID: ", 1, 999999, &id)) return;
    printf("\n");
    st = booking_print_status(sys, user, (unsigned)id, stdout);
    if (st != FMS_OK) show_error(st);
}

static void ui_cancel(FmsSystem *sys, const User *user)
{
    CancelForm cf;
    int        id = 0;
    double     refund = 0.0;
    FmsStatus  st;

    memset(&cf, 0, sizeof cf);
    printf("--- Cancellation form ---\n");
    if (!input_int("Booking ID: ", 1, 999999, &id)) return;
    cf.id = (unsigned)id;
    if (!ask_valid("Passenger name: ", cf.name, sizeof cf.name, valid_person_name,
                   "Letters, spaces, '.' and '-' only.")) return;
    if (!ask_date("Date of journey (YYYY-MM-DD): ", &cf.journey, false)) return;
    if (!ask_valid("Flight ID: ", cf.flight_id, sizeof cf.flight_id, valid_flight_id,
                   "Two capital letters + 1-4 digits.")) return;

    st = booking_cancel(sys, user, &cf, &refund);
    if (st == FMS_OK) {
        printf("\n  >> Booking %u cancelled. Refund: Rs.%.2f (after cancellation fee).\n",
               cf.id, refund);
    } else {
        show_error(st);
    }
}

static void ui_ticket(FmsSystem *sys, const User *user)
{
    int       id = 0;
    FmsStatus st;

    if (!input_int("Booking ID: ", 1, 999999, &id)) return;
    st = booking_print_ticket(sys, user, (unsigned)id, stdout);
    if (st == FMS_ERR_STATE) {
        printf("\n  >> No ticket yet: the booking is not CONFIRMED.\n");
    } else if (st != FMS_OK) {
        show_error(st);
    }
}

static void ui_search_destination(FmsSystem *sys)
{
    char dest[16];

    if (!ask_valid("Destination airport (e.g. DEL): ", dest, sizeof dest, valid_airport,
                   "Three capital letters.")) return;
    printf("\n");
    flight_print_schedule(sys, stdout, dest);
}

static void menu_passenger(FmsSystem *sys, const User *user)
{
    for (;;) {
        int c = 0;
        printf("\n===== PASSENGER MENU (%s) =====\n", user->username);
        printf(" 1. View flight schedule and seat availability\n");
        printf(" 2. Search flights by destination\n");
        printf(" 3. Reserve a seat\n");
        printf(" 4. My reservations (status)\n");
        printf(" 5. Check one reservation by booking ID\n");
        printf(" 6. Print my ticket\n");
        printf(" 7. Cancel a seat\n");
        printf(" 0. Logout\n");
        if (!input_int("Choice: ", 0, 7, &c) || c == 0) return;
        printf("\n");
        switch (c) {
        case 1: flight_print_schedule(sys, stdout, NULL); break;
        case 2: ui_search_destination(sys); break;
        case 3: ui_reserve(sys, user); break;
        case 4: booking_print_list(sys, user, false, stdout); break;
        case 5: ui_status_by_id(sys, user); break;
        case 6: ui_ticket(sys, user); break;
        case 7: ui_cancel(sys, user); break;
        default: break;
        }
    }
}

/* ======================= clerk screens ======================= */

static void ui_review(FmsSystem *sys, const User *user)
{
    int       id = 0;
    int       action = 0;
    FmsStatus st;

    booking_print_list(sys, user, true, stdout);
    if (!input_int("\nBooking ID to review (0 = back): ", 0, 999999, &id) || id == 0) return;
    printf("\n");
    st = booking_print_status(sys, user, (unsigned)id, stdout);
    if (st != FMS_OK) {
        show_error(st);
        return;
    }
    printf("\n 1. Confirm and issue ticket\n 2. Reject (no seats / bad details)\n 0. Back\n");
    if (!input_int("Choice: ", 0, 2, &action) || action == 0) return;
    if (action == 1) {
        st = booking_confirm(sys, user, (unsigned)id);
        if (st == FMS_OK) {
            printf("\n  >> Confirmed. Amount collected, ticket issued:\n");
            booking_print_ticket(sys, user, (unsigned)id, stdout);
        } else {
            show_error(st);
        }
    } else {
        st = booking_reject(sys, user, (unsigned)id);
        if (st == FMS_OK) printf("\n  >> Rejected. The seat is free again.\n");
        else show_error(st);
    }
}

static void menu_reports(FmsSystem *sys, const User *user)
{
    for (;;) {
        int       c = 0;
        char      fid[16];
        FmsStatus st = FMS_OK;

        printf("\n--- REPORTS ---\n");
        printf(" 1. Flight report (details, seat map, passengers)\n");
        printf(" 2. Reservation chart (all flights)\n");
        printf(" 3. Weekly flight schedule\n");
        printf(" 4. Monthly flight schedule\n");
        printf(" 0. Back\n");
        if (!input_int("Choice: ", 0, 4, &c) || c == 0) return;
        switch (c) {
        case 1:
            if (!ask_valid("Flight ID: ", fid, sizeof fid, valid_flight_id,
                           "Two capital letters + 1-4 digits.")) return;
            st = report_flight(sys, user, fid, stdout);
            break;
        case 2: st = report_reservation_chart(sys, user, stdout); break;
        case 3: st = report_schedule(sys, user, 7, stdout); break;
        case 4: st = report_schedule(sys, user, 30, stdout); break;
        default: break;
        }
        if (st != FMS_OK) show_error(st);
    }
}

static void ui_cancel_for_passenger(FmsSystem *sys, const User *user)
{
    ui_cancel(sys, user);       /* clerk uses the same form; RBAC allows any booking */
}

static void menu_clerk(FmsSystem *sys, const User *user)
{
    for (;;) {
        int c = 0;
        printf("\n===== RESERVATION CLERK MENU (%s) =====\n", user->username);
        printf(" 1. View flight schedule and seat availability\n");
        printf(" 2. View all reservations\n");
        printf(" 3. Review pending reservations (confirm / reject)\n");
        printf(" 4. Check one reservation by booking ID\n");
        printf(" 5. Cancel a seat for a passenger\n");
        printf(" 6. Reports\n");
        printf(" 0. Logout\n");
        if (!input_int("Choice: ", 0, 6, &c) || c == 0) return;
        printf("\n");
        switch (c) {
        case 1: flight_print_schedule(sys, stdout, NULL); break;
        case 2: booking_print_list(sys, user, false, stdout); break;
        case 3: ui_review(sys, user); break;
        case 4: ui_status_by_id(sys, user); break;
        case 5: ui_cancel_for_passenger(sys, user); break;
        case 6: menu_reports(sys, user); break;
        default: break;
        }
    }
}

/* ======================= admin screens ======================= */

static void ui_add_flight(FmsSystem *sys, const User *user)
{
    FlightParams p;
    FmsStatus    st;

    memset(&p, 0, sizeof p);
    printf("--- Add flight ---\n");
    if (!ask_valid("Flight ID (e.g. AI101): ", p.id, sizeof p.id, valid_flight_id,
                   "Two capital letters + 1-4 digits.")) return;
    if (!ask_valid("Flight name: ", p.name, sizeof p.name, valid_flight_name,
                   "2-31 letters, digits or spaces.")) return;
    if (!ask_valid("Source airport (e.g. MAA): ", p.source, sizeof p.source, valid_airport,
                   "Three capital letters.")) return;
    if (!ask_valid("Destination airport (e.g. DEL): ", p.destination, sizeof p.destination,
                   valid_airport, "Three capital letters.")) return;
    if (!ask_date("Departure (YYYY-MM-DD HH:MM): ", &p.departure, true)) return;
    if (!input_int("Number of seats (1-500): ", 1, MAX_SEATS, &p.seats)) return;
    if (!input_double("Base fare in Rs: ", 1.0, 1000000.0, &p.fare)) return;

    st = flight_add(sys, user, &p);
    if (st == FMS_OK) printf("\n  >> Flight %s added.\n", p.id);
    else show_error(st);
}

static void ui_update_flight(FmsSystem *sys, const User *user)
{
    char         fid[16];
    int          c = 0;
    int          sure = 0;
    FlightUpdate u;
    FmsStatus    st;

    memset(&u, 0, sizeof u);
    flight_print_schedule(sys, stdout, NULL);
    if (!ask_valid("\nFlight ID to update: ", fid, sizeof fid, valid_flight_id,
                   "Two capital letters + 1-4 digits.")) return;
    printf(" 1. Change flight name\n 2. Change departure time\n 3. Mark as delayed\n"
           " 4. Cancel the flight\n 0. Back\n");
    if (!input_int("Choice: ", 0, 4, &c) || c == 0) return;

    switch (c) {
    case 1:
        u.kind = UPD_NAME;
        if (!ask_valid("New name: ", u.name, sizeof u.name, valid_flight_name,
                       "2-31 letters, digits or spaces.")) return;
        break;
    case 2:
        u.kind = UPD_DEPARTURE;
        if (!ask_date("New departure (YYYY-MM-DD HH:MM): ", &u.departure, true)) return;
        break;
    case 3:
        u.kind = UPD_DELAY;
        if (!input_int("Delay in minutes (1-1440): ", 1, 1440, &u.delay_min)) return;
        break;
    default:
        u.kind = UPD_CANCEL;
        printf("This cancels ALL bookings on %s and refunds them in full.\n", fid);
        if (!input_int("Type 1 to confirm, 0 to abort: ", 0, 1, &sure) || sure == 0) return;
        break;
    }
    st = flight_update(sys, user, fid, &u);
    if (st == FMS_OK) printf("\n  >> Flight %s updated. Passengers will see the change.\n", fid);
    else show_error(st);
}

static void ui_create_staff(FmsSystem *sys, const User *user)
{
    char      name[USERNAME_MAX];
    char      pass[128];
    int       r = 0;
    FmsStatus st;

    printf("--- Create staff account ---\n");
    if (!ask_valid("New username: ", name, sizeof name, valid_username,
                   "3-19 letters, digits or underscore.")) return;
    if (!ask_valid("Password: ", pass, sizeof pass, valid_password,
                   "6-19 chars, at least one letter and one digit, no spaces.")) return;
    printf(" 1. Reservation clerk\n 2. Administrator\n");
    if (!input_int("Role: ", 1, 2, &r)) return;
    st = auth_register(sys, user, name, pass, (r == 1) ? ROLE_CLERK : ROLE_ADMIN);
    memset(pass, 0, sizeof pass);
    if (st == FMS_OK) printf("\n  >> Account '%s' created.\n", name);
    else show_error(st);
}

static void menu_admin(FmsSystem *sys, const User *user)
{
    for (;;) {
        int c = 0;
        printf("\n===== ADMIN MENU (%s) =====\n", user->username);
        printf(" 1. View flight schedule and seat availability\n");
        printf(" 2. Add a flight\n");
        printf(" 3. Update a flight (name / time / delay / cancel)\n");
        printf(" 4. Create clerk or admin account\n");
        printf(" 5. View all reservations\n");
        printf(" 6. Reports\n");
        printf(" 0. Logout\n");
        if (!input_int("Choice: ", 0, 6, &c) || c == 0) return;
        printf("\n");
        switch (c) {
        case 1: flight_print_schedule(sys, stdout, NULL); break;
        case 2: ui_add_flight(sys, user); break;
        case 3: ui_update_flight(sys, user); break;
        case 4: ui_create_staff(sys, user); break;
        case 5: booking_print_list(sys, user, false, stdout); break;
        case 6: menu_reports(sys, user); break;
        default: break;
        }
    }
}

/* ======================= login / register / main ======================= */

static void do_login(FmsSystem *sys)
{
    int attempt;

    for (attempt = 1; attempt <= 3; attempt++) {
        char        name[128];
        char        pass[128];
        const User *user = NULL;
        FmsStatus   st;

        if (!input_line("Username: ", name, sizeof name)) return;
        if (!input_line("Password: ", pass, sizeof pass)) return;
        st = auth_login(sys, name, pass, &user);
        memset(pass, 0, sizeof pass);              /* do not keep passwords in memory */
        if (st == FMS_OK && user != NULL) {
            if (user->role == ROLE_ADMIN)      menu_admin(sys, user);
            else if (user->role == ROLE_CLERK) menu_clerk(sys, user);
            else                               menu_passenger(sys, user);
            return;
        }
        printf("  Login failed (%d of 3).\n", attempt);
    }
    printf("  Too many failed attempts. Returning to the main menu.\n");
}

static void do_register(FmsSystem *sys)
{
    char      name[USERNAME_MAX];
    char      pass[128];
    FmsStatus st;

    printf("--- Create a passenger account ---\n");
    if (!ask_valid("Choose a username: ", name, sizeof name, valid_username,
                   "3-19 letters, digits or underscore.")) return;
    if (!ask_valid("Choose a password: ", pass, sizeof pass, valid_password,
                   "6-19 chars, at least one letter and one digit, no spaces.")) return;
    st = auth_register(sys, NULL, name, pass, ROLE_PASSENGER);
    memset(pass, 0, sizeof pass);
    if (st == FMS_OK) printf("\n  >> Account created. You can log in now.\n");
    else show_error(st);
}

/* Demo accounts and flights so the program is usable immediately. */
static void seed_demo_data(FmsSystem *sys)
{
    static const struct {
        const char *id, *name, *src, *dst;
        int         hours_ahead, seats;
        double      fare;
    } demo[] = {
        {"AI101", "Air India Express", "MAA", "DEL", 72, 6, 5200.0},
        {"BA202", "Blue Sky Airways",  "DEL", "BOM", 30, 4, 4100.0},
        {"UK303", "Vista Air",         "BLR", "MAA", 200, 8, 2500.0},
        {"AI404", "Air India Express", "MAA", "BLR", 8, 3, 2100.0},
    };
    const User *admin = NULL;
    time_t      now = time(NULL);
    size_t      i;

    if (auth_create_first_admin(sys, "admin", "Admin@123") != FMS_OK) return;
    if (auth_login(sys, "admin", "Admin@123", &admin) != FMS_OK) return;
    (void)auth_register(sys, admin, "clerk1", "Clerk@123", ROLE_CLERK);
    (void)auth_register(sys, NULL, "pass1", "Pass@123", ROLE_PASSENGER);

    for (i = 0; i < sizeof demo / sizeof demo[0]; i++) {
        FlightParams p;
        memset(&p, 0, sizeof p);
        snprintf(p.id, sizeof p.id, "%s", demo[i].id);
        snprintf(p.name, sizeof p.name, "%s", demo[i].name);
        snprintf(p.source, sizeof p.source, "%s", demo[i].src);
        snprintf(p.destination, sizeof p.destination, "%s", demo[i].dst);
        p.departure = dt_from_time(now + (time_t)demo[i].hours_ahead * 3600);
        p.seats     = demo[i].seats;
        p.fare      = demo[i].fare;
        (void)flight_add(sys, admin, &p);
    }
}

int main(void)
{
    FmsSystem sys;

    log_init("fms.log");
    if (fms_init(&sys) != FMS_OK) {
        fprintf(stderr, "Could not start the system.\n");
        log_close();
        return 1;
    }
    seed_demo_data(&sys);

    printf("==============================================\n");
    printf("        FLIGHT MANAGEMENT SYSTEM\n");
    printf("==============================================\n");
    printf("Demo logins:  admin/Admin@123   clerk1/Clerk@123   pass1/Pass@123\n");

    for (;;) {
        int c = 0;
        printf("\n===== MAIN MENU =====\n");
        printf(" 1. Login\n 2. Create passenger account\n 0. Exit\n");
        if (!input_int("Choice: ", 0, 2, &c) || c == 0) break;
        printf("\n");
        if (c == 1) do_login(&sys);
        else do_register(&sys);
    }

    printf("\nGoodbye.\n");
    fms_shutdown(&sys);                            /* explicit cleanup */
    log_close();
    return 0;
}
