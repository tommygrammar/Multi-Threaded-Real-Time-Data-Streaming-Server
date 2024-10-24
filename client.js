const net = require('net');

// Define the port and host (localhost in this case)
const PORT = 6436;
const HOST = '127.0.0.1';

// Create a socket to connect to the server
const client = new net.Socket();

// Connect to the server
client.connect(PORT, HOST, () => {
    console.log('Connected to server!');    
});

// Handle data received from the server
client.on('data', (data) => {
    
    console.log(data.toString());  // Log the received data
});

// Handle any errors during the connection
client.on('error', (err) => {
    console.error('Error: ', err.message);
});

// Handle server disconnection
client.on('close', () => {
    console.log('Connection closed.');
});
