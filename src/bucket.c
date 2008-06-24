/** **************************************************************************
 * bucket.c
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

S3Status S3_list_buckets(S3Request *request, 
                         const char *accessKeyId, const char *secretAccessKey,
                         S3ListBucketCallback *callback,
                         void *callbackData)
{
    return S3StatusOK;
}
                         
                            
S3Status S3_test_bucket(S3Request *request,
                        const char *accessKeyId, const char *secretAccessKey,
                        const char *bucketName, 
                        int locationConstraintReturnSize,
                        const char *locationConstraintReturn)
{
    return S3StatusOK;
}

                           
S3Status S3_create_bucket(S3Request *request,
                          const char *accessKeyId, const char *secretAccessKey,
                          const char *bucketName, 
                          const char *locationConstraint)
{
    return S3StatusOK;
}


S3Status S3_delete_bucket(S3Request *request,
                          const char *accessKeyId, const char *secretAccessKey,
                          const char *bucketName)
{
    return S3StatusOK;
}


S3Status S3_list_bucket(S3Request *request, S3BucketContext *bucketContext,
                        const char *prefix, const char *marker, 
                        const char *delimiter, int maxkeys,
                        S3ListBucketCallback *callback,
                        void *callbackData)
{
    return S3StatusOK;
}
