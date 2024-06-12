#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "amcom.h"
#include "amcom_packets.h"

uint8_t playerNumber = 0;
uint8_t playersCount = 0;
float width =0;
float height = 0;
struct playerInfo {
    uint8_t playerNumber;
    uint16_t health;
    float xPos;
    float yPos;
};
struct foodInfo {
    uint16_t foodNumber;
    uint8_t isEaten;
    float xPos;
    float yPos;
};

void Move() {
    //tutaj trzeba tak:
    //dobrać sprawdzić którym numerem gracza sie jest, na podstawie tego dobrać sie do naszych koordynatów
    //potem przejść przez wszystkie struktury tych tranzystorów, żeby sprawdzić który tranzystor jest najbliżej (tu jakieś wzorki z geometri analitycznej sie przydadzą)
    //i na podstawie koordynatów gracza i najbliższego tranzystorka obliczyć kąt pod którym ten pionek ma sie poruszać
    // dodatkowo mozna zrobić cos w stylu uciekania przed większymi graczami co mają dużo hp
    //Tylko jak sie dobrac do tych współrzędnych?
    //zrobic liste dwukierunkową/jednokierunkową i przechodząc przez wszystkie elementy obliczac ktory tranzystorek jest najbliżej?
}

void amPacketHandler(const AMCOM_Packet* packet, void* userContext) {
    uint8_t buf[AMCOM_MAX_PACKET_SIZE];
    size_t toSend = 0;

    switch (packet->header.type) {
    case AMCOM_IDENTIFY_REQUEST:
        printf("Got IDENTIFY.request. Responding with IDENTIFY.response\n");
        AMCOM_IdentifyResponsePayload identifyResponse;
        sprintf(identifyResponse.playerName, "JabTen");
        toSend = AMCOM_Serialize(AMCOM_IDENTIFY_RESPONSE, &identifyResponse, sizeof(identifyResponse), buf);
        break;
        case AMCOM_NEW_GAME_REQUEST:
            printf("Got NEW_GAME.request. Responding with NEW_GAME.response\n");
        AMCOM_NewGameRequestPayload newGameRequest;
        memcpy(&newGameRequest, packet->payload, sizeof(newGameRequest));
        playerNumber = newGameRequest.playerNumber;
        playersCount = newGameRequest.numberOfPlayers;
        width = newGameRequest.mapWidth;
        height = newGameRequest.mapHeight;
        AMCOM_NewGameResponsePayload newGameResponse;
        sprintf(newGameResponse.helloMessage, "Tylko zwyciestwo");
        toSend= AMCOM_Serialize(AMCOM_NEW_GAME_RESPONSE, &newGameResponse,sizeof(newGameResponse), buf);
        printf("Player Number: %hhu, Players Count: %hhu, Width: %hu, Height: %hu\n", playerNumber, playersCount, width, height);
        break;
        case AMCOM_PLAYER_UPDATE_REQUEST:
            printf("Got PLAYER_UPDATE.request.\n");
        AMCOM_PlayerUpdateRequestPayload playerUpdate;
        memcpy(&playerUpdate, packet->payload, sizeof(playerUpdate));
        for(int i =0; i<AMCOM_MAX_PLAYER_UPDATES; i++) {
            AMCOM_PlayerState playerState = playerUpdate.playerState[i];
            struct playerInfo player;
            player.playerNumber=playerState.playerNo;
            player.health = playerState.hp;
            player.xPos = playerState.x;
            player.yPos = playerState.y;
            printf("Player Number: %hhu, Player hp: %hhu, xPos: %f, yPos: %f\n", player.playerNumber, player.health,
                 player.xPos, player.yPos);
        }
        break;
        case AMCOM_FOOD_UPDATE_REQUEST:
            printf("GOT FOOD_UPDATE.request.\n");
            AMCOM_FoodUpdateRequestPayload foodUpdate;
            memcpy(&foodUpdate,packet->payload,sizeof(foodUpdate));
            for(int i =0; i<AMCOM_MAX_FOOD_UPDATES; i++) {
                AMCOM_FoodState foodSt = foodUpdate.foodState[i];
                struct foodInfo food;
                food.foodNumber = foodSt.foodNo;
                food.isEaten = foodSt.state;
                food.xPos = foodSt.x;
                food.yPos = foodSt.y;
                printf("Food Number: %hhu, Food State(1 - available, 0 - eaten): %hhu, xPos: %f, yPos: %f\n", food.foodNumber, food.isEaten,
     food.xPos, food.yPos);
            }
        break;
        case AMCOM_MOVE_REQUEST:
            printf("Got MOVE.request. Responding with MOVE.Response\n");
        AMCOM_MoveRequestPayload moveRequest;
        memcpy(&moveRequest, packet->payload, sizeof(moveRequest));
        uint32_t gameTime = moveRequest.gameTime;
        printf("GameTime: %u\n",gameTime);

        break;
    }

    // retrieve socket from user context
    SOCKET ConnectSocket  = *((SOCKET*)userContext);
    // send response if any
    int bytesSent = send(ConnectSocket, (const char*)buf, toSend, 0 );
    if (bytesSent == SOCKET_ERROR) {
        printf("Socket send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        return;
    }
}


#define GAME_SERVER "localhost"
#define GAME_SERVER_PORT "2001"

int main(int argc, char **argv) {
    printf("This is mniAM player. Let's eat some transistors! \n");

    WSADATA wsaData;
    int iResult;

    // Initialize Winsock library (windows sockets)
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    // Prepare temporary data
    SOCKET ConnectSocket  = INVALID_SOCKET;
    struct addrinfo *result = NULL;
    struct addrinfo *ptr = NULL;
    struct addrinfo hints;
    int iSendResult;
    char recvbuf[512];
    int recvbuflen = sizeof(recvbuf);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the game server address and port
    iResult = getaddrinfo(GAME_SERVER, GAME_SERVER_PORT, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    printf("Connecting to game server...\n");
    // Attempt to connect to an address until one succeeds
    for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
                ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }
    // Free some used resources
    freeaddrinfo(result);

    // Check if we connected to the game server
    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to the game server!\n");
        WSACleanup();
        return 1;
    } else {
        printf("Connected to game server\n");
    }

    AMCOM_Receiver amReceiver;
    AMCOM_InitReceiver(&amReceiver, amPacketHandler, &ConnectSocket);

    // Receive until the peer closes the connection
    do {

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if ( iResult > 0 ) {
            AMCOM_Deserialize(&amReceiver, recvbuf, iResult);
        } else if ( iResult == 0 ) {
            printf("Connection closed\n");
        } else {
            printf("recv failed with error: %d\n", WSAGetLastError());
        }

    } while( iResult > 0 );

    // No longer need the socket
    closesocket(ConnectSocket);
    // Clean up
    WSACleanup();

    return 0;
}
