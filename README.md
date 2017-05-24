# Command Line Interface Chat
Chat software for UNIX OS / for JPN / as school exercises  
Works in a LAN.

## Requirements
UNIX variants  
Tested on Debian, macOS Sierra, Vine Linux.

## Installation
Just compile.  
`gcc -o server server.c`  
`gcc -o client client.c`

## Usage
1. Check the IP address of your server machine.  
2. Start server on the server machine.  
3. Start client on client machines.  
4. Enter the IP address of the server machine.  
5. Enter your name.  
6. You can start chat.

You can use following commands.  
`NAME@your-name` : Change your name.  
`RE@destination (name)@message` : Reply message (Other clients can't see this message.)  
`LOGOUT` : Logout  
`END` : Terminate all clients and server.
