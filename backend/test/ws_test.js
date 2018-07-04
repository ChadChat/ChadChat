const WebSocket = require('ws');

const MAX_CONS = 1;

for (let i = 0; i < MAX_CONS; i++) {
    let ws = new WebSocket('ws://127.0.0.1:12345');

    ws.on('open', () => {
        ws.send('Sup');
        console.log(`client ${i} connected`);
    });

    ws.on('message', (data) => {
        console.log(`Data recieved from ${i}: `, data);
        ws.close(2001, 'Testing');
    });

    ws.on('close', () => {
        console.log(`${i} client closed`);
    })
}