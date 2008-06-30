#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "libs3.h"

struct S3Mutex
{
    pthread_mutex_t m;
};

static unsigned long threadSelfCallback()
{
    return pthread_self();
}

static struct S3Mutex *mutexCreateCallback()
{
    struct S3Mutex *mutex = (struct S3Mutex *) malloc(sizeof(struct S3Mutex));
    
    pthread_mutex_init(&(mutex->m), NULL);

    return mutex;
}

static void mutexLockCallback(struct S3Mutex *mutex)
{
    pthread_mutex_lock(&(mutex->m));
}

static void mutexUnlockCallback(struct S3Mutex *mutex)
{
    pthread_mutex_unlock(&(mutex->m));
}

static void mutexDestroyCallback(struct S3Mutex *mutex)
{
    pthread_mutex_destroy(&(mutex->m));
    free(mutex);
}

static S3Status responseHeadersCallback(const S3ResponseHeaders *headers,
                                        void *callbackData)
{
    return S3StatusOK;
}

static void responseCompleteCallback(S3Status status, int httpResponseCode,
                                     S3Error *error, void *callbackData)
{
}

static S3Status listServiceCallback(const char *ownerId, 
                                    const char *ownerDisplayName,
                                    const char *bucketName,
                                    const struct timeval *creationDate,
                                    void *callbackData)
{
    return S3StatusOK;
}

static S3ListServiceHandler listServiceHandlerG =
{
    { &responseHeadersCallback, &responseCompleteCallback },
    &listServiceCallback
};


int main()
{
    S3Status status = S3_initialize("test", &threadSelfCallback,
                                    &mutexCreateCallback,
                                    &mutexLockCallback,
                                    &mutexUnlockCallback,
                                    &mutexDestroyCallback);

    if (status != S3StatusOK) {
        printf("Failed: %d\n", status);
    }

    status = S3_list_service(S3ProtocolHTTP, "007PT5KV75BPXVHMK0G2",
                             "IdNSYMG8hAmXPHlCD+HiI+s7hssho49QqramxCnn", 0,
                             &listServiceHandlerG, (void *) 17);
                             
    S3_deinitialize();

    return 0;
}
