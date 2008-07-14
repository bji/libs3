/** **************************************************************************
 * acl.c
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


void S3_set_acl(const S3BucketContext *bucketContext, const char *key,
                int aclGrantCount, S3AclGrant *aclGrants,
                S3RequestContext *requestContext,
                const S3ResponseHandler *handler, void *callbackData)
{
}


void S3_add_acl_grants(const S3BucketContext *bucketContext, const char *key,
                       int aclGrantCount, S3AclGrant *aclGrants,
                       S3RequestContext *requestContext,
                       const S3ResponseHandler *handler, void *callbackData)
{
}


void S3_remove_acl_grants(const S3BucketContext *bucketContext,
                          const char *key, int aclGrantsCount,
                          const S3AclGrant *aclGrants,
                          S3RequestContext *requestContext,
                          const S3ResponseHandler *handler,
                          void *callbackData)
{
}


void S3_clear_acl(const S3BucketContext *bucketContext, const char *key,
                  S3RequestContext *requestContext,
                  const S3ResponseHandler *requestHandler,
                  void *callbackData)
{
}
