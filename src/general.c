/** **************************************************************************
 * general.c
 * 
 * Copyright 2008 Bryan Ischo <bryan@ischo.com>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the
 *
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ************************************************************************** **/

#include <ctype.h>
#include <openssl/crypto.h>
#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>
#ifndef OPENSSL_THREADS
#error "Threading support required in OpenSSL library, but not provided"
#endif
#include <pthread.h>
#include "private.h"

typedef struct S3Mutex CRYPTO_dynlock_value;

static struct S3Mutex **pLocksG;

static S3MutexCreateCallback *mutexCreateCallbackG;
static S3MutexLockCallback *mutexLockCallbackG;
static S3MutexUnlockCallback *mutexUnlockCallbackG;
static S3MutexDestroyCallback *mutexDestroyCallbackG;


static void locking_callback(int mode, int index, const char *file, int line)
{
    if (mode & CRYPTO_LOCK) {
        mutex_lock(pLocksG[index]);
    }
    else {
        mutex_unlock(pLocksG[index]);
    }
}


static struct CRYPTO_dynlock_value *dynlock_create(const char *file, int line)
{
    return (struct CRYPTO_dynlock_value *) mutex_create();
}


static void dynlock_lock(int mode, struct CRYPTO_dynlock_value *pLock,
                         const char *file, int line)
{
    if (mode & CRYPTO_LOCK) {
        mutex_lock((struct S3Mutex *) pLock);
    }
    else {
        mutex_unlock((struct S3Mutex *) pLock);
    }
}


static void dynlock_destroy(struct CRYPTO_dynlock_value *pLock,
                            const char *file, int line)
{
    mutex_destroy((struct S3Mutex *) pLock);
}


struct S3Mutex *mutex_create()
{
    return (*mutexCreateCallbackG)();
}

void mutex_lock(struct S3Mutex *mutex)
{
    return (*mutexLockCallbackG)(mutex);
}

void mutex_unlock(struct S3Mutex *mutex)
{
    return (*mutexUnlockCallbackG)(mutex);
}

void mutex_destroy(struct S3Mutex *mutex)
{
    return (*mutexDestroyCallbackG)(mutex);
}

S3Status S3_initialize(const char *userAgentInfo,
                       S3ThreadSelfCallback *threadSelfCallback,
                       S3MutexCreateCallback *mutexCreateCallback,
                       S3MutexLockCallback *mutexLockCallback,
                       S3MutexUnlockCallback *mutexUnlockCallback,
                       S3MutexDestroyCallback *mutexDestroyCallback)
{
    /* As required by the openssl library for thread support */
    int count = CRYPTO_num_locks(), i;
    
    if (!(pLocksG = 
          (struct S3Mutex **) malloc(count * sizeof(struct S3Mutex *)))) {
        return S3StatusOutOfMemory;
    }

    for (i = 0; i < count; i++) {
        if (!(pLocksG[i] = (*mutexCreateCallback)())) {
            while (i-- > 0) {
                (*mutexDestroyCallback)(pLocksG[i]);
            }
            return S3StatusFailedToCreateMutex;
        }
    }

    mutexCreateCallbackG = mutexCreateCallback;
    mutexLockCallbackG = mutexLockCallback;
    mutexUnlockCallbackG = mutexUnlockCallback;
    mutexDestroyCallbackG = mutexDestroyCallback;

    CRYPTO_set_id_callback(threadSelfCallback);
    CRYPTO_set_locking_callback(&locking_callback);
    CRYPTO_set_dynlock_create_callback(dynlock_create);
    CRYPTO_set_dynlock_lock_callback(dynlock_lock);
    CRYPTO_set_dynlock_destroy_callback(dynlock_destroy);

    S3Status status = curl_request_api_initialize(userAgentInfo);
    if (status != S3StatusOK) {
        S3_deinitialize();
        return status;
    }

    return S3StatusOK;
}


void S3_deinitialize()
{
    curl_request_api_deinitialize();

    CRYPTO_set_dynlock_destroy_callback(NULL);
    CRYPTO_set_dynlock_lock_callback(NULL);
    CRYPTO_set_dynlock_create_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    CRYPTO_set_id_callback(NULL);

    int count = CRYPTO_num_locks();
    for (int i = 0; i < count; i++) {
        (*mutexDestroyCallbackG)(pLocksG[i]);
    }

    free(pLocksG);
}


S3Status S3_validate_bucket_name(const char *bucketName, S3UriStyle uriStyle)
{
    int virtualHostStyle = (uriStyle == S3UriStyleVirtualHost);
    int len = 0, maxlen = virtualHostStyle ? 63 : 255;
    const char *b = bucketName;

    int hasDot = 0;
    int hasNonDigit = 0;

    while (*b) {
        if (len == maxlen) {
            return S3StatusInvalidBucketNameTooLong;
        }
        else if (isalpha(*b)) {
            len++, b++;
            hasNonDigit = 1;
        }
        else if (isdigit(*b)) {
            len++, b++;
        }
        else if (len == 0) {
            return S3StatusInvalidBucketNameFirstCharacter;
        }
        else if (*b == '_') {
            /* Virtual host style bucket names cannot have underscores */
            if (virtualHostStyle) {
                return S3StatusInvalidBucketNameCharacter;
            }
            len++, b++;
            hasNonDigit = 1;
        }
        else if (*b == '-') {
            /* Virtual host style bucket names cannot have .- */
            if (virtualHostStyle && (b > bucketName) && (*(b - 1) == '.')) {
                return S3StatusInvalidBucketNameCharacterSequence;
            }
            len++, b++;
            hasNonDigit = 1;
        }
        else if (*b == '.') {
            /* Virtual host style bucket names cannot have -. */
            if (virtualHostStyle && (b > bucketName) && (*(b - 1) == '-')) {
                return S3StatusInvalidBucketNameCharacterSequence;
            }
            len++, b++;
            hasDot = 1;
        }
        else {
            return S3StatusInvalidBucketNameCharacter;
        }
    }

    if (len < 3) {
        return S3StatusInvalidBucketNameTooShort;
    }

    /* It's not clear from Amazon's documentation exactly what 'IP address
       style' means.  In its strictest sense, it could mean 'could be a valid
       IP address', which would mean that 255.255.255.255 would be invalid,
       wherase 256.256.256.256 would be valid.  Or it could mean 'has 4 sets
       of digits separated by dots'.  Who knows.  Let's just be really
       conservative here: if it has any dots, and no non-digit characters,
       then we reject it */
    if (hasDot && !hasNonDigit) {
        return S3StatusInvalidBucketNameDotQuadNotation;
    }

    return S3StatusOK;
}


S3Status S3_convert_acl(char *aclXml, int *aclGrantCountReturn,
                        S3AclGrant *aclGrantsReturn)
{
    return S3StatusOK;
}
