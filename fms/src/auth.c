#include "fms.h"
#include <stdlib.h>
#include <string.h>

/* Role Based Access Control (RBAC): one row per role, one column per action. */
bool auth_can(const User *u, Action a)
{
    static const bool table[ROLE_COUNT][ACT_COUNT] = {
        /*            FLIGHTS STAFF  REQ    CONF   CXL_ANY CXL_OWN VIEW_ALL REPORTS */
        /* PASSENGER */ {false, false, true,  false, false,  true,   false,   false},
        /* CLERK     */ {false, false, true,  true,  true,   false,  true,    false},
        /* ADMIN     */ {true,  true,  false, false, false,  false,  true,    true },
    };

    if (u == NULL) return false;
    if ((unsigned)u->role >= (unsigned)ROLE_COUNT) return false;
    if ((unsigned)a >= (unsigned)ACT_COUNT) return false;
    return table[u->role][a];
}

/* linear search of the user linked list (caller holds the lock) */
static User *find_user_locked(FmsSystem *sys, const char *username)
{
    User *u;
    for (u = sys->users; u != NULL; u = u->next) {
        if (strcmp(u->username, username) == 0) return u;
    }
    return NULL;
}

static FmsStatus add_user_locked(FmsSystem *sys, const char *username,
                                 const char *password, Role role)
{
    User *u;

    if (find_user_locked(sys, username) != NULL) return FMS_ERR_EXISTS;
    u = calloc(1, sizeof *u);
    if (u == NULL) return FMS_ERR_MEM;
    u->id = sys->next_user_id++;
    snprintf(u->username, sizeof u->username, "%s", username);
    u->pwd_hash = hash_password(username, password);
    u->role     = role;
    u->next     = sys->users;                  /* insert at head of the list */
    sys->users  = u;
    return FMS_OK;
}

/* Only works when there are no users at all: creates the very first admin. */
FmsStatus auth_create_first_admin(FmsSystem *sys, const char *username, const char *password)
{
    FmsStatus st;

    if (sys == NULL || username == NULL || password == NULL) return FMS_ERR_NULL;
    if (!valid_username(username) || !valid_password(password)) return FMS_ERR_INVALID;
    pthread_mutex_lock(&sys->lock);
    if (sys->users != NULL) {
        st = FMS_ERR_STATE;
    } else {
        st = add_user_locked(sys, username, password, ROLE_ADMIN);
    }
    pthread_mutex_unlock(&sys->lock);
    return st;
}

/*
 * Passengers may register themselves (actor == NULL).
 * Creating a CLERK or ADMIN account needs an ADMIN actor.
 */
FmsStatus auth_register(FmsSystem *sys, const User *actor, const char *username,
                        const char *password, Role role)
{
    FmsStatus st;

    if (sys == NULL || username == NULL || password == NULL) return FMS_ERR_NULL;
    if ((unsigned)role >= (unsigned)ROLE_COUNT) return FMS_ERR_INVALID;
    if (role != ROLE_PASSENGER && !auth_can(actor, ACT_CREATE_STAFF)) return FMS_ERR_PERM;
    if (!valid_username(username) || !valid_password(password)) return FMS_ERR_INVALID;

    pthread_mutex_lock(&sys->lock);
    st = add_user_locked(sys, username, password, role);
    pthread_mutex_unlock(&sys->lock);

    if (st == FMS_OK) {
        log_write("INFO", "auth", "account created user=%s role=%s", username, role_name(role));
    }
    return st;
}

FmsStatus auth_login(FmsSystem *sys, const char *username, const char *password,
                     const User **out)
{
    User     *u;
    FmsStatus st = FMS_ERR_AUTH;

    if (sys == NULL || username == NULL || password == NULL || out == NULL) return FMS_ERR_NULL;
    *out = NULL;
    if (!valid_username(username)) {
        log_write("WARN", "auth", "login rejected: bad username format");
        return FMS_ERR_AUTH;                   /* same message for every failure */
    }

    pthread_mutex_lock(&sys->lock);
    u = find_user_locked(sys, username);
    if (u != NULL && u->pwd_hash == hash_password(username, password)) {
        *out = u;                              /* users are never freed before shutdown */
        st = FMS_OK;
    }
    pthread_mutex_unlock(&sys->lock);

    if (st == FMS_OK) {
        log_write("INFO", "auth", "login ok user=%s", username);
    } else {
        log_write("WARN", "auth", "login failed user=%s", username);
    }
    return st;
}
