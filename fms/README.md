# Flight Management System (C)

Console application: accounts, login, flight schedule, seat reservation, cancellation,
flight updates and reports. Roles: **Admin**, **Reservation Clerk**, **Passenger**.

## Run it
```
make          # build
make run      # start the program
make test     # CUnit tests
make valgrind # memory leaks / invalid reads and writes / double free
make helgrind # data races in the multithreaded booking test
make cppcheck # static analysis
make coverage # gcov function + branch coverage
make bench    # hash table vs linked list timing
make pool_demo # 8 worker threads handling 60 booking requests
```

Demo logins: `admin / Admin@123`, `clerk1 / Clerk@123`, `pass1 / Pass@123`

## Requirement -> where it is implemented
| # | Requirement | Function(s) | File |
|---|---|---|---|
| 1 | Create account (Admin / Clerk / Passenger) | `auth_register`, `auth_create_first_admin` | auth.c |
| 2 | Login with username + password | `auth_login`, `hash_password` | auth.c, util.c |
| 3 | View reservation status | `booking_print_status`, `booking_print_list` | booking.c |
| 4 | View flight schedule + seats | `flight_print_schedule`, `flight_seat_info` | flight.c |
| 5 | Reserve seats (form, clerk confirms) | `booking_request`, `booking_confirm`, `booking_reject` | booking.c |
| 6 | Cancel seats (form, refund - fee) | `booking_cancel`, `refund_calculate` | booking.c |
| 7 | Update flight (name, time, delay, cancel) | `flight_update` | flight.c |
| 8 | Reports | `report_flight`, `report_reservation_chart`, `report_schedule` | report.c |
| - | Concurrent booking/cancel (worker pool) | `pool_init`, `pool_submit`, `pool_wait`, `pool_destroy` | worker_pool.c |

## Data structures
- **Structures (nested):** `Flight` contains `SeatMap`; `Booking` contains `Passenger`
- **Arrays:** seat map (`unsigned *seats`), option lists, sorted report arrays
- **Linked lists:** users, flights, bookings
- **Hash table:** flight ID -> Flight, booking ID -> Booking (about 14x faster than the list, see `make bench`)

## Rules the code follows
- `fms_init()` / `fms_shutdown()` pair, every `ht_create` has an `ht_destroy`
- Every `malloc/calloc` checked, every pointer parameter checked for NULL, pointers set to NULL after `free`
- Input only through `fgets` (`input_line`), never `gets` or `scanf("%s")`; every field is validated
- RBAC table in `auth_can()`; passengers can never see other passengers' bookings
- Passwords are never stored or logged (only a hash); log lines use `clock_gettime`
- Worker pool: `pthread_create` / `pthread_join`, job queue = linked list, `pthread_cond_wait` to sleep; Helgrind reports 0 races
- One mutex (`sys->lock`) protects all shared data; lock is always taken before touching lists


## Updated cancellation/timestamps (POC)
- Booking, confirmation and cancellation times are recorded (`booked_at`, `confirmed_at`, `cancelled_at`).
- Refund: full fare refund only when cancellation is at least 75 minutes before departure; otherwise zero.
- Seat reuse: a cancelled booking releases its seat only when cancellation is at least 5 hours before departure. Later cancellation leaves that seat unavailable.
- `make test`, `make valgrind`, and `make cppcheck` save output under `reports/`. The supplied test framework is CUnit, not GoogleTest (GoogleTest is C++ and needs a separate C++ harness). Standard cppcheck is not formal MISRA certification; MISRA addon requires a compatible rule file.
- This is a console POC. Payments, digital boarding-pass generation, account suspension, aircraft assignment, and full check-in/baggage workflows are not yet complete.
