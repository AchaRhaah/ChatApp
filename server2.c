// Example code: A simple server side code, which echos back the received message.
// Handle multiple socket connections with select and fd_set on Linux
#include <stdio.h>
#include <string.h> //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>    //close
#include <arpa/inet.h> //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <ctype.h>

#define TRUE 1
#define FALSE 0
#define PORT 8888

struct User
{
    char name[20];
};

struct User Users[25];

void saveUser(int);
void split_string(char *, char *, char **, char **);

int main(int argc, char *argv[])
{
    int opt = TRUE;
    int master_socket, addrlen, new_socket, client_socket[30],
        max_clients = 30, activity, i, valread, sd;
    int max_sd;
    struct sockaddr_in address;

    char buffer[1025]; // data buffer of 1K

    // set of socket descriptors
    fd_set readfds;

    // a message
    char *message = "ECHO Daemon v1.0 \r\nEnter --exit To Close The Connection\n\n";

    // initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++)
    {
        client_socket[i] = 0;
    }

    // create a master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections ,
    // this is just a good habit, it will work without this
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                   sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    // try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while (TRUE)
    {
        // clear the socket set
        FD_ZERO(&readfds);

        // add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // add child sockets to set
        for (i = 0; i < max_clients; i++)
        {
            // socket descriptor
            sd = client_socket[i];

            // if valid socket descriptor then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);

            // highest file descriptor number, need it for the select function
            if (sd > max_sd)
                max_sd = sd;
        }

        // wait for an activity on one of the sockets , timeout is NULL ,
        // so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
        {
            printf("select error");
        }

        // If something happened on the master socket ,
        // then its an incoming connection
        if (FD_ISSET(master_socket, &readfds))
        {
            if ((new_socket = accept(master_socket,
                                     (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n ", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            saveUser(new_socket);

            // send new connection greeting message
            if (send(new_socket, message, strlen(message), 0) != strlen(message))
            {
                perror("send");
            }

            puts("Welcome message sent successfully");

            // add new socket to array of sockets
            for (i = 0; i < max_clients; i++)
            {
                // if position is empty
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    printf("Adding %s to list of sockets as %d\n", Users[new_socket].name, i);

                    break;
                }
            }
        }

        // else its some IO operation on some other socket
        for (i = 0; i < max_clients; i++)
        {
            sd = client_socket[i];

            if (FD_ISSET(sd, &readfds))
            {
                // Check if it was for closing , and also read the
                // incoming message
                if ((valread = read(sd, buffer, 1024)) == 0)
                {
                    // Somebody disconnected , get his details and print
                    getpeername(sd, (struct sockaddr *)&address,
                                (socklen_t *)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    // Close the socket and mark as 0 in list for reuse
                    close(sd);
                    client_socket[i] = 0;
                }
                // Send message to Other Clients
                else
                {
                    // set the string terminating NULL byte on the end
                    // of the data read
                    buffer[valread] = '\0';
                    // send(sd, buffer, strlen(buffer), 0);
                    int temp_sd;
                    char msg[255] = "";
                    char client_dets[50];
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    sprintf(client_dets, "Message from %s: \n", Users[sd].name);
                    strncat(msg, client_dets, 55);
                    strncat(msg, buffer, strlen(buffer));
                    strncat(msg, "\n\n", 4);

                    char confirm_msg[] = "Message Sent!!\n\n";

                    char *ext_ptr = strstr(msg, "--exit");
                    char *list_ptr = strstr(msg, "--list");
                    char *priv_ptr = strstr(msg, "--priv");
                    if (ext_ptr != NULL)
                    {
                        char ext_msg[255];
                        sprintf(ext_msg, "Client %s Left\n\n", Users[sd].name);
                        close(sd);
                        client_socket[i] = 0;
                        for (i = 0; i < max_clients; i++)
                        {
                            int temp_sd = client_socket[i];
                            send(temp_sd, ext_msg, strlen(ext_msg), 0);
                        }
                    }
                    else if (list_ptr != NULL)
                    {
                        char lst_msg[300] = "\nAll Currently Connected Clients: \n";
                        char lst_single[25];
                        for (i = 4; i < (max_sd + 1); i++)
                        {
                            sprintf(lst_single, "> %s\n", Users[i].name);
                            strncat(lst_msg, lst_single, 20);
                        }
                        strncat(lst_msg, "\n", 2);
                        send(sd, lst_msg, strlen(lst_msg), 0);
                    }
                    else if (priv_ptr != NULL)
                    {
                        char *priv_msg;
                        char *username;
                        int priv_sd = 0;
                        char delim[] = "--priv";
                        split_string(msg, delim, &priv_msg, &username);
                        for (i = 4; i < (max_sd + 1); i++)
                        {
                            if (strcmp(username, Users[i].name) == 0)
                            {
                                priv_sd = i;
                            }
                        }
                        if (priv_sd == 0)
                        {
                            char no_priv_msg[100];
                            sprintf(no_priv_msg, "\nNo User Found with Username: %s\nPlease check(using --list command) and try again\n\n", username);
                            send(sd, no_priv_msg, strlen(no_priv_msg), 0);
                        }
                        else
                        {
                            char fin_priv_msg[275];
                            sprintf(fin_priv_msg, "Private %s\n\n", priv_msg);
                            send(priv_sd, fin_priv_msg, strlen(fin_priv_msg), 0);
                            send(sd, confirm_msg, strlen(confirm_msg), 0);
                        }
                    }
                    else
                    {
                        for (i = 0; i < max_clients; i++)
                        {
                            temp_sd = client_socket[i];

                            if (temp_sd != sd)
                            {
                                send(temp_sd, msg, strlen(msg), 0);
                            }
                        }
                        send(sd, confirm_msg, strlen(confirm_msg), 0);
                    }
                }
            }
        }
    }

    return 0;
}

void saveUser(int sd)
{
    char msg[] = "\nPlease Input Your Username (This will be visible to other clients): ";
    char uname[20];
    send(sd, msg, strlen(msg), 0);
    int k = read(sd, uname, 10);
    uname[k - 2] = '\0';
    strncpy(Users[sd].name, uname, 20);
}

void split_string(char *input_string, char *delimiter, char **message, char **username)
{
    char *start = input_string;
    char *end;

    if ((end = strstr(start, delimiter)) != NULL)
    {
        int size = end - start;
        *message = (char *)malloc(sizeof(char) * (size + 1));
        memcpy(*message, start, size);
        (*message)[size] = '\0';
        start = end + strlen(delimiter);
        int i = 0;
        while (isspace(start[i]))
            i++;
        start = start + i;
        int j = strlen(start) - 1;
        while (isspace(start[j]))
            j--;
        int username_size = j + 1;
        *username = (char *)malloc(sizeof(char) * (username_size + 1));
        memcpy(*username, start, username_size);
        (*username)[username_size] = '\0';
    }
}