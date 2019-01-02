# blather

**A Simple Client/Server Chatroom Program.**

# How To Use

**Server**
- Some user starts bl-server which manages the chat "room". The server is non-interactive and will likely only print debugging output as it runs. 

**Client**
- Any user who wishes to chat runs bl-client which takes input typed on the keyboard and sends it to the server. The server broadcasts the input to all other clients who can respond by typing their own input. 

# Features

- Multiple communicating processes: clients to servers
- Communication through FIFOs
- Signal handling for graceful server shutdown
- Alarm signals for periodic behavior
- Input multiplexing with select()
- Multiple threads in the client to handle typed input versus info from the server

