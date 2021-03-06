# ClientServerCommunication
Multithreaded Server and Client programs that are used to create a chat system using text-based messaging protocol over TCP/IP.

## Usage

### Compile files
cd ClientServerCommunication

make

### Server takes the following commandline arguments

**./server authfile [port]** where authfile is the name of a text file that contains a single line of authentication string of choice, allowing only clients with the right authentication to join. port is the port number for the server to establish connection, waiting for clients. port is optional, if not specified, a random port will be chosen(The chosen port will be displayed on stdout).

![image](https://user-images.githubusercontent.com/86181006/127758503-6548dafc-4c4d-41d5-b348-b4f2d8df4b70.png)

Server can:

    -Upon receiving SIGHUP, server will display the server's statistics(clients connected and number of commands used by clients)

### Client takes the following commandline arguments

**./client name authfile port** where name is the name to display, authfile is the name of a text file that contains a single line authentication string in the same format as the server. port is the port number on server to connect to.

![image](https://user-images.githubusercontent.com/86181006/127758517-c398d3ef-34f5-47ed-91a0-b82befa7b6eb.png)

Clients can:

    -Type any message and they will be broadcasted to other clients inside the system.
    -Use the following commands:
        -*LIST: -> list all clients connected
        -*KICK:name -> Kick a connected client with that name  
        -*LEAVE: -> Disconnect from the server 





