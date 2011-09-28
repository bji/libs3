/** **************************************************************************
 * s3.c
 * 
 * Copyright 2008 Bryan Ischo <bryan@ischo.com>
 * 
 * This file is part of libs3.
 * 
 * libs3 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 3 of the License.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of this library and its programs with the
 * OpenSSL library, and distribute linked combinations including the two.
 *
 * libs3 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License version 3
 * along with libs3, in a file named COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 ************************************************************************** **/

/**
 * This is a 'driver' program that simply converts command-line input into
 * calls to libs3 functions, and prints the results.
 **/

#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "libs3.h"

// Some Windows stuff
#ifndef FOPEN_EXTRA_FLAGS
#define FOPEN_EXTRA_FLAGS ""
#endif

// Some Unix stuff (to work around Windows issues)
#ifndef SLEEP_UNITS_PER_SECOND
#define SLEEP_UNITS_PER_SECOND 1
#endif

// Also needed for Windows, because somehow MinGW doesn't define this
extern int putenv(char *);


// Command-line options, saved as globals ------------------------------------

static int forceG = 0;
static int showResponsePropertiesG = 0;
static S3Protocol protocolG = S3ProtocolHTTPS;
static S3UriStyle uriStyleG = S3UriStylePath;
static int retriesG = 5;


// Environment variables, saved as globals ----------------------------------

static const char *accessKeyIdG = 0;
static const char *secretAccessKeyG = 0;


// Request results, saved as globals -----------------------------------------

static int statusG = 0;
static char errorDetailsG[4096] = { 0 };


// Other globals -------------------------------------------------------------

static char putenvBufG[256];


// Option prefixes -----------------------------------------------------------

#define LOCATION_PREFIX "location="
#define LOCATION_PREFIX_LEN (sizeof(LOCATION_PREFIX) - 1)
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
#define ALL_DETAILS_PREFIX "allDetails="
#define ALL_DETAILS_PREFIX_LEN (sizeof(ALL_DETAILS_PREFIX) - 1)
#define NO_STATUS_PREFIX "noStatus="
#define NO_STATUS_PREFIX_LEN (sizeof(NO_STATUS_PREFIX) - 1)
#define RESOURCE_PREFIX "resource="
#define RESOURCE_PREFIX_LEN (sizeof(RESOURCE_PREFIX) - 1)
#define TARGET_BUCKET_PREFIX "targetBucket="
#define TARGET_BUCKET_PREFIX_LEN (sizeof(TARGET_BUCKET_PREFIX) - 1)
#define TARGET_PREFIX_PREFIX "targetPrefix="
#define TARGET_PREFIX_PREFIX_LEN (sizeof(TARGET_PREFIX_PREFIX) - 1)


// util ----------------------------------------------------------------------

static void S3_init()
{
    S3Status status;
    const char *hostname = getenv("S3_HOSTNAME");
    
    if ((status = S3_initialize("s3", S3_INIT_ALL, hostname))
        != S3StatusOK) {
        fprintf(stderr, "Failed to initialize libs3: %s\n", 
                S3_get_status_name(status));
        exit(-1);
    }
}


static void printError()
{
    if (statusG < S3StatusErrorAccessDenied) {
        fprintf(stderr, "\nERROR: %s\n", S3_get_status_name(statusG));
    }
    else {
        fprintf(stderr, "\nERROR: %s\n", S3_get_status_name(statusG));
        fprintf(stderr, "%s\n", errorDetailsG);
    }
}


static void usageExit(FILE *out)
{
    fprintf(out,
"\n Options:\n"
"\n"
"   Command Line:\n"
"\n"
"   -f/--force           : force operation despite warnings\n"
"   -h/--vhost-style     : use virtual-host-style URIs (default is "
                          "path-style)\n"
"   -u/--unencrypted     : unencrypted (use HTTP instead of HTTPS)\n"
"   -s/--show-properties : show response properties on stdout\n"
"   -r/--retries         : retry retryable failures this number of times\n"
"                          (default is 5)\n"
"\n"
"   Environment:\n"
"\n"
"   S3_ACCESS_KEY_ID     : S3 access key ID (required)\n"
"   S3_SECRET_ACCESS_KEY : S3 secret access key (required)\n"
"   S3_HOSTNAME          : specify alternative S3 host (optional)\n"
"\n" 
" Commands (with <required parameters> and [optional parameters]) :\n"
"\n"
"   (NOTE: all command parameters take a value and are specified using the\n"
"          pattern parameter=value)\n"
"\n"
"   help                 : Prints this help text\n"
"\n"
"   list                 : Lists owned buckets\n"
"     [allDetails]       : Show full details\n"
"\n"
"   test                 : Tests a bucket for existence and accessibility\n"
"     <bucket>           : Bucket to test\n"
"\n"
"   create               : Create a new bucket\n"
"     <bucket>           : Bucket to create\n"
"     [cannedAcl]        : Canned ACL for the bucket (see Canned ACLs)\n"
"     [location]         : Location for bucket (for example, EU)\n"
"\n"
"   delete               : Delete a bucket or key\n"
"     <bucket>[/<key>]   : Bucket or bucket/key to delete\n"
"\n"
"   list                 : List bucket contents\n"
"     <bucket>           : Bucket to list\n"
"     [prefix]           : Prefix for results set\n"
"     [marker]           : Where in results set to start listing\n"
"     [delimiter]        : Delimiter for rolling up results set\n"
"     [maxkeys]          : Maximum number of keys to return in results set\n"
"     [allDetails]       : Show full details for each key\n"
"\n"
"   getacl               : Get the ACL of a bucket or key\n"
"     <bucket>[/<key>]   : Bucket or bucket/key to get the ACL of\n"
"     [filename]         : Output filename for ACL (default is stdout)\n"
"\n"
"   setacl               : Set the ACL of a bucket or key\n"
"     <bucket>[/<key>]   : Bucket or bucket/key to set the ACL of\n"
"     [filename]         : Input filename for ACL (default is stdin)\n"
"\n"
"   getlogging           : Get the logging status of a bucket\n"
"     <bucket>           : Bucket to get the logging status of\n"
"     [filename]         : Output filename for ACL (default is stdout)\n"
"\n"
"   setlogging           : Set the logging status of a bucket\n"
"     <bucket>           : Bucket to set the logging status of\n"
"     [targetBucket]     : Target bucket to log to; if not present, disables\n"
"                          logging\n"
"     [targetPrefix]     : Key prefix to use for logs\n"
"     [filename]         : Input filename for ACL (default is stdin)\n"
"\n"
"   put                  : Puts an object\n"
"     <bucket>/<key>     : Bucket/key to put object to\n"
"     [filename]         : Filename to read source data from "
                          "(default is stdin)\n"
"     [contentLength]    : How many bytes of source data to put (required if\n"
"                          source file is stdin)\n"
"     [cacheControl]     : Cache-Control HTTP header string to associate with\n"
"                          object\n"
"     [contentType]      : Content-Type HTTP header string to associate with\n"
"                          object\n"
"     [md5]              : MD5 for validating source data\n"
"     [contentDispositionFilename] : Content-Disposition filename string to\n"
"                          associate with object\n"
"     [contentEncoding]  : Content-Encoding HTTP header string to associate\n"
"                          with object\n"
"     [expires]          : Expiration date to associate with object\n"
"     [cannedAcl]        : Canned ACL for the object (see Canned ACLs)\n"
"     [x-amz-meta-...]]  : Metadata headers to associate with the object\n"
"\n"
"   copy                 : Copies an object; if any options are set, the "
                          "entire\n"
"                          metadata of the object is replaced\n"
"     <sourcebucket>/<sourcekey> : Source bucket/key\n"
"     <destbucket>/<destkey> : Destination bucket/key\n"
"     [cacheControl]     : Cache-Control HTTP header string to associate with\n"
"                          object\n"
"     [contentType]      : Content-Type HTTP header string to associate with\n"
"                          object\n"
"     [contentDispositionFilename] : Content-Disposition filename string to\n"
"                          associate with object\n"
"     [contentEncoding]  : Content-Encoding HTTP header string to associate\n"
"                          with object\n"
"     [expires]          : Expiration date to associate with object\n"
"     [cannedAcl]        : Canned ACL for the object (see Canned ACLs)\n"
"     [x-amz-meta-...]]  : Metadata headers to associate with the object\n"
"\n"
"   get                  : Gets an object\n"
"     <buckey>/<key>     : Bucket/key of object to get\n"
"     [filename]         : Filename to write object data to (required if -s\n"
"                          command line parameter was used)\n"
"     [ifModifiedSince]  : Only return the object if it has been modified "
                          "since\n"
"                          this date\n"
"     [ifNotmodifiedSince] : Only return the object if it has not been "
                          "modified\n"
"                          since this date\n"
"     [ifMatch]          : Only return the object if its ETag header matches\n"
"                          this string\n"
"     [ifNotMatch]       : Only return the object if its ETag header does "
                          "not\n"
"                          match this string\n"
"     [startByte]        : First byte of byte range to return\n"
"     [byteCount]        : Number of bytes of byte range to return\n"
"\n"
"   head                 : Gets only the headers of an object, implies -s\n"
"     <bucket>/<key>     : Bucket/key of object to get headers of\n"
"\n"
"   gqs                  : Generates an authenticated query string\n"
"     <bucket>[/<key>]   : Bucket or bucket/key to generate query string for\n"
"     [expires]          : Expiration date for query string\n"
"     [resource]         : Sub-resource of key for query string, without a\n"
"                          leading '?', for example, \"torrent\"\n"
"\n"
" Canned ACLs:\n"
"\n"
"  The following canned ACLs are supported:\n"
"    private (default), public-read, public-read-write, authenticated-read\n"
"\n"
" ACL Format:\n"
"\n"
"  For the getacl and setacl commands, the format of the ACL list is:\n"
"  1) An initial line giving the owner id in this format:\n"
"       OwnerID <Owner ID> <Owner Display Name>\n"
"  2) Optional header lines, giving column headers, starting with the\n"
"     word \"Type\", or with some number of dashes\n"
"  3) Grant lines, of the form:\n"
"       <Grant Type> (whitespace) <Grantee> (whitespace) <Permission>\n"
"     where Grant Type is one of: Email, UserID, or Group, and\n"
"     Grantee is the identification of the grantee based on this type,\n"
"     and Permission is one of: READ, WRITE, READ_ACP, or FULL_CONTROL.\n"
"\n"
"  Note that the easiest way to modify an ACL is to first get it, saving it\n"
"  into a file, then modifying the file, and then setting the modified file\n"
"  back as the new ACL for the bucket/object.\n"
"\n"
" Date Format:\n"
"\n"
"  The format for dates used in parameters is as ISO 8601 dates, i.e.\n"
"  YYYY-MM-DDTHH:MM:SS[+/-dd:dd].  Examples:\n"
"      2008-07-29T20:36:14\n"
"      2008-07-29T20:36:14-06:00\n"
"      2008-07-29T20:36:14+11:30\n"
"\n");

    exit(-1);
}


static uint64_t convertInt(const char *str, const char *paramName)
{
    uint64_t ret = 0;

    while (*str) {
        if (!isdigit(*str)) {
            fprintf(stderr, "\nERROR: Nondigit in %s parameter: %c\n", 
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
            buf->prev->next = buf->next;
            buf->next->prev = buf->prev;
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


static int64_t parseIso8601Time(const char *str)
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
    snprintf(putenvBufG, sizeof(putenvBufG), "TZ=UTC");
    putenv(putenvBufG);

    int64_t ret = mktime(&stm);

    snprintf(putenvBufG, sizeof(putenvBufG), "TZ=%s", tz ? tz : "");
    putenv(putenvBufG);

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


// Simple ACL format:  Lines of this format:
// Type - ignored
// Starting with a dash - ignored
// Email email_address permission
// UserID user_id (display_name) permission
// Group Authenticated AWS Users permission
// Group All Users  permission
// permission is one of READ, WRITE, READ_ACP, WRITE_ACP, FULL_CONTROL
static int convert_simple_acl(char *aclXml, char *ownerId,
                              char *ownerDisplayName,
                              int *aclGrantCountReturn,
                              S3AclGrant *aclGrants)
{
    *aclGrantCountReturn = 0;
    *ownerId = 0;
    *ownerDisplayName = 0;

#define SKIP_SPACE(require_more)                \
    do {                                        \
        while (isspace(*aclXml)) {              \
            aclXml++;                           \
        }                                       \
        if (require_more && !*aclXml) {         \
            return 0;                           \
        }                                       \
    } while (0)
    
#define COPY_STRING_MAXLEN(field, maxlen)               \
    do {                                                \
        SKIP_SPACE(1);                                  \
        int len = 0;                                    \
        while ((len < maxlen) && !isspace(*aclXml)) {   \
            field[len++] = *aclXml++;                   \
        }                                               \
        field[len] = 0;                                 \
    } while (0)

#define COPY_STRING(field)                              \
    COPY_STRING_MAXLEN(field, (int) (sizeof(field) - 1))

    while (1) {
        SKIP_SPACE(0);

        if (!*aclXml) {
            break;
        }
        
        // Skip Type lines and dash lines
        if (!strncmp(aclXml, "Type", sizeof("Type") - 1) ||
            (*aclXml == '-')) {
            while (*aclXml && ((*aclXml != '\n') && (*aclXml != '\r'))) {
                aclXml++;
            }
            continue;
        }
        
        if (!strncmp(aclXml, "OwnerID", sizeof("OwnerID") - 1)) {
            aclXml += sizeof("OwnerID") - 1;
            COPY_STRING_MAXLEN(ownerId, S3_MAX_GRANTEE_USER_ID_SIZE);
            SKIP_SPACE(1);
            COPY_STRING_MAXLEN(ownerDisplayName,
                               S3_MAX_GRANTEE_DISPLAY_NAME_SIZE);
            continue;
        }

        if (*aclGrantCountReturn == S3_MAX_ACL_GRANT_COUNT) {
            return 0;
        }

        S3AclGrant *grant = &(aclGrants[(*aclGrantCountReturn)++]);

        if (!strncmp(aclXml, "Email", sizeof("Email") - 1)) {
            grant->granteeType = S3GranteeTypeAmazonCustomerByEmail;
            aclXml += sizeof("Email") - 1;
            COPY_STRING(grant->grantee.amazonCustomerByEmail.emailAddress);
        }
        else if (!strncmp(aclXml, "UserID", sizeof("UserID") - 1)) {
            grant->granteeType = S3GranteeTypeCanonicalUser;
            aclXml += sizeof("UserID") - 1;
            COPY_STRING(grant->grantee.canonicalUser.id);
            SKIP_SPACE(1);
            // Now do display name
            COPY_STRING(grant->grantee.canonicalUser.displayName);
        }
        else if (!strncmp(aclXml, "Group", sizeof("Group") - 1)) {
            aclXml += sizeof("Group") - 1;
            SKIP_SPACE(1);
            if (!strncmp(aclXml, "Authenticated AWS Users",
                         sizeof("Authenticated AWS Users") - 1)) {
                grant->granteeType = S3GranteeTypeAllAwsUsers;
                aclXml += (sizeof("Authenticated AWS Users") - 1);
            }
            else if (!strncmp(aclXml, "All Users", sizeof("All Users") - 1)) {
                grant->granteeType = S3GranteeTypeAllUsers;
                aclXml += (sizeof("All Users") - 1);
            }
            else if (!strncmp(aclXml, "Log Delivery", 
                              sizeof("Log Delivery") - 1)) {
                grant->granteeType = S3GranteeTypeLogDelivery;
                aclXml += (sizeof("Log Delivery") - 1);
            }
            else {
                return 0;
            }
        }
        else {
            return 0;
        }

        SKIP_SPACE(1);
        
        if (!strncmp(aclXml, "READ_ACP", sizeof("READ_ACP") - 1)) {
            grant->permission = S3PermissionReadACP;
            aclXml += (sizeof("READ_ACP") - 1);
        }
        else if (!strncmp(aclXml, "READ", sizeof("READ") - 1)) {
            grant->permission = S3PermissionRead;
            aclXml += (sizeof("READ") - 1);
        }
        else if (!strncmp(aclXml, "WRITE_ACP", sizeof("WRITE_ACP") - 1)) {
            grant->permission = S3PermissionWriteACP;
            aclXml += (sizeof("WRITE_ACP") - 1);
        }
        else if (!strncmp(aclXml, "WRITE", sizeof("WRITE") - 1)) {
            grant->permission = S3PermissionWrite;
            aclXml += (sizeof("WRITE") - 1);
        }
        else if (!strncmp(aclXml, "FULL_CONTROL", 
                          sizeof("FULL_CONTROL") - 1)) {
            grant->permission = S3PermissionFullControl;
            aclXml += (sizeof("FULL_CONTROL") - 1);
        }
    }

    return 1;
}

static int should_retry()
{
    if (retriesG--) {
        // Sleep before next retry; start out with a 1 second sleep
        static int retrySleepInterval = 1 * SLEEP_UNITS_PER_SECOND;
        sleep(retrySleepInterval);
        // Next sleep 1 second longer
        retrySleepInterval++;
        return 1;
    }

    return 0;
}


static struct option longOptionsG[] =
{
    { "force",                no_argument,        0,  'f' },
    { "vhost-style",          no_argument,        0,  'h' },
    { "unencrypted",          no_argument,        0,  'u' },
    { "show-properties",      no_argument,        0,  's' },
    { "retries",              required_argument,  0,  'r' },
    { 0,                      0,                  0,   0  }
};


// response properties callback ----------------------------------------------

// This callback does the same thing for every request type: prints out the
// properties if the user has requested them to be so
static S3Status responsePropertiesCallback
    (const S3ResponseProperties *properties, void *callbackData)
{
    (void) callbackData;

    if (!showResponsePropertiesG) {
        return S3StatusOK;
    }

#define print_nonnull(name, field)                                 \
    do {                                                           \
        if (properties-> field) {                                  \
            printf("%s: %s\n", name, properties-> field);          \
        }                                                          \
    } while (0)
    
    print_nonnull("Content-Type", contentType);
    print_nonnull("Request-Id", requestId);
    print_nonnull("Request-Id-2", requestId2);
    if (properties->contentLength > 0) {
        printf("Content-Length: %lld\n", 
               (unsigned long long) properties->contentLength);
    }
    print_nonnull("Server", server);
    print_nonnull("ETag", eTag);
    if (properties->lastModified > 0) {
        char timebuf[256];
        time_t t = (time_t) properties->lastModified;
        // gmtime is not thread-safe but we don't care here.
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
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
static void responseCompleteCallback(S3Status status,
                                     const S3ErrorDetails *error, 
                                     void *callbackData)
{
    (void) callbackData;

    statusG = status;
    // Compose the error details message now, although we might not use it.
    // Can't just save a pointer to [error] since it's not guaranteed to last
    // beyond this callback
    int len = 0;
    if (error && error->message) {
        len += snprintf(&(errorDetailsG[len]), sizeof(errorDetailsG) - len,
                        "  Message: %s\n", error->message);
    }
    if (error && error->resource) {
        len += snprintf(&(errorDetailsG[len]), sizeof(errorDetailsG) - len,
                        "  Resource: %s\n", error->resource);
    }
    if (error && error->furtherDetails) {
        len += snprintf(&(errorDetailsG[len]), sizeof(errorDetailsG) - len,
                        "  Further Details: %s\n", error->furtherDetails);
    }
    if (error && error->extraDetailsCount) {
        len += snprintf(&(errorDetailsG[len]), sizeof(errorDetailsG) - len,
                        "%s", "  Extra Details:\n");
        int i;
        for (i = 0; i < error->extraDetailsCount; i++) {
            len += snprintf(&(errorDetailsG[len]), 
                            sizeof(errorDetailsG) - len, "    %s: %s\n", 
                            error->extraDetails[i].name,
                            error->extraDetails[i].value);
        }
    }
}


// list service --------------------------------------------------------------

typedef struct list_service_data
{
    int headerPrinted;
    int allDetails;
} list_service_data;


static void printListServiceHeader(int allDetails)
{
    printf("%-56s  %-20s", "                         Bucket",
           "      Created");
    if (allDetails) {
        printf("  %-64s  %-12s", 
               "                            Owner ID",
               "Display Name");
    }
    printf("\n");
    printf("--------------------------------------------------------  "
           "--------------------");
    if (allDetails) {
        printf("  -------------------------------------------------"
               "---------------  ------------");
    }
    printf("\n");
}


static S3Status listServiceCallback(const char *ownerId, 
                                    const char *ownerDisplayName,
                                    const char *bucketName,
                                    int64_t creationDate, void *callbackData)
{
    list_service_data *data = (list_service_data *) callbackData;

    if (!data->headerPrinted) {
        data->headerPrinted = 1;
        printListServiceHeader(data->allDetails);
    }

    char timebuf[256];
    if (creationDate >= 0) {
        time_t t = (time_t) creationDate;
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    }
    else {
        timebuf[0] = 0;
    }

    printf("%-56s  %-20s", bucketName, timebuf);
    if (data->allDetails) {
        printf("  %-64s  %-12s", ownerId ? ownerId : "", 
               ownerDisplayName ? ownerDisplayName : "");
    }
    printf("\n");

    return S3StatusOK;
}


static void list_service(int allDetails)
{
    list_service_data data;

    data.headerPrinted = 0;
    data.allDetails = allDetails;

    S3_init();

    S3ListServiceHandler listServiceHandler =
    {
        { &responsePropertiesCallback, &responseCompleteCallback },
        &listServiceCallback
    };

    do {
        S3_list_service(protocolG, accessKeyIdG, secretAccessKeyG, 0, 0, 
                        &listServiceHandler, &data);
    } while (S3_status_is_retryable(statusG) && should_retry());

    if (statusG == S3StatusOK) {
        if (!data.headerPrinted) {
            printListServiceHeader(allDetails);
        }
    }
    else {
        printError();
    }

    S3_deinitialize();
}


// test bucket ---------------------------------------------------------------

static void test_bucket(int argc, char **argv, int optindex)
{
    // test bucket
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optindex++];

    if (optindex != argc) {
        fprintf(stderr, "\nERROR: Extraneous parameter: %s\n", argv[optindex]);
        usageExit(stderr);
    }

    S3_init();

    S3ResponseHandler responseHandler =
    {
        &responsePropertiesCallback, &responseCompleteCallback
    };

    char locationConstraint[64];
    do {
        S3_test_bucket(protocolG, uriStyleG, accessKeyIdG, secretAccessKeyG,
                       0, bucketName, sizeof(locationConstraint),
                       locationConstraint, 0, &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());

    const char *result;

    switch (statusG) {
    case S3StatusOK:
        // bucket exists
        result = locationConstraint[0] ? locationConstraint : "USA";
        break;
    case S3StatusErrorNoSuchBucket:
        result = "Does Not Exist";
        break;
    case S3StatusErrorAccessDenied:
        result = "Access Denied";
        break;
    default:
        result = 0;
        break;
    }

    if (result) {
        printf("%-56s  %-20s\n", "                         Bucket",
               "       Status");
        printf("--------------------------------------------------------  "
               "--------------------\n");
        printf("%-56s  %-20s\n", bucketName, result);
    }
    else {
        printError();
    }

    S3_deinitialize();
}


// create bucket -------------------------------------------------------------

static void create_bucket(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optindex++];

    if (!forceG && (S3_validate_bucket_name
                    (bucketName, S3UriStyleVirtualHost) != S3StatusOK)) {
        fprintf(stderr, "\nWARNING: Bucket name is not valid for "
                "virtual-host style URI access.\n");
        fprintf(stderr, "Bucket not created.  Use -f option to force the "
                "bucket to be created despite\n");
        fprintf(stderr, "this warning.\n\n");
        exit(-1);
    }

    const char *locationConstraint = 0;
    S3CannedAcl cannedAcl = S3CannedAclPrivate;
    while (optindex < argc) {
        char *param = argv[optindex++];
        if (!strncmp(param, LOCATION_PREFIX, LOCATION_PREFIX_LEN)) {
            locationConstraint = &(param[LOCATION_PREFIX_LEN]);
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
                fprintf(stderr, "\nERROR: Unknown canned ACL: %s\n", val);
                usageExit(stderr);
            }
        }
        else {
            fprintf(stderr, "\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    S3_init();

    S3ResponseHandler responseHandler =
    {
        &responsePropertiesCallback, &responseCompleteCallback
    };

    do {
        S3_create_bucket(protocolG, accessKeyIdG, secretAccessKeyG,
                         0, bucketName, cannedAcl, locationConstraint, 0,
                         &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());

    if (statusG == S3StatusOK) {
        printf("Bucket successfully created.\n");
    }
    else {
        printError();
    }
    
    S3_deinitialize();
}


// delete bucket -------------------------------------------------------------

static void delete_bucket(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optindex++];

    if (optindex != argc) {
        fprintf(stderr, "\nERROR: Extraneous parameter: %s\n", argv[optindex]);
        usageExit(stderr);
    }

    S3_init();

    S3ResponseHandler responseHandler =
    {
        &responsePropertiesCallback, &responseCompleteCallback
    };

    do {
        S3_delete_bucket(protocolG, uriStyleG, accessKeyIdG, secretAccessKeyG,
                         0, bucketName, 0, &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());

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
    int keyCount;
    int allDetails;
} list_bucket_callback_data;


static void printListBucketHeader(int allDetails)
{
    printf("%-50s  %-20s  %-5s", 
           "                       Key", 
           "   Last Modified", "Size");
    if (allDetails) {
        printf("  %-34s  %-64s  %-12s", 
               "               ETag", 
               "                            Owner ID",
               "Display Name");
    }
    printf("\n");
    printf("--------------------------------------------------  "
           "--------------------  -----");
    if (allDetails) {
        printf("  ----------------------------------  "
               "-------------------------------------------------"
               "---------------  ------------");
    }
    printf("\n");
}


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
    if ((!nextMarker || !nextMarker[0]) && contentsCount) {
        nextMarker = contents[contentsCount - 1].key;
    }
    if (nextMarker) {
        snprintf(data->nextMarker, sizeof(data->nextMarker), "%s", 
                 nextMarker);
    }
    else {
        data->nextMarker[0] = 0;
    }
    
    if (contentsCount && !data->keyCount) {
        printListBucketHeader(data->allDetails);
    }

    int i;
    for (i = 0; i < contentsCount; i++) {
        const S3ListBucketContent *content = &(contents[i]);
        char timebuf[256];
        if (0) {
            time_t t = (time_t) content->lastModified;
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ",
                     gmtime(&t));
            printf("\nKey: %s\n", content->key);
            printf("Last Modified: %s\n", timebuf);
            printf("ETag: %s\n", content->eTag);
            printf("Size: %llu\n", (unsigned long long) content->size);
            if (content->ownerId) {
                printf("Owner ID: %s\n", content->ownerId);
            }
            if (content->ownerDisplayName) {
                printf("Owner Display Name: %s\n", content->ownerDisplayName);
            }
        }
        else {
            time_t t = (time_t) content->lastModified;
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", 
                     gmtime(&t));
            char sizebuf[16];
            if (content->size < 100000) {
                sprintf(sizebuf, "%5llu", (unsigned long long) content->size);
            }
            else if (content->size < (1024 * 1024)) {
                sprintf(sizebuf, "%4lluK", 
                        ((unsigned long long) content->size) / 1024ULL);
            }
            else if (content->size < (10 * 1024 * 1024)) {
                float f = content->size;
                f /= (1024 * 1024);
                sprintf(sizebuf, "%1.2fM", f);
            }
            else if (content->size < (1024 * 1024 * 1024)) {
                sprintf(sizebuf, "%4lluM", 
                        ((unsigned long long) content->size) / 
                        (1024ULL * 1024ULL));
            }
            else {
                float f = (content->size / 1024);
                f /= (1024 * 1024);
                sprintf(sizebuf, "%1.2fG", f);
            }
            printf("%-50s  %s  %s", content->key, timebuf, sizebuf);
            if (data->allDetails) {
                printf("  %-34s  %-64s  %-12s",
                       content->eTag, 
                       content->ownerId ? content->ownerId : "",
                       content->ownerDisplayName ? 
                       content->ownerDisplayName : "");
            }
            printf("\n");
        }
    }

    data->keyCount += contentsCount;

    for (i = 0; i < commonPrefixesCount; i++) {
        printf("\nCommon Prefix: %s\n", commonPrefixes[i]);
    }

    return S3StatusOK;
}


static void list_bucket(const char *bucketName, const char *prefix,
                        const char *marker, const char *delimiter,
                        int maxkeys, int allDetails)
{
    S3_init();
    
    S3BucketContext bucketContext =
    {
        0,
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

    snprintf(data.nextMarker, sizeof(data.nextMarker), "%s", marker);
    data.keyCount = 0;
    data.allDetails = allDetails;

    do {
        data.isTruncated = 0;
        do {
            S3_list_bucket(&bucketContext, prefix, data.nextMarker,
                           delimiter, maxkeys, 0, &listBucketHandler, &data);
        } while (S3_status_is_retryable(statusG) && should_retry());
        if (statusG != S3StatusOK) {
            break;
        }
    } while (data.isTruncated && (!maxkeys || (data.keyCount < maxkeys)));

    if (statusG == S3StatusOK) {
        if (!data.keyCount) {
            printListBucketHeader(allDetails);
        }
    }
    else {
        printError();
    }

    S3_deinitialize();
}


static void list(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        list_service(0);
        return;
    }

    const char *bucketName = 0;

    const char *prefix = 0, *marker = 0, *delimiter = 0;
    int maxkeys = 0, allDetails = 0;
    while (optindex < argc) {
        char *param = argv[optindex++];
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
        else if (!strncmp(param, ALL_DETAILS_PREFIX,
                          ALL_DETAILS_PREFIX_LEN)) {
            const char *ad = &(param[ALL_DETAILS_PREFIX_LEN]);
            if (!strcmp(ad, "true") || !strcmp(ad, "TRUE") || 
                !strcmp(ad, "yes") || !strcmp(ad, "YES") ||
                !strcmp(ad, "1")) {
                allDetails = 1;
            }
        }
        else if (!bucketName) {
            bucketName = param;
        }
        else {
            fprintf(stderr, "\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    if (bucketName) {
        list_bucket(bucketName, prefix, marker, delimiter, maxkeys, 
                    allDetails);
    }
    else {
        list_service(allDetails);
    }
}

    

// delete object -------------------------------------------------------------

static void delete_object(int argc, char **argv, int optindex)
{
    (void) argc;

    // Split bucket/key
    char *slash = argv[optindex];

    // We know there is a slash in there, put_object is only called if so
    while (*slash && (*slash != '/')) {
        slash++;
    }
    *slash++ = 0;

    const char *bucketName = argv[optindex++];
    const char *key = slash;

    S3_init();
    
    S3BucketContext bucketContext =
    {
        0,
        bucketName,
        protocolG,
        uriStyleG,
        accessKeyIdG,
        secretAccessKeyG
    };

    S3ResponseHandler responseHandler =
    { 
        0,
        &responseCompleteCallback
    };

    do {
        S3_delete_object(&bucketContext, key, 0, &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());

    if ((statusG != S3StatusOK) &&
        (statusG != S3StatusErrorPreconditionFailed)) {
        printError();
    }

    S3_deinitialize();
}


// put object ----------------------------------------------------------------

typedef struct put_object_callback_data
{
    FILE *infile;
    growbuffer *gb;
    uint64_t contentLength, originalContentLength;
    int noStatus;
} put_object_callback_data;


static int putObjectDataCallback(int bufferSize, char *buffer,
                                 void *callbackData)
{
    put_object_callback_data *data = 
        (put_object_callback_data *) callbackData;
    
    int ret = 0;

    if (data->contentLength) {
        int toRead = ((data->contentLength > (unsigned) bufferSize) ?
                      (unsigned) bufferSize : data->contentLength);
        if (data->gb) {
            growbuffer_read(&(data->gb), toRead, &ret, buffer);
        }
        else if (data->infile) {
            ret = fread(buffer, 1, toRead, data->infile);
        }
    }

    data->contentLength -= ret;

    if (data->contentLength && !data->noStatus) {
        // Avoid a weird bug in MingW, which won't print the second integer
        // value properly when it's in the same call, so print separately
        printf("%llu bytes remaining ", 
               (unsigned long long) data->contentLength);
        printf("(%d%% complete) ...\n",
               (int) (((data->originalContentLength - 
                        data->contentLength) * 100) /
                      data->originalContentLength));
    }

    return ret;
}


static void put_object(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket/key\n");
        usageExit(stderr);
    }

    // Split bucket/key
    char *slash = argv[optindex];
    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (!*slash || !*(slash + 1)) {
        fprintf(stderr, "\nERROR: Invalid bucket/key name: %s\n",
                argv[optindex]);
        usageExit(stderr);
    }
    *slash++ = 0;

    const char *bucketName = argv[optindex++];
    const char *key = slash;

    const char *filename = 0;
    uint64_t contentLength = 0;
    const char *cacheControl = 0, *contentType = 0, *md5 = 0;
    const char *contentDispositionFilename = 0, *contentEncoding = 0;
    int64_t expires = -1;
    S3CannedAcl cannedAcl = S3CannedAclPrivate;
    int metaPropertiesCount = 0;
    S3NameValue metaProperties[S3_MAX_METADATA_COUNT];
    int noStatus = 0;

    while (optindex < argc) {
        char *param = argv[optindex++];
        if (!strncmp(param, FILENAME_PREFIX, FILENAME_PREFIX_LEN)) {
            filename = &(param[FILENAME_PREFIX_LEN]);
        }
        else if (!strncmp(param, CONTENT_LENGTH_PREFIX, 
                          CONTENT_LENGTH_PREFIX_LEN)) {
            contentLength = convertInt(&(param[CONTENT_LENGTH_PREFIX_LEN]),
                                       "contentLength");
            if (contentLength > (5LL * 1024 * 1024 * 1024)) {
                fprintf(stderr, "\nERROR: contentLength must be no greater "
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
                fprintf(stderr, "\nERROR: Invalid expires time "
                        "value; ISO 8601 time format required\n");
                usageExit(stderr);
            }
        }
        else if (!strncmp(param, X_AMZ_META_PREFIX, X_AMZ_META_PREFIX_LEN)) {
            if (metaPropertiesCount == S3_MAX_METADATA_COUNT) {
                fprintf(stderr, "\nERROR: Too many x-amz-meta- properties, "
                        "limit %lu: %s\n", 
                        (unsigned long) S3_MAX_METADATA_COUNT, param);
                usageExit(stderr);
            }
            char *name = &(param[X_AMZ_META_PREFIX_LEN]);
            char *value = name;
            while (*value && (*value != '=')) {
                value++;
            }
            if (!*value || !*(value + 1)) {
                fprintf(stderr, "\nERROR: Invalid parameter: %s\n", param);
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
                fprintf(stderr, "\nERROR: Unknown canned ACL: %s\n", val);
                usageExit(stderr);
            }
        }
        else if (!strncmp(param, NO_STATUS_PREFIX, NO_STATUS_PREFIX_LEN)) {
            const char *ns = &(param[NO_STATUS_PREFIX_LEN]);
            if (!strcmp(ns, "true") || !strcmp(ns, "TRUE") || 
                !strcmp(ns, "yes") || !strcmp(ns, "YES") ||
                !strcmp(ns, "1")) {
                noStatus = 1;
            }
        }
        else {
            fprintf(stderr, "\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    put_object_callback_data data;

    data.infile = 0;
    data.gb = 0;
    data.noStatus = noStatus;

    if (filename) {
        if (!contentLength) {
            struct stat statbuf;
            // Stat the file to get its length
            if (stat(filename, &statbuf) == -1) {
                fprintf(stderr, "\nERROR: Failed to stat file %s: ",
                        filename);
                perror(0);
                exit(-1);
            }
            contentLength = statbuf.st_size;
        }
        // Open the file
        if (!(data.infile = fopen(filename, "r" FOPEN_EXTRA_FLAGS))) {
            fprintf(stderr, "\nERROR: Failed to open input file %s: ",
                    filename);
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
                    fprintf(stderr, "\nERROR: Out of memory while reading "
                            "stdin\n");
                    exit(-1);
                }
                contentLength += amtRead;
                if (amtRead < (int) sizeof(buffer)) {
                    break;
                }
            }
        }
        else {
            data.infile = stdin;
        }
    }

    data.contentLength = data.originalContentLength = contentLength;

    S3_init();
    
    S3BucketContext bucketContext =
    {
        0,
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
        metaPropertiesCount,
        metaProperties
    };

    S3PutObjectHandler putObjectHandler =
    {
        { &responsePropertiesCallback, &responseCompleteCallback },
        &putObjectDataCallback
    };

    do {
        S3_put_object(&bucketContext, key, contentLength, &putProperties, 0,
                      &putObjectHandler, &data);
    } while (S3_status_is_retryable(statusG) && should_retry());

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
        fprintf(stderr, "\nERROR: Failed to read remaining %llu bytes from "
                "input\n", (unsigned long long) data.contentLength);
    }

    S3_deinitialize();
}


// copy object ---------------------------------------------------------------

static void copy_object(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: source bucket/key\n");
        usageExit(stderr);
    }

    // Split bucket/key
    char *slash = argv[optindex];
    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (!*slash || !*(slash + 1)) {
        fprintf(stderr, "\nERROR: Invalid source bucket/key name: %s\n",
                argv[optindex]);
        usageExit(stderr);
    }
    *slash++ = 0;

    const char *sourceBucketName = argv[optindex++];
    const char *sourceKey = slash;

    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: "
                "destination bucket/key\n");
        usageExit(stderr);
    }

    // Split bucket/key
    slash = argv[optindex];
    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (!*slash || !*(slash + 1)) {
        fprintf(stderr, "\nERROR: Invalid destination bucket/key name: %s\n",
                argv[optindex]);
        usageExit(stderr);
    }
    *slash++ = 0;

    const char *destinationBucketName = argv[optindex++];
    const char *destinationKey = slash;

    const char *cacheControl = 0, *contentType = 0;
    const char *contentDispositionFilename = 0, *contentEncoding = 0;
    int64_t expires = -1;
    S3CannedAcl cannedAcl = S3CannedAclPrivate;
    int metaPropertiesCount = 0;
    S3NameValue metaProperties[S3_MAX_METADATA_COUNT];
    int anyPropertiesSet = 0;

    while (optindex < argc) {
        char *param = argv[optindex++];
        if (!strncmp(param, CACHE_CONTROL_PREFIX, 
                          CACHE_CONTROL_PREFIX_LEN)) {
            cacheControl = &(param[CACHE_CONTROL_PREFIX_LEN]);
            anyPropertiesSet = 1;
        }
        else if (!strncmp(param, CONTENT_TYPE_PREFIX, 
                          CONTENT_TYPE_PREFIX_LEN)) {
            contentType = &(param[CONTENT_TYPE_PREFIX_LEN]);
            anyPropertiesSet = 1;
        }
        else if (!strncmp(param, CONTENT_DISPOSITION_FILENAME_PREFIX, 
                          CONTENT_DISPOSITION_FILENAME_PREFIX_LEN)) {
            contentDispositionFilename = 
                &(param[CONTENT_DISPOSITION_FILENAME_PREFIX_LEN]);
            anyPropertiesSet = 1;
        }
        else if (!strncmp(param, CONTENT_ENCODING_PREFIX, 
                          CONTENT_ENCODING_PREFIX_LEN)) {
            contentEncoding = &(param[CONTENT_ENCODING_PREFIX_LEN]);
            anyPropertiesSet = 1;
        }
        else if (!strncmp(param, EXPIRES_PREFIX, EXPIRES_PREFIX_LEN)) {
            expires = parseIso8601Time(&(param[EXPIRES_PREFIX_LEN]));
            if (expires < 0) {
                fprintf(stderr, "\nERROR: Invalid expires time "
                        "value; ISO 8601 time format required\n");
                usageExit(stderr);
            }
            anyPropertiesSet = 1;
        }
        else if (!strncmp(param, X_AMZ_META_PREFIX, X_AMZ_META_PREFIX_LEN)) {
            if (metaPropertiesCount == S3_MAX_METADATA_COUNT) {
                fprintf(stderr, "\nERROR: Too many x-amz-meta- properties, "
                        "limit %lu: %s\n", 
                        (unsigned long) S3_MAX_METADATA_COUNT, param);
                usageExit(stderr);
            }
            char *name = &(param[X_AMZ_META_PREFIX_LEN]);
            char *value = name;
            while (*value && (*value != '=')) {
                value++;
            }
            if (!*value || !*(value + 1)) {
                fprintf(stderr, "\nERROR: Invalid parameter: %s\n", param);
                usageExit(stderr);
            }
            *value++ = 0;
            metaProperties[metaPropertiesCount].name = name;
            metaProperties[metaPropertiesCount++].value = value;
            anyPropertiesSet = 1;
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
                fprintf(stderr, "\nERROR: Unknown canned ACL: %s\n", val);
                usageExit(stderr);
            }
            anyPropertiesSet = 1;
        }
        else {
            fprintf(stderr, "\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    S3_init();
    
    S3BucketContext bucketContext =
    {
        0,
        sourceBucketName,
        protocolG,
        uriStyleG,
        accessKeyIdG,
        secretAccessKeyG
    };

    S3PutProperties putProperties =
    {
        contentType,
        0,
        cacheControl,
        contentDispositionFilename,
        contentEncoding,
        expires,
        cannedAcl,
        metaPropertiesCount,
        metaProperties
    };

    S3ResponseHandler responseHandler =
    { 
        &responsePropertiesCallback,
        &responseCompleteCallback
    };

    int64_t lastModified;
    char eTag[256];

    do {
        S3_copy_object(&bucketContext, sourceKey, destinationBucketName,
                       destinationKey, anyPropertiesSet ? &putProperties : 0,
                       &lastModified, sizeof(eTag), eTag, 0,
                       &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());

    if (statusG == S3StatusOK) {
        if (lastModified >= 0) {
            char timebuf[256];
            time_t t = (time_t) lastModified;
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ",
                     gmtime(&t));
            printf("Last-Modified: %s\n", timebuf);
        }
        if (eTag[0]) {
            printf("ETag: %s\n", eTag);
        }
    }
    else {
        printError();
    }

    S3_deinitialize();
}


// get object ----------------------------------------------------------------

static S3Status getObjectDataCallback(int bufferSize, const char *buffer,
                                      void *callbackData)
{
    FILE *outfile = (FILE *) callbackData;

    size_t wrote = fwrite(buffer, 1, bufferSize, outfile);
    
    return ((wrote < (size_t) bufferSize) ? 
            S3StatusAbortedByCallback : S3StatusOK);
}


static void get_object(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket/key\n");
        usageExit(stderr);
    }

    // Split bucket/key
    char *slash = argv[optindex];
    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (!*slash || !*(slash + 1)) {
        fprintf(stderr, "\nERROR: Invalid bucket/key name: %s\n",
                argv[optindex]);
        usageExit(stderr);
    }
    *slash++ = 0;

    const char *bucketName = argv[optindex++];
    const char *key = slash;

    const char *filename = 0;
    int64_t ifModifiedSince = -1, ifNotModifiedSince = -1;
    const char *ifMatch = 0, *ifNotMatch = 0;
    uint64_t startByte = 0, byteCount = 0;

    while (optindex < argc) {
        char *param = argv[optindex++];
        if (!strncmp(param, FILENAME_PREFIX, FILENAME_PREFIX_LEN)) {
            filename = &(param[FILENAME_PREFIX_LEN]);
        }
        else if (!strncmp(param, IF_MODIFIED_SINCE_PREFIX, 
                     IF_MODIFIED_SINCE_PREFIX_LEN)) {
            // Parse ifModifiedSince
            ifModifiedSince = parseIso8601Time
                (&(param[IF_MODIFIED_SINCE_PREFIX_LEN]));
            if (ifModifiedSince < 0) {
                fprintf(stderr, "\nERROR: Invalid ifModifiedSince time "
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
                fprintf(stderr, "\nERROR: Invalid ifNotModifiedSince time "
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
            fprintf(stderr, "\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    FILE *outfile = 0;

    if (filename) {
        // Stat the file, and if it doesn't exist, open it in w mode
        struct stat buf;
        if (stat(filename, &buf) == -1) {
            outfile = fopen(filename, "w" FOPEN_EXTRA_FLAGS);
        }
        else {
            // Open in r+ so that we don't truncate the file, just in case
            // there is an error and we write no bytes, we leave the file
            // unmodified
            outfile = fopen(filename, "r+" FOPEN_EXTRA_FLAGS);
        }
        
        if (!outfile) {
            fprintf(stderr, "\nERROR: Failed to open output file %s: ",
                    filename);
            perror(0);
            exit(-1);
        }
    }
    else if (showResponsePropertiesG) {
        fprintf(stderr, "\nERROR: get -s requires a filename parameter\n");
        usageExit(stderr);
    }
    else {
        outfile = stdout;
    }

    S3_init();
    
    S3BucketContext bucketContext =
    {
        0,
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

    do {
        S3_get_object(&bucketContext, key, &getConditions, startByte,
                      byteCount, 0, &getObjectHandler, outfile);
    } while (S3_status_is_retryable(statusG) && should_retry());

    if (statusG != S3StatusOK) {
        printError();
    }

    fclose(outfile);

    S3_deinitialize();
}


// head object ---------------------------------------------------------------

static void head_object(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket/key\n");
        usageExit(stderr);
    }
    
    // Head implies showing response properties
    showResponsePropertiesG = 1;

    // Split bucket/key
    char *slash = argv[optindex];

    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (!*slash || !*(slash + 1)) {
        fprintf(stderr, "\nERROR: Invalid bucket/key name: %s\n",
                argv[optindex]);
        usageExit(stderr);
    }
    *slash++ = 0;

    const char *bucketName = argv[optindex++];
    const char *key = slash;

    if (optindex != argc) {
        fprintf(stderr, "\nERROR: Extraneous parameter: %s\n", argv[optindex]);
        usageExit(stderr);
    }

    S3_init();
    
    S3BucketContext bucketContext =
    {
        0,
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

    do {
        S3_head_object(&bucketContext, key, 0, &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());

    if ((statusG != S3StatusOK) &&
        (statusG != S3StatusErrorPreconditionFailed)) {
        printError();
    }

    S3_deinitialize();
}


// generate query string ------------------------------------------------------

static void generate_query_string(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket[/key]\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optindex];
    const char *key = 0;

    // Split bucket/key
    char *slash = argv[optindex++];
    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (*slash) {
        *slash++ = 0;
        key = slash;
    }
    else {
        key = 0;
    }

    int64_t expires = -1;

    const char *resource = 0;

    while (optindex < argc) {
        char *param = argv[optindex++];
        if (!strncmp(param, EXPIRES_PREFIX, EXPIRES_PREFIX_LEN)) {
            expires = parseIso8601Time(&(param[EXPIRES_PREFIX_LEN]));
            if (expires < 0) {
                fprintf(stderr, "\nERROR: Invalid expires time "
                        "value; ISO 8601 time format required\n");
                usageExit(stderr);
            }
        }
        else if (!strncmp(param, RESOURCE_PREFIX, RESOURCE_PREFIX_LEN)) {
            resource = &(param[RESOURCE_PREFIX_LEN]);
        }
        else {
            fprintf(stderr, "\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    S3_init();
    
    S3BucketContext bucketContext =
    {
        0,
        bucketName,
        protocolG,
        uriStyleG,
        accessKeyIdG,
        secretAccessKeyG
    };

    char buffer[S3_MAX_AUTHENTICATED_QUERY_STRING_SIZE];

    S3Status status = S3_generate_authenticated_query_string
        (buffer, &bucketContext, key, expires, resource);
    
    if (status != S3StatusOK) {
        printf("Failed to generate authenticated query string: %s\n",
               S3_get_status_name(status));
    }
    else {
        printf("%s\n", buffer);
    }

    S3_deinitialize();
}


// get acl -------------------------------------------------------------------

void get_acl(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket[/key]\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optindex];
    const char *key = 0;

    // Split bucket/key
    char *slash = argv[optindex++];
    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (*slash) {
        *slash++ = 0;
        key = slash;
    }
    else {
        key = 0;
    }

    const char *filename = 0;

    while (optindex < argc) {
        char *param = argv[optindex++];
        if (!strncmp(param, FILENAME_PREFIX, FILENAME_PREFIX_LEN)) {
            filename = &(param[FILENAME_PREFIX_LEN]);
        }
        else {
            fprintf(stderr, "\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    FILE *outfile = 0;

    if (filename) {
        // Stat the file, and if it doesn't exist, open it in w mode
        struct stat buf;
        if (stat(filename, &buf) == -1) {
            outfile = fopen(filename, "w" FOPEN_EXTRA_FLAGS);
        }
        else {
            // Open in r+ so that we don't truncate the file, just in case
            // there is an error and we write no bytes, we leave the file
            // unmodified
            outfile = fopen(filename, "r+" FOPEN_EXTRA_FLAGS);
        }
        
        if (!outfile) {
            fprintf(stderr, "\nERROR: Failed to open output file %s: ",
                    filename);
            perror(0);
            exit(-1);
        }
    }
    else if (showResponsePropertiesG) {
        fprintf(stderr, "\nERROR: getacl -s requires a filename parameter\n");
        usageExit(stderr);
    }
    else {
        outfile = stdout;
    }

    int aclGrantCount;
    S3AclGrant aclGrants[S3_MAX_ACL_GRANT_COUNT];
    char ownerId[S3_MAX_GRANTEE_USER_ID_SIZE];
    char ownerDisplayName[S3_MAX_GRANTEE_DISPLAY_NAME_SIZE];

    S3_init();

    S3BucketContext bucketContext =
    {
        0,
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

    do {
        S3_get_acl(&bucketContext, key, ownerId, ownerDisplayName, 
                   &aclGrantCount, aclGrants, 0, &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());

    if (statusG == S3StatusOK) {
        fprintf(outfile, "OwnerID %s %s\n", ownerId, ownerDisplayName);
        fprintf(outfile, "%-6s  %-90s  %-12s\n", " Type", 
                "                                   User Identifier",
                " Permission");
        fprintf(outfile, "------  "
                "------------------------------------------------------------"
                "------------------------------  ------------\n");
        int i;
        for (i = 0; i < aclGrantCount; i++) {
            S3AclGrant *grant = &(aclGrants[i]);
            const char *type;
            char composedId[S3_MAX_GRANTEE_USER_ID_SIZE + 
                            S3_MAX_GRANTEE_DISPLAY_NAME_SIZE + 16];
            const char *id;

            switch (grant->granteeType) {
            case S3GranteeTypeAmazonCustomerByEmail:
                type = "Email";
                id = grant->grantee.amazonCustomerByEmail.emailAddress;
                break;
            case S3GranteeTypeCanonicalUser:
                type = "UserID";
                snprintf(composedId, sizeof(composedId),
                         "%s (%s)", grant->grantee.canonicalUser.id,
                         grant->grantee.canonicalUser.displayName);
                id = composedId;
                break;
            case S3GranteeTypeAllAwsUsers:
                type = "Group";
                id = "Authenticated AWS Users";
                break;
            case S3GranteeTypeAllUsers:
                type = "Group";
                id = "All Users";
                break;
            default:
                type = "Group";
                id = "Log Delivery";
                break;
            }
            const char *perm;
            switch (grant->permission) {
            case S3PermissionRead:
                perm = "READ";
                break;
            case S3PermissionWrite:
                perm = "WRITE";
                break;
            case S3PermissionReadACP:
                perm = "READ_ACP";
                break;
            case S3PermissionWriteACP:
                perm = "WRITE_ACP";
                break;
            default:
                perm = "FULL_CONTROL";
                break;
            }
            fprintf(outfile, "%-6s  %-90s  %-12s\n", type, id, perm);
        }
    }
    else {
        printError();
    }

    fclose(outfile);

    S3_deinitialize();
}


// set acl -------------------------------------------------------------------

void set_acl(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket[/key]\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optindex];
    const char *key = 0;

    // Split bucket/key
    char *slash = argv[optindex++];
    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (*slash) {
        *slash++ = 0;
        key = slash;
    }
    else {
        key = 0;
    }

    const char *filename = 0;

    while (optindex < argc) {
        char *param = argv[optindex++];
        if (!strncmp(param, FILENAME_PREFIX, FILENAME_PREFIX_LEN)) {
            filename = &(param[FILENAME_PREFIX_LEN]);
        }
        else {
            fprintf(stderr, "\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    FILE *infile;

    if (filename) {
        if (!(infile = fopen(filename, "r" FOPEN_EXTRA_FLAGS))) {
            fprintf(stderr, "\nERROR: Failed to open input file %s: ",
                    filename);
            perror(0);
            exit(-1);
        }
    }
    else {
        infile = stdin;
    }

    // Read in the complete ACL
    char aclBuf[65536];
    aclBuf[fread(aclBuf, 1, sizeof(aclBuf), infile)] = 0;
    char ownerId[S3_MAX_GRANTEE_USER_ID_SIZE];
    char ownerDisplayName[S3_MAX_GRANTEE_DISPLAY_NAME_SIZE];
    
    // Parse it
    int aclGrantCount;
    S3AclGrant aclGrants[S3_MAX_ACL_GRANT_COUNT];
    if (!convert_simple_acl(aclBuf, ownerId, ownerDisplayName,
                            &aclGrantCount, aclGrants)) {
        fprintf(stderr, "\nERROR: Failed to parse ACLs\n");
        fclose(infile);
        exit(-1);
    }

    S3_init();

    S3BucketContext bucketContext =
    {
        0,
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

    do {
        S3_set_acl(&bucketContext, key, ownerId, ownerDisplayName,
                   aclGrantCount, aclGrants, 0, &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());
    
    if (statusG != S3StatusOK) {
        printError();
    }

    fclose(infile);

    S3_deinitialize();
}


// get logging ----------------------------------------------------------------

void get_logging(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optindex++];
    const char *filename = 0;

    while (optindex < argc) {
        char *param = argv[optindex++];
        if (!strncmp(param, FILENAME_PREFIX, FILENAME_PREFIX_LEN)) {
            filename = &(param[FILENAME_PREFIX_LEN]);
        }
        else {
            fprintf(stderr, "\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    FILE *outfile = 0;

    if (filename) {
        // Stat the file, and if it doesn't exist, open it in w mode
        struct stat buf;
        if (stat(filename, &buf) == -1) {
            outfile = fopen(filename, "w" FOPEN_EXTRA_FLAGS);
        }
        else {
            // Open in r+ so that we don't truncate the file, just in case
            // there is an error and we write no bytes, we leave the file
            // unmodified
            outfile = fopen(filename, "r+" FOPEN_EXTRA_FLAGS);
        }
        
        if (!outfile) {
            fprintf(stderr, "\nERROR: Failed to open output file %s: ",
                    filename);
            perror(0);
            exit(-1);
        }
    }
    else if (showResponsePropertiesG) {
        fprintf(stderr, "\nERROR: getlogging -s requires a filename "
                "parameter\n");
        usageExit(stderr);
    }
    else {
        outfile = stdout;
    }

    int aclGrantCount;
    S3AclGrant aclGrants[S3_MAX_ACL_GRANT_COUNT];
    char targetBucket[S3_MAX_BUCKET_NAME_SIZE];
    char targetPrefix[S3_MAX_KEY_SIZE];

    S3_init();

    S3BucketContext bucketContext =
    {
        0,
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

    do {
        S3_get_server_access_logging(&bucketContext, targetBucket, targetPrefix,
                                     &aclGrantCount, aclGrants, 0, 
                                     &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());

    if (statusG == S3StatusOK) {
        if (targetBucket[0]) {
            printf("Target Bucket: %s\n", targetBucket);
            if (targetPrefix[0]) {
                printf("Target Prefix: %s\n", targetPrefix);
            }
            fprintf(outfile, "%-6s  %-90s  %-12s\n", " Type", 
                    "                                   User Identifier",
                    " Permission");
            fprintf(outfile, "------  "
                    "---------------------------------------------------------"
                    "---------------------------------  ------------\n");
            int i;
            for (i = 0; i < aclGrantCount; i++) {
                S3AclGrant *grant = &(aclGrants[i]);
                const char *type;
                char composedId[S3_MAX_GRANTEE_USER_ID_SIZE + 
                                S3_MAX_GRANTEE_DISPLAY_NAME_SIZE + 16];
                const char *id;
                
                switch (grant->granteeType) {
                case S3GranteeTypeAmazonCustomerByEmail:
                    type = "Email";
                    id = grant->grantee.amazonCustomerByEmail.emailAddress;
                    break;
                case S3GranteeTypeCanonicalUser:
                    type = "UserID";
                    snprintf(composedId, sizeof(composedId),
                             "%s (%s)", grant->grantee.canonicalUser.id,
                             grant->grantee.canonicalUser.displayName);
                    id = composedId;
                    break;
                case S3GranteeTypeAllAwsUsers:
                    type = "Group";
                    id = "Authenticated AWS Users";
                    break;
                default:
                    type = "Group";
                    id = "All Users";
                    break;
                }
                const char *perm;
                switch (grant->permission) {
                case S3PermissionRead:
                    perm = "READ";
                    break;
                case S3PermissionWrite:
                    perm = "WRITE";
                    break;
                case S3PermissionReadACP:
                    perm = "READ_ACP";
                    break;
                case S3PermissionWriteACP:
                    perm = "WRITE_ACP";
                    break;
                default:
                    perm = "FULL_CONTROL";
                    break;
                }
                fprintf(outfile, "%-6s  %-90s  %-12s\n", type, id, perm);
            }
        }
        else {
            printf("Service logging is not enabled for this bucket.\n");
        }
    }
    else {
        printError();
    }

    fclose(outfile);

    S3_deinitialize();
}


// set logging ----------------------------------------------------------------

void set_logging(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        fprintf(stderr, "\nERROR: Missing parameter: bucket\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optindex++];

    const char *targetBucket = 0, *targetPrefix = 0, *filename = 0;

    while (optindex < argc) {
        char *param = argv[optindex++];
        if (!strncmp(param, TARGET_BUCKET_PREFIX, TARGET_BUCKET_PREFIX_LEN)) {
            targetBucket = &(param[TARGET_BUCKET_PREFIX_LEN]);
        }
        else if (!strncmp(param, TARGET_PREFIX_PREFIX, 
                          TARGET_PREFIX_PREFIX_LEN)) {
            targetPrefix = &(param[TARGET_PREFIX_PREFIX_LEN]);
        }
        else if (!strncmp(param, FILENAME_PREFIX, FILENAME_PREFIX_LEN)) {
            filename = &(param[FILENAME_PREFIX_LEN]);
        }
        else {
            fprintf(stderr, "\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    int aclGrantCount = 0;
    S3AclGrant aclGrants[S3_MAX_ACL_GRANT_COUNT];

    if (targetBucket) {
        FILE *infile;
        
        if (filename) {
            if (!(infile = fopen(filename, "r" FOPEN_EXTRA_FLAGS))) {
                fprintf(stderr, "\nERROR: Failed to open input file %s: ",
                        filename);
                perror(0);
                exit(-1);
            }
        }
        else {
            infile = stdin;
        }

        // Read in the complete ACL
        char aclBuf[65536];
        aclBuf[fread(aclBuf, 1, sizeof(aclBuf), infile)] = 0;
        char ownerId[S3_MAX_GRANTEE_USER_ID_SIZE];
        char ownerDisplayName[S3_MAX_GRANTEE_DISPLAY_NAME_SIZE];
        
        // Parse it
        if (!convert_simple_acl(aclBuf, ownerId, ownerDisplayName,
                                &aclGrantCount, aclGrants)) {
            fprintf(stderr, "\nERROR: Failed to parse ACLs\n");
            fclose(infile);
            exit(-1);
        }

        fclose(infile);
    }

    S3_init();

    S3BucketContext bucketContext =
    {
        0,
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

    do {
        S3_set_server_access_logging(&bucketContext, targetBucket, 
                                     targetPrefix, aclGrantCount, aclGrants, 
                                     0, &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());
    
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
        int idx = 0;
        int c = getopt_long(argc, argv, "fhusr:", longOptionsG, &idx);

        if (c == -1) {
            // End of options
            break;
        }

        switch (c) {
        case 'f':
            forceG = 1;
            break;
        case 'h':
            uriStyleG = S3UriStyleVirtualHost;
            break;
        case 'u':
            protocolG = S3ProtocolHTTP;
            break;
        case 's':
            showResponsePropertiesG = 1;
            break;
        case 'r': {
            const char *v = optarg;
            retriesG = 0;
            while (*v) {
                retriesG *= 10;
                retriesG += *v - '0';
                v++;
            }
            break;
        }
        default:
            fprintf(stderr, "\nERROR: Unknown option: -%c\n", c);
            // Usage exit
            usageExit(stderr);
        }
    }

    // The first non-option argument gives the operation to perform
    if (optind == argc) {
        fprintf(stderr, "\n\nERROR: Missing argument: command\n\n");
        usageExit(stderr);
    }

    const char *command = argv[optind++];
    
    if (!strcmp(command, "help")) {
        fprintf(stdout, "\ns3 is a program for performing single requests "
                "to Amazon S3.\n");
        usageExit(stdout);
    }

    accessKeyIdG = getenv("S3_ACCESS_KEY_ID");
    if (!accessKeyIdG) {
        fprintf(stderr, "Missing environment variable: S3_ACCESS_KEY_ID\n");
        return -1;
    }
    secretAccessKeyG = getenv("S3_SECRET_ACCESS_KEY");
    if (!secretAccessKeyG) {
        fprintf(stderr, 
                "Missing environment variable: S3_SECRET_ACCESS_KEY\n");
        return -1;
    }

    if (!strcmp(command, "list")) {
        list(argc, argv, optind);
    }
    else if (!strcmp(command, "test")) {
        test_bucket(argc, argv, optind);
    }
    else if (!strcmp(command, "create")) {
        create_bucket(argc, argv, optind);
    }
    else if (!strcmp(command, "delete")) {
        if (optind == argc) {
            fprintf(stderr, 
                    "\nERROR: Missing parameter: bucket or bucket/key\n");
            usageExit(stderr);
        }
        char *val = argv[optind];
        int hasSlash = 0;
        while (*val) {
            if (*val++ == '/') {
                hasSlash = 1;
                break;
            }
        }
        if (hasSlash) {
            delete_object(argc, argv, optind);
        }
        else {
            delete_bucket(argc, argv, optind);
        }
    }
    else if (!strcmp(command, "put")) {
        put_object(argc, argv, optind);
    }
    else if (!strcmp(command, "copy")) {
        copy_object(argc, argv, optind);
    }
    else if (!strcmp(command, "get")) {
        get_object(argc, argv, optind);
    }
    else if (!strcmp(command, "head")) {
        head_object(argc, argv, optind);
    }
    else if (!strcmp(command, "gqs")) {
        generate_query_string(argc, argv, optind);
    }
    else if (!strcmp(command, "getacl")) {
        get_acl(argc, argv, optind);
    }
    else if (!strcmp(command, "setacl")) {
        set_acl(argc, argv, optind);
    }
    else if (!strcmp(command, "getlogging")) {
        get_logging(argc, argv, optind);
    }
    else if (!strcmp(command, "setlogging")) {
        set_logging(argc, argv, optind);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        return -1;
    }

    return 0;
}
