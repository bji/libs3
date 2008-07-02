/** **************************************************************************
 * s3.c
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

/**
 * This is a 'driver' program that simply converts command-line input into
 * calls to libs3 functions, and prints the results.
 **/

#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libs3.h"


// Command-line options, saved as globals -------------------------------------

static int showResponseHeadersG = 0;
// request headers stuff
// acl stuff


// Request results, saved as globals ------------------------------------------

static int statusG = 0, httpResponseCodeG = 0;
static S3Error *errorG = 0;


// Option prefixes ------------------------------------------------------------

#define LOCATION_CONSTRAINT_PREFIX "locationConstraint="
#define LOCATION_CONSTRAINT_PREFIX_LEN (sizeof(LOCATION_CONSTRAINT_PREFIX) - 1)
#define CANNED_ACL_PREFIX "cannedAcl="
#define CANNED_ACL_PREFIX_LEN (sizeof(CANNED_ACL_PREFIX) - 1)


// libs3 mutex stuff ----------------------------------------------------------

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


// response header callback ---------------------------------------------------

static S3Status responseHeadersCallback(const S3ResponseHeaders *headers,
                                        void *callbackData)
{
    if (showResponseHeadersG) {
    }

    return S3StatusOK;
}


// response complete callback -------------------------------------------------

static void responseCompleteCallback(S3Status status, int httpResponseCode,
                                     S3Error *error, void *callbackData)
{
    statusG = status;
    httpResponseCodeG = httpResponseCode;
    errorG = error;
}


// list service callback ------------------------------------------------------

static S3Status listServiceCallback(const char *ownerId, 
                                    const char *ownerDisplayName,
                                    const char *bucketName,
                                    const struct timeval *creationDate,
                                    void *callbackData)
{
    return S3StatusOK;
}


// usage exit -----------------------------------------------------------------

static void usageExit()
{
    fprintf(stderr,
" Options:\n"
"\n"
"  Command Line:\n"
"\n"   
"  -p : use path-style URIs (--path-style)\n"
"  -u : unencrypted (use HTTP instead of HTTPS) (--https)\n"
"  -s : show response headers (--show-headers)\n"
"\n"
"  Environment:\n"
"\n"
"  S3_ACCESS_KEY_ID : S3 access key ID (required)\n"
"  S3_SECRET_ACCESS_KEY : S3 secret access key (required)\n"
"\n" 
"  COMMON: Content-Type (could apply to put and copy)\n"
"\n"
"  Commands:\n"
"\n"
"  list\n"
"  test <bucket>\n"
"  create <bucket> [cannedAcl, locationConstraint]\n"
"  delete <bucket>\n"
"  list <bucket> [prefix, marker, delimiter, maxkeys]\n"
"  put <bucket>/<key> [filename, cacheControl, contentType, md5,\n"
"                      contentDispositionFilename, contentEncoding,\n"
"                      expires, [x-amz-meta-...]]\n"
"  copy <sourcebucket>/<sourcekey> <destbucket>/<destkey> [headers]\n"
"  get <buckey>/<key> [filename (required if -r is used), if-modified-since,\n"
"                      if-unmodified-since, if-match, if-not-match,\n"
"                      range-start, range-end]\n"
"  head <bucket>/<key>\n"
"  delete <bucket>/<key>\n"
"  todo : acl stuff\n"
"\n");

    exit(-1);
}


static struct option longOptionsG[] =
{
    { "path-style",           no_argument      ,  0,  'p' },
    { "unencrypted",          no_argument      ,  0,  'u' },
    { "show-headers",         no_argument,        0,  's' },
    { 0,                      0,                  0,   0  }
};


// main -----------------------------------------------------------------------

int main(int argc, char **argv)
{
    S3Protocol protocol = S3ProtocolHTTPS;
    int usePathStyleUri = 0;

    // Parse args
    while (1) {
        int index = 0;
        int c = getopt_long(argc, argv, "pus", longOptionsG, &index);

        if (c == -1) {
            // End of options
            break;
        }

        switch (c) {
        case 'p':
            usePathStyleUri = 1;
            break;
        case 'u':
            protocol = S3ProtocolHTTP;
            break;
        case 'r':
            showResponseHeadersG = 1;
            break;
        default:
            fprintf(stderr, "ERROR: Unknown options: -%c\n", c);
            // Usage exit
            usageExit();
        }
    }

    const char *accessKeyId = getenv("S3_ACCESS_KEY_ID");
    if (!accessKeyId) {
        fprintf(stderr, "Missing environment variable: S3_ACCESS_KEY_ID\n");
        return -1;
    }
    const char *secretAccessKey = getenv("S3_SECRET_ACCESS_KEY");
    if (!secretAccessKey) {
        fprintf(stderr, "Missing environment variable: S3_SECRET_ACCESS_KEY\n");
        return -1;
    }

    // The first non-option argument gives the operation to perform
    if (optind == argc) {
        fprintf(stderr, "\nERROR: Missing argument: command\n\n");
        usageExit();
    }

    const char *command = argv[optind++];

    S3Status status;

#define S3_initialize() \
    if ((status = S3_initialize("s3", &threadSelfCallback, \
                                &mutexCreateCallback, \
                                &mutexLockCallback, \
                                &mutexUnlockCallback, \
                                &mutexDestroyCallback)) != S3StatusOK) { \
        fprintf(stderr, "Failed to initialize libs3: %d\n", status); \
        return -1; \
    }

    if (!strcmp(command, "list")) {
        if (optind == argc) {
            // list service
            S3_initialize();
            S3ListServiceHandler listServiceHandler =
            {
                { &responseHeadersCallback, &responseCompleteCallback },
                &listServiceCallback
            };
            status = S3_list_service(protocol, accessKeyId, secretAccessKey,
                                     0, &listServiceHandler, 0);
            if (status != S3StatusOK) {
                fprintf(stderr, "ERROR: Failed to send request: %d\n", status);
            }
            else if (statusG != S3StatusOK) {
                fprintf(stderr, "ERROR: Failed to complete request: %d\n",
                        statusG);
            }
            else if (httpResponseCodeG != 200) {
                fprintf(stderr, "ERROR: S3 returned error: %d\n", 
                        httpResponseCodeG);
                status = S3StatusFailure;
            }
        }
        else {
        }
    }
    else if (!strcmp(command, "test")) {
        // test bucket
        if (optind == argc) {
            fprintf(stderr, "ERROR: Missing parameter: bucket\n");
            usageExit();
        }
        const char *bucketName = argv[optind++];
        char locationConstraint[64];
        S3_initialize();
        S3ResponseHandler responseHandler =
        {
            &responseHeadersCallback, &responseCompleteCallback
        };
        status = S3_test_bucket(protocol, accessKeyId, secretAccessKey,
                                bucketName, sizeof(locationConstraint),
                                locationConstraint, 0, &responseHandler, 0);
        if (status != S3StatusOK) {
            fprintf(stderr, "ERROR: Failed to send request: %d\n", status);
        }
        else if (statusG != S3StatusOK) {
            fprintf(stderr, "ERROR: Failed to complete request: %d\n",
                    statusG);
        }
        else if (httpResponseCodeG == 200) {
            printf("Bucket '%s' exists", bucketName);
            // bucket exists
            if (locationConstraint[0]) {
                printf(" in location %s\n", locationConstraint);
            }
            else {
                printf(".\n");
            }
        }
        else if (httpResponseCodeG == 404) {
            // bucket does not exist
            printf("Bucket '%s' does not exist.\n", bucketName);
        }
        else if (httpResponseCodeG == 403) {
            // bucket exists, but no access
            printf("Bucket '%s' exists, but is not accessible.\n", bucketName);
        }
        else {
            fprintf(stderr, "ERROR: S3 returned error: %d\n", 
                    httpResponseCodeG);
            status = S3StatusFailure;
        }
    }
    else if (!strcmp(command, "create")) {
        if (optind == argc) {
            fprintf(stderr, "ERROR: Missing parameter: bucket\n");
            usageExit();
        }
        const char *bucketName = argv[optind++];
        const char *locationConstraint = 0;
        S3CannedAcl cannedAcl = S3CannedAclPrivate;
        while (optind < argc) {
            char *param = argv[optind++];
            if (!strncmp(param, LOCATION_CONSTRAINT_PREFIX, 
                         LOCATION_CONSTRAINT_PREFIX_LEN)) {
                locationConstraint = &(param[LOCATION_CONSTRAINT_PREFIX_LEN]);
            }
            else if (!strncmp(param, CANNED_ACL_PREFIX, 
                              CANNED_ACL_PREFIX_LEN)) {
                char *val = &(param[CANNED_ACL_PREFIX_LEN]);
                if (!strcmp(val, "private")) {
                    cannedAcl = S3CannedAclPrivate;
                }
                else if (!strcmp(val, "public-read")) {
                    cannedAcl = S3CannedAclPublicRead;
                }
                else if (!strcmp(val, "public-read-write")) {
                    cannedAcl = S3CannedAclPublicReadWrite;
                }
                else if (!strcmp(val, "authenticated-read")) {
                    cannedAcl = S3CannedAclAuthenticatedRead;
                }
                else {
                    fprintf(stderr, "ERROR: Unknown canned ACL: %s\n", val);
                    usageExit();
                }
            }
            else {
                fprintf(stderr, "ERROR: Unknown param: %s\n", param);
                usageExit();
            }
        }

        S3_initialize();

        S3ResponseHandler responseHandler =
        {
            &responseHeadersCallback, &responseCompleteCallback
        };
        status = S3_create_bucket(protocol, accessKeyId, secretAccessKey,
                                  bucketName, cannedAcl, locationConstraint,
                                  0, &responseHandler, 0);
        if (status != S3StatusOK) {
            fprintf(stderr, "ERROR: Failed to send request: %d\n", status);
        }
        else if (statusG != S3StatusOK) {
            fprintf(stderr, "ERROR: Failed to complete request: %d\n",
                    statusG);
        }
        else if (httpResponseCodeG != 200) {
            fprintf(stderr, "ERROR: S3 returned error: %d\n", 
                    httpResponseCodeG);
            status = S3StatusFailure;
        }
    }
    else if (!strcmp(command, "delete")) {
        // delete bucket
        if (optind == argc) {
            fprintf(stderr, "ERROR: Missing parameter: bucket\n");
            usageExit();
        }
        const char *bucketName = argv[optind++];
        S3_initialize();
        S3ResponseHandler responseHandler =
        {
            &responseHeadersCallback, &responseCompleteCallback
        };
        status = S3_delete_bucket(protocol, accessKeyId, secretAccessKey,
                                  bucketName, 0, &responseHandler, 0);
        if (status != S3StatusOK) {
            fprintf(stderr, "ERROR: Failed to send request: %d\n", status);
        }
        else if (statusG != S3StatusOK) {
            fprintf(stderr, "ERROR: Failed to complete request: %d\n",
                    statusG);
        }
        else if (httpResponseCodeG != 204) {
            fprintf(stderr, "ERROR: S3 returned error: %d\n", 
                    httpResponseCodeG);
            status = S3StatusFailure;
        }
    }
    else if (!strcmp(command, "put")) {
    }
    else if (!strcmp(command, "copy")) {
    }
    else if (!strcmp(command, "get")) {
    }
    else if (!strcmp(command, "head")) {
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        return -1;
    }

    S3_deinitialize();

    return (status == S3StatusOK) ? 0 : -1;
}
