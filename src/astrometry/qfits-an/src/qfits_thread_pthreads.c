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

#include <pthread.h>

#include "qfits_thread.h"

void qfits_lock_init(qfits_lock* lock) {
	pthread_mutex_init(&(lock->mutex), NULL);
}

void qfits_lock_lock(qfits_lock* lock) {
	pthread_mutex_lock(&(lock->mutex));
}

void qfits_lock_unlock(qfits_lock* lock) {
	pthread_mutex_unlock(&(lock->mutex));
}

