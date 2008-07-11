/** **************************************************************************
 * util.h
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

#ifndef UTIL_H
#define UTIL_H

#include <curl/curl.h>
#include <curl/multi.h>
#include "libs3.h"


// As specified in S3 documntation
#define META_HEADER_NAME_PREFIX "x-amz-meta-"
#define HOSTNAME "s3.amazonaws.com"


// Derived from S3 documentation

// This is the maximum number of x-amz-meta- headers that could be included in
// a request to S3.  The smallest meta header is" x-amz-meta-n: v".  Since S3
// doesn't count the ": " against the total, the smallest amount of data to
// count for a header would be the length of "x-amz-meta-nv".
#define MAX_META_HEADER_COUNT \
    (S3_MAX_META_HEADER_SIZE / (sizeof(META_HEADER_NAME_PREFIX "nv") - 1))

// This is the maximum number of bytes needed in a "compacted meta header"
// buffer, which is a buffer storing all of the compacted meta headers.
#define COMPACTED_META_HEADER_BUFFER_SIZE \
    (MAX_META_HEADER_COUNT * sizeof(META_HEADER_NAME_PREFIX "n: v"))

// Maximum url encoded key size; since every single character could require
// URL encoding, it's 3 times the size of a key (since each url encoded
// character takes 3 characters: %NN)
#define MAX_URLENCODED_KEY_SIZE (3 * S3_MAX_KEY_SIZE)

// This is the maximum size of a URI that could be passed to S3:
// https://s3.amazonaws.com/${BUCKET}/${KEY}?acl
// 255 is the maximum bucket length
#define MAX_URI_SIZE \
    ((sizeof("https://" HOSTNAME "/") - 1) + 255 + 1 + \
     MAX_URLENCODED_KEY_SIZE + (sizeof("?torrent" - 1)) + 1)

// Maximum size of a canonicalized resource
#define MAX_CANONICALIZED_RESOURCE_SIZE \
    (1 + 255 + 1 + MAX_URLENCODED_KEY_SIZE + (sizeof("?torrent") - 1) + 1)


// Mutex functions -----------------------------------------------------------

// Create a mutex.  Returns 0 if none could be created.
struct S3Mutex *mutex_create();

// Lock a mutex
void mutex_lock(struct S3Mutex *mutex);

// Unlock a mutex
void mutex_unlock(struct S3Mutex *mutex);

// Destroy a mutex
void mutex_destroy(struct S3Mutex *mutex);


// Utilities -----------------------------------------------------------------

// URL-encodes a string from [src] into [dest].  [dest] must have at least
// 3x the number of characters that [source] has.   At most [maxSrcSize] bytes
// from [src] are encoded; if more are present in [src], 0 is returned from
// urlEncode, else nonzero is returned.
int urlEncode(char *dest, const char *src, int maxSrcSize);

// Returns < 0 on failure >= 0 on success
time_t parseIso8601Time(const char *str);

uint64_t parseUnsignedInt(const char *str);


#endif /* UTIL_H */
