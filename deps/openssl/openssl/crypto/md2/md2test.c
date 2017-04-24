/* crypto/md2/md2test.c */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../e_os.h"

#ifdef OPENSSL_NO_MD2
int main(int argc, char *argv[])
{
    printf("\x4e\x6f\x20\x4d\x44\x32\x20\x73\x75\x70\x70\x6f\x72\x74\xa");
    return (0);
}
#else
# include <openssl/evp.h>
# include <openssl/md2.h>

# ifdef CHARSET_EBCDIC
#  include <openssl/ebcdic.h>
# endif

static char *test[] = {
    "",
    "\x61",
    "\x61\x62\x63",
    "\x6d\x65\x73\x73\x61\x67\x65\x20\x64\x69\x67\x65\x73\x74",
    "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a",
    "\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39",
    "\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30",
    NULL,
};

static char *ret[] = {
    "\x38\x33\x35\x30\x65\x35\x61\x33\x65\x32\x34\x63\x31\x35\x33\x64\x66\x32\x32\x37\x35\x63\x39\x66\x38\x30\x36\x39\x32\x37\x37\x33",
    "\x33\x32\x65\x63\x30\x31\x65\x63\x34\x61\x36\x64\x61\x63\x37\x32\x63\x30\x61\x62\x39\x36\x66\x62\x33\x34\x63\x30\x62\x35\x64\x31",
    "\x64\x61\x38\x35\x33\x62\x30\x64\x33\x66\x38\x38\x64\x39\x39\x62\x33\x30\x32\x38\x33\x61\x36\x39\x65\x36\x64\x65\x64\x36\x62\x62",
    "\x61\x62\x34\x66\x34\x39\x36\x62\x66\x62\x32\x61\x35\x33\x30\x62\x32\x31\x39\x66\x66\x33\x33\x30\x33\x31\x66\x65\x30\x36\x62\x30",
    "\x34\x65\x38\x64\x64\x66\x66\x33\x36\x35\x30\x32\x39\x32\x61\x62\x35\x61\x34\x31\x30\x38\x63\x33\x61\x61\x34\x37\x39\x34\x30\x62",
    "\x64\x61\x33\x33\x64\x65\x66\x32\x61\x34\x32\x64\x66\x31\x33\x39\x37\x35\x33\x35\x32\x38\x34\x36\x63\x33\x30\x33\x33\x38\x63\x64",
    "\x64\x35\x39\x37\x36\x66\x37\x39\x64\x38\x33\x64\x33\x61\x30\x64\x63\x39\x38\x30\x36\x63\x33\x63\x36\x36\x66\x33\x65\x66\x64\x38",
};

static char *pt(unsigned char *md);
int main(int argc, char *argv[])
{
    int i, err = 0;
    char **P, **R;
    char *p;
    unsigned char md[MD2_DIGEST_LENGTH];

    P = test;
    R = ret;
    i = 1;
    while (*P != NULL) {
        EVP_Digest((unsigned char *)*P, strlen(*P), md, NULL, EVP_md2(),
                   NULL);
        p = pt(md);
        if (strcmp(p, *R) != 0) {
            printf("\x65\x72\x72\x6f\x72\x20\x63\x61\x6c\x63\x75\x6c\x61\x74\x69\x6e\x67\x20\x4d\x44\x32\x20\x6f\x6e\x20\x27\x25\x73\x27\xa", *P);
            printf("\x67\x6f\x74\x20\x25\x73\x20\x69\x6e\x73\x74\x65\x61\x64\x20\x6f\x66\x20\x25\x73\xa", p, *R);
            err++;
        } else
            printf("\x74\x65\x73\x74\x20\x25\x64\x20\x6f\x6b\xa", i);
        i++;
        R++;
        P++;
    }
# ifdef OPENSSL_SYS_NETWARE
    if (err)
        printf("\x45\x52\x52\x4f\x52\x3a\x20\x25\x64\xa", err);
# endif
    EXIT(err);
    return err;
}

static char *pt(unsigned char *md)
{
    int i;
    static char buf[80];

    for (i = 0; i < MD2_DIGEST_LENGTH; i++)
        sprintf(&(buf[i * 2]), "\x25\x30\x32\x78", md[i]);
    return (buf);
}
#endif
