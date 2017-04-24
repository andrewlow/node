/* crypto/buffer/buf_err.c */
/* ====================================================================
 * Copyright (c) 1999-2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "\x4f\x70\x65\x6e\x53\x53\x4c\x20\x54\x6f\x6f\x6c\x6b\x69\x74" and "\x4f\x70\x65\x6e\x53\x53\x4c\x20\x50\x72\x6f\x6a\x65\x63\x74" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "\x4f\x70\x65\x6e\x53\x53\x4c"
 *    nor may "\x4f\x70\x65\x6e\x53\x53\x4c" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

/*
 * NOTE: this file was auto generated by the mkerr.pl script: any changes
 * made to it will be overwritten when the script next updates this file,
 * only reason strings will be preserved.
 */

#include <stdio.h>
#include <openssl/err.h>
#include <openssl/buffer.h>

/* BEGIN ERROR CODES */
#ifndef OPENSSL_NO_ERR

# define ERR_FUNC(func) ERR_PACK(ERR_LIB_BUF,func,0)
# define ERR_REASON(reason) ERR_PACK(ERR_LIB_BUF,0,reason)

static ERR_STRING_DATA BUF_str_functs[] = {
    {ERR_FUNC(BUF_F_BUF_MEMDUP), "\x42\x55\x46\x5f\x6d\x65\x6d\x64\x75\x70"},
    {ERR_FUNC(BUF_F_BUF_MEM_GROW), "\x42\x55\x46\x5f\x4d\x45\x4d\x5f\x67\x72\x6f\x77"},
    {ERR_FUNC(BUF_F_BUF_MEM_GROW_CLEAN), "\x42\x55\x46\x5f\x4d\x45\x4d\x5f\x67\x72\x6f\x77\x5f\x63\x6c\x65\x61\x6e"},
    {ERR_FUNC(BUF_F_BUF_MEM_NEW), "\x42\x55\x46\x5f\x4d\x45\x4d\x5f\x6e\x65\x77"},
    {ERR_FUNC(BUF_F_BUF_STRDUP), "\x42\x55\x46\x5f\x73\x74\x72\x64\x75\x70"},
    {ERR_FUNC(BUF_F_BUF_STRNDUP), "\x42\x55\x46\x5f\x73\x74\x72\x6e\x64\x75\x70"},
    {0, NULL}
};

static ERR_STRING_DATA BUF_str_reasons[] = {
    {0, NULL}
};

#endif

void ERR_load_BUF_strings(void)
{
#ifndef OPENSSL_NO_ERR

    if (ERR_func_error_string(BUF_str_functs[0].error) == NULL) {
        ERR_load_strings(0, BUF_str_functs);
        ERR_load_strings(0, BUF_str_reasons);
    }
#endif
}
