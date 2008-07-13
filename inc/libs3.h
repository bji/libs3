/** **************************************************************************
 * libs3.h
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

#ifndef LIBS3_H
#define LIBS3_H

#include <stdint.h>
#include <sys/time.h>

/** **************************************************************************
 * Overview
 * --------
 *
 * xxx todo
 * NOTE: Response headers from Amazon S3 are limited to 4K (2K of metas is all
 * that Amazon supports, and Amazon is allowed an additional 2K of headers).
 * 
 * Threading
 * ---------
 * 
 * 1. All arguments passed to any function must not be modified directly until
 *    the function returns.
 * 2. All S3RequestContext and S3Request arguments passed to all functions may
 *    not be passed to any other libs3 function by any other thread until the
 *    function returns.
 * 3. All functions may be called simultaneously by multiple threads as long
 *    as (1) and (2) are observed.
 *
 ************************************************************************** **/


/** **************************************************************************
 * Constants
 ************************************************************************** **/

/**
 * This is the hostname that all S3 requests will go through; virtual-host
 * style requests will prepend the bucket name to this host name, and
 * path-style requests will use this hostname directly
 **/
#define S3_HOSTNAME "s3.amazonaws.com"


/**
 * S3_MAX_KEY_SIZE is the maximum size of keys that Amazon S3 supports.
 **/
#define S3_MAX_KEY_SIZE                 1024


/**
 * S3_MAX_METADATA_SIZE is the maximum number of bytes allowed for
 * x-amz-meta header names and values in any request passed to Amazon S3
 **/
#define S3_MAX_METADATA_SIZE            2048


/**
 * S3_METADATA_HEADER_NAME_PREFIX is the prefix of an S3 "meta header"
 **/
#define S3_METADATA_HEADER_NAME_PREFIX "x-amz-meta-"


/**
 * S3_MAX_METADATA_COUNT is the maximum number of x-amz-meta- headers that
 * could be included in a request to S3.  The smallest meta header is
 * "x-amz-meta-n: v".  Since S3 doesn't count the ": " against the total, the
 * smallest amount of data to count for a header would be the length of
 * "x-amz-meta-nv".
 **/
#define S3_MAX_METADATA_COUNT \
    (S3_MAX_METADATA_SIZE / (sizeof(S3_METADATA_HEADER_NAME_PREFIX "nv") - 1))


/**
 * S3_ACL_GRANT_MAXCOUNT is the maximum number of ACL grants that may be
 * set on a bucket or object at one time.  It is also the maximum number of
 * ACL grants that the XML ACL parsing routine will parse.
 **/
#define S3_ACL_GRANT_MAXCOUNT           100


/** **************************************************************************
 * Enumerations
 ************************************************************************** **/

/**
 * S3Status is a status code as returned by a libs3 function.
 **/
typedef enum
{
    S3StatusOK                                              ,

    // Errors that prevent the S3 request from being issued or response from
    // being read
    S3StatusFailure                                         ,
    S3StatusOutOfMemory                                     ,
    S3StatusFailedToCreateMutex                             ,
    S3StatusInvalidBucketNameTooLong                        ,
    S3StatusInvalidBucketNameFirstCharacter                 ,
    S3StatusInvalidBucketNameCharacter                      ,
    S3StatusInvalidBucketNameCharacterSequence              ,
    S3StatusInvalidBucketNameTooShort                       ,
    S3StatusInvalidBucketNameDotQuadNotation                ,
    S3StatusQueryParamsTooLong                              ,
    S3StatusFailedToCreateRequest                           ,
    S3StatusFailedToInitializeRequest                       ,
    S3StatusFailedToCreateRequestContext                    ,
    S3StatusMetaDataHeadersTooLong                          ,
    S3StatusBadMetaData                                     ,
    S3StatusBadContentType                                  ,
    S3StatusContentTypeTooLong                              ,
    S3StatusBadMD5                                          ,
    S3StatusMD5TooLong                                      ,
    S3StatusBadCacheControl                                 ,
    S3StatusCacheControlTooLong                             ,
    S3StatusBadContentDispositionFilename                   ,
    S3StatusContentDispositionFilenameTooLong               ,
    S3StatusBadContentEncoding                              ,
    S3StatusContentEncodingTooLong                          ,
    S3StatusHeadersTooLong                                  ,
    S3StatusKeyTooLong                                      ,
    S3StatusUriTooLong                                      ,
    S3StatusXmlParseFailure                                 ,
    
    // Errors from the S3 service
    S3StatusErrorAccessDenied                               ,
    S3StatusErrorAccountProblem                             ,
    S3StatusErrorAmbiguousGrantByEmailAddress               ,
    S3StatusErrorBadDigest                                  ,
    S3StatusErrorBucketAlreadyExists                        ,
    S3StatusErrorBucketAlreadyOwnedByYou                    ,
    S3StatusErrorBucketNotEmpty                             ,
    S3StatusErrorCredentialsNotSupported                    ,
    S3StatusErrorCrossLocationLoggingProhibited             ,
    S3StatusErrorEntityTooSmall                             ,
    S3StatusErrorEntityTooLarge                             ,
    S3StatusErrorExpiredToken                               ,
    S3StatusErrorIncompleteBody                             ,
    S3StatusErrorIncorrectNumberOfFilesInPostRequest        ,
    S3StatusErrorInlineDataTooLarge                         ,
    S3StatusErrorInternalError                              ,
    S3StatusErrorInvalidAccessKeyId                         ,
    S3StatusErrorInvalidAddressingHeader                    ,
    S3StatusErrorInvalidArgument                            ,
    S3StatusErrorInvalidBucketName                          ,
    S3StatusErrorInvalidDigest                              ,
    S3StatusErrorInvalidLocationConstraint                  ,
    S3StatusErrorInvalidPayer                               ,
    S3StatusErrorInvalidPolicyDocument                      ,
    S3StatusErrorInvalidRange                               ,
    S3StatusErrorInvalidSecurity                            ,
    S3StatusErrorInvalidSOAPRequest                         ,
    S3StatusErrorInvalidStorageClass                        ,
    S3StatusErrorInvalidTargetBucketForLogging              ,
    S3StatusErrorInvalidToken                               ,
    S3StatusErrorInvalidURI                                 ,
    S3StatusErrorKeyTooLong                                 ,
    S3StatusErrorMalformedACLError                          ,
    S3StatusErrorMalformedXML                               ,
    S3StatusErrorMaxMessageLengthExceeded                   ,
    S3StatusErrorMaxPostPreDataLengthExceededError          ,
    S3StatusErrorMetadataTooLarge                           ,
    S3StatusErrorMethodNotAllowed                           ,
    S3StatusErrorMissingAttachment                          ,
    S3StatusErrorMissingContentLength                       ,
    S3StatusErrorMissingSecurityElement                     ,
    S3StatusErrorMissingSecurityHeader                      ,
    S3StatusErrorNoLoggingStatusForKey                      ,
    S3StatusErrorNoSuchBucket                               ,
    S3StatusErrorNoSuchKey                                  ,
    S3StatusErrorNotImplemented                             ,
    S3StatusErrorNotSignedUp                                ,
    S3StatusErrorOperationAborted                           ,
    S3StatusErrorPermanentRedirect                          ,
    S3StatusErrorPreconditionFailed                         ,
    S3StatusErrorRedirect                                   ,
    S3StatusErrorRequestIsNotMultiPartContent               ,
    S3StatusErrorRequestTimeout                             ,
    S3StatusErrorRequestTimeTooSkewed                       ,
    S3StatusErrorRequestTorrentOfBucketError                ,
    S3StatusErrorSignatureDoesNotMatch                      ,
    S3StatusErrorSlowDown                                   ,
    S3StatusErrorTemporaryRedirect                          ,
    S3StatusErrorTokenRefreshRequired                       ,
    S3StatusErrorTooManyBuckets                             ,
    S3StatusErrorUnexpectedContent                          ,
    S3StatusErrorUnresolvableGrantByEmailAddress            ,
    S3StatusErrorUserKeyMustBeSpecified                     ,
    S3StatusErrorUnknown
} S3Status;


/**
 * S3Protocol represents a protocol that may be used for communicating a
 * request to the Amazon S3 service.
 **/
typedef enum
{
    S3ProtocolHTTPS                     = 0,
    S3ProtocolHTTP                      = 1
} S3Protocol;


/**
 * S3UriStyle defines the form that an Amazon S3 Uri identifying a bucket or
 * object can take.  They are of these forms:
 *
 * Virtual Host: ${protocol}://${bucket}.s3.amazonaws.com/[${key}]
 * Path: ${protocol}://s3.amazonaws.com/${bucket}/[${key}]
 *
 * xxx todo - I think there are some operations that can only be performed on
 * virtual host style prefixes, figure out what they are and comment here.
 **/
typedef enum
{
    S3UriStyleVirtualHost               = 0,
    S3UriStylePath                      = 1
} S3UriStyle;


/**
 * S3GranteeType defines the type of Grantee used in an S3 ACL Grant.
 **/
typedef enum
{
    S3GranteeTypeAmazonCustomerByEmail  = 0,
    S3GranteeTypeCanonicalUser          = 1,
    S3GranteeTypeAllAwsUsers            = 2,
    S3GranteeTypeAllUsers               = 3
} S3GranteeType;


/**
 * This is an individual permission granted to a grantee in an S3 ACL Grant.
 **/
typedef enum
{
    S3PermissionRead                    = 0,
    S3PermissionWrite                   = 1,
    S3PermissionReadAcp                 = 2,
    S3PermissionWriteAcp                = 3
} S3Permission;


/**
 * S3CannedAcl is an ACL that can be specified when an object is created or
 * updated.  Each canned ACL has a predefined value when expanded to a full
 * set of S3 ACL Grants.
 **/
typedef enum
{
    S3CannedAclPrivate                  = 0, /* private */
    S3CannedAclPublicRead               = 1, /* public-read */
    S3CannedAclPublicReadWrite          = 2, /* public-read-write */
    S3CannedAclAuthenticatedRead        = 3  /* authenticated-read */
} S3CannedAcl;


/**
 * S3MetaDataDirective identifies what the S3 service should do with an
 * object's metadata when the object is copied.
 **/
typedef enum
{
    S3MetaDataDirectiveCopy             = 0,
    S3MetaDataDirectiveReplace          = 1
} S3MetaDataDirective;


/** **************************************************************************
 * Data Types
 ************************************************************************** **/

struct S3Mutex;

typedef struct S3RequestContext S3RequestContext;


typedef struct S3NameValue
{
    // This is the part after x-amz-meta-
    const char *name;

    // This is the value, not including any line terminators or leading or
    // trailing whitespace.
    const char *value;
} S3NameValue;

/**
 * S3ResponseProperties is passed to the properties callback function which is
 * called when the complete response status code and properties have been
 * received.  Some of the fields of this structure are optional and may not be
 * provided in the response, and some will always be provided in the response.
 **/
typedef struct S3ResponseProperties
{
    /**
     * This optional field identifies the request ID and may be used when
     * reporting problems to Amazon.  It may or may not be provided.
     **/
    const char *requestId;
    /**
     * This optional field identifies the request ID and may be used when
     * reporting problems to Amazon.  It may or may not be provided.
     **/
    const char *requestId2;
    /**
     * This optional field is the content type of the data which is returned
     * by the request.  It may or may not be provided; if not provided, the
     * default can be assumed to be "binary/octet-stream".
     **/
    const char *contentType;
    /**
     * This optional field is the content length of the data which is returned
     * in the response.  A negative value means that this value was not
     * provided in the response.  A value of 0 means that there is no content
     * provided.
     **/
    uint64_t contentLength;
    /**
     * This optional field names the server which serviced the request.  It
     * may or may not be provided.
     **/
    const char *server;
    /**
     * This optional field provides a string identifying the unique contents
     * of the resource identified by the request, such that the contents can
     * be assumed not to be changed if the same eTag is returned at a later
     * time decribing the same resource.  It may or may not be provided.
     **/
    const char *eTag;
    /**
     * This optional field provides the last modified time, relative to the
     * Unix epoch, of the contents.  If this value is > 0, then the last
     * modified date of the contents are availableb as a number of seconds
     * since the UNIX epoch.  Note that this is second precision; HTTP
     * 
     **/
    time_t lastModified;
    /**
     * This is the number of user-provided meta data associated with the
     * resource.
     **/
    int metaDataCount;
    /**
     * These are the meta data associated with the resource.
     **/
    const S3NameValue *metaData;
} S3ResponseProperties;


/**
 * S3AclGrant identifies a single grant in the ACL for a bucket or object
 **/
typedef struct S3AclGrant
{
    /**
     * The granteeType gives the type of grantee specified by this grant.
     **/
    S3GranteeType granteeType;
    /**
     * The identifier of the grantee that is set is determined by the
     * granteeType:
     *
     * S3GranteeTypeAmazonCustomerByEmail - amazonCustomerByEmail.emailAddress
     * S3GranteeTypeCanonicalUser - canonicalUser.id, canonicalUser.displayName
     * S3GranteeTypeAllAwsUsers - none
     * S3GranteeTypeAllUsers - none
     **/
    union
    {
        /**
         * This structure is used iff the granteeType is 
         * S3GranteeTypeAmazonCustomerByEmail.
         **/
        struct
        {
            /**
             * This is the email address of the Amazon Customer being granted
             * permissions by this S3AclGrant.
             **/
            const char *emailAddress;
        } amazonCustomerByEmail;
        /**
         * This structure is used iff the granteeType is
         * S3GranteeTypeCanonicalUser.
         **/
        struct
        {
            /**
             * This is the CanonicalUser ID of the grantee
             **/
            const char *id;
            /**
             * This is the display name of the grantee
             **/
            const char *displayName;
        } canonicalUser;
    } grantee;
    /**
     * This is the S3Permission to be granted to the grantee
     **/
    S3Permission permission;
} S3AclGrant;


/**
 * A context for working with objects within a bucket.  A bucket context holds
 * all information necessary for working with a bucket, and may be used
 * repeatedly over many consecutive (or simultaneous) calls into libs3 bucket
 * operation functions.
 **/
typedef struct S3BucketContext
{
    /**
     * The name of the bucket to use in the bucket context
     **/
    const char *bucketName;
    /**
     * The protocol to use when accessing the bucket
     **/
    S3Protocol protocol;
    /**
     * The URI style to use for all URIs sent to Amazon S3 while working with
     * this bucket context
     **/
    S3UriStyle uriStyle;
    /**
     * The Amazon Access Key ID to use for access to the bucket
     **/
    const char *accessKeyId;
    /**
     *  The Amazon Secret Access Key to use for access to the bucket
     **/
    const char *secretAccessKey;
} S3BucketContext;


/**
 * This is a single entry supplied to the list bucket callback by a call to
 * S3_list_bucket.  It identifies a single matching key from the list
 * operation.
 **/
typedef struct S3ListBucketContent
{
    /**
     * This is the next key in the list bucket results.
     **/
    const char *key;
    /**
     * This is the number of seconds since UNIX epoch of the last modified
     * date of the object identified by the key. 
     **/
    time_t lastModified;
    /**
     * This gives a tag which gives a signature of the contents of the object.
     **/
    const char *eTag;
    /**
     * This is the size of the object
     **/
    uint64_t size;
    /**
     * This is the ID of the owner of the key; it is present only if
     * access permissions allow it to be viewed.
     **/
    const char *ownerId;
    /**
     * This is the display name of the owner of the key; it is present only if
     * access permissions allow it to be viewed.
     **/
    const char *ownerDisplayName;
} S3ListBucketContent;


/**
 * S3PutProperties is the set of properties that may optionally be set by the
 * user when putting objects to S3.  Each field of this structure is optional
 * and may or may not be present.
 **/

/*
// contentType is optional
// md5 is optional
// contentDispositionFilename: Set on the object so that if someone downloads
//   it from S3 with a web browser, they will get a file dialog prompt.
// contentEncoding is optional and free-form.  This is the Content-Encoding
//   header that users downloading the object from S3 will get.  Use with
//   care.
// expires is optional
*/
typedef struct S3PutProperties
{
    /**
     * If present, this is the Content-Type that should be associated with the
     * object.  If not provided, S3 defaults to "binary/octet-stream".
     **/
    const char *contentType;
    /**
     * If present, this provides the MD5 signature of the contents, and is
     * used to validate the contents.  This is highly recommended by Amazon
     * but not required.
     **/
    const char *md5;
    /**
     * If present, this gives a Cache-Control header string to be supplied to
     * HTTP clients which download this
     **/
    const char *cacheControl;
    /**
     * If present, this gives the filename to save the downloaded file to,
     * whenever the object is downloaded via a web browser.  This is only
     * relevent for objects which are intended to be shared to users via web
     * browsers and which is additionally intended to be downloaded rather
     * than viewed.
     **/
    const char *contentDispositionFilename;
    /**
     * If present, this identifies the content encoding of the object.  This
     * is only applicable to encoded (usually, compressed) content, and only
     * relevent if the object is intended to be downloaded via a browser.
     **/
    const char *contentEncoding;
    /**
     * If >= 0, this gives an expiration date for the content.  This
     * information is typically only delivered to users who download the
     * content via a web browser.
     **/
    time_t expires;
    /**
     * This identifies the "canned ACL" that should be used for this object.
     * The default (0) gives only the owner of the object access to it.
     * This value is ignored for all operations except put_object and
     * copy_object.
     **/
    S3CannedAcl cannedAcl;
    /**
     * For copy operations, this identifies the source object; must be
     * of the form /source_bucket/source_key.  This value is ignored for all
     * operations except copy_object.
     **/
    const char *sourceObject;
    /**
     * For copy operations, this gives the metadata directive.  This value is
     * ignored for all operations except copy_object.
     **/
    S3MetaDataDirective metaDataDirective;
    /**
     * This is the number of values in the metaData field.
     **/
    int metaDataCount;
    /**
     * These are the meta data to pass to S3.
     **/
    const S3NameValue *metaData;
} S3PutProperties;


// Used for get object or head object, specify properties for controlling
// the get/head
typedef struct S3GetProperties
{
    /**
     * If >= 0, ...
     **/
    time_t ifModifiedSince;
    /**
     * If >= 0 ...
     **/
    time_t ifNotModifiedSince;
    /**
     * If present ...
     **/
    const char *ifMatchETag;
    /**
     * If present ...
     **/
    const char *ifNotMatchETag;
} S3GetProperties;


typedef struct S3ErrorDetails
{
    const char *message;

    const char *resource;

    const char *furtherDetails;

    int extraDetailsCount;

    S3NameValue *extraDetails;
} S3ErrorDetails;


/** **************************************************************************
 * Callback Signatures
 ************************************************************************** **/

typedef unsigned long (S3ThreadSelfCallback)();


typedef struct S3Mutex *(S3MutexCreateCallback)();


typedef void (S3MutexLockCallback)(struct S3Mutex *mutex);


typedef void (S3MutexUnlockCallback)(struct S3Mutex *mutex);


typedef void (S3MutexDestroyCallback)(struct S3Mutex *mutex);


/**
 * This callback is made whenever the response properties become available for
 * any request.
 *
 * @param callbackData is the callback data as specified when the S3Request
 *        for which this callback was specified was initialized
 * @param properties is the properties that are available from the response.
 * @return S3Status???
 **/
typedef S3Status (S3ResponsePropertiesCallback)
    (const S3ResponseProperties *properties, void *callbackData);

typedef void (S3ResponseCompleteCallback)(S3Status status,
                                          int httpResponseCode,
                                          const S3ErrorDetails *errorDetails,
                                          void *callbackData);

                                    
/**
 * This callback is made for each bucket resulting from a list service
 * operation.
 *
 * @param callbackData is the callback data as specified when the S3Request
 *        for which this callback was specified was initialized
 * @param ownerId is the ID of the owner of the bucket
 * @param ownerDisplayName is the owner display name of the owner of the bucket
 * @param bucketName is the name of the bucket
 * @param creationDateSeconds if < 0 indicates that no creation date was
 *        supplied for the bucket; if > 0 indicates the number of seconds
 *        since UNIX Epoch of the creation date of the bucket
 * @return S3Status???
 **/
typedef S3Status (S3ListServiceCallback)(const char *ownerId, 
                                         const char *ownerDisplayName,
                                         const char *bucketName,
                                         time_t creationDateSeconds,
                                         void *callbackData);


/**
 * This callback is made once for each object resulting from a list bucket
 * operation.
 *
 * @param callbackData is the callback data as specified when the S3Request
 *        for which this callback was specified was initialized
 * @param isTruncated is true if the list bucket request was truncated by the
 *        S3 service, in which case the remainder of the list may be obtained
 *        by querying again using the Marker parameter to start the query
 *        after this set of results
 * @param nextMarker if present, gives the largest (alphabetically) key
 *        returned in the response, which, if isTruncated is true, may be used
 *        as the marker in a subsequent list buckets operation to continue
 *        listing
 * @param contentsLength is the number of ListBucketContent structures in the
 *        contents parameter
 * @param contents is an array of ListBucketContent structures, each one
 *        describing an object in the bucket
 * @return S3Status???
 **/
typedef S3Status (S3ListBucketCallback)(int isTruncated,
                                        const char *nextMarker,
                                        int contentsCount, 
                                        const S3ListBucketContent *contents,
                                        int commonPrefixesCount,
                                        const char **commonPrefixes,
                                        void *callbackData);
                                       

/**
 * This callback is made during a put object operation, to obtain the next
 * chunk of data to put to the S3 service as the contents of the object.  This
 * callback is made repeatedly, each time acquiring the next chunk of data to
 * write to the service, until either the return code is ??? or
 * bufferSizeReturn is returned as 0, indicating that there is no more data to
 * put to the service.
 *
 * @param callbackData is the callback data as specified when the S3Request
 *        for which this callback was specified was initialized
 * @param bufferSizeReturn returns the number of bytes that are being returned
 *        in the bufferReturn parameter
 * @param bufferReturn returns the bext set of bytes to be written to the
 *        service as the contents of the object being put
 * @return S3Status???
 **/
typedef int (S3PutObjectDataCallback)(int bufferSize, char *buffer,
                                      void *callbackData);


/**
 * This callback is made during a get object operation, to provide the next
 * chunk of data available from the S3 service constituting the contents of
 * the object being fetched.  This callback is made repeatedly, each time
 * providing the next chunk of data read, until the complete object contents
 * have been passed through the callback in this way, or the callback
 * returns ???.
 *
 * @param callbackData is the callback data as specified when the S3Request
 *        for which this callback was specified was initialized
 * @param bufferSize gives the number of bytes in buffer
 * @param buffer is the data being passed into the callback
 **/
typedef S3Status (S3GetObjectDataCallback)(int bufferSize, const char *buffer,
                                           void *callbackData);
                                       

/** **************************************************************************
 * General Library Functions
 ************************************************************************** **/

/**
 * Initializes libs3 for use.  This function must be called before any other
 * libs3 function is called.  It must be called once and only once before
 * S3_deinitialize() is called.
 *
 * @param userAgentInfo is a string that will be included in the User-Agent
 *        header of every request made to the S3 service.  You may provide
 *        NULL or the empty string if you don't care about this.  The value
 *        will not be copied by this function and must remain unaltered by the
 *        caller until S3_deinitialize() is called.
 * @return S3Status ???
 **/
S3Status S3_initialize(const char *userAgentInfo,
                       S3ThreadSelfCallback *threadSelfCallback,
                       S3MutexCreateCallback *mutexCreateCallback,
                       S3MutexLockCallback *mutexLockCallback,
                       S3MutexUnlockCallback *mutexUnlockCallback,
                       S3MutexDestroyCallback *mutexDestroyCallback);


/**
 * Must be called once per program for each call to libs3_initialize().  After
 * this call is complete, no libs3 function may be called except
 * S3_initialize().
 **/
void S3_deinitialize();


/**
 * Returns a string with the textual name of an S3Status code
 *
 * @return a string with the textual name of an S3Status code
 **/
const char *S3_get_status_name(S3Status status);


/**
 * This function may be used to validate an S3 bucket name as being in the
 * correct form for use with the S3 service.  Amazon S3 limits the allowed
 * characters in S3 bucket names, as well as imposing some additional rules on
 * the length of bucket names and their structure.  There are actually two
 * limits; one for bucket names used only in path-style URIs, and a more
 * strict limit used for bucket names used in virtual-host-style URIs.  It is
 * advisable to use only bucket names which meet the more strict requirements
 * regardless of how the bucket expected to be used.
 *
 * This method does NOT validate that the bucket is available for use in the
 * S3 service, so the return value of this function cannot be used to decide
 * whether or not a bucket with the give name already exists in Amazon S3 or
 * is accessible by the caller.  It merely validates that the bucket name is
 * valid for use with S3.
 *
 * @param *bucketName is the bucket name to validate
 * @param uriStyle gives the URI style to validate the bucket name against.
 *        It is advisable to always use S3UriStyleVirtuallHost.
 * @return S3Status ???
 **/
S3Status S3_validate_bucket_name(const char *bucketName, S3UriStyle uriStyle);


/**
 * Converts an XML representation of an ACL to a libs3 structured
 * representation.  This method is not strictly necessary for working with
 * ACLs using libs3, but may be convenient for users of the library who read
 * ACLs from elsewhere in XML format and need to use these ACLs with libs3.
 *
 * @param aclXml is the XML representation of the ACL.  This must be a
 *        zero-terminated character string in ASCII format.
 * @param aclGrantCountReturn returns the number of S3AclGrant structures
 *        returned in the aclGrantsReturned array
 * @param aclGransReturned must be passed in as an array of at least
 *        S3_ACL_MAXCOUNT structures, and on return from this function, the
 *        first aclGrantCountReturn structures will be filled in with the ACLs
 *        represented by the input XML.
 * @return S3Status ???
 **/
S3Status S3_convert_acl(char *aclXml, int *aclGrantCountReturn,
                        S3AclGrant *aclGrantsReturn);


/** **************************************************************************
 * Request Context Management Functions
 ************************************************************************** **/

/**
 * Request context - allows multiple S3Requests to be processed at one
 * time.
 **/
S3Status S3_create_request_context(S3RequestContext **requestContextReturn);


/*
// Cancels all live S3Requests in the request context
*/
void S3_destroy_request_context(S3RequestContext *requestContext);


/**
 * Some methods for driving a request context:
 * - Run it to completion
 * - Run it "once"
 * - Get its fds so that someone else can select on them before calling
 *   the "run it once" method
 *
 * As each S3Request within an S3RequestContext completes, it will be
 * automatically removed from the S3RequestContext.  Also if any callback
 * returns a "stop" status then the request will be stopped and removed
 * from the S3RequestContext.  There is thus no way to stop a request from
 * completing without returning a "stop" status.  However, the entire
 * request context can be deinitialized which will stop all requests in it.
 **/
/*
// This will run the request context to completion, not by busy-waiting, but
// using select
*/
S3Status S3_runall_request_context(S3RequestContext *requestContext);


/*
// These allow the application to control when to let libs3 do work on the
// requests.  Each call will do all the work possible without network blocking
// on all requests in the request context.
*/
S3Status S3_runonce_request_context(S3RequestContext *requestContext, 
                                    int *requestsRemainingReturn);

/*
// xxx todo the function for getting the fdsets
*/

typedef struct S3ResponseHandler
{
    // Properties callback
    S3ResponsePropertiesCallback *propertiesCallback;
    
    // Request complete callback - always called if the call which initiates
    // the request doesn't return an error code
    S3ResponseCompleteCallback *completeCallback;
} S3ResponseHandler;


typedef struct S3ListServiceHandler
{
    S3ResponseHandler responseHandler;

    // list buckets callback
    S3ListServiceCallback *listServiceCallback;
} S3ListServiceHandler;


typedef struct S3ListBucketHandler
{
    S3ResponseHandler responseHandler;

    S3ListBucketCallback *listBucketCallback;
} S3ListBucketHandler;


typedef struct S3PutObjectHandler
{
    S3ResponseHandler responseHandler;

    S3PutObjectDataCallback *putObjectDataCallback;
} S3PutObjectHandler;


typedef struct S3GetObjectHandler
{
    S3ResponseHandler responseHandler;

    S3GetObjectDataCallback *getObjectDataCallback;
} S3GetObjectHandler;


/** **************************************************************************
 * Service Functions
 ************************************************************************** **/

/**
 * Sets up an S3Request to lists all S3 buckets belonging to the access key
 * id.
 *
 * @param requestContext if non-NULL, gives the S3RequestContext to add this
 *        request to, and does not perform the request immediately.  If NULL,
 *        performs the request immediately and synchronously.
 * @param accessKeyId gives the Amazon Access Key ID for which to list owned
 *        buckets
 * @param secretAccessKey gives the Amazon Secret Access Key for which to list
 *        owned buckets
 * @param callback will be called back once for each bucket listed in the
 *        response to this operation
 * @param callbackData will be passed in as the callbackData parameter to
 *        callback
 **/
void S3_list_service(S3Protocol protocol, const char *accessKeyId,
                     const char *secretAccessKey,
                     S3RequestContext *requestContext,
                     S3ListServiceHandler *handler, void *callbackData);
                         
                            
/** **************************************************************************
 * Bucket Functions
 ************************************************************************** **/

/**
 * Tests the existence of an S3 bucket, additionally returning the bucket's
 * location if it exists and is accessible.
 *
 * @param requestContext if non-NULL, gives the S3RequestContext to add this
 *        request to, and does not perform the request immediately.  If NULL,
 *        performs the request immediately and synchronously.
 * @param accessKeyId gives the Amazon Access Key ID for which to test the
 *        exitence and accessibility of the bucket
 * @param secretAccessKey gives the Amazon Secret Access Key for which to test
 *        the exitence and accessibility of the bucket
 * @param bucketName is the bucket name to test
 * @param locationConstraintReturnSize gives the number of bytes in the
 *        locationConstraintReturn parameter
 * @param locationConstraintReturn provides the location into which to write
 *        the name of the location constraint naming the geographic location
 *        of the S3 bucket.  This must have at least as many characters in it
 *        as specified by locationConstraintReturn, and should start out
 *        NULL-terminated.  On successful return of this function, this will
 *        be set to the name of the geographic location of S3 bucket, or will
 *        be left as a zero-length string if no location was available.
 **/
void S3_test_bucket(S3Protocol protocol, S3UriStyle uriStyle,
                    const char *accessKeyId, const char *secretAccessKey,
                    const char *bucketName, int locationConstraintReturnSize,
                    char *locationConstraintReturn,
                    S3RequestContext *requestContext,
                    S3ResponseHandler *handler, void *callbackData);

                           
/**
 * Creates a new bucket.
 *
 * @param requestContext if non-NULL, gives the S3RequestContext to add this
 *        request to, and does not perform the request immediately.  If NULL,
 *        performs the request immediately and synchronously.
 * @param accessKeyId gives the Amazon Access Key ID for the owner of the
 *        bucket which will be created
 * @param secretAccessKey gives the Amazon Secret Access Key for the owner of
 *        the bucket which will be created
 * @param bucketName is the name of the bucket to be created
 * @param locationConstraint, if non-NULL, gives the geographic location for
 *        the bucket to create.
 * @return S3Status ???
 **/
void S3_create_bucket(S3Protocol protocol, const char *accessKeyId,
                      const char *secretAccessKey, const char *bucketName, 
                      S3CannedAcl cannedAcl, const char *locationConstraint,
                      S3RequestContext *requestContext,
                      S3ResponseHandler *handler, void *callbackData);


/**
 * Deletes a bucket.  The bucket must be empty.
 *
 * @param requestContext if non-NULL, gives the S3RequestContext to add this
 *        request to, and does not perform the request immediately.  If NULL,
 *        performs the request immediately and synchronously.
 * @param accessKeyId gives the Amazon Access Key ID for the bucket
 * @param secretAccessKey gives the Amazon Secret Access Key for the bucket
 * @param bucketName is the name of the bucket to be deleted
 * @return S3Status ???
 **/
void S3_delete_bucket(S3Protocol protocol, S3UriStyle uriStyle,
                      const char *accessKeyId, const char *secretAccessKey,
                      const char *bucketName, S3RequestContext *requestContext,
                      S3ResponseHandler *handler, void *callbackData);


/**
 * Lists keys within a bucket.
 *
 * @param requestContext if non-NULL, gives the S3RequestContext to add this
 *        request to, and does not perform the request immediately.  If NULL,
 *        performs the request immediately and synchronously.
 * @param bucketContext gives the bucket and associated parameters for this
 *        request
 * @param prefix if present, gives a prefix for matching keys
 * @param marker if present, only keys occuring after this value will be
 *        listed
 * @param delimiter if present, causes keys that contain the same string
 *        between the prefix and the first occurrence of the delimiter to be
 *        rolled up into a single result element
 * @param maxkeys is the maximum number of keys to return
 * @param callback is the callback which will be called repeatedly with
 *        resulting keys
 * @param callbackData will be passed into the callback
 * @return S3Status ???
 **/
void S3_list_bucket(S3BucketContext *bucketContext,
                    const char *prefix, const char *marker, 
                    const char *delimiter, int maxkeys,
                    S3RequestContext *requestContext,
                    S3ListBucketHandler *handler, void *callbackData);


/**
 * xxx todo - document remaining functions
 **/

/** **************************************************************************
 * Object Functions
 ************************************************************************** **/

/*
// xxx todo - possible Cache-Control
*/
void S3_put_object(S3BucketContext *bucketContext, const char *key,
                   uint64_t contentLength,
                   const S3PutProperties *putProperties,
                   S3RequestContext *requestContext,
                   S3PutObjectHandler *handler, void *callbackData);
                        

/*
// destinationBucket NULL means the same bucket as in pBucketContext
// destinationKey NULL means the same object key as [key]
// if putProperties is NULL, existing properties will not be changed
*/
void S3_copy_object(S3BucketContext *bucketContext,
                    const char *key, const char *destinationBucket,
                    const char *destinationKey,
                    const S3PutProperties *putProperties,
                    S3RequestContext *requestContext,
                    S3ResponseHandler *handler, void *callbackData);


/*
// NOTE: ensure that if Range is requested, that Range is returned, and if
// not, fail and close the request.  We expect S3 to be sensible about
// Range and anything not returned properly must indicate an error in the
// request.
// byteRangeCount == 0 means get everything
// We only allow complete ranges and we enforce this on the request
// The response has to have the exact same set of ranges, or it is an error.
// In this way, the caller can be sure that they will get exactly what they
// expect.
// ifModifiedSince and ifUnmodifiedSince if > 0 will be used
*/
void S3_get_object(S3BucketContext *bucketContext, const char *key,
                   const S3GetProperties *getProperties,
                   uint64_t startByte, uint64_t byteCount,
                   S3RequestContext *requestContext,
                   S3GetObjectHandler *handler, void *callbackData);


// ifModifiedSince and ifUnmodifiedSince if > 0 will be used
void S3_head_object(S3BucketContext *bucketContext, const char *key,
                    const S3GetProperties *getProperties,
                    S3RequestContext *requestContext,
                    S3ResponseHandler *handler, void *callbackData);
                         

void S3_delete_object(S3BucketContext *bucketContext, const char *key,
                      S3RequestContext *requestContext,
                      S3ResponseHandler *handler, void *callbackData);


/** **************************************************************************
 * Access Control List Functions
 ************************************************************************** **/

/*
// key is optional, if not present the ACL applies to the bucket
// aclBuffer must be less than or equal to S3_ACL_BUFFER_MAXLEN bytes in size,
// and does not need to be zero-terminated
*/
void S3_set_acl(S3BucketContext *bucketContext, const char *key, 
                int aclGrantCount, S3AclGrant *aclGrants, 
                S3RequestContext *requestContext,
                S3ResponseHandler *handler, void *callbackData);


void S3_add_acl_grants(S3BucketContext *bucketContext, const char *key,
                       int aclGrantCount, S3AclGrant *aclGrants,
                       S3RequestContext *requestContext,
                       S3ResponseHandler *handler, void *callbackData);
                           

void S3_remove_acl_grants(S3BucketContext *bucketContext, const char *key,
                          int aclGrantsCount, S3AclGrant *aclGrants,
                          S3RequestContext *requestContext,
                          void *callbackData);


void S3_clear_acl(S3BucketContext *bucketContext, const char *key,
                  S3RequestContext *requestContext,
                  S3ResponseHandler *hander, void *callbackData);


/**
 * xxx todo
 * Service Logging ...
 **/

/**
 * xxx todo
 * function for generating an HTTP authenticated query string
 **/

/**
 * xxx todo
 * functions for generating form stuff for posting to s3
 **/


#endif /* LIBS3_H */
