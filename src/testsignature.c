/*
 * testsignature.c
 *
 *  Created on: Sep 29, 2016
 *      Author: eric
 */

#include <stdlib.h>
#include <stdio.h>
#include <libs3.h>

static S3Status responsePropertiesCallback(
        const S3ResponseProperties *properties, void *callbackData)
{
    return S3StatusOK;
}

static void responseCompleteCallback(S3Status status,
                                     const S3ErrorDetails *error,
                                     void *callbackData)
{
    if (error) {
        printf("\nERROR\n");
        if (error->message) {
            printf("%s\n", error->message);
        }
        if (error->extraDetailsCount > 0) {
            for (int i = 0; i < error->extraDetailsCount; i++) {
                printf("%s: %s\n", error->extraDetails[i].name, error->extraDetails[i].value);
            }
        }
    }
}

static S3Status getObjectDataCallback(int bufferSize, const char *buffer,
                                      void *callbackData)
{
    return S3StatusOK;
}

int main(int argc, char **argv)
{
    S3_initialize(NULL, S3_INIT_ALL, NULL);

    S3BucketContext bucketContext =
    { 0, "examplebucket", S3ProtocolHTTPS, S3UriStyleVirtualHost,
        "AKIAIOSFODNN7EXAMPLE", "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY", 0 };

    S3GetObjectHandler getObjectHandler =
    {
        { &responsePropertiesCallback, &responseCompleteCallback },
        &getObjectDataCallback
    };

    S3_get_object(&bucketContext, "test.txt", NULL, 0, 10, NULL,
                   &getObjectHandler, NULL);

    S3_deinitialize();
}
