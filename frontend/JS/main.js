/*
function stringToArray(string) {
  let arr = new Uint8Array(string.length);
  for (let i = 0; i < string.length; i++) arr[i] = string.charCodeAt(i);
  return arr;
}

function arrayToString(array) {
  let str = "";
  var view = new DataView(array);
  for (let i = 0; i < array.byteLength; i++) {
    str += String.fromCharCode(view.getUint8(i));
  }
  return str;
}

function testE2EE() {
  let alice = new E2EE();
  let bob = new E2EE();
  alicesPubKey = alice.getKeyBundle();
  bobsPubKey = bob.getKeyBundle();
  iv = alice.generateIV();
  bob.iv = iv;
  alice.handlePartnersBundle(bobsPubKey);
  bob.handlePartnersBundle(alicesPubKey);

  let plaintext_alice = stringToArray("Get a life nerd!");
  let plaintext_bob = stringToArray("Shut Up thot!");

  setTimeout(() => {
    alice
      .encrypt(plaintext_alice)
      .then(cipher => {
        return bob.decrypt(cipher);
      })
      .then(text => {
        console.log("Alice send me: ", arrayToString(text));
      })
      .catch(err => console.error(err));

    bob
      .encrypt(plaintext_bob)
      .then(cipher => {
        return alice.decrypt(cipher);
      })
      .then(text => {
        console.log("Bob send me: ", arrayToString(text));
      })
      .catch(err => console.error(err));
  }, 10);
}

testE2EE();
*/

class ChadChat {
  constructor(host, port) {
    this.url = `ws://${host}:${port}/`;
    this.sessionKey = null;
    this.clients = {};
  }

  setCookie() {
    localStorage.setItem("clientSecret", this.sessionKey);
  }

  getAllClients() {
    let ws = new WebSocket(this.url + "clients");
    ws.onmessage(event => {
      event.data;
    });
  }

  registerClient(username) {
    this.key_bundle = new E2EE();
    iv = this.key_bundle.generateIV();
    let ws = new WebSocket(this.url + 'registerClient');
      ws.send
  }

  sendKeyBundle(bundle) {}

  getKeyBundle(client) {}
}
