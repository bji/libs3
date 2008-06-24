/** **************************************************************************
 * request_context.c
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

S3Status S3_initialize_request_context(S3RequestContext *context,
                                       S3Protocol protocol)
{
    return S3StatusOK;
}


S3Status S3_deinitialize_request_context(S3RequestContext *context)
{
    return S3StatusOK;
}


S3Status S3_add_request_to_request_context(S3RequestContext *context,
                                           S3Request *request)
{
    return S3StatusOK;
}


S3Status S3_remove_request_from_request_context(S3RequestContext *context,
                                                S3Request *request)
{
    return S3StatusOK;
}


S3Status S3_complete_request_context(S3RequestContext *context)
{
    return S3StatusOK;
}


S3Status S3_runonce_request_context(S3RequestContext *context, 
                                    int *requestsRemainingReturn)
{
    return S3StatusOK;
}
