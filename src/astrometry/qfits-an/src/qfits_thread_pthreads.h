/**
 This file was added to the QFITS library by Astrometry.net

 Copyright 2009 Dustin Lang

 The Astrometry.net suite is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation, version 2.

 The Astrometry.net suite is distributed in the hope that it will be
 useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with the Astrometry.net suite; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef QFITS_THREAD_PHTREADS_H
#define QFITS_THREAD_PHTREADS_H

#include <pthread.h>

/*
 struct qfits_lock_s {
 pthread_mutex_t mutex;
 };
 typedef struct qfits_lock_s qfits_lock_t;
//#define QFITS_LOCK_INIT
//#define QFITS_LOCK_INIT { PTHREAD_MUTEX_INITIALIZER }
 */

typedef pthread_mutex_t qfits_lock_t;

#define QFITS_LOCK_INIT PTHREAD_MUTEX_INITIALIZER


#endif
