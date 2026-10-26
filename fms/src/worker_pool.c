#include "fms.h"
#include <stdlib.h>
#include <string.h>

/* Rule to avoid deadlock: the pool lock is NEVER held while a job runs,
 * so it can never be held at the same time as sys->lock. */

static void run_job(WorkerPool *p, Job *j)
{
    if (j->type == JOB_BOOK) {
        j->result = booking_request(p->sys, j->actor, &j->form, &j->booking_id);
    } else {
        j->result = booking_cancel(p->sys, j->actor, &j->cancel, &j->refund);
    }
}

static void *worker_main(void *arg)
{
    WorkerPool *p = (WorkerPool *)arg;

    for (;;) {
        Job *j;

        pthread_mutex_lock(&p->lock);
        while (p->head == NULL && !p->stop) {
            pthread_cond_wait(&p->not_empty, &p->lock);     /* sleep until work arrives */
        }
        if (p->head == NULL) {                              /* stop requested, queue empty */
            pthread_mutex_unlock(&p->lock);
            break;
        }
        j = p->head;
        p->head = j->next;
        if (p->head == NULL) p->tail = NULL;
        j->next = NULL;
        pthread_mutex_unlock(&p->lock);

        run_job(p, j);                                      /* work happens outside the lock */

        pthread_mutex_lock(&p->lock);
        p->pending--;
        if (p->pending == 0) pthread_cond_broadcast(&p->all_done);
        pthread_mutex_unlock(&p->lock);
    }
    return NULL;
}

FmsStatus pool_init(WorkerPool *p, FmsSystem *sys, int nthreads)
{
    int i;

    if (p == NULL || sys == NULL) return FMS_ERR_NULL;
    if (nthreads < 1 || nthreads > 32) return FMS_ERR_INVALID;
    memset(p, 0, sizeof *p);
    p->sys = sys;
    p->threads = calloc((size_t)nthreads, sizeof *p->threads);
    if (p->threads == NULL) return FMS_ERR_MEM;
    if (pthread_mutex_init(&p->lock, NULL) != 0) {
        free(p->threads);
        p->threads = NULL;
        return FMS_ERR_MEM;
    }
    (void)pthread_cond_init(&p->not_empty, NULL);
    (void)pthread_cond_init(&p->all_done, NULL);

    for (i = 0; i < nthreads; i++) {
        if (pthread_create(&p->threads[i], NULL, worker_main, p) != 0) {
            p->nthreads = i;                                /* only join the ones we started */
            pool_destroy(p);
            return FMS_ERR_MEM;
        }
    }
    p->nthreads = nthreads;
    log_write("INFO", "pool", "worker pool started with %d threads", nthreads);
    return FMS_OK;
}

FmsStatus pool_submit(WorkerPool *p, Job *job)
{
    FmsStatus st = FMS_OK;

    if (p == NULL || job == NULL) return FMS_ERR_NULL;
    job->next   = NULL;
    job->result = FMS_ERR_STATE;
    pthread_mutex_lock(&p->lock);
    if (p->stop) {
        st = FMS_ERR_STATE;
    } else {
        if (p->tail == NULL) p->head = job;
        else p->tail->next = job;
        p->tail = job;
        p->pending++;
        pthread_cond_signal(&p->not_empty);                 /* wake one worker */
    }
    pthread_mutex_unlock(&p->lock);
    return st;
}

void pool_wait(WorkerPool *p)
{
    if (p == NULL) return;
    pthread_mutex_lock(&p->lock);
    while (p->pending > 0) {
        pthread_cond_wait(&p->all_done, &p->lock);
    }
    pthread_mutex_unlock(&p->lock);
}

void pool_destroy(WorkerPool *p)
{
    int i;

    if (p == NULL || p->threads == NULL) return;
    pthread_mutex_lock(&p->lock);
    p->stop = true;
    pthread_cond_broadcast(&p->not_empty);                  /* wake everyone so they can exit */
    pthread_mutex_unlock(&p->lock);

    for (i = 0; i < p->nthreads; i++) {
        (void)pthread_join(p->threads[i], NULL);            /* wait for each worker */
    }
    free(p->threads);
    p->threads = NULL;
    (void)pthread_cond_destroy(&p->not_empty);
    (void)pthread_cond_destroy(&p->all_done);
    (void)pthread_mutex_destroy(&p->lock);
    log_write("INFO", "pool", "worker pool stopped");
}
