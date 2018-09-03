#if !defined(DH_IMP_CHAD)
#define DH_IMP_CHAD

#include <openssl/bn.h>
#include <openssl/aes.h>

#define IV_LEN_BYTES 16
#define GET_MAX_CIPHER_LEN(n) (n+AES_BLOCK_SIZE)   // This includes the NULL byte too.
#define PRIV_KEY_LEN (512/8)
#define PUB_KEY_LEN  (2048/8)

typedef struct
{
    BIGNUM* a;
    BIGNUM* A;
    BIGNUM* ka;
    unsigned char* key;
    unsigned char* iv;
    BN_CTX* bnctx;
}DHM;

#define P_2048_HEX "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3BE39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF6955817183995497CEA956AE515D2261898FA051015728E5A8AACAA68FFFFFFFFFFFFFFFF"
#define GENERATOR "02" 

DHM* DHM_new();
unsigned char* DHM_get_key_bundle(DHM* dhm, int* key_len);
void DHM_generate_secret_key(DHM* dhm, const unsigned char* B_pub_key, int key_len);
void DHM_setIV(DHM* dhm, unsigned char* iv);
size_t DHM_encrypt(DHM *dhm, unsigned char *plaintext, int plaintext_len, unsigned char *ciphertext);
size_t DHM_decrypt(DHM *dhm, unsigned char *ciphertext, int ciphertext_len, unsigned char *plaintext);
void DHM_destroy(DHM* dhm);

#endif // DH_IMP_CHAD
