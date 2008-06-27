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
#include "libs3.h"
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

PrivateData *create_private_data(CURL *curl,
                                 S3RequestHandler *handler,
                                 S3ListServiceCallback *listServiceCallback,
                                 S3ListBucketCallback *listBucketCallback,
                                 S3PutObjectCallback *putObjectCallback,
                                 S3GetObjectCallback *getObjectCallback,
                                 void *data)
{
    PrivateData *ret = (PrivateData *) calloc(sizeof(PrivateData));

    if (ret) {
        ret->curl = curl;
        ret->headersCallback = handler->headersCallback;
        ret->errorCallback = handler->errorCallback;
        ret->completeCallback = handler->completeCallback;
        ret->listServiceCallback = listServiceCallback;
        ret->listBucketCallback = listBucketCallback;
        ret->putObjectCallback = putObjectCallback;
        ret->getObjectCallback = getObjectCallback;
        ret->data = data;
    }

    return ret;
}

S3Status handle_multi_request(CurlRequest *request, 
                              S3RequestContext *requestContext)
{
    switch (curl_multi_add_handle(requestContext->curlm, request->curl)) {
    case CURLM_OK:
        return S3StatusOK;
    // xxx todo - more specific errors
    default:
        pool_release(request);
        return S3StatusFailure;
    }
}

S3Status handle_easy_request(CurlRequest *request)
{
    CURLcode status = curl_easy_perform(request->curl);

    pool_release(request);

    switch (status) {
    case CURLE_OK:
        return S3StatusOK;
    // xxx todo - more specific errors
    default:
        return S3StatusFailure;
    }
}

size_t curl_header_func(void *ptr, size_t size, size_t nmemb, void *fstream)
{
    PrivateData *pd = (PrivateData *) fstream;
    S3ResponseHeadersPrivate *responseHeaders = &(pd->responseHeaders);

    // Curl might call back the header function after the body has been
    // received, for 'chunked encoded' contents.  We don't handle this as of
    // yet, and it's not clear that it would ever be useful.
    if (pd->headersCallbackMade) {
        return;
    }

    // If we haven't gotten the result code yet, attempt to get it now.  It
    // should be available as soon as the first header is available.
    if (!responseHeaders->resultCodeSet) {
        responseHeaders->resultCodeSet = 1;
        if (curl_easy_getinfo(pd->curl, CURLINFO_RESPONSE_CODE,
                              &(responseHeaders->resultCode)) != CURLE_OK) {
            // Weird, couldn't get the status.  Set it to 0
            responseHeaders->resultCode = 0;
        }
    }

    // The header must end in \r\n, so we can set the \r to 0 to terminate it
    size_t len = size * nmemb;
    char *header = (char *) ptr;
    header[len - 2] = 0;
    
    // Find the colon to split the header up
    char *colon = header;
    while (*colon && (*colon != ':')) {
        colon++;
    }
    
    int namelen = colon - header;
    
    if (!strncmp(header, "RequestId", namelen)) {
    }
    else if (!strncmp(header, "RequestId2", namelen)) {
    }
    else if (!strncmp(header, "ContentType", namelen)) {
    }
    else if (!strncmp(header, "ContentLength", namelen)) {
    }
    else if (!strncmp(header, "Server", namelen)) {
    }
    else if (!strncmp(header, "ETag", namelen)) {
    }
    else if (!strncmp(header, "LastModified", namelen)) {
    }
    else if (!strncmp(header, "x-amz-meta-", 
                      (namelen > strlen("x-amz-meta-") ? 
                       strlen("x-amz-meta-") : namelen))) {
        
    }
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

    S3Status status = pool_initialize(userAgentInfo);
    if (status != S3StatusOK) {
        S3_deinitialize();
        return status;
    }

    return S3StatusOK;
}


void S3_deinitialize()
{
    pool_deinitialize();

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
