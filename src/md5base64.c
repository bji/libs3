/** **************************************************************************
 * md5base64.c
 *
 * Copyright 2008 Bryan Ischo <bryan@ischo.com>
 *
 * This file is part of libs3.
 *
 * libs3 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, version 3 or above of the License.  You can also
 * redistribute and/or modify it under the terms of the GNU General Public
 * License, version 2 or above of the License.
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
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with libs3, in a file named COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * You should also have received a copy of the GNU General Public License
 * version 2 along with libs3, in a file named COPYING-GPLv2.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 ************************************************************************** **/

#ifndef __APPLE__
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#include <string.h>

// Calculate MD5 and encode it as base64
void generate_content_md5(const char* data, int size,
                          char* retBuffer, int retBufferSize) {
    MD5_CTX mdContext;
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    char md5Buffer[MD5_DIGEST_LENGTH];

    MD5_Init(&mdContext);
    MD5_Update(&mdContext, data, size);
    MD5_Final((unsigned char*)md5Buffer, &mdContext);


    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); //Ignore newlines - write everything in one line
    BIO_write(bio, md5Buffer, sizeof(md5Buffer));
    (void) BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    (void) BIO_set_close(bio, BIO_NOCLOSE);

#if OPENSSL_VERSION_NUMBER < 0x1000207fL
    // Older version of OpenSSL have buffer lengths as ints
    // 0x1000207fL is just an arbitrary version based on Ubuntu 16.04
    if (retBufferSize + 1 < bufferPtr->length) {
#else
    // Newer version have size_t instead of int
    if ((size_t)(retBufferSize + 1UL) < bufferPtr->length) {
#endif
        retBuffer[0] = '\0';
        BIO_free_all(bio);
        return;
    }

    memcpy(retBuffer, bufferPtr->data, bufferPtr->length);
    retBuffer[bufferPtr->length] = '\0';

    BIO_free_all(bio);
}
#endif

