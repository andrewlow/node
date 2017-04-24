/* crypto/cpt_err.c */
/* ====================================================================
 * Copyright (c) 1999-2011 The OpenSSL Project.  All rights reserved.
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
#include <openssl/crypto.h>

/* BEGIN ERROR CODES */
#ifndef OPENSSL_NO_ERR

# define ERR_FUNC(func) ERR_PACK(ERR_LIB_CRYPTO,func,0)
# define ERR_REASON(reason) ERR_PACK(ERR_LIB_CRYPTO,0,reason)

static ERR_STRING_DATA CRYPTO_str_functs[] = {
    {ERR_FUNC(CRYPTO_F_CRYPTO_GET_EX_NEW_INDEX), "\x43\x52\x59\x50\x54\x4f\x5f\x67\x65\x74\x5f\x65\x78\x5f\x6e\x65\x77\x5f\x69\x6e\x64\x65\x78"},
    {ERR_FUNC(CRYPTO_F_CRYPTO_GET_NEW_DYNLOCKID), "\x43\x52\x59\x50\x54\x4f\x5f\x67\x65\x74\x5f\x6e\x65\x77\x5f\x64\x79\x6e\x6c\x6f\x63\x6b\x69\x64"},
    {ERR_FUNC(CRYPTO_F_CRYPTO_GET_NEW_LOCKID), "\x43\x52\x59\x50\x54\x4f\x5f\x67\x65\x74\x5f\x6e\x65\x77\x5f\x6c\x6f\x63\x6b\x69\x64"},
    {ERR_FUNC(CRYPTO_F_CRYPTO_SET_EX_DATA), "\x43\x52\x59\x50\x54\x4f\x5f\x73\x65\x74\x5f\x65\x78\x5f\x64\x61\x74\x61"},
    {ERR_FUNC(CRYPTO_F_DEF_ADD_INDEX), "\x44\x45\x46\x5f\x41\x44\x44\x5f\x49\x4e\x44\x45\x58"},
    {ERR_FUNC(CRYPTO_F_DEF_GET_CLASS), "\x44\x45\x46\x5f\x47\x45\x54\x5f\x43\x4c\x41\x53\x53"},
    {ERR_FUNC(CRYPTO_F_FIPS_MODE_SET), "\x46\x49\x50\x53\x5f\x6d\x6f\x64\x65\x5f\x73\x65\x74"},
    {ERR_FUNC(CRYPTO_F_INT_DUP_EX_DATA), "\x49\x4e\x54\x5f\x44\x55\x50\x5f\x45\x58\x5f\x44\x41\x54\x41"},
    {ERR_FUNC(CRYPTO_F_INT_FREE_EX_DATA), "\x49\x4e\x54\x5f\x46\x52\x45\x45\x5f\x45\x58\x5f\x44\x41\x54\x41"},
    {ERR_FUNC(CRYPTO_F_INT_NEW_EX_DATA), "\x49\x4e\x54\x5f\x4e\x45\x57\x5f\x45\x58\x5f\x44\x41\x54\x41"},
    {0, NULL}
};

static ERR_STRING_DATA CRYPTO_str_reasons[] = {
    {ERR_REASON(CRYPTO_R_FIPS_MODE_NOT_SUPPORTED), "\x66\x69\x70\x73\x20\x6d\x6f\x64\x65\x20\x6e\x6f\x74\x20\x73\x75\x70\x70\x6f\x72\x74\x65\x64"},
    {ERR_REASON(CRYPTO_R_NO_DYNLOCK_CREATE_CALLBACK),
     "\x6e\x6f\x20\x64\x79\x6e\x6c\x6f\x63\x6b\x20\x63\x72\x65\x61\x74\x65\x20\x63\x61\x6c\x6c\x62\x61\x63\x6b"},
    {0, NULL}
};

#endif

void ERR_load_CRYPTO_strings(void)
{
#ifndef OPENSSL_NO_ERR

    if (ERR_func_error_string(CRYPTO_str_functs[0].error) == NULL) {
        ERR_load_strings(0, CRYPTO_str_functs);
        ERR_load_strings(0, CRYPTO_str_reasons);
    }
#endif
}
