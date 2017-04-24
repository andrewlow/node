/* crypto/cversion.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "\x54\x68\x69\x73\x20\x70\x72\x6f\x64\x75\x63\x74\x20\x69\x6e\x63\x6c\x75\x64\x65\x73\x20\x73\x6f\x66\x74\x77\x61\x72\x65\x20\x77\x72\x69\x74\x74\x65\x6e\x20\x62\x79\x20\x54\x69\x6d\x20\x48\x75\x64\x73\x6f\x6e\x20\x28\x74\x6a\x68\x40\x63\x72\x79\x70\x74\x73\x6f\x66\x74\x2e\x63\x6f\x6d\x29"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include "cryptlib.h"

#ifndef NO_WINDOWS_BRAINDEATH
# include "buildinf.h"
#endif

const char *SSLeay_version(int t)
{
    if (t == SSLEAY_VERSION)
        return OPENSSL_VERSION_TEXT;
    if (t == SSLEAY_BUILT_ON) {
#ifdef DATE
# ifdef OPENSSL_USE_BUILD_DATE
        return (DATE);
# else
        return ("\x62\x75\x69\x6c\x74\x20\x6f\x6e\x3a\x20\x72\x65\x70\x72\x6f\x64\x75\x63\x69\x62\x6c\x65\x20\x62\x75\x69\x6c\x64\x2c\x20\x64\x61\x74\x65\x20\x75\x6e\x73\x70\x65\x63\x69\x66\x69\x65\x64");
# endif
#else
        return ("\x62\x75\x69\x6c\x74\x20\x6f\x6e\x3a\x20\x64\x61\x74\x65\x20\x6e\x6f\x74\x20\x61\x76\x61\x69\x6c\x61\x62\x6c\x65");
#endif
    }
    if (t == SSLEAY_CFLAGS) {
#ifdef CFLAGS
        return (CFLAGS);
#else
        return ("\x63\x6f\x6d\x70\x69\x6c\x65\x72\x3a\x20\x69\x6e\x66\x6f\x72\x6d\x61\x74\x69\x6f\x6e\x20\x6e\x6f\x74\x20\x61\x76\x61\x69\x6c\x61\x62\x6c\x65");
#endif
    }
    if (t == SSLEAY_PLATFORM) {
#ifdef PLATFORM
        return (PLATFORM);
#else
        return ("\x70\x6c\x61\x74\x66\x6f\x72\x6d\x3a\x20\x69\x6e\x66\x6f\x72\x6d\x61\x74\x69\x6f\x6e\x20\x6e\x6f\x74\x20\x61\x76\x61\x69\x6c\x61\x62\x6c\x65");
#endif
    }
    if (t == SSLEAY_DIR) {
#ifdef OPENSSLDIR
        return "\x4f\x50\x45\x4e\x53\x53\x4c\x44\x49\x52\x3a\x20\x22" OPENSSLDIR "\x22";
#else
        return "\x4f\x50\x45\x4e\x53\x53\x4c\x44\x49\x52\x3a\x20\x4e\x2f\x41";
#endif
    }
    return ("\x6e\x6f\x74\x20\x61\x76\x61\x69\x6c\x61\x62\x6c\x65");
}

unsigned long SSLeay(void)
{
    return (SSLEAY_VERSION_NUMBER);
}
