#include <gtest/gtest.h>

extern "C" {
#include "fms.h"
}

TEST(ValidationTest, AcceptsValidUsername) {
    EXPECT_TRUE(valid_username("ravi_01"));
}

TEST(ValidationTest, RejectsInvalidUsername) {
    EXPECT_FALSE(valid_username("ab"));
    EXPECT_FALSE(valid_username("bad name"));
    EXPECT_FALSE(valid_username(nullptr));
}

TEST(ValidationTest, AcceptsValidFlightId) {
    EXPECT_TRUE(valid_flight_id("AI101"));
}

TEST(ValidationTest, RejectsInvalidFlightId) {
    EXPECT_FALSE(valid_flight_id("ai101"));
    EXPECT_FALSE(valid_flight_id("A101"));
}

TEST(UtilityTest, CalculatesFareWithTax) {
    EXPECT_NEAR(fare_calculate(1000.0), 1050.0, 0.01);
}

TEST(UtilityTest, RoundsMoneyToTwoDecimalPlaces) {
    EXPECT_NEAR(round_money(123.456), 123.46, 0.001);
}

TEST(UtilityTest, ReturnsStatusMessage) {
    EXPECT_NE(fms_strerror(FMS_OK), nullptr);
}
// ---------- Date and time tests ----------

TEST(DateTimeTest, AcceptsValidDate) {
    DateTime d = {2026, 10, 25, 14, 30};
    EXPECT_TRUE(valid_datetime(&d));
}

TEST(DateTimeTest, RejectsInvalidDate) {
    DateTime d = {2026, 2, 30, 10, 0};
    EXPECT_FALSE(valid_datetime(&d));
}

TEST(DateTimeTest, RejectsNullDate) {
    EXPECT_FALSE(valid_datetime(nullptr));
}

TEST(DateTimeTest, FormatsDateAndTime) {
    DateTime d = {2026, 10, 25, 14, 30};
    char buffer[32];

    dt_format(&d, buffer, sizeof(buffer));

    EXPECT_STREQ(buffer, "2026-10-25 14:30");
}

TEST(DateTimeTest, DetectsSameDay) {
    DateTime first  = {2026, 10, 25, 10, 0};
    DateTime second = {2026, 10, 25, 18, 30};

    EXPECT_TRUE(dt_same_day(&first, &second));
}

TEST(DateTimeTest, DetectsDifferentDays) {
    DateTime first  = {2026, 10, 25, 10, 0};
    DateTime second = {2026, 10, 26, 10, 0};

    EXPECT_FALSE(dt_same_day(&first, &second));
}

TEST(DateTimeTest, ParsesDateAndTime) {
    DateTime result = {};

    ASSERT_TRUE(parse_datetime("2026-10-25 14:30", &result, true));

    EXPECT_EQ(result.year, 2026);
    EXPECT_EQ(result.month, 10);
    EXPECT_EQ(result.day, 25);
    EXPECT_EQ(result.hour, 14);
    EXPECT_EQ(result.minute, 30);
}

TEST(DateTimeTest, RejectsInvalidDateText) {
    DateTime result = {};

    EXPECT_FALSE(parse_datetime("2026-02-30", &result, false));
}

TEST(DateTimeTest, RejectsNullInput) {
    DateTime result = {};

    EXPECT_FALSE(parse_datetime(nullptr, &result, false));
    EXPECT_FALSE(parse_datetime("2026-10-25", nullptr, false));
}

TEST(DateTimeTest, ConvertsDateToTimeAndBack) {
    DateTime original = {2026, 10, 25, 12, 0};

    time_t timestamp = dt_to_time(&original);
    DateTime result = dt_from_time(timestamp);

    EXPECT_EQ(result.year, original.year);
    EXPECT_EQ(result.month, original.month);
    EXPECT_EQ(result.day, original.day);
    EXPECT_EQ(result.hour, original.hour);
    EXPECT_EQ(result.minute, original.minute);
}

// ---------- Name and status tests ----------

TEST(NameTest, ReturnsRoleNames) {
    EXPECT_STREQ(role_name(ROLE_PASSENGER), "PASSENGER");
    EXPECT_STREQ(role_name(ROLE_CLERK), "CLERK");
    EXPECT_STREQ(role_name(ROLE_ADMIN), "ADMIN");
}

TEST(NameTest, ReturnsUnknownForInvalidRole) {
    EXPECT_STREQ(role_name(static_cast<Role>(999)), "UNKNOWN");
}

TEST(NameTest, ReturnsFlightStatusName) {
    EXPECT_STREQ(flight_status_name(FL_CANCELLED), "CANCELLED");
    EXPECT_STREQ(
        flight_status_name(static_cast<FlightStatus>(999)), "UNKNOWN");
}

TEST(NameTest, ReturnsBookingStatusName) {
    EXPECT_STREQ(
        booking_status_name(BK_CANCELLED_BY_AIRLINE),
        "CANCELLED_BY_AIRLINE");

    EXPECT_STREQ(
        booking_status_name(static_cast<BookingStatus>(999)), "UNKNOWN");
}

TEST(NameTest, ReturnsMessagesForStatuses) {
    EXPECT_STREQ(fms_strerror(FMS_OK), "OK");
    EXPECT_STREQ(fms_strerror(FMS_ERR_NOT_FOUND), "Not found");
    EXPECT_STREQ(
        fms_strerror(static_cast<FmsStatus>(999)), "Unknown error");
}

// ---------- Input-validation tests ----------

TEST(ValidationTest, ValidatesFlightNames) {
    EXPECT_TRUE(valid_flight_name("Air India"));
    EXPECT_FALSE(valid_flight_name("A"));
    EXPECT_FALSE(valid_flight_name("Air@India"));
    EXPECT_FALSE(valid_flight_name(nullptr));
}

TEST(ValidationTest, ValidatesAirports) {
    EXPECT_TRUE(valid_airport("MAA"));
    EXPECT_FALSE(valid_airport("maa"));
    EXPECT_FALSE(valid_airport("MA"));
    EXPECT_FALSE(valid_airport(nullptr));
}

TEST(ValidationTest, ValidatesPersonNames) {
    EXPECT_TRUE(valid_person_name("Ravi Kumar"));
    EXPECT_FALSE(valid_person_name("R"));
    EXPECT_FALSE(valid_person_name(" Ravi"));
    EXPECT_FALSE(valid_person_name("Ravi123"));
}

TEST(ValidationTest, ValidatesAddresses) {
    EXPECT_TRUE(valid_address("12 Main Road"));
    EXPECT_FALSE(valid_address("abc"));
    EXPECT_FALSE(valid_address(nullptr));
}

TEST(ValidationTest, ValidatesIdentificationNumbers) {
    EXPECT_TRUE(valid_id_number("AB1234"));
    EXPECT_FALSE(valid_id_number("A1"));
    EXPECT_FALSE(valid_id_number("AB 123"));
    EXPECT_FALSE(valid_id_number(nullptr));
}

TEST(ValidationTest, ValidatesPasswords) {
    EXPECT_TRUE(valid_password("hello123"));
    EXPECT_FALSE(valid_password("hello"));
    EXPECT_FALSE(valid_password("123456"));
    EXPECT_FALSE(valid_password("hello 123"));
    EXPECT_FALSE(valid_password(nullptr));
}

TEST(ValidationTest, ValidatesChoices) {
    const char* options[] = {"CASH", "CARD", "UPI"};

    EXPECT_TRUE(valid_choice("CASH", options, 3));
    EXPECT_FALSE(valid_choice("cash", options, 3));
    EXPECT_FALSE(valid_choice("CHEQUE", options, 3));
    EXPECT_FALSE(valid_choice(nullptr, options, 3));
    EXPECT_FALSE(valid_choice("CASH", nullptr, 3));
}

// ---------- Password hashing tests ----------

TEST(PasswordHashTest, ProducesSameHashForSameInputs) {
    EXPECT_EQ(hash_password("ravi", "hello123"),
              hash_password("ravi", "hello123"));
}

TEST(PasswordHashTest, ChangesHashWhenPasswordChanges) {
    EXPECT_NE(hash_password("ravi", "hello123"),
              hash_password("ravi", "hello456"));
}

TEST(PasswordHashTest, ChangesHashWhenUsernameChanges) {
    EXPECT_NE(hash_password("ravi", "hello123"),
              hash_password("arun", "hello123"));
}

TEST(InputTest, InputLineRejectsNullBuffer) {
    EXPECT_FALSE(input_line(nullptr, nullptr, 10));
}

TEST(InputTest, InputLineRejectsSmallBuffer) {
    char buffer[1];
    EXPECT_FALSE(input_line(nullptr, buffer, sizeof(buffer)));
}

TEST(InputTest, InputIntRejectsNullOutput) {
    EXPECT_FALSE(input_int(nullptr, 0, 100, nullptr));
}

TEST(InputTest, InputDoubleRejectsNullOutput) {
    EXPECT_FALSE(input_double(nullptr, 0.0, 100.0, nullptr));
}

#include <cstring>

// ---------- Hashtable tests ----------

TEST(HashTableTest, RejectsZeroBuckets) {
    EXPECT_EQ(ht_create(0), nullptr);
}

TEST(HashTableTest, CreatesAndDestroysTable) {
    HashTable* table = ht_create(10);

    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->count, 0u);

    ht_destroy(table);
}

TEST(HashTableTest, RejectsNullTableKeyAndValue) {
    HashTable* table = ht_create(10);
    ASSERT_NE(table, nullptr);

    int value = 100;

    EXPECT_EQ(ht_put(nullptr, "key", &value), FMS_ERR_NULL);
    EXPECT_EQ(ht_put(table, nullptr, &value), FMS_ERR_NULL);
    EXPECT_EQ(ht_put(table, "key", nullptr), FMS_ERR_NULL);

    ht_destroy(table);
}

TEST(HashTableTest, RejectsInvalidKeys) {
    HashTable* table = ht_create(10);
    ASSERT_NE(table, nullptr);

    int value = 100;
    char longKey[KEY_MAX + 1];
    std::memset(longKey, 'A', sizeof(longKey));
    longKey[KEY_MAX] = '\0';

    EXPECT_EQ(ht_put(table, "", &value), FMS_ERR_INVALID);
    EXPECT_EQ(ht_put(table, longKey, &value), FMS_ERR_INVALID);

    ht_destroy(table);
}

TEST(HashTableTest, InsertsAndRetrievesValue) {
    HashTable* table = ht_create(10);
    ASSERT_NE(table, nullptr);

    int value = 100;

    EXPECT_EQ(ht_put(table, "flight1", &value), FMS_OK);
    EXPECT_EQ(table->count, 1u);
    EXPECT_EQ(ht_get(table, "flight1"), &value);

    ht_destroy(table);
}

TEST(HashTableTest, RejectsDuplicateKey) {
    HashTable* table = ht_create(10);
    ASSERT_NE(table, nullptr);

    int first = 100;
    int second = 200;

    ASSERT_EQ(ht_put(table, "flight1", &first), FMS_OK);
    EXPECT_EQ(ht_put(table, "flight1", &second), FMS_ERR_EXISTS);
    EXPECT_EQ(ht_get(table, "flight1"), &first);
    EXPECT_EQ(table->count, 1u);

    ht_destroy(table);
}

TEST(HashTableTest, ReturnsNullForMissingKey) {
    HashTable* table = ht_create(10);
    ASSERT_NE(table, nullptr);

    int value = 100;
    ASSERT_EQ(ht_put(table, "flight1", &value), FMS_OK);

    EXPECT_EQ(ht_get(table, "missing"), nullptr);
    EXPECT_EQ(ht_get(nullptr, "flight1"), nullptr);
    EXPECT_EQ(ht_get(table, nullptr), nullptr);

    ht_destroy(table);
}

TEST(HashTableTest, RemovesExistingKey) {
    HashTable* table = ht_create(10);
    ASSERT_NE(table, nullptr);

    int value = 100;
    ASSERT_EQ(ht_put(table, "flight1", &value), FMS_OK);

    EXPECT_EQ(ht_remove(table, "flight1"), FMS_OK);
    EXPECT_EQ(ht_get(table, "flight1"), nullptr);
    EXPECT_EQ(table->count, 0u);

    ht_destroy(table);
}

TEST(HashTableTest, ReportsMissingKeyDuringRemoval) {
    HashTable* table = ht_create(10);
    ASSERT_NE(table, nullptr);

    EXPECT_EQ(ht_remove(table, "missing"), FMS_ERR_NOT_FOUND);
    EXPECT_EQ(ht_remove(nullptr, "key"), FMS_ERR_NULL);
    EXPECT_EQ(ht_remove(table, nullptr), FMS_ERR_NULL);

    ht_destroy(table);
}

TEST(HashTableTest, RemovesOneKeyWithoutRemovingAnother) {
    HashTable* table = ht_create(10);
    ASSERT_NE(table, nullptr);

    int first = 100;
    int second = 200;

    ASSERT_EQ(ht_put(table, "first", &first), FMS_OK);
    ASSERT_EQ(ht_put(table, "second", &second), FMS_OK);

    EXPECT_EQ(ht_remove(table, "first"), FMS_OK);
    EXPECT_EQ(ht_get(table, "first"), nullptr);
    EXPECT_EQ(ht_get(table, "second"), &second);
    EXPECT_EQ(table->count, 1u);

    ht_destroy(table);
}

TEST(HashTableTest, SafelyDestroysNullTable) {
    EXPECT_NO_THROW(ht_destroy(nullptr));
}

#include <fstream>
#include <sstream>
#include <cstdio>
#include <string>

// ---------- Logger tests ----------

TEST(LoggerTest, InitializesAndWritesMessage)
{
    const char* path = "/tmp/fms_gtest_logger.log";

    log_close();
    std::remove(path);

    log_init(path);
    log_write("INFO", "test", "booking %d confirmed", 1000);
    log_close();

    std::ifstream file(path);
    ASSERT_TRUE(file.is_open());

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    EXPECT_NE(content.find("[INFO] [test]"), std::string::npos);
    EXPECT_NE(content.find("booking 1000 confirmed"), std::string::npos);
    EXPECT_NE(content.find('\n'), std::string::npos);

    file.close();
    std::remove(path);
}

TEST(LoggerTest, DoesNotCrashWhenWritingBeforeInitialization)
{
    log_close();

    EXPECT_NO_THROW(log_write("INFO", "test", "message without a log file"));
}

TEST(LoggerTest, HandlesNullPath)
{
    log_close();

    EXPECT_NO_THROW(log_init(nullptr));
    EXPECT_NO_THROW(log_write("INFO", "test", "this should not be saved"));

    log_close();
}

TEST(LoggerTest, CanInitializeAgainAfterClosing)
{
    const char* path = "/tmp/fms_gtest_logger.log";

    log_close();
    std::remove(path);

    log_init(path);
    log_write("DEBUG", "test", "first message");
    log_close();

    log_init(path);
    log_write("INFO", "test", "second message");
    log_close();

    std::ifstream file(path);
    ASSERT_TRUE(file.is_open());

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    EXPECT_NE(content.find("first message"), std::string::npos);
    EXPECT_NE(content.find("second message"), std::string::npos);

    file.close();
    std::remove(path);
}

TEST(LoggerTest, HandlesInvalidFilePath)
{
    log_close();

    // This directory should not exist, so opening the file should fail.
    log_init("/this_directory_should_not_exist/fms.log");

    EXPECT_NO_THROW(log_write("INFO", "test", "message after open failure"));

    log_close();
}

#include <cstdlib>
#include <cstring>

// ---------- System tests ----------

TEST(SystemTest, InitializesSystemSuccessfully)
{
    FmsSystem sys{};

    EXPECT_EQ(fms_init(&sys), FMS_OK);
    EXPECT_EQ(sys.next_user_id, 1);
    EXPECT_EQ(sys.next_booking_id, 1000);
    ASSERT_NE(sys.flight_index, nullptr);
    ASSERT_NE(sys.booking_index, nullptr);

    fms_shutdown(&sys);
}

TEST(SystemTest, RejectsNullSystemDuringInitialization)
{
    EXPECT_EQ(fms_init(nullptr), FMS_ERR_NULL);
}

TEST(SystemTest, ShutsDownInitializedSystem)
{
    FmsSystem sys{};

    ASSERT_EQ(fms_init(&sys), FMS_OK);

    fms_shutdown(&sys);

    EXPECT_EQ(sys.users, nullptr);
    EXPECT_EQ(sys.flights, nullptr);
    EXPECT_EQ(sys.bookings, nullptr);
    EXPECT_EQ(sys.flight_index, nullptr);
    EXPECT_EQ(sys.booking_index, nullptr);
}

TEST(SystemTest, SafelyHandlesNullShutdown)
{
    EXPECT_NO_THROW(fms_shutdown(nullptr));
}

TEST(SystemTest, CanInitializeAgainAfterShutdown)
{
    FmsSystem sys{};

    ASSERT_EQ(fms_init(&sys), FMS_OK);
    fms_shutdown(&sys);

    EXPECT_EQ(fms_init(&sys), FMS_OK);

    EXPECT_EQ(sys.next_user_id, 1);
    EXPECT_EQ(sys.next_booking_id, 1000);

    fms_shutdown(&sys);
}
// ---------- Authentication, flight, booking, report and worker-pool tests ----------

class FmsWorkflowTest : public ::testing::Test {
protected:
    FmsSystem sys{};
    bool initialized = false;
    const User* admin = nullptr;
    const User* clerk = nullptr;
    const User* passenger = nullptr;
    DateTime departure{};

    void SetUp() override {
        ASSERT_EQ(fms_init(&sys), FMS_OK);
        initialized = true;

        ASSERT_EQ(
            auth_create_first_admin(&sys, "admin01", "admin123"),
            FMS_OK
        );

        ASSERT_EQ(
            auth_register(&sys, nullptr, "passenger1", "pass1234",
                          ROLE_PASSENGER),
            FMS_OK
        );

        ASSERT_EQ(
            auth_login(&sys, "admin01", "admin123", &admin),
            FMS_OK
        );

        ASSERT_EQ(
            auth_register(&sys, admin, "clerk01", "clerk123",
                          ROLE_CLERK),
            FMS_OK
        );

        ASSERT_EQ(
            auth_login(&sys, "clerk01", "clerk123", &clerk),
            FMS_OK
        );

        ASSERT_EQ(
            auth_login(&sys, "passenger1", "pass1234", &passenger),
            FMS_OK
        );

        departure = dt_from_time(time(nullptr) + 3 * 24 * 60 * 60);

        FlightParams p{};
        std::snprintf(p.id, sizeof(p.id), "%s", "AI101");
        std::snprintf(p.name, sizeof(p.name), "%s", "Air India");
        std::snprintf(p.source, sizeof(p.source), "%s", "MAA");
        std::snprintf(p.destination, sizeof(p.destination), "%s", "DEL");
        p.departure = departure;
        p.seats = 3;
        p.fare = 1000.0;

        ASSERT_EQ(flight_add(&sys, admin, &p), FMS_OK);
    }

    void TearDown() override {
        if (initialized) {
            fms_shutdown(&sys);
            initialized = false;
        }
    }

    BookingForm makeBookingForm() {
        BookingForm f{};
        std::snprintf(f.flight_id, sizeof(f.flight_id), "%s", "AI101");
        std::snprintf(f.passenger.name, sizeof(f.passenger.name),
                      "%s", "Ravi Kumar");
        std::snprintf(f.passenger.address, sizeof(f.passenger.address),
                      "%s", "12 Main Road");
        std::snprintf(f.passenger.id_type, sizeof(f.passenger.id_type),
                      "%s", "AADHAAR");
        std::snprintf(f.passenger.id_number, sizeof(f.passenger.id_number),
                      "%s", "AB1234");
        f.journey = departure;
        std::snprintf(f.boarding, sizeof(f.boarding), "%s", "MAA");
        std::snprintf(f.destination, sizeof(f.destination), "%s", "DEL");
        std::snprintf(f.purpose, sizeof(f.purpose), "%s", "LEISURE");
        std::snprintf(f.payment, sizeof(f.payment), "%s", "UPI");
        return f;
    }

    CancelForm makeCancelForm(unsigned id) {
        CancelForm f{};
        f.id = id;
        std::snprintf(f.name, sizeof(f.name), "%s", "Ravi Kumar");
        f.journey = departure;
        std::snprintf(f.flight_id, sizeof(f.flight_id), "%s", "AI101");
        return f;
    }

    unsigned createBooking() {
        BookingForm f = makeBookingForm();
        unsigned id = 0;
        EXPECT_EQ(booking_request(&sys, passenger, &f, &id), FMS_OK);
        return id;
    }
};

// ---------- Authentication tests ----------

TEST_F(FmsWorkflowTest, ChecksPermissionsForEachRole) {
    EXPECT_TRUE(auth_can(passenger, ACT_REQUEST_BOOKING));
    EXPECT_TRUE(auth_can(clerk, ACT_CONFIRM_BOOKING));
    EXPECT_TRUE(auth_can(admin, ACT_MANAGE_FLIGHTS));
    EXPECT_FALSE(auth_can(passenger, ACT_MANAGE_FLIGHTS));
}

TEST_F(FmsWorkflowTest, RejectsNullAndInvalidPermissionInputs) {
    EXPECT_FALSE(auth_can(nullptr, ACT_REQUEST_BOOKING));

    User invalid{};
    invalid.role = static_cast<Role>(999);

    EXPECT_FALSE(auth_can(&invalid, ACT_REQUEST_BOOKING));
    EXPECT_FALSE(auth_can(admin, static_cast<Action>(999)));
}

TEST_F(FmsWorkflowTest, RejectsCreatingSecondFirstAdmin) {
    EXPECT_EQ(
        auth_create_first_admin(&sys, "anotheradmin", "admin456"),
        FMS_ERR_STATE
    );
}

TEST_F(FmsWorkflowTest, RejectsNullFirstAdminArguments) {
    EXPECT_EQ(auth_create_first_admin(nullptr, "admin02", "admin123"),
              FMS_ERR_NULL);
    EXPECT_EQ(auth_create_first_admin(&sys, nullptr, "admin123"),
              FMS_ERR_NULL);
    EXPECT_EQ(auth_create_first_admin(&sys, "admin02", nullptr),
              FMS_ERR_NULL);
}

TEST_F(FmsWorkflowTest, RejectsInvalidFirstAdminDetails) {
    EXPECT_EQ(auth_create_first_admin(&sys, "ab", "admin123"),
              FMS_ERR_INVALID);
    EXPECT_EQ(auth_create_first_admin(&sys, "admin02", "short"),
              FMS_ERR_INVALID);
}

TEST_F(FmsWorkflowTest, RejectsDuplicateRegistration) {
    EXPECT_EQ(
        auth_register(&sys, nullptr, "passenger1", "pass1234",
                      ROLE_PASSENGER),
        FMS_ERR_EXISTS
    );
}

TEST_F(FmsWorkflowTest, RejectsUnauthorizedStaffRegistration) {
    EXPECT_EQ(
        auth_register(&sys, passenger, "clerk02", "clerk123",
                      ROLE_CLERK),
        FMS_ERR_PERM
    );
}

TEST_F(FmsWorkflowTest, RejectsInvalidRegistrationDetails) {
    EXPECT_EQ(
        auth_register(&sys, nullptr, "ab", "pass1234", ROLE_PASSENGER),
        FMS_ERR_INVALID
    );

    EXPECT_EQ(
        auth_register(&sys, nullptr, "passenger2", "short", ROLE_PASSENGER),
        FMS_ERR_INVALID
    );

    EXPECT_EQ(
        auth_register(&sys, admin, "user02", "pass1234",
                      static_cast<Role>(999)),
        FMS_ERR_INVALID
    );
}

TEST_F(FmsWorkflowTest, RejectsNullRegistrationArguments) {
    EXPECT_EQ(auth_register(nullptr, nullptr, "user02", "pass1234",
                            ROLE_PASSENGER), FMS_ERR_NULL);

    EXPECT_EQ(auth_register(&sys, nullptr, nullptr, "pass1234",
                            ROLE_PASSENGER), FMS_ERR_NULL);

    EXPECT_EQ(auth_register(&sys, nullptr, "user02", nullptr,
                            ROLE_PASSENGER), FMS_ERR_NULL);
}

TEST_F(FmsWorkflowTest, HandlesLoginSuccessAndFailures) {
    const User* result = nullptr;

    EXPECT_EQ(auth_login(&sys, "passenger1", "pass1234", &result), FMS_OK);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->role, ROLE_PASSENGER);

    result = nullptr;
    EXPECT_EQ(auth_login(&sys, "passenger1", "wrongpass", &result),
              FMS_ERR_AUTH);
    EXPECT_EQ(result, nullptr);

    EXPECT_EQ(auth_login(&sys, "unknownuser", "pass1234", &result),
              FMS_ERR_AUTH);

    EXPECT_EQ(auth_login(&sys, "ab", "pass1234", &result), FMS_ERR_AUTH);
}

TEST_F(FmsWorkflowTest, RejectsNullLoginArguments) {
    const User* result = nullptr;

    EXPECT_EQ(auth_login(nullptr, "admin01", "admin123", &result),
              FMS_ERR_NULL);
    EXPECT_EQ(auth_login(&sys, nullptr, "admin123", &result), FMS_ERR_NULL);
    EXPECT_EQ(auth_login(&sys, "admin01", nullptr, &result), FMS_ERR_NULL);
    EXPECT_EQ(auth_login(&sys, "admin01", "admin123", nullptr), FMS_ERR_NULL);
}

// ---------- Flight tests ----------

TEST_F(FmsWorkflowTest, RejectsUnauthorizedFlightCreation) {
    FlightParams p{};
    std::snprintf(p.id, sizeof(p.id), "%s", "AI202");
    std::snprintf(p.name, sizeof(p.name), "%s", "Air India");
    std::snprintf(p.source, sizeof(p.source), "%s", "MAA");
    std::snprintf(p.destination, sizeof(p.destination), "%s", "DEL");
    p.departure = departure;
    p.seats = 5;
    p.fare = 500.0;

    EXPECT_EQ(flight_add(&sys, passenger, &p), FMS_ERR_PERM);
}

TEST_F(FmsWorkflowTest, RejectsNullFlightCreationArguments) {
    FlightParams p{};

    EXPECT_EQ(flight_add(nullptr, admin, &p), FMS_ERR_NULL);
    EXPECT_EQ(flight_add(&sys, admin, nullptr), FMS_ERR_NULL);
}

TEST_F(FmsWorkflowTest, RejectsDuplicateFlightId) {
    FlightParams p{};
    std::snprintf(p.id, sizeof(p.id), "%s", "AI101");
    std::snprintf(p.name, sizeof(p.name), "%s", "Air India");
    std::snprintf(p.source, sizeof(p.source), "%s", "MAA");
    std::snprintf(p.destination, sizeof(p.destination), "%s", "DEL");
    p.departure = departure;
    p.seats = 5;
    p.fare = 500.0;

    EXPECT_EQ(flight_add(&sys, admin, &p), FMS_ERR_EXISTS);
}

TEST_F(FmsWorkflowTest, RejectsInvalidFlightDetails) {
    FlightParams p{};
    std::snprintf(p.id, sizeof(p.id), "%s", "AI202");
    std::snprintf(p.name, sizeof(p.name), "%s", "Air India");
    std::snprintf(p.source, sizeof(p.source), "%s", "MAA");
    std::snprintf(p.destination, sizeof(p.destination), "%s", "MAA");
    p.departure = departure;
    p.seats = 5;
    p.fare = 500.0;

    EXPECT_EQ(flight_add(&sys, admin, &p), FMS_ERR_INVALID);

    std::snprintf(p.destination, sizeof(p.destination), "%s", "DEL");
    p.seats = 0;
    EXPECT_EQ(flight_add(&sys, admin, &p), FMS_ERR_INVALID);

    p.seats = 5;
    p.fare = -1.0;
    EXPECT_EQ(flight_add(&sys, admin, &p), FMS_ERR_INVALID);
}

TEST_F(FmsWorkflowTest, FindsFlightSeatInformation) {
    int available = -1;
    int total = -1;

    EXPECT_EQ(flight_seat_info(&sys, "AI101", &available, &total), FMS_OK);
    EXPECT_EQ(available, 3);
    EXPECT_EQ(total, 3);

    EXPECT_EQ(flight_seat_info(&sys, "NO999", &available, &total),
              FMS_ERR_NOT_FOUND);
}

TEST_F(FmsWorkflowTest, RejectsInvalidSeatInformationArguments) {
    int available = 0;
    int total = 0;

    EXPECT_EQ(flight_seat_info(nullptr, "AI101", &available, &total),
              FMS_ERR_NULL);
    EXPECT_EQ(flight_seat_info(&sys, nullptr, &available, &total),
              FMS_ERR_NULL);
    EXPECT_EQ(flight_seat_info(&sys, "AI101", nullptr, &total),
              FMS_ERR_NULL);
    EXPECT_EQ(flight_seat_info(&sys, "AI101", &available, nullptr),
              FMS_ERR_NULL);
}

TEST_F(FmsWorkflowTest, PrintsFlightSchedule) {
    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(flight_print_schedule(&sys, out, nullptr), FMS_OK);
    EXPECT_EQ(flight_print_schedule(&sys, out, "DEL"), FMS_OK);

    fclose(out);
}

TEST_F(FmsWorkflowTest, RejectsInvalidScheduleArguments) {
    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(flight_print_schedule(nullptr, out, nullptr), FMS_ERR_NULL);
    EXPECT_EQ(flight_print_schedule(&sys, nullptr, nullptr), FMS_ERR_NULL);
    EXPECT_EQ(flight_print_schedule(&sys, out, "bad"), FMS_ERR_INVALID);

    fclose(out);
}

TEST_F(FmsWorkflowTest, UpdatesFlightNameAndDelay) {
    FlightUpdate u{};

    u.kind = UPD_NAME;
    std::snprintf(u.name, sizeof(u.name), "%s", "New Airline");
    EXPECT_EQ(flight_update(&sys, admin, "AI101", &u), FMS_OK);

    u = {};
    u.kind = UPD_DELAY;
    u.delay_min = 30;
    EXPECT_EQ(flight_update(&sys, admin, "AI101", &u), FMS_OK);

    Flight* f = flight_lookup_locked(&sys, "AI101");
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->status, FL_DELAYED);
    EXPECT_EQ(f->delay_min, 30);
    EXPECT_STREQ(f->name, "New Airline");
}

TEST_F(FmsWorkflowTest, RejectsInvalidFlightUpdates) {
    FlightUpdate u{};

    EXPECT_EQ(flight_update(nullptr, admin, "AI101", &u), FMS_ERR_NULL);
    EXPECT_EQ(flight_update(&sys, admin, nullptr, &u), FMS_ERR_NULL);
    EXPECT_EQ(flight_update(&sys, admin, "AI101", nullptr), FMS_ERR_NULL);
    EXPECT_EQ(flight_update(&sys, passenger, "AI101", &u), FMS_ERR_PERM);
    EXPECT_EQ(flight_update(&sys, admin, "NO999", &u), FMS_ERR_NOT_FOUND);

    u.kind = UPD_DELAY;
    u.delay_min = 0;
    EXPECT_EQ(flight_update(&sys, admin, "AI101", &u), FMS_ERR_INVALID);
}

TEST_F(FmsWorkflowTest, TakesAndReleasesSeats) {
    unsigned seats[2] = {0, 0};
    Flight f{};
    f.seat_map.total = 2;
    f.seat_map.available = 2;
    f.seat_map.seats = seats;

    EXPECT_EQ(flight_take_seat(&f, 1000), 1);
    EXPECT_EQ(flight_take_seat(&f, 1001), 2);
    EXPECT_EQ(f.seat_map.available, 0);
    EXPECT_EQ(flight_take_seat(&f, 1002), 0);

    flight_release_seat(&f, 1);
    EXPECT_EQ(f.seat_map.available, 1);

    flight_release_seat(&f, 1);
    EXPECT_EQ(f.seat_map.available, 1);

    flight_release_seat(&f, 99);
    flight_release_seat(nullptr, 1);
    EXPECT_EQ(flight_take_seat(nullptr, 1003), 0);
}

// ---------- Booking tests ----------

TEST_F(FmsWorkflowTest, CreatesAndReadsBooking) {
    unsigned id = createBooking();
    ASSERT_NE(id, 0u);

    BookingStatus status = BK_REJECTED;
    EXPECT_EQ(booking_get_status(&sys, passenger, id, &status), FMS_OK);
    EXPECT_EQ(status, BK_PENDING);
}

TEST_F(FmsWorkflowTest, RejectsInvalidBookingRequests) {
    BookingForm f = makeBookingForm();
    unsigned id = 0;

    EXPECT_EQ(booking_request(nullptr, passenger, &f, &id), FMS_ERR_NULL);
    EXPECT_EQ(booking_request(&sys, nullptr, &f, &id), FMS_ERR_NULL);
    EXPECT_EQ(booking_request(&sys, passenger, nullptr, &id), FMS_ERR_NULL);
    EXPECT_EQ(booking_request(&sys, passenger, &f, nullptr), FMS_ERR_NULL);

    EXPECT_EQ(booking_request(&sys, admin, &f, &id), FMS_ERR_PERM);

    std::snprintf(f.flight_id, sizeof(f.flight_id), "%s", "NO999");
    EXPECT_EQ(booking_request(&sys, passenger, &f, &id), FMS_ERR_NOT_FOUND);
}

TEST_F(FmsWorkflowTest, RejectsBookingFormThatDoesNotMatchFlight) {
    BookingForm f = makeBookingForm();
    unsigned id = 0;

    std::snprintf(f.destination, sizeof(f.destination), "%s", "BOM");
    EXPECT_EQ(booking_request(&sys, passenger, &f, &id), FMS_ERR_INVALID);
}

TEST_F(FmsWorkflowTest, ConfirmsBookingAndPrintsTicket) {
    unsigned id = createBooking();
    ASSERT_NE(id, 0u);

    EXPECT_EQ(booking_confirm(&sys, clerk, id), FMS_OK);

    BookingStatus status = BK_PENDING;
    EXPECT_EQ(booking_get_status(&sys, passenger, id, &status), FMS_OK);
    EXPECT_EQ(status, BK_CONFIRMED);

    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(booking_print_ticket(&sys, passenger, id, out), FMS_OK);
    fclose(out);
}

TEST_F(FmsWorkflowTest, RejectsBookingConfirmationErrors) {
    EXPECT_EQ(booking_confirm(nullptr, clerk, 1000), FMS_ERR_NULL);
    EXPECT_EQ(booking_confirm(&sys, nullptr, 1000), FMS_ERR_NULL);
    EXPECT_EQ(booking_confirm(&sys, passenger, 1000), FMS_ERR_PERM);
    EXPECT_EQ(booking_confirm(&sys, clerk, 9999), FMS_ERR_NOT_FOUND);

    unsigned id = createBooking();
    ASSERT_NE(id, 0u);
    ASSERT_EQ(booking_confirm(&sys, clerk, id), FMS_OK);

    EXPECT_EQ(booking_confirm(&sys, clerk, id), FMS_ERR_STATE);
}

TEST_F(FmsWorkflowTest, RejectsPendingBookingAndReleasesSeat) {
    unsigned id = createBooking();
    ASSERT_NE(id, 0u);

    EXPECT_EQ(booking_reject(&sys, clerk, id), FMS_OK);

    BookingStatus status = BK_PENDING;
    EXPECT_EQ(booking_get_status(&sys, passenger, id, &status), FMS_OK);
    EXPECT_EQ(status, BK_REJECTED);

    int available = 0;
    int total = 0;
    EXPECT_EQ(flight_seat_info(&sys, "AI101", &available, &total), FMS_OK);
    EXPECT_EQ(available, 3);
}

TEST_F(FmsWorkflowTest, RejectsBookingRejectionErrors) {
    EXPECT_EQ(booking_reject(nullptr, clerk, 1000), FMS_ERR_NULL);
    EXPECT_EQ(booking_reject(&sys, nullptr, 1000), FMS_ERR_NULL);
    EXPECT_EQ(booking_reject(&sys, passenger, 1000), FMS_ERR_PERM);
    EXPECT_EQ(booking_reject(&sys, clerk, 9999), FMS_ERR_NOT_FOUND);
}

TEST_F(FmsWorkflowTest, CancelsOwnBooking) {
    unsigned id = createBooking();
    ASSERT_NE(id, 0u);

    CancelForm f = makeCancelForm(id);
    double refund = -1.0;

    EXPECT_EQ(booking_cancel(&sys, passenger, &f, &refund), FMS_OK);
    EXPECT_EQ(refund, 0.0);

    BookingStatus status = BK_PENDING;
    EXPECT_EQ(booking_get_status(&sys, passenger, id, &status), FMS_OK);
    EXPECT_EQ(status, BK_CANCELLED);
}

TEST_F(FmsWorkflowTest, RejectsInvalidCancellationArguments) {
    CancelForm f = makeCancelForm(1000);
    double refund = 0.0;

    EXPECT_EQ(booking_cancel(nullptr, passenger, &f, &refund), FMS_ERR_NULL);
    EXPECT_EQ(booking_cancel(&sys, nullptr, &f, &refund), FMS_ERR_NULL);
    EXPECT_EQ(booking_cancel(&sys, passenger, nullptr, &refund), FMS_ERR_NULL);
    EXPECT_EQ(booking_cancel(&sys, passenger, &f, nullptr), FMS_ERR_NULL);
}

TEST_F(FmsWorkflowTest, RejectsCancellationForUnknownBooking) {
    CancelForm f = makeCancelForm(9999);
    double refund = 0.0;

    EXPECT_EQ(booking_cancel(&sys, passenger, &f, &refund),
              FMS_ERR_NOT_FOUND);
}

TEST_F(FmsWorkflowTest, RejectsTicketForPendingBooking) {
    unsigned id = createBooking();
    ASSERT_NE(id, 0u);

    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(booking_print_ticket(&sys, passenger, id, out), FMS_ERR_STATE);
    fclose(out);
}

TEST_F(FmsWorkflowTest, PrintsBookingStatusAndList) {
    unsigned id = createBooking();
    ASSERT_NE(id, 0u);

    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(booking_print_status(&sys, passenger, id, out), FMS_OK);
    EXPECT_EQ(booking_print_list(&sys, passenger, false, out), FMS_OK);
    EXPECT_EQ(booking_print_list(&sys, passenger, true, out), FMS_OK);

    fclose(out);
}

TEST_F(FmsWorkflowTest, RejectsInvalidBookingViewArguments) {
    BookingStatus status = BK_PENDING;
    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(booking_get_status(nullptr, passenger, 1000, &status),
              FMS_ERR_NULL);
    EXPECT_EQ(booking_get_status(&sys, nullptr, 1000, &status), FMS_ERR_NULL);
    EXPECT_EQ(booking_get_status(&sys, passenger, 1000, nullptr),
              FMS_ERR_NULL);

    EXPECT_EQ(booking_print_status(nullptr, passenger, 1000, out),
              FMS_ERR_NULL);
    EXPECT_EQ(booking_print_status(&sys, passenger, 1000, nullptr),
              FMS_ERR_NULL);
    EXPECT_EQ(booking_print_list(&sys, passenger, false, nullptr),
              FMS_ERR_NULL);
    EXPECT_EQ(booking_print_ticket(&sys, passenger, 1000, nullptr),
              FMS_ERR_NULL);

    fclose(out);
}

TEST_F(FmsWorkflowTest, CancellingFlightCancelsItsBooking) {
    unsigned id = createBooking();
    ASSERT_NE(id, 0u);

    FlightUpdate u{};
    u.kind = UPD_CANCEL;
    EXPECT_EQ(flight_update(&sys, admin, "AI101", &u), FMS_OK);

    BookingStatus status = BK_PENDING;
    EXPECT_EQ(booking_get_status(&sys, passenger, id, &status), FMS_OK);
    EXPECT_EQ(status, BK_CANCELLED_BY_AIRLINE);
}

TEST_F(FmsWorkflowTest, CalculatesRefundAtThreshold) {
    time_t now = time(nullptr);

    EXPECT_DOUBLE_EQ(refund_calculate(100.0, now + 60 * 60, now), 0.0);
    EXPECT_NEAR(refund_calculate(100.0, now + 120 * 60, now), 100.0, 0.01);
}

// ---------- Report tests ----------

TEST_F(FmsWorkflowTest, GeneratesFlightReport) {
    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(report_flight(&sys, admin, "AI101", out), FMS_OK);
    fclose(out);
}

TEST_F(FmsWorkflowTest, HandlesFlightReportErrors) {
    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(report_flight(nullptr, admin, "AI101", out), FMS_ERR_NULL);
    EXPECT_EQ(report_flight(&sys, admin, nullptr, out), FMS_ERR_NULL);
    EXPECT_EQ(report_flight(&sys, passenger, "AI101", out), FMS_ERR_PERM);
    EXPECT_EQ(report_flight(&sys, admin, "NO999", out), FMS_ERR_NOT_FOUND);
    EXPECT_EQ(report_flight(&sys, admin, "bad", out), FMS_ERR_INVALID);
    EXPECT_EQ(report_flight(&sys, admin, "AI101", nullptr), FMS_ERR_NULL);

    fclose(out);
}

TEST_F(FmsWorkflowTest, GeneratesReservationChart) {
    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(report_reservation_chart(&sys, admin, out), FMS_OK);
    fclose(out);
}

TEST_F(FmsWorkflowTest, HandlesReservationChartErrors) {
    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(report_reservation_chart(nullptr, admin, out), FMS_ERR_NULL);
    EXPECT_EQ(report_reservation_chart(&sys, passenger, out), FMS_ERR_PERM);
    EXPECT_EQ(report_reservation_chart(&sys, admin, nullptr), FMS_ERR_NULL);

    fclose(out);
}

TEST_F(FmsWorkflowTest, GeneratesScheduleReport) {
    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(report_schedule(&sys, admin, 7, out), FMS_OK);
    EXPECT_EQ(report_schedule(&sys, admin, 30, out), FMS_OK);
    fclose(out);
}

TEST_F(FmsWorkflowTest, RejectsInvalidScheduleReportArguments) {
    FILE* out = tmpfile();
    ASSERT_NE(out, nullptr);

    EXPECT_EQ(report_schedule(nullptr, admin, 7, out), FMS_ERR_NULL);
    EXPECT_EQ(report_schedule(&sys, passenger, 7, out), FMS_ERR_PERM);
    EXPECT_EQ(report_schedule(&sys, admin, 0, out), FMS_ERR_INVALID);
    EXPECT_EQ(report_schedule(&sys, admin, 367, out), FMS_ERR_INVALID);
    EXPECT_EQ(report_schedule(&sys, admin, 7, nullptr), FMS_ERR_NULL);

    fclose(out);
}

// ---------- Worker-pool tests ----------

TEST_F(FmsWorkflowTest, RejectsInvalidWorkerPoolInitialization) {
    WorkerPool pool{};

    EXPECT_EQ(pool_init(nullptr, &sys, 2), FMS_ERR_NULL);
    EXPECT_EQ(pool_init(&pool, nullptr, 2), FMS_ERR_NULL);
    EXPECT_EQ(pool_init(&pool, &sys, 0), FMS_ERR_INVALID);
    EXPECT_EQ(pool_init(&pool, &sys, 33), FMS_ERR_INVALID);
}

TEST_F(FmsWorkflowTest, InitializesWaitsAndDestroysWorkerPool) {
    WorkerPool pool{};

    ASSERT_EQ(pool_init(&pool, &sys, 2), FMS_OK);
    pool_wait(&pool);
    pool_destroy(&pool);

    EXPECT_EQ(pool.threads, nullptr);
}

TEST_F(FmsWorkflowTest, SubmitsBookingJobToWorkerPool) {
    WorkerPool pool{};
    ASSERT_EQ(pool_init(&pool, &sys, 2), FMS_OK);

    Job job{};
    job.type = JOB_BOOK;
    job.actor = passenger;
    job.form = makeBookingForm();

    EXPECT_EQ(pool_submit(&pool, &job), FMS_OK);
    pool_wait(&pool);

    EXPECT_EQ(job.result, FMS_OK);
    EXPECT_NE(job.booking_id, 0u);

    pool_destroy(&pool);
}

TEST_F(FmsWorkflowTest, RejectsNullWorkerPoolSubmission) {
    WorkerPool pool{};
    Job job{};

    EXPECT_EQ(pool_submit(nullptr, &job), FMS_ERR_NULL);
    EXPECT_EQ(pool_submit(&pool, nullptr), FMS_ERR_NULL);
}

TEST_F(FmsWorkflowTest, SafelyHandlesNullWorkerPoolWaitAndDestroy) {
    EXPECT_NO_THROW(pool_wait(nullptr));
    EXPECT_NO_THROW(pool_destroy(nullptr));
}
