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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "libs3.h"


// Something is weird with glibc ... setenv/unsetenv/ftruncate are not defined
// in stdlib.h as they should be.  And fileno is not in stdio.h
extern int setenv(const char *, const char *, int);
extern int unsetenv(const char *);
extern int ftruncate(int, off_t);
extern int fileno(FILE *);


// Command-line options, saved as globals ------------------------------------

static int showResponsePropertiesG = 0;
static S3Protocol protocolG = S3ProtocolHTTPS;
static S3UriStyle uriStyleG = S3UriStyleVirtualHost;
// request properties stuff
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
#define PREFIX_PREFIX "prefix="
#define PREFIX_PREFIX_LEN (sizeof(PREFIX_PREFIX) - 1)
#define MARKER_PREFIX "marker="
#define MARKER_PREFIX_LEN (sizeof(MARKER_PREFIX) - 1)
#define DELIMITER_PREFIX "delimiter="
#define DELIMITER_PREFIX_LEN (sizeof(DELIMITER_PREFIX) - 1)
#define MAXKEYS_PREFIX "maxkeys="
#define MAXKEYS_PREFIX_LEN (sizeof(MAXKEYS_PREFIX) - 1)
#define FILENAME_PREFIX "filename="
#define FILENAME_PREFIX_LEN (sizeof(FILENAME_PREFIX) - 1)
#define CONTENT_LENGTH_PREFIX "contentLength="
#define CONTENT_LENGTH_PREFIX_LEN (sizeof(CONTENT_LENGTH_PREFIX) - 1)
#define CACHE_CONTROL_PREFIX "cacheControl="
#define CACHE_CONTROL_PREFIX_LEN (sizeof(CACHE_CONTROL_PREFIX) - 1)
#define CONTENT_TYPE_PREFIX "contentType="
#define CONTENT_TYPE_PREFIX_LEN (sizeof(CONTENT_TYPE_PREFIX) - 1)
#define MD5_PREFIX "md5="
#define MD5_PREFIX_LEN (sizeof(MD5_PREFIX) - 1)
#define CONTENT_DISPOSITION_FILENAME_PREFIX "contentDispositionFilename="
#define CONTENT_DISPOSITION_FILENAME_PREFIX_LEN \
    (sizeof(CONTENT_DISPOSITION_FILENAME_PREFIX) - 1)
#define CONTENT_ENCODING_PREFIX "contentEncoding="
#define CONTENT_ENCODING_PREFIX_LEN (sizeof(CONTENT_ENCODING_PREFIX) - 1)
#define EXPIRES_PREFIX "expires="
#define EXPIRES_PREFIX_LEN (sizeof(EXPIRES_PREFIX) - 1)
#define X_AMZ_META_PREFIX "x-amz-meta-"
#define X_AMZ_META_PREFIX_LEN (sizeof(X_AMZ_META_PREFIX) - 1)
#define IF_MODIFIED_SINCE_PREFIX "ifModifiedSince="
#define IF_MODIFIED_SINCE_PREFIX_LEN (sizeof(IF_MODIFIED_SINCE_PREFIX) - 1)
#define IF_NOT_MODIFIED_SINCE_PREFIX "ifNotmodifiedSince="
#define IF_NOT_MODIFIED_SINCE_PREFIX_LEN \
    (sizeof(IF_NOT_MODIFIED_SINCE_PREFIX) - 1)
#define IF_MATCH_PREFIX "ifMatch="
#define IF_MATCH_PREFIX_LEN (sizeof(IF_MATCH_PREFIX) - 1)
#define IF_NOT_MATCH_PREFIX "ifNotMatch="
#define IF_NOT_MATCH_PREFIX_LEN (sizeof(IF_NOT_MATCH_PREFIX) - 1)
#define START_BYTE_PREFIX "startByte="
#define START_BYTE_PREFIX_LEN (sizeof(START_BYTE_PREFIX) - 1)
#define BYTE_COUNT_PREFIX "byteCount="
#define BYTE_COUNT_PREFIX_LEN (sizeof(BYTE_COUNT_PREFIX) - 1)


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


static void usageExit(FILE *out)
{
    fprintf(out,
" Options:\n"
"\n"
"   Command Line:\n"
"\n"   
"   -p : use path-style URIs (--path-style)\n"
"   -u : unencrypted (use HTTP instead of HTTPS) (--https)\n"
"   -s : show response properties (--show-properties)\n"
"\n"
"   Environment:\n"
"\n"
"   S3_ACCESS_KEY_ID : S3 access key ID\n"
"   S3_SECRET_ACCESS_KEY : S3 secret access key\n"
"\n" 
" Commands:\n"
"\n"
"   help\n"            
"   list\n"
"   test <bucket>\n"
"   create <bucket> [cannedAcl, locationConstraint]\n"
"   delete <bucket>\n"
"   list <bucket> [prefix, marker, delimiter, maxkeys]\n"
"   put <bucket>/<key> [filename, contentLength, cacheControl, contentType,\n"
"                       md5, contentDispositionFilename, contentEncoding,\n"
"                       validDuration, cannedAcl, [x-amz-meta-...]]\n"
"   copy <sourcebucket>/<sourcekey> <destbucket>/<destkey> [properties]\n"
"   get <buckey>/<key> [filename (required if -s is used), ifModifiedSince,\n"
"                       ifNotmodifiedSince, ifMatch, ifNotMatch,\n"
"                       startByte, byteCount]\n"
"   head <bucket>/<key> [ifModifiedSince, ifNotmodifiedSince, ifMatch,\n"
"                       ifNotMatch] (implies -s)\n"
"   delete <bucket>/<key>\n"
"   todo : acl stuff\n"
"\n");

    exit(-1);
}


static uint64_t convertInt(const char *str, const char *paramName)
{
    uint64_t ret = 0;

    while (*str) {
        if (!isdigit(*str)) {
            fprintf(stderr, "ERROR: Nondigit in %s parameter: %c\n", 
                    paramName, *str);
            usageExit(stderr);
        }
        ret *= 10;
        ret += (*str++ - '0');
    }

    return ret;
}


typedef struct growbuffer
{
    // The total number of bytes, and the start byte
    int size;
    // The start byte
    int start;
    // The blocks
    char data[64 * 1024];
    struct growbuffer *prev, *next;
} growbuffer;


// returns nonzero on success, zero on out of memory
static int growbuffer_append(growbuffer **gb, const char *data, int dataLen)
{
    while (dataLen) {
        growbuffer *buf = *gb ? (*gb)->prev : 0;
        if (!buf || (buf->size == sizeof(buf->data))) {
            buf = (growbuffer *) malloc(sizeof(growbuffer));
            if (!buf) {
                return 0;
            }
            buf->size = 0;
            buf->start = 0;
            if (*gb) {
                buf->prev = (*gb)->prev;
                buf->next = *gb;
                (*gb)->prev->next = buf;
                (*gb)->prev = buf;
            }
            else {
                buf->prev = buf->next = buf;
                *gb = buf;
            }
        }

        int toCopy = (sizeof(buf->data) - buf->size);
        if (toCopy > dataLen) {
            toCopy = dataLen;
        }

        memcpy(&(buf->data[buf->size]), data, toCopy);
        
        buf->size += toCopy, data += toCopy, dataLen -= toCopy;
    }

    return 1;
}


static void growbuffer_read(growbuffer **gb, int amt, int *amtReturn, 
                            char *buffer)
{
    *amtReturn = 0;

    growbuffer *buf = *gb;

    if (!buf) {
        return;
    }

    *amtReturn = (buf->size > amt) ? amt : buf->size;

    memcpy(buffer, &(buf->data[buf->start]), *amtReturn);
    
    buf->start += *amtReturn, buf->size -= *amtReturn;

    if (buf->size == 0) {
        if (buf->next == buf) {
            *gb = 0;
        }
        else {
            *gb = buf->next;
        }
        free(buf);
    }
}


static void growbuffer_destroy(growbuffer *gb)
{
    growbuffer *start = gb;

    while (gb) {
        growbuffer *next = gb->next;
        free(gb);
        gb = (next == start) ? 0 : next;
    }
}


// Convenience utility for making the code look nicer.  Tests a string
// against a format; only the characters specified in the format are
// checked (i.e. if the string is longer than the format, the string still
// checks out ok).  Format characters are:
// d - is a digit
// anything else - is that character
// Returns nonzero the string checks out, zero if it does not.
static int checkString(const char *str, const char *format)
{
    while (*format) {
        if (*format == 'd') {
            if (!isdigit(*str)) {
                return 0;
            }
        }
        else if (*str != *format) {
            return 0;
        }
        str++, format++;
    }

    return 1;
}


static time_t parseIso8601Time(const char *str)
{
    // Check to make sure that it has a valid format
    if (!checkString(str, "dddd-dd-ddTdd:dd:dd")) {
        return -1;
    }

#define nextnum() (((*str - '0') * 10) + (*(str + 1) - '0'))

    // Convert it
    struct tm stm;
    memset(&stm, 0, sizeof(stm));

    stm.tm_year = (nextnum() - 19) * 100;
    str += 2;
    stm.tm_year += nextnum();
    str += 3;

    stm.tm_mon = nextnum() - 1;
    str += 3;

    stm.tm_mday = nextnum();
    str += 3;

    stm.tm_hour = nextnum();
    str += 3;

    stm.tm_min = nextnum();
    str += 3;

    stm.tm_sec = nextnum();
    str += 2;

    stm.tm_isdst = -1;

    // This is hokey but it's the recommended way ...
    char *tz = getenv("TZ");
    setenv("TZ", "UTC", 1);

    time_t ret = mktime(&stm);

    if (tz) {
        setenv("TZ", tz, 1);
    }
    else {
        unsetenv("TZ");
    }

    // Skip the millis

    if (*str == '.') {
        str++;
        while (isdigit(*str)) {
            str++;
        }
    }
    
    if (checkString(str, "-dd:dd") || checkString(str, "+dd:dd")) {
        int sign = (*str++ == '-') ? -1 : 1;
        int hours = nextnum();
        str += 3;
        int minutes = nextnum();
        ret += (-sign * (((hours * 60) + minutes) * 60));
    }
    // Else it should be Z to be a conformant time string, but we just assume
    // that it is rather than enforcing that

    return ret;
}


static struct option longOptionsG[] =
{
    { "path-style",           no_argument,        0,  'p' },
    { "unencrypted",          no_argument,        0,  'u' },
    { "show-proerties",         no_argument,      0,  's' },
    { 0,                      0,                  0,   0  }
};


// response properties callback ----------------------------------------------

// This callback does the same thing for every request type: prints out the
// properties if the user has requested them to be so
static S3Status responsePropertiesCallback
    (const S3ResponseProperties *properties, void *callbackData)
{
    if (!showResponsePropertiesG) {
        return S3StatusOK;
    }

#define print_nonnull(name, field)                              \
    do {                                                        \
        if (properties-> field) {                                  \
            printf("%s: %s\n", name, properties-> field);          \
        }                                                       \
    } while (0)
    
    print_nonnull("Request-Id", requestId);
    print_nonnull("Request-Id-2", requestId2);
    if (properties->contentLength > 0) {
        printf("Content-Length: %lld\n", properties->contentLength);
    }
    print_nonnull("Server", server);
    print_nonnull("ETag", eTag);
    if (properties->lastModified > 0) {
        char timebuf[256];
        // localtime is not thread-safe but we don't care here.  xxx note -
        // localtime doesn't seem to actually do anything, 0 locatime of 0
        // returns EST Unix epoch, it should return the NZST equivalent ...
        strftime(timebuf, sizeof(timebuf), "%Y/%m/%d %H:%M:%S %Z",
                 localtime(&(properties->lastModified)));
        printf("Last-Modified: %s\n", timebuf);
    }
    int i;
    for (i = 0; i < properties->metaDataCount; i++) {
        printf("x-amz-meta-%s: %s\n", properties->metaData[i].name,
               properties->metaData[i].value);
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
                                    time_t creationDate, void *callbackData)
{
    static int ownerPrinted = 0;

    if (!ownerPrinted) {
        printf("Owner ID: %s\n", ownerId);
        printf("Owner Display Name: %s\n", ownerDisplayName);
        ownerPrinted = 1;
    }

    printf("Bucket Name: %s\n", bucketName);
    if (creationDate >= 0) {
        char timebuf[256];
        // localtime is not thread-safe but we don't care here.  xxx note -
        // localtime doesn't seem to actually do anything, 0 locatime of 0
        // returns EST Unix epoch, it should return the NZST equivalent ...
        strftime(timebuf, sizeof(timebuf), "%Y/%m/%d %H:%M:%S %Z",
                 localtime(&creationDate));
        printf("Creation Date: %s\n", timebuf);
    }

    return S3StatusOK;
}


static void list_service()
{
    S3_init();

    S3ListServiceHandler listServiceHandler =
    {
        { &responsePropertiesCallback, &responseCompleteCallback },
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
        usageExit(stderr);
    }

    const char *bucketName = argv[optind++];

    if (optind != argc) {
        fprintf(stderr, "ERROR: Extraneous parameter: %s\n", argv[optind]);
        usageExit(stderr);
    }

    S3_init();

    S3ResponseHandler responseHandler =
    {
        &responsePropertiesCallback, &responseCompleteCallback
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
        usageExit(stderr);
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
        else if (!strncmp(param, CANNED_ACL_PREFIX, CANNED_ACL_PREFIX_LEN)) {
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
                usageExit(stderr);
            }
        }
        else {
            fprintf(stderr, "ERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    S3_init();

    S3ResponseHandler responseHandler =
    {
        &responsePropertiesCallback, &responseCompleteCallback
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
    if (optind == argc) {
        fprintf(stderr, "ERROR: Missing parameter: bucket\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optind++];

    if (optind != argc) {
        fprintf(stderr, "ERROR: Extraneous parameter: %s\n", argv[optind]);
        usageExit(stderr);
    }

    S3_init();

    S3ResponseHandler responseHandler =
    {
        &responsePropertiesCallback, &responseCompleteCallback
    };

    S3_delete_bucket(protocolG, uriStyleG, accessKeyIdG, secretAccessKeyG,
                     bucketName, 0, &responseHandler, 0);

    if (statusG != S3StatusOK) {
        printError();
    }

    S3_deinitialize();
}


// list bucket ---------------------------------------------------------------

typedef struct list_bucket_callback_data
{
    int isTruncated;
    char nextMarker[1024];
} list_bucket_callback_data;


static S3Status listBucketCallback(int isTruncated, const char *nextMarker,
                                   int contentsCount, 
                                   const S3ListBucketContent *contents,
                                   int commonPrefixesCount,
                                   const char **commonPrefixes,
                                   void *callbackData)
{
    list_bucket_callback_data *data = 
        (list_bucket_callback_data *) callbackData;

    data->isTruncated = isTruncated;
    // This is tricky.  S3 doesn't return the NextMarker if there is no
    // delimiter.  Why, I don't know, since it's still useful for paging
    // through results.  We want NextMarker to be the last content in the
    // list, so set it to that if necessary.
    if (!nextMarker && contentsCount) {
        nextMarker = contents[contentsCount - 1].key;
    }
    if (nextMarker) {
        snprintf(data->nextMarker, sizeof(data->nextMarker), "%s", nextMarker);
    }
    else {
        data->nextMarker[0] = 0;
    }

    int i;
    for (i = 0; i < contentsCount; i++) {
        const S3ListBucketContent *content = &(contents[i]);
        printf("\nKey: %s\n", content->key);
        char timebuf[256];
        // localtime is not thread-safe but we don't care here.  xxx note -
        // localtime doesn't seem to actually do anything, 0 locatime of 0
        // returns EST Unix epoch, it should return the NZST equivalent ...
        strftime(timebuf, sizeof(timebuf), "%Y/%m/%d %H:%M:%S %Z",
                 localtime(&(content->lastModified)));
        printf("Last Modified: %s\n", timebuf);
        printf("ETag: %s\n", content->eTag);
        printf("Size: %llu\n", content->size);
        if (content->ownerId) {
            printf("Owner ID: %s\n", content->ownerId);
        }
        if (content->ownerDisplayName) {
            printf("Owner Display Name: %s\n", content->ownerDisplayName);
        }
    }

    for (i = 0; i < commonPrefixesCount; i++) {
        printf("\nCommon Prefix: %s\n", commonPrefixes[i]);
    }

    return S3StatusOK;
}


static void list_bucket(int argc, char **argv, int optind)
{
    if (optind == argc) {
        fprintf(stderr, "ERROR: Missing parameter: bucket\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optind++];

    const char *prefix = 0, *marker = 0, *delimiter = 0;
    int maxkeys = 0;
    while (optind < argc) {
        char *param = argv[optind++];
        if (!strncmp(param, PREFIX_PREFIX, PREFIX_PREFIX_LEN)) {
            prefix = &(param[PREFIX_PREFIX_LEN]);
        }
        else if (!strncmp(param, MARKER_PREFIX, MARKER_PREFIX_LEN)) {
            marker = &(param[MARKER_PREFIX_LEN]);
        }
        else if (!strncmp(param, DELIMITER_PREFIX, DELIMITER_PREFIX_LEN)) {
            delimiter = &(param[DELIMITER_PREFIX_LEN]);
        }
        else if (!strncmp(param, MAXKEYS_PREFIX, MAXKEYS_PREFIX_LEN)) {
            maxkeys = convertInt(&(param[MAXKEYS_PREFIX_LEN]), "maxkeys");
        }
        else {
            fprintf(stderr, "ERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }
    
    S3_init();
    
    S3BucketContext bucketContext =
    {
        bucketName,
        protocolG,
        uriStyleG,
        accessKeyIdG,
        secretAccessKeyG
    };

    S3ListBucketHandler listBucketHandler =
    {
        { &responsePropertiesCallback, &responseCompleteCallback },
        &listBucketCallback
    };

    list_bucket_callback_data data;

    do {
        data.isTruncated = 0;
        S3_list_bucket(&bucketContext, prefix, marker, delimiter, maxkeys,
                       0, &listBucketHandler, &data);
        if (statusG != S3StatusOK) {
            printError();
            break;
        }
        marker = data.nextMarker;
    } while (data.isTruncated);

    S3_deinitialize();
}


// put object ----------------------------------------------------------------

typedef struct put_object_callback_data
{
    FILE *infile;
    growbuffer *gb;
    uint64_t contentLength;
} put_object_callback_data;


static int putObjectDataCallback(int bufferSize, char *buffer,
                                 void *callbackData)
{
    put_object_callback_data *data = (put_object_callback_data *) callbackData;
    
    int ret = 0;

    if (data->contentLength) {
        int toRead = ((data->contentLength > bufferSize) ?
                      bufferSize : data->contentLength);
        if (data->infile) {
            ret = fread(buffer, 1, toRead, data->infile);
        }
        else if (data->gb) {
            growbuffer_read(&(data->gb), data->contentLength, &ret, buffer);
        }
    }

    data->contentLength -= ret;

    return ret;
}


static void put_object(int argc, char **argv, int optind)
{
    if (optind == argc) {
        fprintf(stderr, "ERROR: Missing parameter: bucket/key\n");
        usageExit(stderr);
    }

    // Split bucket/key
    char *slash = argv[optind];
    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (!*slash || !*(slash + 1)) {
        fprintf(stderr, "ERROR: Invalid bucket/key name: %s\n", argv[optind]);
        usageExit(stderr);
    }
    *slash++ = 0;

    const char *bucketName = argv[optind++];
    const char *key = slash;

    const char *filename = 0;
    uint64_t contentLength = 0;
    const char *cacheControl = 0, *contentType = 0, *md5 = 0;
    const char *contentDispositionFilename = 0, *contentEncoding = 0;
    time_t expires = -1;
    S3CannedAcl cannedAcl = S3CannedAclPrivate;
    int metaPropertiesCount = 0;
    S3NameValue metaProperties[S3_MAX_METADATA_COUNT];

    while (optind < argc) {
        char *param = argv[optind++];
        if (!strncmp(param, FILENAME_PREFIX, FILENAME_PREFIX_LEN)) {
            filename = &(param[FILENAME_PREFIX_LEN]);
        }
        else if (!strncmp(param, CONTENT_LENGTH_PREFIX, 
                          CONTENT_LENGTH_PREFIX_LEN)) {
            contentLength = convertInt(&(param[CONTENT_LENGTH_PREFIX_LEN]),
                                       "contentLength");
            if (contentLength > (5LL * 1024 * 1024 * 1024)) {
                fprintf(stderr, "ERROR: contentLength must be no greater "
                        "than 5 GB\n");
                usageExit(stderr);
            }
        }
        else if (!strncmp(param, CACHE_CONTROL_PREFIX, 
                          CACHE_CONTROL_PREFIX_LEN)) {
            cacheControl = &(param[CACHE_CONTROL_PREFIX_LEN]);
        }
        else if (!strncmp(param, CONTENT_TYPE_PREFIX, 
                          CONTENT_TYPE_PREFIX_LEN)) {
            contentType = &(param[CONTENT_TYPE_PREFIX_LEN]);
        }
        else if (!strncmp(param, MD5_PREFIX, MD5_PREFIX_LEN)) {
            md5 = &(param[MD5_PREFIX_LEN]);
        }
        else if (!strncmp(param, CONTENT_DISPOSITION_FILENAME_PREFIX, 
                          CONTENT_DISPOSITION_FILENAME_PREFIX_LEN)) {
            contentDispositionFilename = 
                &(param[CONTENT_DISPOSITION_FILENAME_PREFIX_LEN]);
        }
        else if (!strncmp(param, CONTENT_ENCODING_PREFIX, 
                          CONTENT_ENCODING_PREFIX_LEN)) {
            contentEncoding = &(param[CONTENT_ENCODING_PREFIX_LEN]);
        }
        else if (!strncmp(param, EXPIRES_PREFIX, EXPIRES_PREFIX_LEN)) {
            expires = parseIso8601Time(&(param[EXPIRES_PREFIX_LEN]));
            if (expires < 0) {
                fprintf(stderr, "ERROR: Invalid expires time "
                        "value; ISO 8601 time format required\n");
                usageExit(stderr);
            }
        }
        else if (!strncmp(param, X_AMZ_META_PREFIX, X_AMZ_META_PREFIX_LEN)) {
            if (metaPropertiesCount == S3_MAX_METADATA_COUNT) {
                fprintf(stderr, "ERROR: Too many x-amz-meta- properties, "
                        "limit %d: %s\n", S3_MAX_METADATA_COUNT, param);
                usageExit(stderr);
            }
            char *name = &(param[X_AMZ_META_PREFIX_LEN]);
            char *value = name;
            while (*value && (*value != '=')) {
                value++;
            }
            if (!*value || !*(value + 1)) {
                fprintf(stderr, "ERROR: Invalid parameter: %s\n", param);
                usageExit(stderr);
            }
            *value++ = 0;
            metaProperties[metaPropertiesCount].name = name;
            metaProperties[metaPropertiesCount++].value = value;
        }
        else if (!strncmp(param, CANNED_ACL_PREFIX, CANNED_ACL_PREFIX_LEN)) {
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
                usageExit(stderr);
            }
        }
        else {
            fprintf(stderr, "ERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    put_object_callback_data data;

    data.infile = 0;
    data.gb = 0;

    if (filename) {
        if (!contentLength) {
            struct stat statbuf;
            // Stat the file to get its length
            if (stat(filename, &statbuf) == -1) {
                fprintf(stderr, "ERROR: Failed to stat file %s: ", filename);
                perror(0);
                exit(-1);
            }
            contentLength = statbuf.st_size;
        }
        // Open the file
        if (!(data.infile = fopen(filename, "r"))) {
            fprintf(stderr, "ERROR: Failed to open input file %s: ", filename);
            perror(0);
            exit(-1);
        }
    }
    else {
        // Read from stdin.  If contentLength is not provided, we have
        // to read it all in to get contentLength.
        if (!contentLength) {
            // Read all if stdin to get the data
            char buffer[64 * 1024];
            while (1) {
                int amtRead = fread(buffer, 1, sizeof(buffer), stdin);
                if (amtRead == 0) {
                    break;
                }
                if (!growbuffer_append(&(data.gb), buffer, amtRead)) {
                    fprintf(stderr, "ERROR: Out of memory while reading "
                            "stdin\n");
                    exit(-1);
                }
                contentLength += amtRead;
                if (amtRead < sizeof(buffer)) {
                    break;
                }
            }
        }
        else {
            data.infile = stdin;
        }
    }

    data.contentLength = contentLength;

    S3_init();
    
    S3BucketContext bucketContext =
    {
        bucketName,
        protocolG,
        uriStyleG,
        accessKeyIdG,
        secretAccessKeyG
    };

    S3PutProperties putProperties =
    {
        contentType,
        md5,
        cacheControl,
        contentDispositionFilename,
        contentEncoding,
        expires,
        cannedAcl,
        0,
        0,
        metaPropertiesCount,
        metaProperties
    };

    S3PutObjectHandler putObjectHandler =
    {
        { &responsePropertiesCallback, &responseCompleteCallback },
        &putObjectDataCallback
    };

    S3_put_object(&bucketContext, key, contentLength, &putProperties, 0,
                  &putObjectHandler, &data);

    if (data.infile) {
        fclose(data.infile);
    }
    else if (data.gb) {
        growbuffer_destroy(data.gb);
    }

    if (statusG != S3StatusOK) {
        printError();
    }
    else if (data.contentLength) {
        fprintf(stderr, "ERROR: Failed to read remaining %llu bytes from "
                "input\n", data.contentLength);
    }

    S3_deinitialize();
}


// get object ----------------------------------------------------------------

static S3Status getObjectDataCallback(int bufferSize, const char *buffer,
                                      void *callbackData)
{
    FILE *outfile = (FILE *) callbackData;

    size_t wrote = fwrite(buffer, 1, bufferSize, outfile);
    
    return (wrote < bufferSize) ? S3StatusFailure : S3StatusOK;
}


static void get_object(int argc, char **argv, int optind)
{
    if (optind == argc) {
        fprintf(stderr, "ERROR: Missing parameter: bucket/key\n");
        usageExit(stderr);
    }

    // Split bucket/key
    char *slash = argv[optind];
    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (!*slash || !*(slash + 1)) {
        fprintf(stderr, "ERROR: Invalid bucket/key name: %s\n", argv[optind]);
        usageExit(stderr);
    }
    *slash++ = 0;

    const char *bucketName = argv[optind++];
    const char *key = slash;

    const char *filename = 0;
    time_t ifModifiedSince = -1, ifNotModifiedSince = -1;
    const char *ifMatch = 0, *ifNotMatch = 0;
    uint64_t startByte = 0, byteCount = 0;

    while (optind < argc) {
        char *param = argv[optind++];
        if (!strncmp(param, FILENAME_PREFIX, FILENAME_PREFIX_LEN)) {
            filename = &(param[FILENAME_PREFIX_LEN]);
        }
        else if (!strncmp(param, IF_MODIFIED_SINCE_PREFIX, 
                     IF_MODIFIED_SINCE_PREFIX_LEN)) {
            // Parse ifModifiedSince
            ifModifiedSince = parseIso8601Time
                (&(param[IF_MODIFIED_SINCE_PREFIX_LEN]));
            if (ifModifiedSince < 0) {
                fprintf(stderr, "ERROR: Invalid ifModifiedSince time "
                        "value; ISO 8601 time format required\n");
                usageExit(stderr);
            }
        }
        else if (!strncmp(param, IF_NOT_MODIFIED_SINCE_PREFIX, 
                          IF_NOT_MODIFIED_SINCE_PREFIX_LEN)) {
            // Parse ifModifiedSince
            ifNotModifiedSince = parseIso8601Time
                (&(param[IF_NOT_MODIFIED_SINCE_PREFIX_LEN]));
            if (ifNotModifiedSince < 0) {
                fprintf(stderr, "ERROR: Invalid ifNotModifiedSince time "
                        "value; ISO 8601 time format required\n");
                usageExit(stderr);
            }
        }
        else if (!strncmp(param, IF_MATCH_PREFIX, IF_MATCH_PREFIX_LEN)) {
            ifMatch = &(param[IF_MATCH_PREFIX_LEN]);
        }
        else if (!strncmp(param, IF_NOT_MATCH_PREFIX,
                          IF_NOT_MATCH_PREFIX_LEN)) {
            ifNotMatch = &(param[IF_NOT_MATCH_PREFIX_LEN]);
        }
        else if (!strncmp(param, START_BYTE_PREFIX, START_BYTE_PREFIX_LEN)) {
            startByte = convertInt
                (&(param[START_BYTE_PREFIX_LEN]), "startByte");
        }
        else if (!strncmp(param, BYTE_COUNT_PREFIX, BYTE_COUNT_PREFIX_LEN)) {
            byteCount = convertInt
                (&(param[BYTE_COUNT_PREFIX_LEN]), "byteCount");
        }
        else {
            fprintf(stderr, "ERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    FILE *outfile;

    if (filename) {
        // Open in r+ so that we don't truncate the file, just in case there
        // is an error and we write no bytes, we leave the file unmodified
        if ((outfile = fopen(filename, "r+")) == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file %s: ",
                    filename);
            perror(0);
            exit(-1);
        }
    }
    else if (showResponsePropertiesG) {
        fprintf(stderr, "ERROR: get -s requires a filename parameter\n");
        usageExit(stderr);
    }
    else {
        outfile = stdout;
    }

    S3_init();
    
    S3BucketContext bucketContext =
    {
        bucketName,
        protocolG,
        uriStyleG,
        accessKeyIdG,
        secretAccessKeyG
    };

    S3GetConditions getConditions =
    {
        ifModifiedSince,
        ifNotModifiedSince,
        ifMatch,
        ifNotMatch
    };

    S3GetObjectHandler getObjectHandler =
    {
        { &responsePropertiesCallback, &responseCompleteCallback },
        &getObjectDataCallback
    };

    S3_get_object(&bucketContext, key, &getConditions, startByte, byteCount,
                  0, &getObjectHandler, outfile);

    if (statusG == S3StatusOK) {
        if (outfile != stdout) {
            ftruncate(fileno(outfile), ftell(outfile));
        }
    }
    else if (statusG != S3StatusErrorPreconditionFailed) {
        printError();
    }

    fclose(outfile);

    S3_deinitialize();
}


// head object ---------------------------------------------------------------

static void head_object(int argc, char **argv, int optind)
{
    if (optind == argc) {
        fprintf(stderr, "ERROR: Missing parameter: bucket/key\n");
        usageExit(stderr);
    }
    
    // Head implies showing response properties
    showResponsePropertiesG = 1;

    // Split bucket/key
    char *slash = argv[optind];

    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (!*slash || !*(slash + 1)) {
        fprintf(stderr, "ERROR: Invalid bucket/key name: %s\n", argv[optind]);
        usageExit(stderr);
    }
    *slash++ = 0;

    const char *bucketName = argv[optind++];
    const char *key = slash;

    if (optind != argc) {
        fprintf(stderr, "ERROR: Extraneous parameter: %s\n", argv[optind]);
        usageExit(stderr);
    }

    S3_init();
    
    S3BucketContext bucketContext =
    {
        bucketName,
        protocolG,
        uriStyleG,
        accessKeyIdG,
        secretAccessKeyG
    };

    S3ResponseHandler responseHandler =
    { 
        &responsePropertiesCallback,
        &responseCompleteCallback
    };

    S3_head_object(&bucketContext, key, 0, &responseHandler, 0);

    if ((statusG != S3StatusOK) &&
        (statusG != S3StatusErrorPreconditionFailed)) {
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
            showResponsePropertiesG = 1;
            break;
        default:
            fprintf(stderr, "ERROR: Unknown options: -%c\n", c);
            // Usage exit
            usageExit(stderr);
        }
    }

    // The first non-option argument gives the operation to perform
    if (optind == argc) {
        fprintf(stderr, "\nERROR: Missing argument: command\n\n");
        usageExit(stderr);
    }

    const char *command = argv[optind++];
    
    if (!strcmp(command, "help")) {
        usageExit(stdout);
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

    if (!strcmp(command, "list")) {
        if (optind == argc) {
            list_service();
        }
        else {
            list_bucket(argc, argv, optind);
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
        put_object(argc, argv, optind);
    }
    else if (!strcmp(command, "copy")) {
    }
    else if (!strcmp(command, "get")) {
        get_object(argc, argv, optind);
    }
    else if (!strcmp(command, "head")) {
        head_object(argc, argv, optind);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        return -1;
    }

    return 0;
}
