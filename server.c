#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#include "arrays/send-array.h"
#include "arrays/active-conn-array.h"
#include "arrays/int-array.h"

#include "server.h"
#include "chat.h"

// server constants
const int MAX_CLIENTS = FD_SETSIZE; //max oped connection count
const int SERVER_SOCKET_QUEGE = 10; //amount of clients that can be waiting to accept at once
const int DISCARD_INACTIVE_CLIENT_AFTER_SEC = 60;   //terminate connection after ..seconds if client is not sending anything

const int MAX_MESSAGE_COUNT = 1024; // max number of messages waiting to be send

const int MAX_MESSAGE_SIZE = 255;   //both incoming and sending message buffer size
int DISCARD_MESSAGE_AFTER_SEC = 20;  //discard message after ..seconds if cannot be send

const int TIMEOUT_VAL = 10;     //not important, select() timeout value in seconds

// custom made arrays with the ability limit loop range; they work the same way
// basically a stack with an int refering to current max index held
// every time you remove something, the top of that stack is moved to the removed index
// so if you are looping through all elementss using i++, remember to lower i-- every time you remove an element at index i
to_send_set *messages;              //with messages to send
active_conn_set *connections;       //with active connections
int_array *to_close;                //with connections to close after sending messages

// file descriptor sets
// read_mask and write_mask are for select
// open_con is a mask with all open connections, it sbeing copied to read_mask every select()
fd_set read_mask, write_mask, open_conn;

/*
* function initializing sever socket and adress; called only once at startup;
* its separated to make the code more clear
*/
int prepare_server_fd (int *server_fd, struct sockaddr_in *server_adr) {

    //create server socket 
    *server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(*server_fd == -1){
        perror("Error function socket() durning opening server socket");
        return 0;
    }

    //socket port will be free() right after server programm termination
    int on = 1;
    setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));

    //bind server socket to server adress
    if(bind(*server_fd, (struct sockaddr*)server_adr, sizeof(*server_adr)) == -1){
        perror("Error function bind(); port may be busy");
        return 0;
    }

    //open socket and listen for incoming data
    if(listen(*server_fd, SERVER_SOCKET_QUEGE) == -1){
        perror("Error function listen()");
        return 0;
    }

    return 1;
}

/*
* prepare read_mask and write_mask at the start of every while(1) loop iteration, before select()
*/
void prepare_fd_mask(int *server_fd) {

    // read_mask = server_fd + open_conn
    FD_ZERO(&read_mask);
    read_mask = open_conn;
    FD_SET(*server_fd, &read_mask);
        
    // write_mask = client_fd from all messages
    FD_ZERO(&write_mask);
    for (int i=0; i <= messages->high_index; i++) {
        // unique
        if(!FD_ISSET(messages->array[i].client_fd, &write_mask)){
            FD_SET(messages->array[i].client_fd, &write_mask);
        } 
    }

}

/*
* accept new connection at server socket and add it to active_conn array
*/
void new_connection (int *server_fd) {

    //client address and size, also client_fd values
    struct sockaddr_in client_adr;
    socklen_t client_addr_size = sizeof(client_adr);

    //accept connection
    int client_fd = accept(*server_fd, (struct sockaddr*)&client_adr, &client_addr_size);

    //if error
    if(client_fd == -1){
        // no reaction
        printf("Error function accept()");
    } else {
        //no error; add connection to array; add client_fd to open_conn
        add_conn(connections, client_fd, client_adr);
        FD_SET(client_fd, &open_conn);
    }
}

/*
* receive message and save it to the buffer;
* return message length, or -1 if there was an error
*/
int receive_message(active_conn_type *client, char *buf, int size){
    //zero buffer and load message
    memset(buf, 0, size);
    int lng = recv(client->client_fd, buf, size, MSG_DONTWAIT);
    if (lng <= 0){
        //if error return -1
        return -1;
    }
    //return receiver byte count
    return lng;
}

/*
* add message to to_send array
*/
void new_message(int client_fd, char *buf, int size){
    //add message to array to_send; also keep track of creation time to calculate timeouts
    add_to_send(messages, client_fd, buf, size);
}

/*
* try to send the message;
* return 0 if the operation would block, try again later
* return 1 if the message has been sent OR there have been any other error
*/
int send_message(int *i){
    to_send_type *message = &messages->array[*i];
    //try to send message
    ssize_t len = send(message->client_fd, message->buf, message->size, MSG_DONTWAIT);

    if (len <= 0 && (errno == EAGAIN || errno == EWOULDBLOCK)){
        //operation would block; did not removed message - no change, try again later
        return 0;

    } else {
        // could be both: message has been sent or other error (like client closed socket); remove message
        remove_to_send(messages, *i);
        //--*i is for purpose of next loop iteration and the way the to_send array works
        --*i;
        //return true
        return 1;
    }
}

/*
* get index of value client_fd in active_conn array
*/
int get_index_of_client_fd(int client_fd){
    for(int i=0; i<=connections->high_index; i++){
        if(connections->array[i].client_fd == client_fd){
            return i;
        }
    }
    return -1;
}

/*
* return 1 if there is at least one message to send to given client_fd, else return 0
*/
int client_in_messages(int client_fd){
    for (int i=0; i<=messages->high_index; i++){
        if(messages->array[i].client_fd == client_fd){
            return 1;
        }
    }
    return 0;
}

/*
* removes all messages that should be send to a given cliet_fd
*/
void remove_msg_to (int client_fd){
    for (int i=0; i<=messages->high_index; i++){
        if(messages->array[i].client_fd == client_fd){
            remove_to_send(messages, i);
            i--;
        }
    }
}

/*
* try to send all pending messages to that client_fd before disconnecting
* if it is not possible to disconnect now, remember client_fd by adding it to an int_array,
* then remove client from read mask but do not close the socket yet;
*/
void disconnect(int client_fd){
    //remove it from open_conn, and so from read_mask
    FD_CLR(client_fd, &open_conn);

    //if  there are no messages
    if(!client_in_messages(client_fd)){
        //close his connection and remove connection from array
        int i = get_index_of_client_fd(client_fd);
        if(i != -1){
            close(connections->array[i].client_fd);
            remove_conn(connections, i);
        }
    } else {
        //else remember client_fd to close it later
        add_int(to_close, client_fd);
    }
}

/*
* try to finish all disconnect()-ed client_fd
*/
void finalize_disconnection(){

    //for every client_fd to disconnect
    for (int i=0; i<=to_close->high_index; i++){

        //if there are no messages to send
        if(!client_in_messages(to_close->array[i])){

            //close connection and remove from aray
            int index = get_index_of_client_fd(to_close->array[i]);
            if(index != -1){
                close(connections->array[index].client_fd);
                remove_conn(connections, index);
            }

            //also remove from disconnect array
            remove_int(to_close, i);
            //i-- is for purpose of next loop iteration and the way the int_array array works
            i--;
        }
    }
}

/*
* erase all pending messages to a given client_fd and close the connection now
*/
void force_disconnect(int client_fd) {
    //remove client_fd from open_conn and so from read_mask
    FD_CLR(client_fd, &open_conn);
    //delete all messages to_sent to client_fd
    remove_msg_to(client_fd);
    //close scket and remove it from array
    int index = get_index_of_client_fd(client_fd);
    close(connections->array[index].client_fd);
    remove_conn(connections, index);
}

/*
* check if connection is timed out (not active for X seconds); if yes, remove it forcibly
*/
int remove_if_timeout_conn(int *i, int timeout_seconds){
    //check for timeout
    if ( (time(NULL) - connections->array[*i].lastly_used) > timeout_seconds){
        // inform server content about client_disconnected
        client_disconnected(connections->array[*i].client_fd, connections->array[*i].client_addr);

        // if yes - remove all messages to client_fd, remove clirnt_fd from open_conn, 
        // close client_fd, remove client_fd from array
        remove_msg_to(connections->array[*i].client_fd);
        FD_CLR(connections->array[*i].client_fd, &open_conn);
        close(connections->array[*i].client_fd);
        remove_conn(connections, *i);
        //--*i is for purpose of next loop iteration and the way the active_conn array works
        --*i;
        return 1;
    } 
    return 0;
}

/*
* check if connection is timed out (ncould not be sent for X seconds); if yes, forget it 
*/
int remove_if_timeout_msg(int *i, int timeout_seconds){
    // check for timeout
    if ( time(NULL) - messages->array[*i].init_time > timeout_seconds){ 
        //if yes - remove message
        remove_to_send(messages, *i);
        //--*i is for purpose of next loop iteration and the way the to_send array works
        --*i;
        return 1;
    }
    return 0;
}

// --------------- MAIN ------------------------------ //

int main(int argc, char **argv) {

    //check for the amount of arguments
    if(argc<2){
        printf("Need port number.\n");
        return EXIT_FAILURE;
    }

    // variables for both server and client: descriptors and adress
    // initialize server adress
    int server_fd;
    struct sockaddr_in server_adr;
        server_adr.sin_family = AF_INET;
        server_adr.sin_port = htons(atoi(argv[1]));
        server_adr.sin_addr.s_addr = INADDR_ANY;

    // initialize server file decryptor
    if(!prepare_server_fd(&server_fd, &server_adr)){
        return EXIT_FAILURE;
    }

    // initialize dynamic-loop range lists
    messages = create_to_send(MAX_MESSAGE_COUNT);
    connections = create_conn(MAX_CLIENTS);
    to_close = create_int_array(MAX_CLIENTS);

    //clean open_connections
    FD_ZERO(&open_conn);
    //set up timeout for select
    struct timeval timeout;
        timeout.tv_sec = TIMEOUT_VAL;
        timeout.tv_usec = 0;

    printf("Server OK!\n");

    // ----- SERVER LOOP ---------- //
    while(1){
        //prepare read and write masks
        prepare_fd_mask(&server_fd);
        
        //select filter the masks so only the ones ready to work with remain in sets
        int r_count = select(MAX_CLIENTS, &read_mask, &write_mask, (fd_set*)0, &timeout);

        timeout.tv_sec = 1 * 10;
        
        if(r_count == -1){
            //if error
            perror("select()");
            return EXIT_FAILURE;

        } else if(r_count == 0){
            // no connection for timeout seconds, just go on.
            // is file decriptors given to select() are empty - which they never are, as server descriptor at least is passed
            // then - without timeout - select() may never exit
            continue;

        } else {
            // no error
            //if server decryptor is still in read_mask - make new connection
            if(FD_ISSET(server_fd, &read_mask)){
                new_connection(&server_fd);
                write(1, "Got a connection!", 17);
            }

            //then deal with any pending output - send pending messages
            // for every message
            for(int i=0; i <= messages->high_index; i++){
                // if its client_fd is in write_mask
                if(FD_ISSET(messages->array[i].client_fd, &write_mask)){
                    //send it
                    if(!send_message(&i)){
                        // the message has not been send - check if the message is timed out; remove if yes
                        remove_if_timeout_msg(&i, DISCARD_MESSAGE_AFTER_SEC);
                    }
                //if its not in write_mask 
                } else {
                    // check if the message is timed out; remove if yes
                    remove_if_timeout_msg(&i, DISCARD_MESSAGE_AFTER_SEC);
                }
            }

            // some connections required to send their pending messages before closing - try to close them now, after sending
            finalize_disconnection();

            // and deal with input - you might close sockets here
            for(int i=0; i <= connections->high_index; i++){
                //else check if connections client_fd is in read_mask
                if(FD_ISSET(connections->array[i].client_fd, &read_mask)){
                    //if so, receive message to buf[], lng = messaage length
                    char buf[MAX_MESSAGE_SIZE];
                    int lng = receive_message( &connections->array[i], buf, MAX_MESSAGE_SIZE);
                    // if error, force disconnect - this will also remove all pending messages to this client_fd
                    if(lng == -1){
                        client_disconnected(connections->array[i].client_fd, connections->array[i].client_addr);
                        force_disconnect(connections->array[i].client_fd);
                    } else if (lng == 0){ //if no message
                        // check if the connection is timed out (not doing anything); remove if yes
                        remove_if_timeout_conn(&i, DISCARD_INACTIVE_CLIENT_AFTER_SEC);
                    } else if(lng > 0){ //if no error - respond
                        //call server content
                        respond(connections->array[i].client_fd, connections->array[i].client_addr, buf, lng);
                        write(1, "Got a message!", 14);
                        printf("\n%s\n", buf);
                        //refresh client_fd used time (reset timeout)
                        connections->array[i].lastly_used = time(NULL);
                    }
                } else {
                    // if not in read_mask: alse check if the connection is timed out (not doing anything); remove if yes
                    remove_if_timeout_conn(&i, DISCARD_INACTIVE_CLIENT_AFTER_SEC);
                }
            }
        }
    }

    // you should never get here anyway, but free all allocated memory on programm termination
    close(server_fd);
    drop_conn_array(connections);
    drop_to_send_array(messages);
    drop_int_array(to_close);
    return EXIT_SUCCESS;
}