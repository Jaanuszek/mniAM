#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "amcom.h"
#include "amcom_packets.h"
enum {
    MAX_FOOD_QUANTITY = 64
};
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
struct foodObject {
    uint16_t foodNumber;
    uint8_t isEaten;
    float xPos;
    float yPos;
    struct foodObject* next;
};
static AMCOM_NewGameRequestPayload gameInfo;
static AMCOM_PlayerState playersInfo[AMCOM_MAX_PLAYER_UPDATES];
static AMCOM_PlayerState ourPlayer;
static AMCOM_FoodState foodInfo[MAX_FOOD_QUANTITY];
static int foodCount = 0;
uint8_t foodPacket =0;
uint32_t gameTime = 0;

static AMCOM_IdentifyRequestPayload gameVersion;

float Move() {
    //food
    AMCOM_FoodState *lastClosestFood = NULL;
    float closestFoodDistance = 9999.0f;
    AMCOM_FoodState *closestFood = NULL;
    //player
    AMCOM_PlayerState *closestPlayer = NULL;
    float closestPlayerDistance = 9999.0f;
    bool allFoodEaten = TRUE;

    // Szukamy nowego najbliższego jedzenia
    for (int i = 0; i < MAX_FOOD_QUANTITY; i++) {
        if (foodInfo[i].state != 0) {
            float dx = ourPlayer.x - foodInfo[i].x;
            float dy = ourPlayer.y - foodInfo[i].y;
            float distance = sqrtf(dx * dx + dy * dy);
            if (distance < closestFoodDistance) {
                closestFoodDistance = distance;
                closestFood = &foodInfo[i];
                allFoodEaten = false;
            }
        }
    }

    // Szukamy najbliższego gracza z mniejszym HP
    for (int i = 0; i < AMCOM_MAX_PLAYER_UPDATES; i++) {
        if (playersInfo[i].hp > 0) {
            float dx = ourPlayer.x - playersInfo[i].x;
            float dy = ourPlayer.y - playersInfo[i].y;
            float distance = sqrtf(dx * dx + dy * dy);
            if (distance < closestPlayerDistance) {
                if (ourPlayer.hp > playersInfo[i].hp) {
                    closestPlayer = &playersInfo[i];
                    closestPlayerDistance = distance;
                }
            }
        }
    }

    // Decyzja o kierunku ruchu
    if (!allFoodEaten&& closestFood!=NULL) {
            //lastClosestFood = closestFood;
            float dx = closestFood->x - ourPlayer.x;
            float dy = closestFood->y - ourPlayer.y;
            float angle = atan2f(dy, dx);  // Używamy dy, dx, aby uzyskać prawidłowy kąt

            printf("Closest Food: FoodNo: %hhu, State: %hhu, x: %f, y: %f\n", closestFood->foodNo, closestFood->state, closestFood->x, closestFood->y);
            printf("Angle to Food: %f radians\n", angle);

            //lastAngle = angle;
            return angle;
    } else {
        printf("No available food found, looking for player with lower hp\n");
        if (closestPlayer != NULL) {
            float dx = closestPlayer->x - ourPlayer.x;
            float dy = closestPlayer->y - ourPlayer.y;
            float angle = atan2f(dy, dx);
            //lastAngle = angle;
            return angle;
        } else {
            printf("No players with lower HP found, avoiding stronger players\n");
            closestPlayer=NULL;
            closestPlayerDistance = 9999.0f;//?
            for (int i = 0; i < AMCOM_MAX_PLAYER_UPDATES; i++) {
                if (playersInfo[i].hp > ourPlayer.hp) {
                    float dx = ourPlayer.x - playersInfo[i].x;
                    float dy = ourPlayer.y - playersInfo[i].y;
                    float distance = sqrtf(dx * dx + dy * dy);
                    if (distance < closestPlayerDistance) {
                        closestPlayer = &playersInfo[i];
                        closestPlayerDistance = distance;
                    }
                }
            }

            if (closestPlayer != NULL) {
                float dx = ourPlayer.x - closestPlayer->x;
                float dy = ourPlayer.y - closestPlayer->y;
                float angle = atan2f(dy, dx);  // Odwracamy kierunek, aby uciekać

                printf("Closest Stronger Player: PlayerNo: %hhu, HP: %hu, x: %f, y: %f\n", closestPlayer->playerNo, closestPlayer->hp, closestPlayer->x, closestPlayer->y);
                printf("Angle to Avoid Player: %f radians\n", angle);

                return angle;
            }
        }
    }

    return 0.0f;
}
void amPacketHandler(const AMCOM_Packet* packet, void* userContext) {
    uint8_t buf[AMCOM_MAX_PACKET_SIZE];
    size_t toSend = 0;

    switch (packet->header.type) {
         case AMCOM_IDENTIFY_REQUEST:
            printf("Got IDENTIFY.request. Responding with IDENTIFY.response\n");
            gameVersion.gameVerHi = (packet->payload)[0];
            gameVersion.gameVerLo = (packet->payload)[1];
            gameVersion.gameRevision = (packet->payload)[2]+((uint16_t)((packet->payload)[3]))<<8;

            AMCOM_IdentifyResponsePayload identifyResponse;
            sprintf(identifyResponse.playerName, "JabTen");
            toSend = AMCOM_Serialize(AMCOM_IDENTIFY_RESPONSE, &identifyResponse, sizeof(identifyResponse), buf);
        break;
        case AMCOM_NEW_GAME_REQUEST:
            printf("Got NEW_GAME.request. Responding with NEW_GAME.response\n");
            memcpy(&gameInfo, packet->payload, sizeof(gameInfo));
            for (int i = 0; i < AMCOM_MAX_FOOD_UPDATES; ++i) {
                foodInfo[i].state = 0;
            }
            foodPacket = 0;
           //memset(foodInfo,0,sizeof(foodInfo)); // cleaning the foodInfo structure
            foodCount=0;
            AMCOM_NewGameResponsePayload newGameResponse;
            sprintf(newGameResponse.helloMessage, "Tylko zwyciestwo");
            toSend= AMCOM_Serialize(AMCOM_NEW_GAME_RESPONSE, &newGameResponse,sizeof(newGameResponse), buf);
            printf("Player Number: %hhu, Players Count: %hhu, Width: %f, Height: %f\n", gameInfo.playerNumber,
                   gameInfo.numberOfPlayers, gameInfo.mapWidth, gameInfo.mapHeight);
        break;
        case AMCOM_PLAYER_UPDATE_REQUEST:
            printf("Got PLAYER_UPDATE.request.\n");
        uint8_t numPlayers = packet->header.length/11;
        for(uint8_t i=0; i < numPlayers; i++) {
            int offset = 11*i;
            AMCOM_PlayerState playerState;
            playerState.playerNo=packet->payload[offset];
            playerState.hp = (uint16_t)(packet->payload[offset+1]|(packet->payload[offset+2]<<8));
            memcpy(&playerState.x,&packet->payload[offset+3],sizeof(float));
            memcpy(&playerState.y,&packet->payload[offset+7],sizeof(float));
            playersInfo[playerState.playerNo]=playerState; //tu byla zmiana
            if(playerState.playerNo == gameInfo.playerNumber) {
                ourPlayer=playerState;
            }
            printf("Player Number: %hhu, Player hp: %hu, xPos: %f, yPos: %f\n",
              playersInfo[i].playerNo, playersInfo[i].hp, playersInfo[i].x, playersInfo[i].y);
        }
        printf("Our Player Number: %hhu, Player hp: %hu, xPos: %f, yPos: %f\n",
       ourPlayer.playerNo, ourPlayer.hp, ourPlayer.x, ourPlayer.y);
        break;
        case AMCOM_FOOD_UPDATE_REQUEST:
        printf("Got food info, no response, packet length: %d\n", packet->header.length);

        uint8_t numRecords = packet->header.length / 11;
        for (uint8_t i = 0; i < numRecords; i++) {
            int offset = i * 11;
            uint16_t foodNo = (uint16_t)(packet->payload[offset] | (packet->payload[offset + 1] << 8));
            uint8_t state = packet->payload[offset + 2];
            float x, y;
            memcpy(&x, &packet->payload[offset + 3], sizeof(x));
            memcpy(&y, &packet->payload[offset + 7], sizeof(y));

            //searching for food with certain food number

            int foodIndex = -1;
            int emptySlotIndex = -1;
            for (int j = 0; j < MAX_FOOD_QUANTITY; j++) {
                if (foodInfo[j].foodNo == foodNo) {
                    foodIndex = j;
                    break;
                }
                if (emptySlotIndex == -1 && foodInfo[j].state == 0) {
                    emptySlotIndex = j;
                }
            }

            if (foodIndex != -1) {
                // Update the existing food item
                foodInfo[foodIndex].state = state;
                foodInfo[foodIndex].x = x;
                foodInfo[foodIndex].y = y;
                printf("Updated food: Food Number: %hu, State: %hhu, xPos: %f, yPos: %f\n",
                       foodInfo[foodIndex].foodNo, foodInfo[foodIndex].state, foodInfo[foodIndex].x, foodInfo[foodIndex].y);
            } else if (emptySlotIndex != -1 && state == 1) {
                // Add a new food item in an empty slot
                foodInfo[emptySlotIndex].foodNo = foodNo;
                foodInfo[emptySlotIndex].state = state;
                foodInfo[emptySlotIndex].x = x;
                foodInfo[emptySlotIndex].y = y;
                printf("Added new food: Food Number: %hu, State: %hhu, xPos: %f, yPos: %f\n",
                       foodInfo[emptySlotIndex].foodNo, foodInfo[emptySlotIndex].state, foodInfo[emptySlotIndex].x, foodInfo[emptySlotIndex].y);
            } else {
                // Log if there's no space to add new food
                if (state == 1) {
                    printf("Error: No empty slot to add new food: Food Number: %hu, State: %hhu, xPos: %f, yPos: %f\n",
                           foodNo, state, x, y);
                }
            }
        }
        break;
        case AMCOM_MOVE_REQUEST:
            printf("Got MOVE.request. Responding with MOVE.Response\n");
            AMCOM_MoveRequestPayload moveRequest;
            memcpy(&moveRequest, packet->payload, sizeof(moveRequest));
            gameTime = moveRequest.gameTime;
            printf("GameTime: %u\n",gameTime);

            AMCOM_MoveResponsePayload moveResponse;
            float angle = Move();
            printf("angle:%f\n",angle);
            moveResponse.angle = angle;
            toSend = AMCOM_Serialize(AMCOM_MOVE_RESPONSE, &moveResponse, sizeof(moveResponse), buf);
        break;
        case AMCOM_GAME_OVER_REQUEST:
            printf("Got GAME_OVER.request. Responding with GAME_OVER.Response\n");
        AMCOM_GameOverRequestPayload gameOverRequest;
        memcpy(&gameOverRequest, packet->payload, sizeof(gameOverRequest));
        for(int i =0; i<AMCOM_MAX_PLAYER_UPDATES;i++) {
            AMCOM_PlayerState playerState = gameOverRequest.playerState[i];
            // printf("Player Number: %hhu, Player hp: %hhu, xPos: %f, yPos: %f\n", playerState.playerNo, playerState.hp,
            //        playerState.x, playerState.y);
        }
        AMCOM_GameOverResponsePayload gameOverResponse;
        sprintf(gameOverResponse.endMessage, "GG!");
        toSend = AMCOM_Serialize(AMCOM_GAME_OVER_RESPONSE, &gameOverResponse, sizeof(gameOverResponse), buf);
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
