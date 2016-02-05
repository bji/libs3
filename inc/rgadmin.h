
#ifndef RG_ADMIN_H
#define RG_ADMIN_H

#include "libs3.h"

#define RG_USER_LEN 48
#define RG_ACCESS_KEY_LEN 256
#define RG_SECRET_KEY_LEN 48
#define RG_EMAIL_LEN 128
#define RG_PERMISSIONS_LEN 48
#define RG_DISPLAY_NAME_LEN 128

#define RG_MAX_SUB_USERS 16
#define RG_MAX_KEYS 16
#define RG_MAX_SWIFT_KEYS 16
#define RG_MAX_CAPS 16

#define RG_CAP_TYPE_LEN 16
#define RG_CAP_PERM_LEN 16

typedef struct RGCap
{
    char type[RG_CAP_TYPE_LEN];
    char perm[RG_CAP_PERM_LEN];
}
RGCap;

typedef struct RGSubUser
{
    char id[RG_USER_LEN];
    char permissions[RG_PERMISSIONS_LEN];
}
RGSubUser;

typedef struct RGKey
{
    char user[RG_USER_LEN];
    char accessKeyId[RG_ACCESS_KEY_LEN];
    char secretAccessKey[RG_SECRET_KEY_LEN];
}
RGKey;

typedef struct RGSwiftKey
{
    char user[RG_USER_LEN];
    char secretKey[RG_SECRET_KEY_LEN];
}
RGSwiftKey;

typedef struct RGGetUserInfo
{
    char userId[RG_USER_LEN];
    char displayName[RG_DISPLAY_NAME_LEN];
    char email[RG_EMAIL_LEN];
    int suspended;
    int maxBuckets;
    int subUsersCount;
    RGSubUser subUsers[RG_MAX_SUB_USERS];
    int keysCount;
    RGKey keys[RG_MAX_KEYS];
    int swiftKeysCount;
    RGSwiftKey swiftKeys[RG_MAX_SWIFT_KEYS];

    int capsCount;
    RGCap caps[RG_MAX_CAPS];
}
RGGetUserInfo;

typedef S3Status (*RGGetUserInfoCallback)(const RGGetUserInfo *userInfo, void *callbackArg);

typedef struct RGGetUserInfoHandler
{
    S3ResponseHandler responseHandler;
    RGGetUserInfoCallback getUserInfoCallback;
}
RGGetUserInfoHandler;

void RG_get_user_info(const S3BucketContext *bucketContext, const char *uid,
                 S3RequestContext *requestContext,
                 const RGGetUserInfoHandler *handler, void *callbackData);
#endif /* RG_ADMIN_H */
