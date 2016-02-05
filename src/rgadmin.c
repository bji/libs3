#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rgadmin.h"
#include "request.h"
#include "simplexml.h"


typedef struct RGGetUserInfoPrivate
{
    RGGetUserInfo getUserInfo;
    RGGetUserInfoHandler handler;
    void *callbackData;
    SimpleXml simpleXml;
}
RGGetUserInfoPrivate;

static inline int string_append(
    char *dest,
    size_t dest_len,
    const char *newData,
    int newDataLen)
{
    size_t oldlen = strlen(dest);
    size_t len = oldlen + newDataLen;

    if (len >= dest_len) {
        return S3StatusOutOfMemory;
    }

    memcpy(dest + oldlen, newData, newDataLen);
    dest[oldlen + newDataLen] = 0;

    return S3StatusOK;
}

static S3Status RGgetUserInfoXmlCallback(const char *elementPath,
                                   const char *data, int dataLen,
                                   void *callbackData)
{
    RGGetUserInfoPrivate *getUserInfoPrivate = callbackData;
    RGGetUserInfo * getUserInfo = &getUserInfoPrivate->getUserInfo;
    S3Status rc = S3StatusOK;

    if (data) {
        if (!strcmp(elementPath, "user_info/user_id")) {
            rc = string_append(getUserInfo->userId, sizeof(getUserInfo->userId), data, dataLen);
        }
        else if (!strcmp(elementPath, "user_info/display_name")) {
            rc = string_append(getUserInfo->displayName, sizeof(getUserInfo->displayName), data, dataLen);
        }
        else if (!strcmp(elementPath, "user_info/email")) {
            rc = string_append(getUserInfo->email, sizeof(getUserInfo->email), data, dataLen);
        }
        else if (!strcmp(elementPath, "user_info/suspended")) {
            getUserInfo->suspended = atoi(data);
        }
        else if (!strcmp(elementPath, "user_info/max_buckets")) {
            getUserInfo->maxBuckets = atoi(data);
        }
        else if (!strncmp(elementPath, "user_info/subusers/", strlen("user_info/subusers/"))) {
            int i = getUserInfo->subUsersCount;

            if (i == sizeof(getUserInfo->subUsers) / sizeof(getUserInfo->subUsers[0])) {
                rc = S3StatusOutOfMemory;
                goto done;
            }

            if (!strcmp(elementPath, "user_info/subusers/id")) {
                rc = string_append(getUserInfo->subUsers[i].id, sizeof(getUserInfo->subUsers[i].id), data, dataLen);
            }
            else if (!strcmp(elementPath, "user_info/subusers/permissions")) {
                rc = string_append(getUserInfo->subUsers[i].permissions, sizeof(getUserInfo->subUsers[i].permissions), data, dataLen);
            }
        }
        else if (!strncmp(elementPath, "user_info/keys/key", strlen("user_info/keys/key"))) {
            int i = getUserInfo->keysCount;

            if (i == sizeof(getUserInfo->keys) / sizeof(getUserInfo->keys[0])) {
                rc = S3StatusOutOfMemory;
                goto done;
            }

            if (!strcmp(elementPath, "user_info/keys/key/user")) {
                rc = string_append(getUserInfo->keys[i].user, sizeof(getUserInfo->keys[i].user), data, dataLen);
            }
            else if (!strcmp(elementPath, "user_info/keys/key/access_key")) {
                rc = string_append(getUserInfo->keys[i].accessKeyId, sizeof(getUserInfo->keys[i].accessKeyId), data, dataLen);
            }
            else if (!strcmp(elementPath, "user_info/keys/key/secret_key")) {
                rc = string_append(getUserInfo->keys[i].secretAccessKey, sizeof(getUserInfo->keys[i].secretAccessKey),  data, dataLen);
            }
        }
        else if (!strncmp(elementPath, "user_info/swift_keys/key", strlen("user_info/swift_keys/key"))) {
            int i = getUserInfo->swiftKeysCount;

            if (i == sizeof(getUserInfo->swiftKeys) / sizeof(getUserInfo->swiftKeys[0])) {
                rc = S3StatusOutOfMemory;
                goto done;
            }


            if (!strcmp(elementPath, "user_info/swift_keys/key/user")) {
                rc = string_append(getUserInfo->swiftKeys[i].user, sizeof(getUserInfo->swiftKeys[i].user), data, dataLen);
            }
            else if (!strcmp(elementPath, "user_info/swift_keys/key/secret_key")) {
                rc = string_append(getUserInfo->swiftKeys[i].secretKey, sizeof(getUserInfo->swiftKeys[i].secretKey), data, dataLen);
            }
        }
        else if (!strncmp(elementPath, "user_info/caps/cap", strlen("user_info/caps/cap"))) {
            int i = getUserInfo->capsCount;

            if (i == sizeof(getUserInfo->caps) / sizeof(getUserInfo->caps[0])) {
                rc = S3StatusOutOfMemory;
                goto done;
            }

            if (!strcmp(elementPath, "user_info/caps/cap/type")) {
                rc = string_append(getUserInfo->caps[i].type, sizeof(getUserInfo->caps[i].type), data, dataLen);
            }
            else if (!strcmp(elementPath, "user_info/caps/cap/perm")) {
                rc = string_append(getUserInfo->caps[i].perm, sizeof(getUserInfo->caps[i].perm), data, dataLen);
            }
        }
    }
    else {
        if (!strcmp(elementPath, "user_info")) {
            getUserInfoPrivate->handler.getUserInfoCallback(
                    getUserInfo,
                    getUserInfoPrivate->callbackData);
        }
        else if (!strcmp(elementPath, "user_info/subusers")) {
            getUserInfo->subUsersCount++;
        }
        else if (!strcmp(elementPath, "user_info/keys/key")) {
            getUserInfo->keysCount++;
        }
        else if (!strcmp(elementPath, "user_info/swift_keys/key")) {
            getUserInfo->swiftKeysCount++;
        }
        else if (!strcmp(elementPath, "user_info/caps/cap")) {
            getUserInfo->capsCount++;
        }
    }
done:
    return rc;
}

static S3Status RGgetUserInfoDataCallback(int bufferSize, const char *buffer, void *callbackData)
{
    RGGetUserInfoPrivate *getUserInfoPrivate = callbackData;

    return simplexml_add(&getUserInfoPrivate->simpleXml, buffer, bufferSize);
}

void RG_get_user_info(const S3BucketContext *bucketContext, const char *uid,
                 S3RequestContext *requestContext,
                 const RGGetUserInfoHandler *handler, void *callbackData)
{
    const char queryParamsFormat[] = "format=xml&uid=%s";
    const size_t queryParamsLen = snprintf(NULL, 0, queryParamsFormat, uid);
    char queryParams[queryParamsLen+1];
    RGGetUserInfoPrivate getUserInfoPrivate = {
        .handler = *handler,
        .callbackData = callbackData
    };
    simplexml_initialize(&getUserInfoPrivate.simpleXml, RGgetUserInfoXmlCallback, &getUserInfoPrivate);

    snprintf(queryParams, sizeof(queryParams), queryParamsFormat, uid);

    RequestParams params =
    {
        .httpRequestType = HttpRequestTypeGET,
        .bucketContext = *bucketContext,
        .key = "user",
        .queryParams = queryParams,
        .propertiesCallback = handler->responseHandler.propertiesCallback,
        .fromS3Callback = RGgetUserInfoDataCallback,
        .completeCallback = handler->responseHandler.completeCallback,
        .callbackData = &getUserInfoPrivate
    };

    request_perform(&params, requestContext);
}
