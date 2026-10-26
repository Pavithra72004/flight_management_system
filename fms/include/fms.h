/*
 * fms.h - Flight Management System: all shared types and function prototypes.
 *
 * Data structures used (and ONLY these):
 *   - structures (nested)        : Flight contains SeatMap, Booking contains Passenger
 *   - arrays                     : seat map, option lists, sorted report arrays
 *   - linked lists               : users, flights, bookings
 *   - hash table                 : fast lookup of flights and bookings by ID
 */
#ifndef FMS_H
#define FMS_H

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

/* ---------- size limits (buffer sizes, including the '\0') ---------- */
#define USERNAME_MAX     20
#define PASSWORD_MAX     20
#define FLIGHT_ID_MAX     8
#define FLIGHT_NAME_MAX  32
#define AIRPORT_MAX       4
#define PERSON_NAME_MAX  48
#define ADDRESS_MAX      64
#define ID_TYPE_MAX      16
#define ID_NUMBER_MAX    24
#define PURPOSE_MAX      16
#define PAYMENT_MAX       8
#define KEY_MAX          16
#define MAX_SEATS       500
#define TAX_RATE       0.05

/* ---------- status codes returned by almost every function ---------- */
typedef enum {
    FMS_OK = 0,
    FMS_ERR_NULL,       /* a pointer argument was NULL            */
    FMS_ERR_INVALID,    /* input failed validation                */
    FMS_ERR_EXISTS,     /* duplicate username / flight ID         */
    FMS_ERR_NOT_FOUND,  /* flight / booking / user not found      */
    FMS_ERR_NO_SEATS,   /* flight is full                         */
    FMS_ERR_AUTH,       /* wrong username or password             */
    FMS_ERR_PERM,       /* role is not allowed to do this         */
    FMS_ERR_STATE,      /* action not allowed in current state    */
    FMS_ERR_MEM         /* malloc/calloc failed                   */
} FmsStatus;

typedef enum { ROLE_PASSENGER = 0, ROLE_CLERK, ROLE_ADMIN, ROLE_COUNT } Role;
typedef enum { FL_SCHEDULED, FL_DELAYED, FL_CANCELLED } FlightStatus;
typedef enum {
    BK_PENDING, BK_CONFIRMED, BK_REJECTED, BK_CANCELLED, BK_CANCELLED_BY_AIRLINE
} BookingStatus;

/* Things a user may try to do (used for role based access control). */
typedef enum {
    ACT_MANAGE_FLIGHTS,
    ACT_CREATE_STAFF,
    ACT_REQUEST_BOOKING,
    ACT_CONFIRM_BOOKING,
    ACT_CANCEL_ANY_BOOKING,
    ACT_CANCEL_OWN_BOOKING,
    ACT_VIEW_ALL_BOOKINGS,
    ACT_VIEW_REPORTS,
    ACT_COUNT
} Action;

/* ---------- plain structures ---------- */
typedef struct {
    int year, month, day, hour, minute;
} DateTime;

typedef struct User {
    unsigned      id;
    char          username[USERNAME_MAX];
    unsigned long pwd_hash;              /* never store the real password */
    Role          role;
    struct User  *next;                  /* linked list */
} User;

typedef struct {                         /* ARRAY of seats */
    int       total;
    int       available;
    unsigned *seats;                     /* seats[i] = 0 (free) or booking id */
} SeatMap;

typedef struct Flight {
    char          id[FLIGHT_ID_MAX];     /* e.g. "AI101" */
    char          name[FLIGHT_NAME_MAX];
    char          source[AIRPORT_MAX];   /* e.g. "MAA"   */
    char          destination[AIRPORT_MAX];
    DateTime      departure;
    int           delay_min;
    FlightStatus  status;
    double        base_fare;
    SeatMap       seat_map;              /* nested structure */
    struct Flight *next;                 /* linked list */
} Flight;

typedef struct {
    char name[PERSON_NAME_MAX];
    char address[ADDRESS_MAX];
    char id_type[ID_TYPE_MAX];
    char id_number[ID_NUMBER_MAX];
} Passenger;

typedef struct Booking {
    unsigned       id;
    unsigned       user_id;
    char           flight_id[FLIGHT_ID_MAX];
    Passenger      passenger;            /* nested structure */
    DateTime       journey;
    char           boarding[AIRPORT_MAX];
    char           destination[AIRPORT_MAX];
    char           purpose[PURPOSE_MAX];
    char           payment[PAYMENT_MAX];
    int            seat_no;
    double         fare;
    double         refund;
    BookingStatus  status;
    time_t         booked_at;
    time_t         confirmed_at;
    time_t         cancelled_at;
    struct Booking *next;                /* linked list */
} Booking;

/* ---------- hash table (separate chaining) ---------- */
typedef struct HashNode {
    char             key[KEY_MAX];
    void            *value;              /* not owned by the table */
    struct HashNode *next;
} HashNode;

typedef struct {
    HashNode **buckets;                  /* ARRAY of linked lists */
    size_t     nbuckets;
    size_t     count;
} HashTable;

/* ---------- the whole system in one place (no global variables) ---------- */
typedef struct {
    User           *users;
    Flight         *flights;
    Booking        *bookings;
    HashTable      *flight_index;        /* flight id  -> Flight*  */
    HashTable      *booking_index;       /* booking id -> Booking* */
    unsigned        next_user_id;
    unsigned        next_booking_id;
    pthread_mutex_t lock;                /* protects everything above */
} FmsSystem;

/* ---------- request structures ---------- */
typedef struct {
    char     id[FLIGHT_ID_MAX];
    char     name[FLIGHT_NAME_MAX];
    char     source[AIRPORT_MAX];
    char     destination[AIRPORT_MAX];
    DateTime departure;
    int      seats;
    double   fare;
} FlightParams;

typedef enum { UPD_NAME, UPD_DEPARTURE, UPD_DELAY, UPD_CANCEL } UpdateKind;
typedef struct {
    UpdateKind kind;
    char       name[FLIGHT_NAME_MAX];
    DateTime   departure;
    int        delay_min;
} FlightUpdate;

typedef struct {                         /* the reservation form */
    char      flight_id[FLIGHT_ID_MAX];
    Passenger passenger;
    DateTime  journey;
    char      boarding[AIRPORT_MAX];
    char      destination[AIRPORT_MAX];
    char      purpose[PURPOSE_MAX];
    char      payment[PAYMENT_MAX];
} BookingForm;

typedef struct {                         /* the cancellation form */
    unsigned id;
    char     name[PERSON_NAME_MAX];
    DateTime journey;
    char     flight_id[FLIGHT_ID_MAX];
} CancelForm;

/* ================= util.c ================= */
extern const char *const ID_TYPES[];
extern const int         ID_TYPE_COUNT;
extern const char *const PURPOSES[];
extern const int         PURPOSE_COUNT;
extern const char *const PAYMENT_MODES[];
extern const int         PAYMENT_COUNT;

const char *fms_strerror(FmsStatus s);
const char *role_name(Role r);
const char *flight_status_name(FlightStatus s);
const char *booking_status_name(BookingStatus s);

time_t   dt_to_time(const DateTime *d);
DateTime dt_from_time(time_t t);
void     dt_format(const DateTime *d, char *buf, size_t size);
bool     dt_same_day(const DateTime *a, const DateTime *b);
bool     parse_datetime(const char *s, DateTime *out, bool with_time);

bool valid_username(const char *s);
bool valid_password(const char *s);
bool valid_flight_id(const char *s);
bool valid_flight_name(const char *s);
bool valid_airport(const char *s);
bool valid_person_name(const char *s);
bool valid_address(const char *s);
bool valid_id_number(const char *s);
bool valid_datetime(const DateTime *d);
bool valid_choice(const char *s, const char *const *options, int count);

bool input_line(const char *prompt, char *buf, size_t size);
bool input_int(const char *prompt, int min, int max, int *out);
bool input_double(const char *prompt, double min, double max, double *out);

unsigned long hash_password(const char *salt, const char *password);
double        round_money(double v);

/* ================= logger.c ================= */
void log_init(const char *path);
void log_close(void);
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
void log_write(const char *level, const char *module, const char *fmt, ...);

/* ================= hashtable.c ================= */
HashTable *ht_create(size_t nbuckets);
FmsStatus  ht_put(HashTable *h, const char *key, void *value);
void      *ht_get(const HashTable *h, const char *key);
FmsStatus  ht_remove(HashTable *h, const char *key);
void       ht_destroy(HashTable *h);

/* ================= system.c ================= */
FmsStatus fms_init(FmsSystem *sys);
void      fms_shutdown(FmsSystem *sys);

/* ================= auth.c ================= */
bool      auth_can(const User *u, Action a);
FmsStatus auth_create_first_admin(FmsSystem *sys, const char *username, const char *password);
FmsStatus auth_register(FmsSystem *sys, const User *actor, const char *username,
                        const char *password, Role role);
FmsStatus auth_login(FmsSystem *sys, const char *username, const char *password,
                     const User **out);

/* ================= flight.c ================= */
/* functions ending in _locked need sys->lock to be held by the caller */
Flight  *flight_lookup_locked(const FmsSystem *sys, const char *id);
Flight **flight_collect_sorted_locked(FmsSystem *sys, size_t *n_out); /* caller frees */
int      flight_take_seat(Flight *f, unsigned booking_id);            /* 0 = none */
void     flight_release_seat(Flight *f, int seat_no);
void     flight_print_header(FILE *out);
void     flight_print_row(FILE *out, const Flight *f);

FmsStatus flight_add(FmsSystem *sys, const User *actor, const FlightParams *p);
FmsStatus flight_update(FmsSystem *sys, const User *actor, const char *flight_id,
                        const FlightUpdate *u);
FmsStatus flight_print_schedule(FmsSystem *sys, FILE *out, const char *dest_filter);
FmsStatus flight_seat_info(FmsSystem *sys, const char *flight_id, int *available, int *total);

/* ================= booking.c ================= */
double    fare_calculate(double base_fare);
double    refund_calculate(double fare, time_t departure, time_t now);
void      booking_cancel_for_flight_locked(FmsSystem *sys, Flight *f);

FmsStatus booking_request(FmsSystem *sys, const User *actor, const BookingForm *f,
                          unsigned *out_id);
FmsStatus booking_confirm(FmsSystem *sys, const User *actor, unsigned id);
FmsStatus booking_reject(FmsSystem *sys, const User *actor, unsigned id);
FmsStatus booking_cancel(FmsSystem *sys, const User *actor, const CancelForm *f,
                         double *refund_out);
FmsStatus booking_get_status(FmsSystem *sys, const User *actor, unsigned id,
                             BookingStatus *out);
FmsStatus booking_print_status(FmsSystem *sys, const User *actor, unsigned id, FILE *out);
FmsStatus booking_print_ticket(FmsSystem *sys, const User *actor, unsigned id, FILE *out);
FmsStatus booking_print_list(FmsSystem *sys, const User *actor, bool pending_only, FILE *out);

/* ================= report.c ================= */
FmsStatus report_flight(FmsSystem *sys, const User *actor, const char *flight_id, FILE *out);
FmsStatus report_reservation_chart(FmsSystem *sys, const User *actor, FILE *out);
FmsStatus report_schedule(FmsSystem *sys, const User *actor, int days, FILE *out);

/* ================= worker_pool.c ================= */
/* A fixed set of worker threads takes booking / cancel jobs from a queue
 * (a linked list protected by a mutex + condition variable). */
typedef enum { JOB_BOOK, JOB_CANCEL } JobType;

typedef struct Job {
    JobType        type;
    const User    *actor;
    BookingForm    form;          /* used by JOB_BOOK   */
    CancelForm     cancel;        /* used by JOB_CANCEL */
    FmsStatus      result;        /* filled in by the worker */
    unsigned       booking_id;    /* JOB_BOOK result    */
    double         refund;        /* JOB_CANCEL result  */
    struct Job    *next;          /* queue link */
} Job;

typedef struct {
    FmsSystem      *sys;
    pthread_t      *threads;      /* array of worker threads */
    int             nthreads;
    Job            *head, *tail;  /* queue (linked list) */
    int             pending;      /* jobs queued or running */
    bool            stop;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  all_done;
} WorkerPool;

FmsStatus pool_init(WorkerPool *p, FmsSystem *sys, int nthreads);   /* pthread_create */
FmsStatus pool_submit(WorkerPool *p, Job *job);   /* job memory stays owned by the caller */
void      pool_wait(WorkerPool *p);               /* block until every submitted job is done */
void      pool_destroy(WorkerPool *p);            /* stop workers, pthread_join, free */

#endif /* FMS_H */
