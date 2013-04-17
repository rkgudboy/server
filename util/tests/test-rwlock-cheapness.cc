/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "$Id: test-rwlock.cc 46971 2012-08-18 22:03:43Z zardosht $"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include <toku_portability.h>
#include <toku_assert.h>
#include <portability/toku_fair_rwlock.h>
#include <portability/toku_pthread.h>
#include <portability/toku_time.h>
#include <util/frwlock.h>
#include <util/rwlock.h>
#include "rwlock_condvar.h"

toku_mutex_t mutex;
toku::frwlock w;

static void grab_write_lock(bool expensive) {
    toku_mutex_lock(&mutex);
    w.write_lock(expensive);
    toku_mutex_unlock(&mutex);
}

static void release_write_lock(void) {
    toku_mutex_lock(&mutex);
    w.write_unlock();
    toku_mutex_unlock(&mutex);
}

static void grab_read_lock(void) {
    toku_mutex_lock(&mutex);
    w.read_lock();
    toku_mutex_unlock(&mutex);
}

static void release_read_lock(void) {
    toku_mutex_lock(&mutex);
    w.read_unlock();
    toku_mutex_unlock(&mutex);
}

static void *do_cheap_wait(void *arg) {
    grab_write_lock(false);
    release_write_lock();
    return arg;
}

static void *do_expensive_wait(void *arg) {
    grab_write_lock(true);
    release_write_lock();
    return arg;
}

static void *do_read_wait(void *arg) {
    grab_read_lock();
    release_read_lock();
    return arg;
}

static void launch_cheap_waiter(void) {
    toku_pthread_t tid;
    int r = toku_pthread_create(&tid, NULL, do_cheap_wait, NULL); 
    assert_zero(r);
    toku_pthread_detach(tid);
    sleep(1);
}

static void launch_expensive_waiter(void) {
    toku_pthread_t tid;
    int r = toku_pthread_create(&tid, NULL, do_expensive_wait, NULL); 
    assert_zero(r);
    toku_pthread_detach(tid);
    sleep(1);
}

static void launch_reader(void) {
    toku_pthread_t tid;
    int r = toku_pthread_create(&tid, NULL, do_read_wait, NULL); 
    assert_zero(r);
    toku_pthread_detach(tid);
    sleep(1);
}

static bool locks_are_expensive(void) {
    toku_mutex_lock(&mutex);
    assert(w.write_lock_is_expensive() == w.read_lock_is_expensive());
    bool is_expensive = w.write_lock_is_expensive();
    toku_mutex_unlock(&mutex);
    return is_expensive;
}

static void test_write_cheapness(void) {
    toku_mutex_init(&mutex, NULL);    
    w.init(&mutex);

    // single expensive write lock
    grab_write_lock(true);
    assert(locks_are_expensive());
    release_write_lock();
    assert(!locks_are_expensive());

    // single cheap write lock
    grab_write_lock(false);
    assert(!locks_are_expensive());
    release_write_lock();
    assert(!locks_are_expensive());

    // multiple read locks
    grab_read_lock();
    assert(!locks_are_expensive());
    grab_read_lock();
    grab_read_lock();
    assert(!locks_are_expensive());
    release_read_lock();
    release_read_lock();
    release_read_lock();
    assert(!locks_are_expensive());

    // expensive write lock and cheap writers waiting
    grab_write_lock(true);
    launch_cheap_waiter();
    assert(locks_are_expensive());
    launch_cheap_waiter();
    launch_cheap_waiter();
    assert(locks_are_expensive());
    release_write_lock();
    sleep(1);
    assert(!locks_are_expensive());

    // cheap write lock and expensive writer waiter
    grab_write_lock(false);
    launch_expensive_waiter();
    assert(locks_are_expensive());
    release_write_lock();
    sleep(1);

    // expensive write lock and expensive waiter
    grab_write_lock(true);
    launch_expensive_waiter();
    assert(locks_are_expensive());
    release_write_lock();
    sleep(1);

    // cheap write lock and cheap waiter
    grab_write_lock(false);
    launch_cheap_waiter();
    assert(!locks_are_expensive());
    release_write_lock();
    sleep(1);

    // read lock held and cheap waiter
    grab_read_lock();
    launch_cheap_waiter();
    assert(!locks_are_expensive());
    // add expensive waiter
    launch_expensive_waiter();
    assert(locks_are_expensive());
    release_read_lock();
    sleep(1);

    // read lock held and expensive waiter
    grab_read_lock();
    launch_expensive_waiter();
    assert(locks_are_expensive());
    // add expensive waiter
    launch_cheap_waiter();
    assert(locks_are_expensive());
    release_read_lock();
    sleep(1);

    // cheap write lock held and waiting read
    grab_write_lock(false);
    launch_reader();
    assert(!locks_are_expensive());
    launch_expensive_waiter();
    toku_mutex_lock(&mutex);
    assert(w.write_lock_is_expensive());
    // tricky case here, because we have a launched reader
    // that should be in the queue, a new read lock
    // should piggy back off that
    assert(!w.read_lock_is_expensive());
    toku_mutex_unlock(&mutex);
    release_write_lock();
    sleep(1);

    // expensive write lock held and waiting read
    grab_write_lock(true);
    launch_reader();
    assert(locks_are_expensive());
    launch_cheap_waiter();
    assert(locks_are_expensive());
    release_write_lock();
    sleep(1);

    w.deinit();
    toku_mutex_destroy(&mutex);
}

int main (int UU(argc), const char* UU(argv[])) {
    test_write_cheapness();
    return 0;
}