#include "pw.h"
#include "pw-sched.h"

#include <stdlib.h>
#include <rl/err.h>
#include <rl/array.h>
#include <string.h>
#include <unistd.h>

void pw_init(Pw *pw, uint jobs) {
    ASSERT_ARG(pw);
    ASSERT_ARG(jobs);
    pw->sched.jobs = jobs = jobs;
    array_resize(pw->tasks, jobs);
}

bool pw_is_busy(Pw *pw) {
    pthread_mutex_lock(&pw->queue.mutex);
    size_t len = array_len(pw->queue.data);
    pthread_mutex_unlock(&pw->queue.mutex);
    pthread_mutex_lock(&pw->sched.mutex);
    uint ready = pw->sched.ready;
    pthread_mutex_unlock(&pw->sched.mutex);
    return len || !(ready == pw->sched.jobs);
}

void pw_dispatch(Pw *pw) {
    int err = 0;
    for(size_t i = 0; i < pw->sched.jobs; ++i) {
        Pw_Task *task = array_it(pw->tasks, i);
        task->pw = pw;
        task->id = i;
        err = pthread_create(&task->thread, 0, pw_task, task);
        usleep(1000);
    }
}

void pw_cancel(Pw *pw) {
    pw->sched.cancel = true;
    pthread_mutex_lock(&pw->sched.wait);
    for(uint i = 0; i < pw->sched.jobs; ++i) {
        pthread_cond_signal(&pw->sched.cond);
    }
    pthread_mutex_unlock(&pw->sched.wait);
}

void pw_free(Pw *pw) {
    pw_cancel(pw);
    for(uint i = 0; i < pw->sched.jobs; ++i) {
        Pw_Task *task = array_it(pw->tasks, i);
        pthread_join(task->thread, 0);
    }
    array_free(pw->queue);
    array_free(pw->tasks);
    memset(pw, 0, sizeof(*pw));
}

