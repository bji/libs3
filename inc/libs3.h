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
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 ************************************************************************** **/

/** **************************************************************************
 * Overview
 * --------
 *
 * xxx todo
 * 
 * Threading
 * ---------
 * 
 * 1. All arguments passed to any function must not be modified directly until
 *    the function returns (obviously).
 * 2. All S3RequestContext and S3Request arguments passed to all functions may
 *    not be used by any other thread until the function returns.
 * 3. All functions may be called simultaneously by multiple threads as long
 *    as (1) and (2) are guaranteed.
 *
 ************************************************************************** **/


/** **************************************************************************
 * Constants
 ************************************************************************** **/

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
    S3StatusOK                          = 0
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
typedef enum S3CannedAcl
{
    S3CannedAclNone                     = 0, /* private */
    S3CannedAclRead                     = 1, /* public-read */
    S3CannedAclReadWrite                = 2, /* public-read-write */
    S3CannedAclAuthenticatedRead        = 3  /* authenticated-read */
} S3CannedAcl;


/** **************************************************************************
 * Data Types
 ************************************************************************** **/

/**
 * S3ResponseHeaders is passed to the header callback function which is called
 * when the complete response status code and headers have been received.
 **/
typedef struct S3ResponseHeaders
{
    /**
     * HTTP status result code
     **/
    int resultCode;
    /**
     * If non-NULL, identifies the request ID and may be used when reporting
     * problems to Amazon
     **/
    const char *requestId;
    /**
     * If non-NULL, identifies the request ID and may be used when reporting
     * problems to Amazon
     **/
    const char *requestId2;
    /**
     * If non-NULL, this is the content type of the data which is returned by
     * the request
     **/
    const char *contentType;
    /**
     * If nonnegative, this is the content length of the data which is
     * returned in the response.  A negative value means that this value was
     * not provided in the response.  A value of 0 means that there is no
     * content provided.
     **/
    int64_t contentLength;
    /**
     * If non-NULL, this names the server which serviced the request
     **/
    const char *server;
    /**
     * If non-NULL, this provides a string identifying the unique contents of
     * the resource identified by the request, such that the contents can be
     * assumed not to be changed if the same eTag is returned at a later time
     * decribing the same resource.
     **/
    const char *eTag;
    /**
     * If non-NULL, provides the last modified time, relative to the Unix
     * epoch, of the contents
     **/
    struct timeval *lastModified;
    /**
     * This is the number of user-provided metadata headers associated with
     * the resource.
     **/
    int metaHeadersCount;
    /**
     * These are the metadata headers associated with the resource.  These are
     * strings of the form:
     * x-amz-meta-${NAME}:${VALUE}
     **/
    const char **metaHeaders;
} S3ResponseHeaders;


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
    };
    /**
     * This is the S3Permission to be granted to the grantee
     **/
    S3Permission permission;
} S3AclGrant;


typedef struct ListBucketContent
{
    const char *key;
    struct timeval lastModified;
    const char *eTag;
    uint64_t size;
} ListBucketContent;


// contentType is optional
// md5 is optional
// contentDispositionFilename: Set on the object so that if someone downloads
//   it from S3 with a web browser, they will get a file dialog prompt.
// contentEncoding is optional and free-form.  This is the Content-Encoding
//   header that users downloading the object from S3 will get.  Use with
//   care.
// expires is optional
typedef struct S3OptionalHeaders
{
    const char *contentType;
    const char *md5;
    const char *contentDispositionFilename;
    const char *contentEncoding;
    struct timeval *expires;
    S3CannedAcl cannedAcl;
    int metaHeadersCount;
    // All must be prefixed by 'x-amz-meta-'
    const char **metaHeaders;
} S3PutHeaders;


/** **************************************************************************
 * Callback Signatures
 ************************************************************************** **/

typedef S3CallbackStatus (S3ResponseHeadersCallback)
    (void *pCallbackData, const S3ResponseHeadersData *pMetaData);
                                    
/**
 * Returns 0 if it wants more bucket callbacks, 1 if it wants no more.
 **/
typedef S3CallbackStatus (S3ListBucketCallback)(void *pCallbackData,
                                                const char *ownerId, 
                                                const char *ownerDisplayName,
                                                const char *bucketName,
                                                struct timeval *pCreationDate);

typedef S3CallbackStatus (S3ListBucketCallback)(void *pCallbackData,
                                                bool isTruncated,
                                                int contentsLength, 
                                                ListBucketContent *pContents);

typedef S3CallbackStatus (S3PutObjectCallback)(void *pCallbackData,
                                               int *bufferSizeReturn,
                                               char **bufferReturn);

typedef S3CallbackStatus (S3GetObjectCallback)(void *pCallbackData,
                                               int bufferSize, char *buffer);


/** **************************************************************************
 * General Library Functions
 ************************************************************************** **/

/**
 * Initialize libs3.
 **/
S3Status S3_initialize(const char *userAgentInfo);


/**
 * Must be called once per program for each occurrence of libs3_initialize().
 **/
void S3_deinitialize();


S3Status S3_validate_bucket_name(const char *bucket, S3UriStyle uriStyle);

// Converts an XML representation of an ACL to a structured representation.
// aclXml is the XML representation of the ACL aclCountReturn returns the
// number of S3Acl objects in the result pAclsReturn must be passed in as an
// array of at least S3_ACL_MAXCOUNT S3Acl structures.  The first
// [*aclCountReturn] structures will be filled in with the ACLs represented by
// the input XML.
S3Status S3_convert_acl(char *aclXml, int *aclGrantCountReturn, 
                        S3AclGrant *pAclGrantsReturn);

/**
 * Create a bucket context, for working with objects within a bucket
 **/
S3Status S3_initialize_bucket_context(S3BucketContext *pBucketContext,
                                      const char *bucketName,
                                      S3UriStyle uriStyle,
                                      const char *accessKeyId,
                                      const char *secretAccessKey);


/** **************************************************************************
 * Request Management Functions
 ************************************************************************** **/

// We limit response headers to 4K (2K of metas is all that Amazon supports,
// and we allow Amazon an additional 2K of headers)

S3Status S3_initialize_request(S3Request *pRequest, 
                               S3ResponseHeadersCallback *pCallback,
                               void *pCallbackData);

// Removes it from the request context, and must be called on all requests
S3Status S3_deinitialize_request(S3Request *pRequest);

// Removes it from the request context and finishes it
S3Status S3_complete_request(S3Request *pRequest);


/** **************************************************************************
 * Bucket Functions
 ************************************************************************** **/

/**
 * List all S3 buckets belonging to the access key id
 **/

S3Status S3_list_buckets(S3Request *pRequest, 
                         const char *accessKeyId, const char *secretAccessKey,
                         S3ListBucketCallback *pCallback);
                            

/**
 * Tests the existence of an S3 bucket, additionally returning the bucket's
 * location.
 **/

S3Status S3_test_bucket(S3Request *pRequest,
                        const char *accessKeyId, const char *secretAccessKey,
                        const char *bucketName, 
                        const char *locationConstraintReturn,
                        int locationConstraintReturnSize);
                           
/**
 * Creates a new bucket.
 **/

S3Status S3_create_bucket(S3Request *pRequest,
                          const char *accessKeyId, const char *secretAccessKey,
                          const char *bucketName, 
                          const char *locationConstraint);

/**
 * Deletes a new bucket.
 **/
S3Status S3_delete_bucket(S3Request *pRequest,
                          const char *accessKeyId, const char *secretAccessKey,
                          const char *bucketName);


S3Status S3_list_bucket(S3Request *pRequest, S3BucketContext *pBucketContext,
                        const char *prefix, const char *marker, 
                        const char *delimiter, int maxkeys,
                        S3ListBucketCallback *pCallback,
                        void *pCallbackData);


/** **************************************************************************
 * Access Control List Functions
 ************************************************************************** **/

// key is optional, if not present the ACL applies to the bucket
// aclBuffer must be less than or equal to S3_ACL_BUFFER_MAXLEN bytes in size,
// and does not need to be zero-terminated
S3Status S3_set_acl(S3Request *pRequest, S3BucketContext *pBucketContext,
                    const char *key, int aclGrantCount, 
                    S3AclGrant *pAclGrants);

S3Status S3_add_acl_grants(S3Request *pRequest, 
                           S3BucketContext *pBucketContext,
                           const char *key, int aclGrantCount, 
                           S3AclGrant *pAclGrants);

S3Status S3_remove_acl_grants(S3Request *pRequest, 
                              S3BucketContext *pBucketContext,
                              const char *key, int aclGrantsCount, 
                              S3AclGrants *pAclGrants);

S3Status S3_clear_acl(S3Request *pRequest, S3BucketContext *pBucketContext,
                      const char *key);


/** **************************************************************************
 * Object Functions
 ************************************************************************** **/

// xxx todo - possible Cache-Control
S3Status S3_put_object(S3Request *pRequest, S3BucketContext *pBucketContext,
                       const char *key, uint64_t contentLength;
                       S3OptionalHeaders *pOptionalHeaders,
                       S3PutObjectCallback *pCallback, void *pCallbackData);
                        

// destinationBucket NULL means the same bucket as in pBucketContext
// destinationKey NULL means the same object key as [key]
// if pOptionalHeaders is NULL, existing headers will not be changed
S3Status S3_copy_object(S3Request *pRequest, S3BucketContext *pBucketContext,
                        const char *key, const char *destinationBucket,
                        const char *destinationKey,
                        S3OptionalHeaders *pOptionalHeaders);

// NOTE: ensure that if Range is requested, that Range is returned, and if
// not, fail and close the request.  We expect S3 to be sensible about
// Range and anything not returned properly must indicate an error in the
// request.
// byte range is a freeform Byte Range specification string, of the form:
// MMM-NNN[,OOO-PPP...]
// We only allow complete ranges and we enforce this on the request
// The response has to have the exact same set of ranges, or it is an error.
// In this way, the caller can be sure that they will get exactly what they
// expect.
S3Status S3_get_object(S3Request *pRequest, S3BucketContext *pBucketContext,
                       const char *key, const timeval *ifModifiedSince,
                       const timeval *ifUnmodifiedSince, 
                       const char *ifMatchETag, const char *ifNotMatchETag,
                       const char *byteRange,
                       S3GetObjectCallback *pCallback, void *pCallbackData);


S3Status S3_head_object(S3Request *pRequest, S3BucketContext *pBucketContext,
                        const char *key, const timeval *ifModifiedSince,
                        const timeval *ifUnmodifiedSince, 
                        const char *ifMatchETag, const char *ifNotMatchETag);
                         

S3Status S3_delete_object(S3Request *pRequest, S3BucketContext *pBucketContext,
                          const char *key);

/** **************************************************************************
 * Request Context Management Functions
 ************************************************************************** **/

/**
 * Request context - allows multiple S3Requests to be processed at one
 * time.
 **/
S3Status S3_initialize_request_context(S3RequestContext *pContext,
                                       S3Protocol protocol);

// Removes all S3Requests but does not stop them
S3Status S3_deinitialize_request_context(S3RequestContext *pContext);

S3Status S3_add_request_to_request_context(S3RequestContext *pContext,
                                           S3Request *pRequest);

S3Status S3_remove_request_from_request_context(S3RequestContext *pContext,
                                                S3Request *pRequest);

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
// This will run the request context to completion, not by busy-waiting, but
// using select
S3Status S3_complete_request_context(S3RequestContext *pContext);


// These allow the application to control when to let libs3 do work on the
// requests.  Each call will do all the work possible without network blocking
// on all requests in the request context.
S3Status S3_runonce_request_context(S3RequestContext *pContext, 
                                    int *pRequestsRemainingReturn);
// xxx the function for getting the fdsets


/**
 * Service Logging ...
 **/
