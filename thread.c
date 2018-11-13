#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"
#include <math.h>

struct wait_que_object {
    Tid id;
    struct wait_que_object* next;
};

/* This is the wait queue structure */
struct wait_queue {
    struct wait_que_object* head;
};

/* This is the thread control block */
struct thread {
    Tid id;
    ucontext_t context;
    struct wait_queue* wq;
    volatile bool setcontext_called;
    void* sp;
    bool existence;
    bool not_ready;
};
Tid asd;

struct exit_que_object {
    Tid exit_id;
    struct exit_que_object* next;
};

Tid ID_to_be_destroyed;
struct exit_que_object* eq_head;

struct thread thread_list[THREAD_MAX_THREADS];
unsigned num_of_threads = 0;

struct thread* currently_running;

struct ready_queue_object {
    unsigned thread_id;
    struct ready_queue_object* next;
};



struct ready_queue_object* rq_head;


void add_to_ready_que(Tid id);
void remove_from_ready_que(Tid id);
void thread_stub(void (*thread_main)(void *), void *arg);
void thread_destroy(Tid id);

void
thread_init(void) {

    int old_status;
    old_status = interrupts_set(0);

    rq_head = NULL;


    thread_list[0].existence = true;
    thread_list[0].id = 0;
    thread_list[0].setcontext_called = false;
    thread_list[0].sp = NULL;
    thread_list[0].wq = NULL;

    for (unsigned i = 1; i < THREAD_MAX_THREADS; ++i) {
        thread_list[i].existence = false;
        thread_list[i].not_ready = false;
        thread_list[i].wq = NULL;
    }

    currently_running = &thread_list[0];
    num_of_threads = 1;

    interrupts_set(old_status);

    return;

}

Tid
thread_id() {
    return currently_running->id;
}

Tid
thread_create(void (*fn) (void *), void *parg) {


    int old_status;
    old_status = interrupts_set(0);

    unsigned usable_id;
    bool id_found = false;
    if (num_of_threads >= THREAD_MAX_THREADS) {
        return THREAD_NOMORE;
    } else {
        for (unsigned i = 0; i < THREAD_MAX_THREADS; ++i) {


            if (thread_list[i].existence == false) {
                thread_list[i].not_ready = false;
                usable_id = i;
                id_found = true;
                break;

            }
        }
    }
    if (!id_found) {
        interrupts_set(old_status);
        return THREAD_NOMORE;
    }

    if (thread_list[usable_id].wq == NULL) {
        thread_list[usable_id].wq = wait_queue_create();
    }

    thread_list[usable_id].sp = malloc(THREAD_MIN_STACK + 8);

    if (thread_list[usable_id].sp == NULL) {
        interrupts_set(old_status);
        return THREAD_NOMEMORY;
    }


    getcontext(&(thread_list[usable_id].context));

    thread_list[usable_id].existence = true;
    thread_list[usable_id].id = usable_id;
    thread_list[usable_id].setcontext_called = false;

    thread_list[usable_id].context.uc_mcontext.gregs[REG_RIP] = (long long int) (void*) thread_stub;
    thread_list[usable_id].context.uc_mcontext.gregs[REG_RDI] = (long long int) (void*) fn;
    thread_list[usable_id].context.uc_mcontext.gregs[REG_RSI] = (long long int) (void*) parg;

    thread_list[usable_id].context.uc_mcontext.gregs[REG_RSP] = (long long int) (void*) thread_list[usable_id].sp + (THREAD_MIN_STACK + 8);


    add_to_ready_que(usable_id);

    num_of_threads++;

    interrupts_set(old_status);
    return usable_id;
}

Tid
thread_yield(Tid want_tid) {

    asd = want_tid;
    int old_status;
    old_status = interrupts_set(0);
    Tid selected;


    struct ready_queue_object* temp = rq_head;
    struct exit_que_object* ex_que_ptr;

    currently_running->setcontext_called = false;




    if (want_tid == THREAD_SELF || currently_running->id == want_tid) {
        getcontext(&(currently_running->context));



        if (!(currently_running->setcontext_called)) {
            currently_running->setcontext_called = true;

            setcontext(&(currently_running->context));
        }
        currently_running->setcontext_called = false;



        ex_que_ptr = eq_head;
        while (ex_que_ptr != NULL) {
            if (ex_que_ptr->exit_id != currently_running->id) {
                thread_destroy(ex_que_ptr->exit_id);
            }
            ex_que_ptr = ex_que_ptr->next;
        }

        interrupts_set(old_status);
        return currently_running->id;
    } else if (want_tid == THREAD_ANY) {
        if (rq_head == NULL) {



            ex_que_ptr = eq_head;
            while (ex_que_ptr != NULL) {
                if (ex_que_ptr->exit_id != currently_running->id) {
                    thread_destroy(ex_que_ptr->exit_id);
                }
                ex_que_ptr = ex_que_ptr->next;
            }


            interrupts_set(old_status);
            return THREAD_NONE;
        } else {

            getcontext(&(currently_running->context));

            if (!(currently_running->setcontext_called)) {
                currently_running->setcontext_called = true;

                if (!(currently_running->not_ready)) {
                    add_to_ready_que(currently_running->id);
                }

                currently_running = &(thread_list[rq_head->thread_id]);
                selected = rq_head->thread_id;
                rq_head = rq_head->next;
                temp->next = NULL;
                free(temp);

                setcontext(&(currently_running->context));
            }

            currently_running->setcontext_called = false;


            ex_que_ptr = eq_head;
            while (ex_que_ptr != NULL) {
                if (ex_que_ptr->exit_id != currently_running->id) {
                    thread_destroy(ex_que_ptr->exit_id);
                }
                ex_que_ptr = ex_que_ptr->next;
            }


            interrupts_set(old_status);
            return selected;

        }
    } else {

        struct ready_queue_object* follower = NULL;

        while (temp != NULL) {

            if (temp->thread_id == want_tid) {

                if (follower == NULL) {
                    //means want id is head
                    getcontext(&(currently_running->context));



                    if (!(currently_running->setcontext_called)) {
                        currently_running->setcontext_called = true;
                        if (!(currently_running->not_ready)) {
                            add_to_ready_que(currently_running->id);
                        }
                        currently_running = &(thread_list[rq_head->thread_id]);

                        //remove want_tid thread from the ready queue
                        rq_head = rq_head->next;
                        free(temp);

                        setcontext(&(currently_running->context));
                    }
                    currently_running->setcontext_called = false;

                    ex_que_ptr = eq_head;
                    while (ex_que_ptr != NULL) {
                        if (ex_que_ptr->exit_id != currently_running->id) {
                            thread_destroy(ex_que_ptr->exit_id);
                        }
                        ex_que_ptr = ex_que_ptr->next;
                    }


                    interrupts_set(old_status);
                    return want_tid;
                } else {
                    follower->next = temp->next;
                    getcontext(&(currently_running->context));


                    if (!(currently_running->setcontext_called)) {
                        currently_running->setcontext_called = true;
                        if (!(currently_running->not_ready)) {
                            add_to_ready_que(currently_running->id);
                        }
                        currently_running = &(thread_list[temp->thread_id]);

                        temp->next = NULL;
                        free(temp);

                        setcontext(&(currently_running->context));
                    }
                    currently_running->setcontext_called = false;



                    ex_que_ptr = eq_head;
                    while (ex_que_ptr != NULL) {
                        if (ex_que_ptr->exit_id != currently_running->id) {
                            thread_destroy(ex_que_ptr->exit_id);
                        }
                        ex_que_ptr = ex_que_ptr->next;
                    }


                    interrupts_set(old_status);
                    return want_tid;
                }
            }

            follower = temp;
            temp = temp->next;
        }
    }

    if (rq_head != NULL) {
        getcontext(&(currently_running->context));


        if (!(currently_running->setcontext_called)) {
            currently_running->setcontext_called = true;
            if (!(currently_running->not_ready)) {
                add_to_ready_que(currently_running->id);
            }
            currently_running = &(thread_list[rq_head->thread_id]);

            temp = rq_head;
            rq_head = rq_head->next;
            temp->next = NULL;
            free(temp);

            setcontext(&(currently_running->context));
        }
        currently_running->setcontext_called = false;




        ex_que_ptr = eq_head;
        while (ex_que_ptr != NULL) {
            if (ex_que_ptr->exit_id != currently_running->id) {
                thread_destroy(ex_que_ptr->exit_id);
            }
            ex_que_ptr = ex_que_ptr->next;
        }

        interrupts_set(old_status);
        return THREAD_INVALID;
    } else {
        if (want_tid == THREAD_ANY) {
            interrupts_set(old_status);


            ex_que_ptr = eq_head;
            while (ex_que_ptr != NULL) {
                if (ex_que_ptr->exit_id != currently_running->id) {
                    thread_destroy(ex_que_ptr->exit_id);
                }
                ex_que_ptr = ex_que_ptr->next;
            }


            return THREAD_NONE;
        } else {




            ex_que_ptr = eq_head;
            while (ex_que_ptr != NULL) {
                if (ex_que_ptr->exit_id != currently_running->id) {
                    thread_destroy(ex_que_ptr->exit_id);
                }
                ex_que_ptr = ex_que_ptr->next;
            }

            interrupts_set(old_status);
            return THREAD_INVALID;
        }
    }

}

Tid
thread_exit() {


    int old_status;
    old_status = interrupts_set(0);

    unsigned woken_ups = 0;
    Tid returnVal;



    if (currently_running->id == 0) {
        if (rq_head == NULL) {
            interrupts_set(old_status);
            return THREAD_NONE;
        }
    }


    remove_from_ready_que(currently_running->id);

    currently_running->not_ready = true;
    currently_running->existence = false;
    currently_running->setcontext_called = false;


    struct exit_que_object* thread_to_be_exited = (struct exit_que_object*) malloc(sizeof (struct exit_que_object));
    thread_to_be_exited->exit_id = currently_running->id;
    thread_to_be_exited->next = NULL;
    if (eq_head == NULL) {
        eq_head = thread_to_be_exited;
    } else {

        struct exit_que_object* temp = eq_head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = thread_to_be_exited;
    }
start:
    returnVal = thread_yield(THREAD_ANY);

    woken_ups = 0;

    if (returnVal == THREAD_NONE) {
        woken_ups = thread_wakeup(currently_running->wq, 1);

        if (woken_ups != 0) {
            goto start;
        }
    }


    free(thread_list[thread_to_be_exited->exit_id].sp);

    thread_list[thread_to_be_exited->exit_id].sp = NULL;
    thread_list[thread_to_be_exited->exit_id].existence = false;
    thread_list[thread_to_be_exited->exit_id].setcontext_called = false;
    interrupts_set(old_status);
    free(thread_to_be_exited);
    return returnVal;

}

Tid
thread_kill(Tid tid) {

    int old_status;
    old_status = interrupts_set(0);

    if (tid < 0 || tid == currently_running->id || tid >= THREAD_MAX_THREADS) {
        interrupts_set(old_status);
        return THREAD_INVALID;
    } else if (!(thread_list[tid].existence)) {
        interrupts_set(old_status);
        return THREAD_INVALID;
    } else {
        remove_from_ready_que(tid);
        thread_list[tid].not_ready = true;
        thread_list[tid].existence = false;
        thread_list[tid].setcontext_called = false;

        struct exit_que_object* thread_to_be_exited = (struct exit_que_object*) malloc(sizeof (struct exit_que_object));

        thread_to_be_exited->exit_id = tid;
        thread_to_be_exited->next = NULL;

        if (eq_head == NULL) {
            eq_head = thread_to_be_exited;
        } else {

            struct exit_que_object* temp = eq_head;
            while (temp->next != NULL) {
                temp = temp->next;
            }
            temp->next = thread_to_be_exited;
        }

        interrupts_set(old_status);
        return tid;
    }
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create() {

    int old_status;
    old_status = interrupts_set(0);

    struct wait_queue *wq;

    wq = (struct wait_queue*) malloc(sizeof (struct wait_queue));
    assert(wq);

    wq->head = NULL;

    interrupts_set(old_status);
    return wq;
}

void
wait_queue_destroy(struct wait_queue *wq) {

    int old_status;
    old_status = interrupts_set(0);

    assert(wq->head == NULL);

    free(wq);

    wq = NULL;

    interrupts_set(old_status);
    return;

}

Tid
thread_sleep(struct wait_queue *queue) {


    int old_status;
    old_status = interrupts_set(0);
    assert(interrupts_enabled() == 0);
    int returnVal;


    if (queue == NULL) {
        interrupts_set(old_status);
        return THREAD_INVALID;
    }

    if (rq_head == NULL) {

        interrupts_set(old_status);
        return THREAD_NONE;
    } else {
        struct wait_que_object* object = (struct wait_que_object*) malloc(sizeof (struct wait_que_object));
        assert(object);
        object->id = currently_running->id;
        object->next = NULL;

        if (queue->head == NULL) {
            queue->head = object;
        } else {
            struct wait_que_object* temp = queue->head;
            while (temp->next != NULL) {
                temp = temp->next;
            }
            temp->next = object;
        }
        currently_running->not_ready = true;
        returnVal = thread_yield(THREAD_ANY);
        interrupts_set(old_status);
        return returnVal;

    }
    interrupts_set(old_status);
    return THREAD_INVALID;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all) {

    int old_status;
    old_status = interrupts_set(0);



    if (queue == NULL) {
        interrupts_set(old_status);
        return 0;
    }

    if (queue->head == NULL) {
        interrupts_set(old_status);
        return 0;
    } else {

        int woken_ups = 0;


        if (all) {
            //wake all up
            struct wait_que_object* temp = queue->head;
            struct wait_que_object* prev = NULL;


            while (temp != NULL) {

                thread_list[temp->id].not_ready = false;
                add_to_ready_que(temp->id);
                woken_ups++;
                prev = temp;
                temp = temp->next;
                free(prev);
            }

            queue->head = NULL;
            interrupts_set(old_status);
            return woken_ups;

        } else {
            //wake up one thread

            struct wait_que_object* temp = queue->head;
            queue->head = queue->head->next;
            add_to_ready_que(temp->id);
            thread_list[temp->id].not_ready = false;
            free(temp);
            woken_ups++;
            interrupts_set(old_status);
            return woken_ups;
        }

    }
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid) {



    int old_status;
    old_status = interrupts_set(0);



    if (tid < 0 || tid == currently_running->id || !(thread_list[tid].existence) || tid == 0) {
        interrupts_set(old_status);
        return THREAD_INVALID;
    } else {

        thread_sleep(thread_list[tid].wq);
        interrupts_set(old_status);
        return tid;
    }

}

struct lock {
    Tid acquired_id;
    bool available;
    struct wait_queue *wq;

};

struct lock *
lock_create() {

    int old_status;
    old_status = interrupts_set(0);

    struct lock *lock;

    lock = malloc(sizeof (struct lock));

    lock->available = true;
    lock->wq = wait_queue_create();


    interrupts_set(old_status);
    return lock;
}

void
lock_destroy(struct lock *lock) {

    int old_status;
    old_status = interrupts_set(0);

    assert(lock != NULL);

    assert(lock->available);
    assert(lock->wq->head == NULL);


    lock->available = false;

    wait_queue_destroy(lock->wq);

    free(lock);


    interrupts_set(old_status);

    return;
}

void
lock_acquire(struct lock *lock) {

    int old_status;
    old_status = interrupts_set(0);

    assert(lock != NULL);

    if (lock->available) {
        lock->acquired_id = currently_running->id;
        lock->available = false;
        interrupts_set(old_status);
        return;
    } else {
        while (!(lock->available)) {
            thread_sleep(lock->wq);
        }

        lock->acquired_id = currently_running->id;
        lock->available = false;
        interrupts_set(old_status);
        return;
    }

}

void
lock_release(struct lock *lock) {

    int old_status;
    old_status = interrupts_set(0);

    assert(lock != NULL);

    if (!(lock->available)) {
        if (lock->acquired_id == currently_running->id) {
            lock->available = true;
            thread_wakeup(lock->wq, 1);
            interrupts_set(old_status);
            return;
        } else {
            lock_acquire(lock);
            lock->available = true;
            thread_wakeup(lock->wq, 1);
            interrupts_set(old_status);
            return;
        }
    }
    return;
}

struct cv {
    struct wait_queue *wq;
};

struct cv *
cv_create() {

    int old_status;
    old_status = interrupts_set(0);


    struct cv *cv;

    cv = malloc(sizeof (struct cv));
    assert(cv);



    cv->wq = wait_queue_create();

    interrupts_set(old_status);
    return cv;
}

void
cv_destroy(struct cv *cv) {

    int old_status;
    old_status = interrupts_set(0);

    assert(cv != NULL);

    assert(cv->wq->head == NULL);


    wait_queue_destroy(cv->wq);
    cv->wq = NULL;
    free(cv);
    cv = NULL;


    interrupts_set(old_status);
    return;
}

void
cv_wait(struct cv *cv, struct lock *lock) {

    int old_status;
    old_status = interrupts_set(0);

    assert(cv != NULL);
    assert(lock != NULL);

    assert(lock->acquired_id == currently_running->id);

    lock_release(lock);

    thread_sleep(cv->wq);
    lock_acquire(lock);
    interrupts_set(old_status);
    return;


}

void
cv_signal(struct cv *cv, struct lock *lock) {

    int old_status;
    old_status = interrupts_set(0);

    assert(cv != NULL);
    assert(lock != NULL);
    assert(lock->acquired_id == currently_running->id);

    thread_wakeup(cv->wq, 0);


    interrupts_set(old_status);

    return;


}

void
cv_broadcast(struct cv *cv, struct lock *lock) {

    int old_status;
    old_status = interrupts_set(0);

    assert(cv != NULL);
    assert(lock != NULL);
    assert(lock->acquired_id == currently_running->id);

    thread_wakeup(cv->wq, 1);
    interrupts_set(old_status);
    return;

}

void add_to_ready_que(Tid id) {

    //in case the thread is already in the ready queue
    remove_from_ready_que(id);


    struct ready_queue_object* object = (struct ready_queue_object*) malloc
            (sizeof (struct ready_queue_object));

    object->thread_id = id;
    object->next = NULL;


    if (rq_head == NULL) {
        rq_head = object;
        return;
    } else {

        struct ready_queue_object* temp = rq_head;

        while (temp->next != NULL) {
            temp = temp->next;
        }

        temp->next = object;
        return;
    }

}

void remove_from_ready_que(Tid id) {


    if (rq_head == NULL) {
        return;
    } else {
        struct ready_queue_object* temp = rq_head;
        struct ready_queue_object* prev = NULL;
        while (temp != NULL) {

            if (temp->thread_id == id) {
                if (prev == NULL) {
                    rq_head = rq_head->next;
                    temp->next = NULL;
                    free(temp);
                    return;
                } else {
                    prev->next = temp->next;
                    temp->next = NULL;
                    free(temp);
                    return;
                }
            }
            prev = temp;
            temp = temp->next;
        }

        return;

    }
}

void
thread_stub(void (*thread_main)(void *), void *arg) {

    interrupts_on();

    Tid ret;

    thread_main(arg); // call thread_main() function with arg
    ret = thread_exit();
    // we should only get here if we are the last thread. 
    assert(ret == THREAD_NONE);
    // all threads are done, so process should exit
    exit(0);
}

void thread_destroy(Tid id) {


    remove_from_ready_que(id);


    if (thread_list[id].wq != NULL) {
        if (thread_list[id].wq->head != NULL) {
            thread_wakeup(thread_list[id].wq, 1);

        }
    }

    if (id != 0) {



        wait_queue_destroy(thread_list[id].wq);

        thread_list[id].existence = false;

        free(thread_list[id].sp);
        thread_list[id].sp = NULL;

        thread_list[id].setcontext_called = false;
        struct exit_que_object* temp = eq_head;
        eq_head = eq_head->next;
        free(temp);

        num_of_threads--;

    }


    return;

}
