/** **************************************************************************
 * object.c
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

#include "libs3.h"


void S3_put_object(S3BucketContext *bucketContext, const char *key,
                   uint64_t contentLength,
                   const S3RequestHeaders *requestHeaders,
                   S3RequestContext *requestContext,
                   S3PutObjectHandler *handler, void *callbackData)
{
}


void S3_copy_object(S3BucketContext *bucketContext, const char *key,
                    const char *destinationBucket, const char *destinationKey,
                    const S3RequestHeaders *requestHeaders,
                    S3RequestContext *requestContext,
                    S3ResponseHandler *handler, void *callbackData)
{
}


void S3_get_object(S3BucketContext *bucketContext, const char *key,
                   long ifModifiedSince, long ifUnmodifiedSince,
                   const char *ifMatchETag, const char *ifNotMatchETag,
                   const char *byteRange, S3RequestContext *requestContext,
                   S3GetObjectHandler *handler, void *callbackData)
{
}


void S3_head_object(S3BucketContext *bucketContext, const char *key,
                    long ifModifiedSince, long ifUnmodifiedSince,
                    const char *ifMatchETag, const char *ifNotMatchETag,
                    S3RequestContext *requestContext,
                    S3ResponseHandler *handler, void *callbackData)
{
}
                         

void S3_delete_object(S3BucketContext *bucketContext, const char *key,
                      S3RequestContext *requestContext,
                      S3ResponseHandler *handler, void *callbackData)
{
}
