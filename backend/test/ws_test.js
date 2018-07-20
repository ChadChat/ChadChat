const WebSocket = require('ws');

const MAX_CONS = 1;
const READ_BUFFER_SIZE = 4096;

// For testing purposes make the server Echo whatever the client sends.

for (let i = 0; i < MAX_CONS; i++) {
    // we need to send GET/POST data here.
    let ws = new WebSocket('ws://127.0.0.1:12345/');

    ws.on('open', () => {
        console.log(`client ${i} connected`);
        var data = "01 Hello How r'ya";
        console.log(`sending UTf-8: "${data}" to the server`);
        ws.send(data);
        data = new Float32Array(10);
        for (let i = 0; i < data.length; i++) {
            data[i] = Math.PI * i;
        }
        data[0] = 0x00003230;   // This should be '02' in ascii
        console.log(`sending Binary: "${data}" to the server`);
        ws.send(data);
        data = new Float32Array(READ_BUFFER_SIZE+100);
        console.log('sending a frame larger than the read limit of the server');
        ws.send(data);
        // we need to send WS fragmented message here.
    });

    ws.on('message', (data) => {
        console.log(`Data recieved from ${i}: ${data}`);
        ws.close(2001, 'Testing');
    });

    ws.on('close', () => {
        console.log(`${i} client closed`);
    })
}