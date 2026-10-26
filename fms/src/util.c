#define _POSIX_C_SOURCE 200809L
#include "fms.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* ---------- fixed option lists (arrays) used by the reservation form ---------- */
const char *const ID_TYPES[]      = {"PASSPORT", "AADHAAR", "DRIVING_LICENSE", "VOTER_ID"};
const int         ID_TYPE_COUNT   = 4;
const char *const PURPOSES[]      = {"BUSINESS", "LEISURE", "MEDICAL", "EDUCATION", "OTHER"};
const int         PURPOSE_COUNT   = 5;
const char *const PAYMENT_MODES[] = {"CASH", "CARD", "UPI"};
const int         PAYMENT_COUNT   = 3;

/* ---------- names for enums ---------- */
const char *fms_strerror(FmsStatus s)
{
    switch (s) {
    case FMS_OK:            return "OK";
    case FMS_ERR_NULL:      return "Missing data (NULL pointer)";
    case FMS_ERR_INVALID:   return "Invalid input or details do not match";
    case FMS_ERR_EXISTS:    return "Already exists";
    case FMS_ERR_NOT_FOUND: return "Not found";
    case FMS_ERR_NO_SEATS:  return "No seats available";
    case FMS_ERR_AUTH:      return "Wrong username or password";
    case FMS_ERR_PERM:      return "You are not allowed to do this";
    case FMS_ERR_STATE:     return "Not possible in the current state";
    case FMS_ERR_MEM:       return "Out of memory";
    default:                return "Unknown error";
    }
}

const char *role_name(Role r)
{
    static const char *const names[] = {"PASSENGER", "CLERK", "ADMIN"};
    return ((unsigned)r < (unsigned)ROLE_COUNT) ? names[r] : "UNKNOWN";
}

const char *flight_status_name(FlightStatus s)
{
    static const char *const names[] = {"ON TIME", "DELAYED", "CANCELLED"};
    return ((unsigned)s <= (unsigned)FL_CANCELLED) ? names[s] : "UNKNOWN";
}

const char *booking_status_name(BookingStatus s)
{
    static const char *const names[] = {"PENDING", "CONFIRMED", "REJECTED",
                                        "CANCELLED", "CANCELLED_BY_AIRLINE"};
    return ((unsigned)s <= (unsigned)BK_CANCELLED_BY_AIRLINE) ? names[s] : "UNKNOWN";
}

/* ---------- date and time helpers ---------- */
time_t dt_to_time(const DateTime *d)
{
    struct tm t;
    memset(&t, 0, sizeof t);
    t.tm_year  = d->year - 1900;
    t.tm_mon   = d->month - 1;
    t.tm_mday  = d->day;
    t.tm_hour  = d->hour;
    t.tm_min   = d->minute;
    t.tm_isdst = -1;
    return mktime(&t);
}

DateTime dt_from_time(time_t t)
{
    struct tm tmv;
    DateTime  d;
    localtime_r(&t, &tmv);
    d.year   = tmv.tm_year + 1900;
    d.month  = tmv.tm_mon + 1;
    d.day    = tmv.tm_mday;
    d.hour   = tmv.tm_hour;
    d.minute = tmv.tm_min;
    return d;
}

void dt_format(const DateTime *d, char *buf, size_t size)
{
    snprintf(buf, size, "%04d-%02d-%02d %02d:%02d", d->year, d->month, d->day,
             d->hour, d->minute);
}

bool dt_same_day(const DateTime *a, const DateTime *b)
{
    return a->year == b->year && a->month == b->month && a->day == b->day;
}

static int days_in_month(int year, int month)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    if (month == 2 && leap) {
        return 29;
    }
    return days[month - 1];
}

bool valid_datetime(const DateTime *d)
{
    if (d == NULL) return false;
    if (d->year < 2000 || d->year > 2100) return false;
    if (d->month < 1 || d->month > 12) return false;
    if (d->day < 1 || d->day > days_in_month(d->year, d->month)) return false;
    if (d->hour < 0 || d->hour > 23) return false;
    if (d->minute < 0 || d->minute > 59) return false;
    return true;
}

/* "2026-10-25" or "2026-10-25 14:30" */
bool parse_datetime(const char *s, DateTime *out, bool with_time)
{
    DateTime d = {0, 0, 0, 0, 0};
    int      n = 0;

    if (s == NULL || out == NULL) return false;
    if (with_time) {
        if (sscanf(s, "%d-%d-%d %d:%d%n", &d.year, &d.month, &d.day,
                   &d.hour, &d.minute, &n) != 5) return false;
    } else {
        if (sscanf(s, "%d-%d-%d%n", &d.year, &d.month, &d.day, &n) != 3) return false;
    }
    if (s[n] != '\0') return false;
    if (!valid_datetime(&d)) return false;
    *out = d;
    return true;
}

/* ---------- validators ---------- */
static bool check_text(const char *s, size_t min_len, size_t max_len, int (*ok)(int))
{
    size_t len;
    size_t i;

    if (s == NULL) return false;
    len = strlen(s);
    if (len < min_len || len > max_len) return false;
    for (i = 0; i < len; i++) {
        if (!ok((unsigned char)s[i])) return false;
    }
    return true;
}

static int is_user_char(int c) { return isalnum(c) || c == '_'; }
static int is_name_char(int c) { return isalpha(c) || c == ' ' || c == '.' || c == '-'; }
static int is_addr_char(int c)
{
    return isalnum(c) || c == ' ' || c == ',' || c == '.' || c == '-' || c == '/' || c == '#';
}
static int is_fname_char(int c) { return isalnum(c) || c == ' '; }

bool valid_username(const char *s)    { return check_text(s, 3, USERNAME_MAX - 1, is_user_char); }
bool valid_flight_name(const char *s) { return check_text(s, 2, FLIGHT_NAME_MAX - 1, is_fname_char); }
bool valid_address(const char *s)     { return check_text(s, 5, ADDRESS_MAX - 1, is_addr_char); }
bool valid_id_number(const char *s)   { return check_text(s, 4, ID_NUMBER_MAX - 1, isalnum); }

bool valid_person_name(const char *s)
{
    return check_text(s, 2, PERSON_NAME_MAX - 1, is_name_char) && isalpha((unsigned char)s[0]);
}

bool valid_password(const char *s)
{
    size_t len;
    size_t i;
    bool   has_letter = false;
    bool   has_digit  = false;

    if (s == NULL) return false;
    len = strlen(s);
    if (len < 6 || len > PASSWORD_MAX - 1) return false;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!isgraph(c)) return false;          /* no spaces or control chars */
        if (isalpha(c)) has_letter = true;
        if (isdigit(c)) has_digit = true;
    }
    return has_letter && has_digit;
}

/* two capital letters followed by 1 to 4 digits, e.g. AI101 */
bool valid_flight_id(const char *s)
{
    size_t len;
    size_t i;

    if (s == NULL) return false;
    len = strlen(s);
    if (len < 3 || len > 6) return false;
    if (!isupper((unsigned char)s[0]) || !isupper((unsigned char)s[1])) return false;
    for (i = 2; i < len; i++) {
        if (!isdigit((unsigned char)s[i])) return false;
    }
    return true;
}

/* exactly three capital letters, e.g. MAA */
bool valid_airport(const char *s)
{
    if (s == NULL || strlen(s) != 3) return false;
    return isupper((unsigned char)s[0]) && isupper((unsigned char)s[1]) &&
           isupper((unsigned char)s[2]);
}

bool valid_choice(const char *s, const char *const *options, int count)
{
    int i;
    if (s == NULL || options == NULL) return false;
    for (i = 0; i < count; i++) {
        if (strcmp(s, options[i]) == 0) return true;
    }
    return false;
}

/* ---------- safe keyboard input (never use gets or scanf("%s")) ---------- */
bool input_line(const char *prompt, char *buf, size_t size)
{
    size_t len;

    if (buf == NULL || size < 2) return false;
    if (prompt != NULL) {
        printf("%s", prompt);
        fflush(stdout);
    }
    if (fgets(buf, (int)size, stdin) == NULL) {
        buf[0] = '\0';
        return false;                          /* end of input (Ctrl+D) */
    }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        len--;
    } else {
        int c;                                 /* line too long: discard the rest */
        while ((c = getchar()) != '\n' && c != EOF) {
        }
        buf[0] = '\0';                         /* and treat it as invalid (empty) */
        return true;
    }
    if (len > 0 && buf[len - 1] == '\r') {
        buf[len - 1] = '\0';
    }
    return true;
}

bool input_int(const char *prompt, int min, int max, int *out)
{
    char buf[64];

    if (out == NULL) return false;
    for (;;) {
        char *end = NULL;
        long  v;

        if (!input_line(prompt, buf, sizeof buf)) return false;
        errno = 0;
        v = strtol(buf, &end, 10);
        if (end != buf && *end == '\0' && errno == 0 && v >= min && v <= max) {
            *out = (int)v;
            return true;
        }
        printf("  Please enter a whole number from %d to %d.\n", min, max);
    }
}

bool input_double(const char *prompt, double min, double max, double *out)
{
    char buf[64];

    if (out == NULL) return false;
    for (;;) {
        char  *end = NULL;
        double v;

        if (!input_line(prompt, buf, sizeof buf)) return false;
        errno = 0;
        v = strtod(buf, &end);
        if (end != buf && *end == '\0' && errno == 0 && v >= min && v <= max) {
            *out = v;
            return true;
        }
        printf("  Please enter a number from %.2f to %.2f.\n", min, max);
    }
}

/*
 * Password hashing (djb2). The username is used as a "salt".
 * NOTE: this is fine for learning. A real system must use bcrypt or argon2.
 */
unsigned long hash_password(const char *salt, const char *password)
{
    unsigned long h = 5381UL;
    const char   *p;

    for (p = salt; *p != '\0'; ++p) {
        h = h * 33UL + (unsigned char)*p;
    }
    h = h * 33UL + (unsigned char)':';
    for (p = password; *p != '\0'; ++p) {
        h = h * 33UL + (unsigned char)*p;
    }
    return h;
}

double round_money(double v)
{
    return (double)(long)(v * 100.0 + 0.5) / 100.0;
}
