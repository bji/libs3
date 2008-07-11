/** **************************************************************************
 * util.c
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

#include <ctype.h>
#include <string.h>
#include "util.h"

static const char *urlSafeG = "-_.!~*'()/";
static const char *hexG = "0123456789ABCDEF";


// Convenience utility for making the code look nicer.  Tests a string
// against a format; only the characters specified in the format are
// checked (i.e. if the string is longer than the format, the string still
// checks out ok).  Format characters are:
// d - is a digit
// anything else - is that character
// Returns 0 if the string checks out, nonzero if it does not.
static int checkString(const char *str, const char *format)
{
    while (*format) {
        if (*format == 'd') {
            if (!isdigit(*str)) {
                return 1;
            }
        }
        else if (*str != *format) {
            return 1;
        }
        str++, format++;
    }

    return 0;
}


int urlEncode(char *dest, const char *src, int maxSrcSize)
{
    int len = 0;

    if (src) while (*src) {
        if (++len > maxSrcSize) {
            return 0;
        }
        const char *urlsafe = urlSafeG;
        int isurlsafe = 0;
        while (*urlsafe) {
            if (*urlsafe == *src) {
                isurlsafe = 1;
                break;
            }
            urlsafe++;
        }
        if (isurlsafe || isalnum(*src)) {
            *dest++ = *src++;
        }
        else if (*src == ' ') {
            *dest++ = '+';
            src++;
        }
        else {
            *dest++ = '%';
            *dest++ = hexG[*src / 16];
            *dest++ = hexG[*src % 16];
            src++;
        }
    }

    *dest = 0;

    return 1;
}


time_t parseIso8601Time(const char *str)
{
    // Check to make sure that it has a valid format
    if (checkString(str, "dddd-dd-ddTdd:dd:dd")) {
        return -1;
    }

#define nextnum() (((*str - '0') * 10) + (*(str + 1) - '0'))

    // Convert it
    struct tm stm;
    memset(&stm, 0, sizeof(stm));

    stm.tm_year = (nextnum() - 19) * 100;
    str += 2;
    stm.tm_year += nextnum();
    str += 3;

    stm.tm_mon = nextnum() - 1;
    str += 3;

    stm.tm_mday = nextnum();
    str += 3;

    stm.tm_hour = nextnum();
    str += 3;

    stm.tm_min = nextnum();
    str += 3;

    stm.tm_sec = nextnum();
    str += 2;

    stm.tm_isdst = -1;
    
    time_t ret = mktime(&stm);

    // Skip the millis

    if (*str == '.') {
        str++;
        while (isdigit(*str)) {
            str++;
        }
    }
    
    if (checkString(str, "-dd:dd") || checkString(str, "+dd:dd")) {
        int sign = (*str++ == '-') ? -1 : 1;
        int hours = nextnum();
        str += 3;
        int minutes = nextnum();
        ret += (-sign * (((hours * 60) + minutes) * 60));
    }
    // Else it should be Z to be a conformant time string, but we just assume
    // that it is rather than enforcing that

    return ret;
}


uint64_t parseUnsignedInt(const char *str)
{
    // Skip whitespace
    while (isblank(*str)) {
        str++;
    }

    uint64_t ret = 0;

    while (isdigit(*str)) {
        ret *= 10;
        ret += (*str++ - '0');
    }

    return ret;
}
