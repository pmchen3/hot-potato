/******************************************************************************
 *
 *  File Name........: player.c
 *
 *  Description......:
 *  Player in a game of hot potato. Players connect in a ring and to the
 *  ring master. They get potato, decrement hops, append their player
 *  number to the potato and pass it.
 *
 *  This program takes two arguments. The first is the host name on which
 *  the master process is running. (Note: master must be started first.)
 *  The second is the port number on which listen is accepting connections.
 *
 *  Revision History.:
 *
 *  When            Who                     What
 *  03/28/2016      Peter Chen (pmchen)     Created
 *
 *****************************************************************************/

/*........................ Include Files ....................................*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define LEN 64
#define LEN_BUF 5000

// Prototypes
int newSocket();
int bindSocket(int s, struct hostent *hp);
int connectSocket(int s, int port, struct hostent *hp);

// Global variables
int s, rc, len, port;
int player_num, player_num_l, player_num_r, tot_players;
int s_left, p_left, fd_left;
int s_right, p_right;
int r;
char addr[LEN];
struct hostent *hp_right;
struct sockaddr_in sin_left, sin_right;

// Main
main (int argc, char *argv[]) {
    char host[LEN], str[LEN];
    char buf[LEN_BUF], *token;
    struct hostent *hp_master, *hp_player;
    //struct sockaddr_in sin;
    
    // Read host and port number from command line
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <master-machine-name> <port-number>\n", argv[0]);
        exit(1);
    }
    
    // Fill in hostent struct of master
    hp_master = gethostbyname(argv[1]);
    if (hp_master == NULL) {
        fprintf(stderr, "%s: host not found (%s)\n", argv[0], host);
        exit(1);
    }
    port = atoi(argv[2]);
    
    // Fill in hostent struct for self
    gethostname(host, sizeof host);
    hp_player = gethostbyname(host);
    if (hp_player == NULL) {
        fprintf(stderr, "%s: host not found (%s)\n", argv[0], host);
        exit(1);
    }
    
    // Create and connect socket to master
    s = newSocket();
    connectSocket(s, port, hp_master);
    
    // Send host-name to master
    if (send(s, host, strlen(host), 0) != strlen(host)) {
        perror("send");
        exit(1);
    }
    
    // Get player number from master and print
    memset(buf, 0, LEN_BUF);
    len = recv(s, buf, 32, 0);
    if (len < 0) {
        perror("recv");
        exit(1);
    }
    //buf[len] = '\0';
    token = strtok(buf, ":"); // player_num
    player_num = atoi(token);
    token = strtok(NULL, ":"); // tot_players
    tot_players = atoi(token);
    printf("Connected as player %d\n", player_num);
    
    // Set up left and right player number
    if (player_num == 0) {
        player_num_l = tot_players - 1;
    } else {
        player_num_l = player_num - 1;
    }
    if (player_num == tot_players - 1) {
        player_num_r = 0;
    } else {
        player_num_r = player_num + 1;
    }
    
    //--------------------Connect to players---------------------
    
    s_left = newSocket();
    p_left = bindSocket(s_left, hp_player);
    
    rc = listen(s_left, 5);
    if (rc < 0) {
        perror("listen:");
        exit(rc);
    }
    //printf("player %d listening\n", player_num); //test
    
    // Send p_left and addr to master
    inet_ntop(AF_INET, hp_player->h_addr, addr, sizeof(addr));
    
    sprintf(buf, "%d:%s", p_left, addr);
    //printf("p_left:addr (send): %s\n", buf); //test
    if (send(s, buf, strlen(buf), 0) != strlen(buf)) {
        perror("send");
        exit(1);
    }
    
    // Receive p_left/addr (of right neighbor) from master and connect
    memset(buf, 0, LEN_BUF);
    if (recv(s, buf, LEN_BUF - 2, 0) < 0) {
        perror("recv");
        exit(1);
    }
    //printf("connect or accept (recv): %s\n", buf); //test
    makeConnection(buf);
    
    memset(buf, 0, LEN_BUF);
    if (recv(s, buf, LEN_BUF - 2, 0) < 0) {
        perror("recv");
        exit(1);
    }
    //printf("connect or accept (recv): %s\n", buf); //test
    makeConnection(buf);
    
    // Send confirmation to master to sync
    if (send(s, "y", 1, 0) != 1) {
        perror("send");
        exit(1);
    }
    //-------------Player connections established-----------------
    
    //----------------------Start Game----------------------------
    
    // Set up fd for select call
    fd_set readfds;
    int nfds = s;
    
    if (fd_left > nfds) {
        nfds = fd_left;
    }
    if (s_right > nfds) {
        nfds = s_right;
    }
    nfds++;
    
    srand(player_num); // Seed is set to player number. Change if needed.
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(s, &readfds);
        FD_SET(fd_left, &readfds);
        FD_SET(s_right, &readfds);
        
        //printf("start select\n"); //test
        rc = select(nfds, &readfds, NULL, NULL, NULL);
        if (rc < 0) {
            perror("select");
        }
        if (rc == 0) {
            printf("No activity on select\n"); // Should not get here - no time out
        }
        //printf("passed select\n"); // test
        
        // Receive and handle the potato
        memset(buf, 0, LEN_BUF);
        if (FD_ISSET(s, &readfds)) {
            if (recv(s, buf, LEN_BUF - 2, 0) < 0) {
                perror("recv");
                exit(1);
            }
            handlePotato(buf);
        }
        if (FD_ISSET(fd_left, &readfds)) {
            if (recv(fd_left, buf, LEN_BUF - 2, 0) < 0) {
                perror("recv");
                exit(1);
            }
            handlePotato(buf);
        }
        if (FD_ISSET(s_right, &readfds)) {
            if (recv(s_right, buf, LEN_BUF - 2, 0) < 0) {
                perror("recv");
                exit(1);
            }
            handlePotato(buf);
        }
    } // While
    
    exit(0);
} // Main

//----------------------------Helper Methods------------------------------------

// Create socket
// Use address family INET (IPv4) and STREAM sockets (TCP)
int newSocket() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket:");
        exit(s);
    }
    return s;
}

// Bind socket to an address/port
int bindSocket(int s, struct hostent *hp) {
    // Set up the address and port
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);
    
    // Bind socket s to address sin
    int port;
    //for (port = 10000; port < 20000; port++) {
    while (1) {
        port = rand() % 10000 + 10000;
        sin.sin_port = htons(port);
        if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0) {
            //printf("binded port: %d\n", port); //test
            return port;
        }
    }
    // Only reach here on failure to bind
    perror("bind:");
    exit(-1);
}

// Connect socket to listener (port and hp)
int connectSocket(int s, int port, struct hostent *hp) {
    // Set up the address and port
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);
  
    // Connect to socket at above address and port
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("connect:");
        exit(-1);
    }
    return 0;
}

// Connect player to player using message
int makeConnection(char *str) {
    char *token;
    token = strtok(str, ":");
    
    if (strcmp(token, "accept") == 0) {
        len = sizeof(sin_left);
        fd_left = accept(s_left, (struct sockaddr *)&sin_left, &len);
        if (fd_left < 0) {
            perror("accept:");
            exit(fd_left);
        }
        //printf("player %d accepted\n", player_num); //test
    }
    else if (strcmp(token, "connect") == 0) {
        s_right = newSocket();
        
        token = strtok(NULL, ":"); // p_right
        p_right = atoi(token);
        //printf("p_right: %d\n", p_right); //test
        token = strtok(NULL, ":"); // addr_right
        //printf("addr token: %s\n", token); //test
        if (inet_pton(AF_INET, token, &sin_right.sin_addr) != 1) {
            perror("inet_pton");
        }
        
        hp_right = gethostbyaddr((char *)&sin_right.sin_addr, 
            sizeof(struct in_addr), AF_INET);
        if (hp_right == NULL) {
            fprintf(stderr, "host not found\n");
            exit(1);
        }
        connectSocket(s_right, p_right, hp_right);
        //printf("player %d connected\n", player_num); //test
    }
}

// Handle the potato
int handlePotato(char *potato) {
    //printf("got msg\n"); //test
    char *token;
    char buffer[LEN_BUF];
    int hops, start = 0;
    
    token = strtok(potato, ":"); // Keyword
    
    if (strcmp(token, "start") == 0) {
        start = 1;
    }
    if (strcmp(token, "hop") == 0 || start) {
        token = strtok(NULL, ":"); // Hops
        hops = atoi(token);
        hops--;
        token = strtok(NULL, ":"); // Trace
        
        //sleep(2); //test
        if (hops == 0) {
            // Send to master
            printf("I'm it\n");
            if (start) {
                sprintf(buffer, "%d", player_num);
            }
            else {
                sprintf(buffer, "%s,%d", token, player_num);
            }
            if (send(s, buffer, strlen(buffer), 0) != strlen(buffer)) {
                perror("send");
                exit(1);
            }
        }
        else {
            // Send to player
            
            r = rand() % 2;
            if (start) {
                sprintf(buffer, "hop:%d:%d", hops, player_num);
            }
            else {
                sprintf(buffer, "hop:%d:%s,%d", hops, token, player_num);
            }
            
            if (r == 0) {
                //printf("going left\n"); //test
                printf("Sending potato to %d\n", player_num_l);
                if (send(fd_left, buffer, strlen(buffer), 0) != strlen(buffer)) {
                    perror("send");
                    exit(1);
                }
            }
            else {
                //printf("going right\n"); //test
                printf("Sending potato to %d\n", player_num_r);
                if (send(s_right, buffer, strlen(buffer), 0) != strlen(buffer)) {
                    perror("send");
                    exit(1);
                }
            }
        }
    }
    else if (strcmp(token, "end") == 0) {
        // Shut everything down
        //printf("exiting\n"); //test
        close(s);
        close(fd_left);
        close(s_right);
        exit(0);
    }
}