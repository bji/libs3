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


// Command-line options, saved as globals ------------------------------------

static int showResponseHeadersG = 0;
static S3Protocol protocolG = S3ProtocolHTTPS;
static S3UriStyle uriStyleG = S3UriStyleVirtualHost;
// request headers stuff
// acl stuff


// Environment variables, saved as globals ----------------------------------

static const char *accessKeyIdG = 0;
static const char *secretAccessKeyG = 0;


// Request results, saved as globals -----------------------------------------

static int statusG = 0, httpResponseCodeG = 0;
static const S3ErrorDetails *errorG = 0;


// Option prefixes -----------------------------------------------------------

#define LOCATION_CONSTRAINT_PREFIX "locationConstraint="
#define LOCATION_CONSTRAINT_PREFIX_LEN (sizeof(LOCATION_CONSTRAINT_PREFIX) - 1)
#define CANNED_ACL_PREFIX "cannedAcl="
#define CANNED_ACL_PREFIX_LEN (sizeof(CANNED_ACL_PREFIX) - 1)


// libs3 mutex stuff ---------------------------------------------------------

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


// util ----------------------------------------------------------------------

static void S3_init()
{
    S3Status status;
    if ((status = S3_initialize("s3", &threadSelfCallback, 
                                &mutexCreateCallback,
                                &mutexLockCallback, &mutexUnlockCallback,
                                &mutexDestroyCallback)) != S3StatusOK) {
        fprintf(stderr, "Failed to initialize libs3: %d\n", status);
        exit(-1);
    }
}


static void printError()
{
    if (statusG < S3StatusErrorAccessDenied) {
        fprintf(stderr, "ERROR: %s\n", S3_get_status_name(statusG));
    }
    else {
        fprintf(stderr, "ERROR: S3 returned an unexpected error:\n");
        fprintf(stderr, "  HTTP Code: %d\n", httpResponseCodeG);
        fprintf(stderr, "  S3 Error: %s\n", S3_get_status_name(statusG));
        if (errorG) {
            if (errorG->message) {
                fprintf(stderr, "  Message: %s\n", errorG->message);
            }
            if (errorG->resource) {
                fprintf(stderr, "  Resource: %s\n", errorG->resource);
            }
            if (errorG->furtherDetails) {
                fprintf(stderr, "  Further Details: %s\n", 
                        errorG->furtherDetails);
            }
            if (errorG->extraDetailsCount) {
                printf("  Extra Details:\n");
                int i;
                for (i = 0; i < errorG->extraDetailsCount; i++) {
                    printf("    %s: %s\n", errorG->extraDetails[i].name,
                           errorG->extraDetails[i].value);
                }
            }
        }
    }
}


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
"  S3_ACCESS_KEY_ID : S3 access key ID\n"
"  S3_SECRET_ACCESS_KEY : S3 secret access key\n"
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


// response header callback --------------------------------------------------

// This callback does the same thing for every request type: prints out the
// headers if the user has requested them to be so
static S3Status responseHeadersCallback(const S3ResponseHeaders *headers,
                                        void *callbackData)
{
    if (!showResponseHeadersG) {
        return S3StatusOK;
    }

#define print_nonnull(name, field)                              \
    do {                                                        \
        if (headers-> field) {                                  \
            printf("%s: %s\n", name, headers-> field);          \
        }                                                       \
    } while (0)
    
    print_nonnull("Request-Id", requestId);
    print_nonnull("Request-Id-2", requestId2);
    if (headers->contentLength > 0) {
        printf("Content-Length: %lld\n", headers->contentLength);
    }
    print_nonnull("Server", server);
    print_nonnull("ETag", eTag);
    if (headers->lastModified > 0) {
        char timebuf[1024];
        // localtime is not thread-safe but we don't care here.  xxx note -
        // localtime doesn't seem to actually do anything, 0 locatime of 0
        // returns EST Unix epoch, it should return the NZST equivalent ...
        strftime(timebuf, sizeof(timebuf), "%Y/%m/%d %H:%M:%S %Z",
                 localtime(&(headers->lastModified)));
        printf("Last-Modified: %s\n", timebuf);
    }
    int i;
    for (i = 0; i < headers->metaHeadersCount; i++) {
        printf("x-amz-meta-%s: %s\n", headers->metaHeaders[i].name,
               headers->metaHeaders[i].value);
    }

    return S3StatusOK;
}


// response complete callback ------------------------------------------------

// This callback does the same thing for every request type: saves the status
// and error stuff in global variables
static void responseCompleteCallback(S3Status status, int httpResponseCode,
                                     const S3ErrorDetails *error, 
                                     void *callbackData)
{
    statusG = status;
    httpResponseCodeG = httpResponseCode;
    errorG = error;
}


// list service --------------------------------------------------------------

static S3Status listServiceCallback(const char *ownerId, 
                                    const char *ownerDisplayName,
                                    const char *bucketName,
                                    int creationDateSeconds,
                                    int creationDateMilliseconds,
                                    void *callbackData)
{
    static int ownerPrinted = 0;

    if (!ownerPrinted) {
        printf("Owner ID: %s\n", ownerId);
        printf("Owner Display Name: %s\n", ownerDisplayName);
        ownerPrinted = 1;
    }

    printf("Bucket Name: %s\n", bucketName);
    if (creationDateSeconds >= 0) {
        char fmtbuf[256];
        snprintf(fmtbuf, sizeof(fmtbuf), "%%Y/%%m/%%d %%H:%%M:%%S.%03d %%Z", 
                 creationDateMilliseconds);
        char timebuf[1024];
        time_t d = (time_t) creationDateSeconds;
        // localtime is not thread-safe but we don't care here.  xxx note -
        // localtime doesn't seem to actually do anything, 0 locatime of 0
        // returns EST Unix epoch, it should return the NZST equivalent ...
        strftime(timebuf, sizeof(timebuf), fmtbuf, localtime(&d));
        printf("Creation Date: %s\n", timebuf);
    }

    return S3StatusOK;
}


static void list_service()
{
    S3_init();

    S3ListServiceHandler listServiceHandler =
    {
        { &responseHeadersCallback, &responseCompleteCallback },
        &listServiceCallback
    };

    S3_list_service(protocolG, accessKeyIdG, secretAccessKeyG, 0, 
                    &listServiceHandler, 0);

    if (statusG != S3StatusOK) {
        printError();
    }

    S3_deinitialize();
}


// test bucket ---------------------------------------------------------------

static void test_bucket(int argc, char **argv, int optind)
{
    // test bucket
    if (optind == argc) {
        fprintf(stderr, "ERROR: Missing parameter: bucket\n");
        usageExit();
    }

    const char *bucketName = argv[optind++];

    if (optind != argc) {
        fprintf(stderr, "ERROR: Extraneous parameter: %s\n", argv[optind]);
        usageExit();
    }

    S3_init();

    S3ResponseHandler responseHandler =
    {
        &responseHeadersCallback, &responseCompleteCallback
    };

    char locationConstraint[64];
    S3_test_bucket(protocolG, uriStyleG, accessKeyIdG, secretAccessKeyG,
                   bucketName, sizeof(locationConstraint), locationConstraint,
                   0, &responseHandler, 0);

    switch (statusG) {
    case S3StatusOK:
        // bucket exists
        printf("Bucket '%s' exists", bucketName);
        if (locationConstraint[0]) {
            printf(" in location %s\n", locationConstraint);
        }
        else {
            printf(".\n");
        }
        break;
    case S3StatusErrorNoSuchBucket:
        // bucket does not exist
        printf("Bucket '%s' does not exist.\n", bucketName);
        break;
    case S3StatusErrorAccessDenied:
        // bucket exists, but no access
        printf("Bucket '%s' exists, but is not accessible.\n", bucketName);
        break;
    default:
        printError();
        break;
    }

    S3_deinitialize();
}


// create bucket -------------------------------------------------------------


static void create_bucket(int argc, char **argv, int optind)
{
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

    S3_init();

    S3ResponseHandler responseHandler =
    {
        &responseHeadersCallback, &responseCompleteCallback
    };

    S3_create_bucket(protocolG, accessKeyIdG, secretAccessKeyG, bucketName,
                     cannedAcl, locationConstraint, 0, &responseHandler, 0);

    if (statusG != S3StatusOK) {
        printError();
    }
    
    S3_deinitialize();
}


// delete bucket -------------------------------------------------------------


static void delete_bucket(int argc, char **argv, int optind)
{
    // delete bucket
    if (optind == argc) {
        fprintf(stderr, "ERROR: Missing parameter: bucket\n");
        usageExit();
    }

    const char *bucketName = argv[optind++];

    S3_init();

    S3ResponseHandler responseHandler =
    {
        &responseHeadersCallback, &responseCompleteCallback
    };

    S3_delete_bucket(protocolG, uriStyleG, accessKeyIdG, secretAccessKeyG,
                     bucketName, 0, &responseHandler, 0);

    if (statusG != S3StatusOK) {
        printError();
    }

    S3_deinitialize();
}


// main ----------------------------------------------------------------------

int main(int argc, char **argv)
{
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
            uriStyleG = S3UriStylePath;
            break;
        case 'u':
            protocolG = S3ProtocolHTTP;
            break;
        case 's':
            showResponseHeadersG = 1;
            break;
        default:
            fprintf(stderr, "ERROR: Unknown options: -%c\n", c);
            // Usage exit
            usageExit();
        }
    }

    accessKeyIdG = getenv("S3_ACCESS_KEY_ID");
    if (!accessKeyIdG) {
        fprintf(stderr, "Missing environment variable: S3_ACCESS_KEY_ID\n");
        return -1;
    }
    secretAccessKeyG = getenv("S3_SECRET_ACCESS_KEY");
    if (!secretAccessKeyG) {
        fprintf(stderr, "Missing environment variable: S3_SECRET_ACCESS_KEY\n");
        return -1;
    }

    // The first non-option argument gives the operation to perform
    if (optind == argc) {
        fprintf(stderr, "\nERROR: Missing argument: command\n\n");
        usageExit();
    }

    const char *command = argv[optind++];

    if (!strcmp(command, "list")) {
        if (optind == argc) {
            list_service();
        }
        else {
            // list bucket
        }
    }
    else if (!strcmp(command, "test")) {
        test_bucket(argc, argv, optind);
    }
    else if (!strcmp(command, "create")) {
        create_bucket(argc, argv, optind);
    }
    else if (!strcmp(command, "delete")) {
        delete_bucket(argc, argv, optind);
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

    return 0;
}
