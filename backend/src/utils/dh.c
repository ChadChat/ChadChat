#include "dh.h"
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <string.h>
#include <stdio.h>

static BIGNUM* p_2048_bit = NULL;
static BIGNUM* generator;

DHM* DHM_new()
{
    DHM* dhm = malloc(sizeof(DHM));
    dhm->key = malloc(SHA256_DIGEST_LENGTH);
    dhm->iv = malloc(IV_LEN_BYTES);
    if(p_2048_bit == NULL)
    {
        p_2048_bit = BN_new();
        BN_hex2bn(&p_2048_bit, P_2048_HEX);
        generator = BN_new();
        BN_hex2bn(&generator, GENERATOR);
    }

    if((dhm->a = BN_new()) == NULL)
    {
        free(dhm);
        return NULL;
    }

    if(!BN_rand(dhm->a, PRIV_KEY_LEN ,BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY))
    {
        BN_free(dhm->a);
        free(dhm);
        return NULL;
    }

    if((dhm->A = BN_new()) == NULL)
    {
        BN_free(dhm->a);
        free(dhm);
        return NULL;
    }
    if((dhm->bnctx = BN_CTX_new()) == NULL)
    {
        BN_free(dhm->a);
        BN_free(dhm->A);
        free(dhm);
        return NULL;
    }
    if(!BN_mod_exp(dhm->A, generator, dhm->a, p_2048_bit, dhm->bnctx))
    {
        BN_CTX_free(dhm->bnctx);
        BN_free(dhm->a);
        BN_free(dhm->A);
        free(dhm);
        return NULL;
    }
    if((dhm->ka = BN_new()) == NULL)
    {
        BN_CTX_free(dhm->bnctx);
        BN_free(dhm->a);
        BN_free(dhm->A);
        free(dhm);
        return NULL;
    }
    return dhm;
}

unsigned char* DHM_get_key_bundle(DHM* dhm, int* key_len)
{
    *key_len = BN_num_bytes(dhm->A);
    unsigned char* to = malloc(*key_len);
    if(to == NULL)
        return NULL;
    BN_bn2bin(dhm->A, to);
    return to;
}

void DHM_generate_secret_key(DHM* dhm, const unsigned char* B_pub_key, int key_len)
{
    BIGNUM* B = BN_bin2bn(B_pub_key, key_len, NULL);
    // BN_hex2bn(&B, B_pub_key);

    BN_CTX* bnctx = BN_CTX_new();
    BN_mod_exp(dhm->ka, B, dhm->a, p_2048_bit, bnctx);

    int len = BN_num_bytes(dhm->ka);
    unsigned char* buf = malloc(len);
    BN_bn2bin(dhm->ka, buf);

    SHA256(buf, len, dhm->key);

    free(buf);
    BN_clear_free(B);
}

inline void DHM_setIV(DHM* dhm, unsigned char* iv)
{
    memcpy(dhm->iv, iv, IV_LEN_BYTES);
}

size_t DHM_encrypt(DHM *dhm, unsigned char *plaintext, int plaintext_len, unsigned char *ciphertext)
{
  EVP_CIPHER_CTX *ctx;
  int len;
  size_t ciphertext_len;

  if(!(ctx = EVP_CIPHER_CTX_new()) || !plaintext_len)
    return 0;

  if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, dhm->key, dhm->iv))
    return 0;

  if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
    return 0;
  ciphertext_len = len;

  if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
    return 0;
  ciphertext_len += len;

  /* Clean up */
  EVP_CIPHER_CTX_free(ctx);

  return ciphertext_len;
}

size_t DHM_decrypt(DHM *dhm, unsigned char *ciphertext, int ciphertext_len, unsigned char *plaintext)
{
  EVP_CIPHER_CTX *ctx;
  int len;
  size_t plaintext_len;

  if(!(ctx = EVP_CIPHER_CTX_new()))
    return 0;

  if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, dhm->key, dhm->iv))
    return 0;

  /* Provide the message to be decrypted, and obtain the plaintext output.
   * EVP_DecryptUpdate can be called multiple times if necessary
   */
  if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
    return 0;
  plaintext_len = len;

  /* Finalise the decryption. Further plaintext bytes may be written at
   * this stage.
   */
  if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len))
    return 0;
  plaintext_len += len;

  EVP_CIPHER_CTX_free(ctx);

  return plaintext_len;
}

void DHM_destroy(DHM* dhm)
{
    BN_free(dhm->a);
    BN_free(dhm->A);
    BN_free(dhm->ka);
    BN_CTX_free(dhm->bnctx); 
    free(dhm->key);
    free(dhm->iv);
    free(dhm);
}
