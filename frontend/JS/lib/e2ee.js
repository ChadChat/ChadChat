"use strict";

function hexToBigInt(hex) {
  if ((hex.length & 1) === 1) hex = "0" + hex;
  let myInt = bigInt(); // zero
  for (let i = 0; i < hex.length; i += 2) {
    myInt = myInt.shiftLeft(8);
    myInt = myInt.or(parseInt(hex.substr(i, 2), 16));
  }
  return myInt;
}

/* These predetermined g and p are from: https://tools.ietf.org/html/rfc3526 */
const PRIMES = {
  g: bigInt("2"),
  // "p": "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3BE39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF6955817183995497CEA956AE515D2261898FA051015728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6BF12FFA06D98A0864D87602733EC86A64521F2B18177B200CBBE117577A615D6C770988C0BAD946E208E24FA074E5AB3143DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D788719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA993B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934063199FFFFFFFFFFFFFFFF"
  p: hexToBigInt(
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3BE39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF6955817183995497CEA956AE515D2261898FA051015728E5A8AACAA68FFFFFFFFFFFFFFFF"
  )
};

/*
 * Ok, with that out of the way... once you've got the symmetric key out of the DH exchange, use it in a symmetric cryptographic cipher! There are lots to choose from,
 * but the most common is probably AES, using either a 128-bit or 256-bit key, in CBC mode, with PKCS7 padding, using a strong randomly-generated IV, and with an HMAC
 * of the ciphertext for integrity. If your shared key is the wrong length for the cipher, you can run it through a function such as SHA-256 to produce a key suitable for 256-bit AES.
 * You can send the IV and the HMAC in the clear; they are useless to an attacker without the key and the intended recipient needs to know them. The recipient takes the ciphertext,
 * re-computes the HMAC and verifies that it matches, then uses the supplied IV and the shared symmetric key to decrypt the message using the same AES cipher in CBC mode.
 */

class DiffieHellmanMerkle {
  constructor() {
    this.g = PRIMES.g;
    this.p = PRIMES.p;
    this.a = DiffieHellmanMerkle.generateRandomBigIntOfBits(512);
    this.A = this.g.modPow(this.a, this.p);
    // A = (g ^ a) % p;
    this.ka = null;
  }

  /*
     * generateSecretKey - Generates the secret Key from B's public value and our private values
     * @param B {Uint8Array} - B's public value
     */
  generateSecretKey(B) {
    // S = (B ** a) % p;
    this.ka = DiffieHellmanMerkle.bigIntFromArray(B).modPow(this.a, this.p);
  }

  get256BitKeyFromDH() {
    if (this.ka === null) return null;
    let buffer = Uint8Array.from(this.ka.toArray(256).value);
    return crypto.subtle.digest("SHA-256", buffer);
  }

  static generateRandomBigIntOfBits(bits) {
    bits += bits % 8;
    let array = new Uint8Array(Math.floor(bits / 8));
    window.crypto.getRandomValues(array);
    return DiffieHellmanMerkle.bigIntFromArray(array);
  }

  static bigIntFromArray(array) {
    let myInt = bigInt(); // zero
    for (let i = 0; i < array.length; i++) {
      // myInt = (myInt << 8) | array[1]
      myInt = myInt.shiftLeft(8);
      myInt = myInt.or(array[i]);
    }
    return myInt;
  }
}

class E2EE {
  constructor() {
    this.dh = new DiffieHellmanMerkle();
    this.key = null;
    this.iv = null;
  }

  getKeyBundle() {
    // while(!this.dh.filled);
    let bundle = this.dh.A;
    return Uint8Array.from(bundle.toArray(256).value);
  }

  handlePartnersBundle(B) {
    if (!(B instanceof Uint8Array)) return;
    this.dh.generateSecretKey(B);
    this.dh
      .get256BitKeyFromDH()
      .then(buffer => {
        return crypto.subtle.importKey(
          "raw",
          buffer,
          { name: "AES-CBC", iv: this.iv },
          false,
          ["encrypt", "decrypt"]
        );
      })
      .then(key => {
        this.key = key;
      })
      .catch(err => console.error(err));
  }

  generateIV() {
    this.iv = new Uint8Array(16);
    window.crypto.getRandomValues(this.iv);
    return this.iv;
  }

  decrypt(cipher) {
    if (this.key == null || this.iv == null) {
      return;
    }
    return crypto.subtle.decrypt(
      { name: "AES-CBC", iv: this.iv },
      this.key,
      cipher
    );
  }

  encrypt(data) {
    if (this.key == null || this.iv == null) {
      console.log("Here");

      return;
    }
    return crypto.subtle.encrypt(
      { name: "AES-CBC", iv: this.iv },
      this.key,
      data
    );
  }
}
