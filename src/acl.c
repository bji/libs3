/** **************************************************************************
 * acl.c
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

#include <stdlib.h>
#include <string.h>
#include "libs3.h"
#include "request.h"

// Use a rather arbitrary max size for the document of 64K
#define ACL_XML_DOC_MAXSIZE (64 * 1024)


// get acl -------------------------------------------------------------------

typedef struct GetAclData
{
    SimpleXml simpleXml;

    S3ResponsePropertiesCallback *responsePropertiesCallback;
    S3ResponseCompleteCallback *responseCompleteCallback;
    void *callbackData;

    int *aclGrantCountReturn;
    S3AclGrant *aclGrants;
    char *ownerId;
    char *ownerDisplayName;
    string_buffer(aclXmlDocument, ACL_XML_DOC_MAXSIZE);
} GetAclData;


static S3Status getAclPropertiesCallback
    (const S3ResponseProperties *responseProperties, void *callbackData)
{
    GetAclData *gaData = (GetAclData *) callbackData;
    
    return (*(gaData->responsePropertiesCallback))
        (responseProperties, gaData->callbackData);
}


static S3Status getAclDataCallback(int bufferSize, const char *buffer,
                                   void *callbackData)
{
    GetAclData *gaData = (GetAclData *) callbackData;

    int fit;

    string_buffer_append(gaData->aclXmlDocument, buffer, bufferSize, fit);
    
    return fit ? S3StatusOK : S3StatusXmlDocumentTooLarge;
}


static void getAclCompleteCallback(S3Status requestStatus, 
                                   const S3ErrorDetails *s3ErrorDetails,
                                   void *callbackData)
{
    GetAclData *gaData = (GetAclData *) callbackData;

    if (requestStatus == S3StatusOK) {
        // Parse the document
        requestStatus = S3_convert_acl
            (gaData->aclXmlDocument, gaData->ownerId, gaData->ownerDisplayName,
             gaData->aclGrantCountReturn, gaData->aclGrants);
    }

    (*(gaData->responseCompleteCallback))
        (requestStatus, s3ErrorDetails, gaData->callbackData);

    free(gaData);
}


void S3_get_acl(const S3BucketContext *bucketContext, const char *key, 
                char *ownerId, char *ownerDisplayName,
                int *aclGrantCountReturn, S3AclGrant *aclGrants, 
                S3RequestContext *requestContext,
                const S3ResponseHandler *handler, void *callbackData)
{
    // Create the callback data
    GetAclData *gaData = (GetAclData *) malloc(sizeof(GetAclData));
    if (!gaData) {
        (*(handler->completeCallback))(S3StatusOutOfMemory, 0, callbackData);
        return;
    }

    gaData->responsePropertiesCallback = handler->propertiesCallback;
    gaData->responseCompleteCallback = handler->completeCallback;
    gaData->callbackData = callbackData;

    gaData->aclGrantCountReturn = aclGrantCountReturn;
    gaData->aclGrants = aclGrants;
    gaData->ownerId = ownerId;
    gaData->ownerDisplayName = ownerDisplayName;
    string_buffer_initialize(gaData->aclXmlDocument);
    *aclGrantCountReturn = 0;

    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeGET,                           // httpRequestType
        { bucketContext->hostName,                    // hostName
          bucketContext->bucketName,                  // bucketName
          bucketContext->protocol,                    // protocol
          bucketContext->uriStyle,                    // uriStyle
          bucketContext->accessKeyId,                 // accessKeyId
          bucketContext->secretAccessKey },           // secretAccessKey
        key,                                          // key
        0,                                            // queryParams
        "acl",                                        // subResource
        0,                                            // copySourceBucketName
        0,                                            // copySourceKey
        0,                                            // getConditions
        0,                                            // startByte
        0,                                            // byteCount
        0,                                            // putProperties
        &getAclPropertiesCallback,                    // propertiesCallback
        0,                                            // toS3Callback
        0,                                            // toS3CallbackTotalSize
        &getAclDataCallback,                          // fromS3Callback
        &getAclCompleteCallback,                      // completeCallback
        gaData                                        // callbackData
    };

    // Perform the request
    request_perform(&params, requestContext);
}


// set acl -------------------------------------------------------------------

static S3Status generateAclXmlDocument(const char *ownerId, 
                                       const char *ownerDisplayName,
                                       int aclGrantCount, 
                                       const S3AclGrant *aclGrants,
                                       int *xmlDocumentLenReturn,
                                       char *xmlDocument,
                                       int xmlDocumentBufferSize)
{
    *xmlDocumentLenReturn = 0;

#define append(fmt, ...)                                        \
    do {                                                        \
        *xmlDocumentLenReturn += snprintf                       \
            (&(xmlDocument[*xmlDocumentLenReturn]),             \
             xmlDocumentBufferSize - *xmlDocumentLenReturn - 1, \
             fmt, __VA_ARGS__);                                 \
        if (*xmlDocumentLenReturn >= xmlDocumentBufferSize) {   \
            return S3StatusXmlDocumentTooLarge;                 \
        } \
    } while (0)

    append("<AccessControlPolicy><Owner><ID>%s</ID><DisplayName>%s"
           "</DisplayName></Owner><AccessControlList>", ownerId,
           ownerDisplayName);

    int i;
    for (i = 0; i < aclGrantCount; i++) {
        append("%s", "<Grant><Grantee xmlns:xsi=\"http://www.w3.org/2001/"
               "XMLSchema-instance\" xsi:type=\"");
        const S3AclGrant *grant = &(aclGrants[i]);
        switch (grant->granteeType) {
        case S3GranteeTypeAmazonCustomerByEmail:
            append("AmazonCustomerByEmail\"><EmailAddress>%s</EmailAddress>",
                   grant->grantee.amazonCustomerByEmail.emailAddress);
            break;
        case S3GranteeTypeCanonicalUser:
            append("CanonicalUser\"><ID>%s</ID><DisplayName>%s</DisplayName>",
                   grant->grantee.canonicalUser.id, 
                   grant->grantee.canonicalUser.displayName);
            break;
        default: { // case S3GranteeTypeAllAwsUsers/S3GranteeTypeAllUsers:
            const char *grantee;
            switch (grant->granteeType) {
            case S3GranteeTypeAllAwsUsers:
                grantee = ACS_GROUP_AWS_USERS;
                break;
            case S3GranteeTypeAllUsers:
                grantee = ACS_GROUP_ALL_USERS;
                break;
            default:
                grantee = ACS_GROUP_LOG_DELIVERY;
                break;
            }
            append("Group\"><URI>%s</URI>", grantee);
        }
            break;
        }
        append("</Grantee><Permission>%s</Permission></Grant>",
               ((grant->permission == S3PermissionRead) ? "READ" :
                (grant->permission == S3PermissionWrite) ? "WRITE" :
                (grant->permission == S3PermissionReadACP) ? "READ_ACP" :
                (grant->permission == S3PermissionWriteACP) ? "WRITE_ACP" :
                "FULL_CONTROL"));
    }

    append("%s", "</AccessControlList></AccessControlPolicy>");

    return S3StatusOK;
}


typedef struct SetAclData
{
    S3ResponsePropertiesCallback *responsePropertiesCallback;
    S3ResponseCompleteCallback *responseCompleteCallback;
    void *callbackData;

    int aclXmlDocumentLen;
    char aclXmlDocument[ACL_XML_DOC_MAXSIZE];
    int aclXmlDocumentBytesWritten;

} SetAclData;


static S3Status setAclPropertiesCallback
    (const S3ResponseProperties *responseProperties, void *callbackData)
{
    SetAclData *paData = (SetAclData *) callbackData;
    
    return (*(paData->responsePropertiesCallback))
        (responseProperties, paData->callbackData);
}


static int setAclDataCallback(int bufferSize, char *buffer, void *callbackData)
{
    SetAclData *paData = (SetAclData *) callbackData;

    int remaining = (paData->aclXmlDocumentLen - 
                     paData->aclXmlDocumentBytesWritten);

    int toCopy = bufferSize > remaining ? remaining : bufferSize;
    
    if (!toCopy) {
        return 0;
    }

    memcpy(buffer, &(paData->aclXmlDocument
                     [paData->aclXmlDocumentBytesWritten]), toCopy);

    paData->aclXmlDocumentBytesWritten += toCopy;

    return toCopy;
}


static void setAclCompleteCallback(S3Status requestStatus, 
                                   const S3ErrorDetails *s3ErrorDetails,
                                   void *callbackData)
{
    SetAclData *paData = (SetAclData *) callbackData;

    (*(paData->responseCompleteCallback))
        (requestStatus, s3ErrorDetails, paData->callbackData);

    free(paData);
}


void S3_set_acl(const S3BucketContext *bucketContext, const char *key,
                const char *ownerId, const char *ownerDisplayName,
                int aclGrantCount, const S3AclGrant *aclGrants,
                S3RequestContext *requestContext,
                const S3ResponseHandler *handler, void *callbackData)
{
    if (aclGrantCount > S3_MAX_ACL_GRANT_COUNT) {
        (*(handler->completeCallback))
            (S3StatusTooManyGrants, 0, callbackData);
        return;
    }

    SetAclData *data = (SetAclData *) malloc(sizeof(SetAclData));
    if (!data) {
        (*(handler->completeCallback))(S3StatusOutOfMemory, 0, callbackData);
        return;
    }
    
    // Convert aclGrants to XML document
    S3Status status = generateAclXmlDocument
        (ownerId, ownerDisplayName, aclGrantCount, aclGrants,
         &(data->aclXmlDocumentLen), data->aclXmlDocument, 
         sizeof(data->aclXmlDocument));
    if (status != S3StatusOK) {
        free(data);
        (*(handler->completeCallback))(status, 0, callbackData);
        return;
    }

    data->responsePropertiesCallback = handler->propertiesCallback;
    data->responseCompleteCallback = handler->completeCallback;
    data->callbackData = callbackData;

    data->aclXmlDocumentBytesWritten = 0;

    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypePUT,                           // httpRequestType
        { bucketContext->hostName,                    // hostName
          bucketContext->bucketName,                  // bucketName
          bucketContext->protocol,                    // protocol
          bucketContext->uriStyle,                    // uriStyle
          bucketContext->accessKeyId,                 // accessKeyId
          bucketContext->secretAccessKey },           // secretAccessKey
        key,                                          // key
        0,                                            // queryParams
        "acl",                                        // subResource
        0,                                            // copySourceBucketName
        0,                                            // copySourceKey
        0,                                            // getConditions
        0,                                            // startByte
        0,                                            // byteCount
        0,                                            // putProperties
        &setAclPropertiesCallback,                    // propertiesCallback
        &setAclDataCallback,                          // toS3Callback
        data->aclXmlDocumentLen,                      // toS3CallbackTotalSize
        0,                                            // fromS3Callback
        &setAclCompleteCallback,                      // completeCallback
        data                                          // callbackData
    };

    // Perform the request
    request_perform(&params, requestContext);
}
