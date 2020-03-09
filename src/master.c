/******************************************************************************
 *
 *  File Name........: master.c
 *
 *  Description......:
 *  Ring master in a game of hot potato. Master connects to all players and
 *  pass potato randomly to a player. Then wait to receive potato back and
 *  print the trace of the potato.
 *
 *  Takes three arguments: the port number of the socket, number of players,
 *  and number of hops. Choose a port number that isn't assigned. Invoke the
 *  player program using same port number.
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

struct player_info {
    int player_num;
    int p_left;
    int p_right;
    int fd_player;
    char addr[LEN];
    char host[LEN];
    //struct hostent *hp_player;
};

// Prototypes
int newSocket();
int bindSocket(int s, int port, struct hostent *hp);

// Global variables
int tot_players, hops;

// Main
main (int argc, char *argv[]) {
    char host[LEN], str[LEN];
    char buf[LEN_BUF];
    int s, p, fp, rc, len, port;

    struct hostent *hp, *ihp;
    struct sockaddr_in sin, incoming;
    
    // Get initial info from command line
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port-number> <number-of-players> <hops>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);
    tot_players = atoi(argv[2]);
    hops = atoi(argv[3]);
    
    // Handle improper inputs
    if (tot_players <= 1) {
        //fprintf(stderr, "number-of-players need to be greater than one\n");
        exit(1);
    }
    if (hops < 0) {
        //fprintf(stderr, "hops need to be non-negative\n");
        exit(1);
    }
    
    struct player_info players[tot_players];
    
    // Fill in hostent struct for self
    gethostname(host, sizeof host);
    hp = gethostbyname(host);
    if (hp == NULL) {
        fprintf(stderr, "%s: host not found (%s)\n", argv[0], host);
        exit(1);
    }
    
    // Initial outputs
    printf("Potato Master on %s\n", host);
    printf("Players = %d\n", tot_players);
    printf("Hops = %d\n", hops);
    
    // Open a socket for listening
    s = newSocket();
    bindSocket(s, port, hp);
    
    rc = listen(s, 15);
    if (rc < 0) {
        perror("listen:");
        exit(rc);
    }
    
    //----------------Establish master-player connections-----------------
    
    // Accept connections from players
    int i;
    for (i = 0; i < tot_players; i++) {
        len = sizeof(sin);
        p = accept(s, (struct sockaddr *)&incoming, &len);
        if (p < 0) {
            perror("accept:");
            exit(p);
        }
        players[i].player_num = i;
        players[i].fd_player = p;
        
        // Get the player host-name
        memset(players[i].host, 0, LEN);
        if (recv(p, players[i].host, LEN - 2, 0) < 0) {
            perror("recv");
            exit(1);
        }
        printf("player %d is on %s\n", i, players[i].host);
        //players[i].hp_player = gethostbyaddr((char *)&incoming.sin_addr, 
        //    sizeof(struct in_addr), AF_INET);
        //printf("player %d is on %s\n", i, players[i].hp_player->h_name);
        
        // Send player number and total number of players to each player
        sprintf(buf, "%d:%d", i, tot_players);
        if (send(p, buf, strlen(buf), 0) != strlen(buf)) {
            perror("send");
            exit(1);
        }
    }
    
    //--------------Establish player-player connections-----------------
    
    // Store p_left and addr from player
    for (i = 0; i < tot_players; i++) {
        memset(buf, 0, LEN_BUF);
        if (recv(players[i].fd_player, buf, 32, 0) < 0) {
            perror("recv");
            exit(1);
        }
        //printf("%d p_left:addr (recv): %s\n", i, buf); //test
        
        char* token;
        token = strtok(buf, ":"); // p_left
        players[i].p_left = atoi(buf);
        token = strtok(NULL, ":"); // addr
        strcpy(players[i].addr, token);
        //printf("%d addr (recv): %s\n", i, players[i].addr); //test
    }
    
    // Send p_left and addr to player's left neighbor to connect
    for (i = 0; i < tot_players; i++) {
        sprintf(buf, "connect:%d:%s", players[i].p_left, players[i].addr);
        int connector = i - 1;
        if (i == 0 ) {
            connector = tot_players - 1;
        }
        len = send(players[connector].fd_player, buf, strlen(buf), 0);
        if (len != strlen(buf)) {
            perror("send");
            exit(1);
        }
        //printf("%d l_port:addr (send): %s\n", connector, buf); //test
        
        // Tell player to accept connection
        sprintf(buf, "accept");
        len = send(players[i].fd_player, buf, strlen(buf), 0);
        if (len != strlen(buf)) {
            perror("send");
            exit(1);
        }
        //printf("%d l_port:addr (send): %s\n", i, buf); //test
    }
    
    // Receive confirmation from players to sync everyone
    for (i = 0; i< tot_players; i++) {
        if (recv(players[i].fd_player, buf, LEN_BUF - 2, 0) < 0) {
            perror("recv");
            exit(1);
        }
    }
    
    //-----------------------Start Game------------------------------
    
    if (hops == 0) {
        endGame(&players, s);
    }
    
    int r;
    int nfds = 0;
    fd_set readfds;
    srand(port); // Seed is set to port number. Change if needed.
    r = rand() % tot_players;
    
    // Set up fd_set and nfds for select call
    FD_ZERO(&readfds);
    for (i = 0; i < tot_players; i++) {
        FD_SET(players[i].fd_player, &readfds);
        if (players[i].fd_player > nfds) {
            nfds = players[i].fd_player;
        }
    }
    nfds++;
    
    // All players present, sending potato to player <number>
    printf("All players present, sending potato to player %d\n", r);
    sprintf(buf, "start:%d", hops);
    if (send(players[r].fd_player, buf, strlen(buf), 0) != strlen(buf)) {
        perror("send");
        exit(1);
    }
    
    // Select, wait for response from a player
    //printf("start select\n"); //test
    rc = select(nfds, &readfds, NULL, NULL, NULL);
    if (rc < 0) {
        perror("select");
    }
    if (rc == 0) {
        printf("No activity on select\n"); // Should not get here - no time out
    }
    
    // Handle the return message
    for (i = 0; i < tot_players; i++) {
        if (FD_ISSET(players[i].fd_player, &readfds)) {
            memset(buf, 0, LEN_BUF);
            if (recv(players[i].fd_player, buf, LEN_BUF - 2, 0) < 0) {
                perror("recv");
                exit(1);
            }
            printf("Trace of potato:\n");
            printf("%s\n", buf);
            break;
        }
    }
    
    // End game
    endGame(&players, s);
    
    exit(0);
} // Main

//--------------------------Helper Methods-----------------------------

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
int bindSocket(int s, int port, struct hostent *hp) {
    // Set up the address and port
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);
    
    // Bind socket s to address sin
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("bind:");
        exit(-1);
    }
    return port; // On success
}

// End the game. Send end command to all players.
int endGame(struct player_info *players, int s) {
    char buffer[LEN_BUF];
    strcpy(buffer, "end");
    int i;
    for (i = 0; i < tot_players; i++) {
        if (send(players[i].fd_player, buffer, strlen(buffer), 0) != strlen(buffer)) {
            perror("send");
            exit(1);
        }
        close(players[i].fd_player);
    }
    close(s);
    exit(0);
}