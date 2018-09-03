#include "../src/utils/dh.h"
#include <stdio.h>
#include <string.h>

char unsigned iv[IV_LEN_BYTES];

int main(int argc, char const *argv[])
{
    int i = 0;
    for (i = 0; i < 16; i++) {
        iv[i] = i;
    }

    char plaintext_alice2Bob[] = "Get a life Nerd";
    unsigned char ciphertext_alice2Bob[GET_MAX_CIPHER_LEN(strlen(plaintext_alice2Bob))];
    char decypted_alice2Bob[GET_MAX_CIPHER_LEN(strlen(plaintext_alice2Bob))];
    char plaintext_bob2alice[] = "Shut up thot";
    unsigned char ciphertext_bob2alice[GET_MAX_CIPHER_LEN(strlen(plaintext_bob2alice))];
    char decrypted_bob2alice[GET_MAX_CIPHER_LEN(strlen(plaintext_bob2alice))];
    unsigned char *A, *B;
    int A_len, B_len;
    DHM* alice = DHM_new();
    DHM* bob = DHM_new();
    DHM_setIV(alice, iv);
    DHM_setIV(bob, iv);

    // printf("Alice Pub Key len: %d\n", DHM_get_key_bundle(alice, A));
    // printf("Bob Pub Key Len: %d\n", DHM_get_key_bundle(bob, B));
    A = DHM_get_key_bundle(alice, &A_len); 
    B = DHM_get_key_bundle(bob, &B_len); 
    printf("Alice Pub Key len: %d\n", A_len);
    printf("Bob Pub Key len: %d\n", B_len);

    DHM_generate_secret_key(alice, B, B_len);
    DHM_generate_secret_key(bob, A, A_len);

    size_t ciphertextLenAlice2Bob = DHM_encrypt(alice, plaintext_alice2Bob, strlen(plaintext_alice2Bob), ciphertext_alice2Bob);
    printf("Alice cipher text len: %ld\n", ciphertextLenAlice2Bob);
    size_t plaintextLenAlice2Bob = DHM_decrypt(bob, ciphertext_alice2Bob, ciphertextLenAlice2Bob, decypted_alice2Bob);
    decypted_alice2Bob[plaintextLenAlice2Bob] = 0;  // NUll terminate it!!
    printf("Alice Send Bob: \"%s\" with len %ld\n", decypted_alice2Bob, plaintextLenAlice2Bob);

    size_t ciphertextLenBob2Alice = DHM_encrypt(bob, plaintext_bob2alice, strlen(plaintext_bob2alice), ciphertext_bob2alice);
    printf("Alice cipher text len: %ld\n", ciphertextLenBob2Alice);
    size_t plaintextLenBob2Alice = DHM_decrypt(alice, ciphertext_bob2alice, ciphertextLenBob2Alice, decrypted_bob2alice);
    decrypted_bob2alice[plaintextLenBob2Alice] = 0; // NULL terminate it!!
    printf("Bob Send Alice: \"%s\" with len %ld\n", decrypted_bob2alice, plaintextLenBob2Alice);

    DHM_destroy(alice);
    DHM_destroy(bob);
    free(A);
    free(B);
    return 0;
}
