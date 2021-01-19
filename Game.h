/*
 * Game.h
 *
 *  Created on: Feb 27, 2017
 *      Author: Anthony
 *
 */
#ifndef GAME_H_
#define GAME_H_

/*********************************************** Includes ********************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include "G8RTOS.h"
#include "cc3100_usage.h"
/*********************************************** Includes ********************************************************************/

/*********************************************** Externs ********************************************************************/

/* Semaphores here */
semaphore_t LCD_semaphore;
semaphore_t WIFI_semaphore;
semaphore_t LED_semaphore;

/*********************************************** Externs ********************************************************************/

/*********************************************** Global Defines ********************************************************************/
#define MAX_NUM_OF_PLAYERS      2
#define MAX_NUM_OF_HOST_BOMBS   1
#define MAX_NUM_OF_CLIENT_BOMBS 1
#define MAX_NUM_OF_BOMBS  (MAX_NUM_OF_HOST_BOMBS + MAX_NUM_OF_CLIENT_BOMBS)


// This game can actually be played with 4 players... a little bit more challenging, but doable!
#define NUM_OF_PLAYERS_PLAYING 2

/* Size of game arena */
#define ARENA_MIN_X                  40
#define ARENA_MAX_X                  280
#define ARENA_MIN_Y                  0
#define ARENA_MAX_Y                  240

/* Size of objects */
#define BOMB_SIZE                   10
#define BOMB_SIZE_D2                 (BOMB_SIZE >> 1)


/* Edge limitations for player's center coordinate */
#define HORIZ_CENTER_MAX_PL          (ARENA_MAX_X - PADDLE_LEN_D2)
#define HORIZ_CENTER_MIN_PL          (ARENA_MIN_X + PADDLE_LEN_D2)

/* Constant enters of each player */
#define TOP_PLAYER_CENTER_Y          (ARENA_MIN_Y + PADDLE_WID_D2)
#define BOTTOM_PLAYER_CENTER_Y       (ARENA_MAX_Y - PADDLE_WID_D2)

/* Edge coordinates for paddles */
#define TOP_PADDLE_EDGE              (ARENA_MIN_Y + PADDLE_WID)
#define BOTTOM_PADDLE_EDGE           (ARENA_MAX_Y - PADDLE_WID)

/* Amount of allowable space for collisions with the sides of paddles */
#define WIGGLE_ROOM                  2

/* Edge limitations for ball's center coordinate */
#define HORIZ_CENTER_MAX_BALL        (ARENA_MAX_X - BALL_SIZE_D2)
#define HORIZ_CENTER_MIN_BALL        (ARENA_MIN_X + BALL_SIZE_D2)
#define VERT_CENTER_MAX_BALL         (ARENA_MAX_Y - BALL_SIZE_D2)
#define VERT_CENTER_MIN_BALL         (ARENA_MIN_Y + BALL_SIZE_D2)

/* Maximum ball speed */
#define BOMB_SPEED               10

/* Offset for printing player to avoid blips from left behind ball */
#define PRINT_OFFSET             10


/* Enums for player colors */
typedef enum
{
    PLAYER_RED = LCD_RED,
    PLAYER_BLUE = LCD_BLUE
}playerColor;

typedef enum Direction
{
    UP = 'W',
    DOWN = 'A',
    RIGHT = 'D',
    LEFT = 'S',
    NONE = 0
}Direction_t;

/*********************************************** Global Defines ********************************************************************/

/*********************************************** Data Structures ********************************************************************/

#pragma pack ( push, 1)
//Keep track of where on the grid the players are

/*
 * Struct to be sent from the client to the host
 */
typedef struct
{
    uint32_t IP_address;
    uint8_t playerNumber;
    Direction_t direction;
    bool clientDroppedBomb;
    bool clientCleared;
    bool hostCleared;
    bool ready;
    bool joined;
    bool acknowledge;
    bool done;
} SpecificPlayerInfo_t;

/*
 * General player info to be used by both host and client
 * Client responsible for translation
 */
typedef struct
{
    int16_t positionX;
    int16_t positionY;
    Direction_t direction;
    uint16_t color;
} GeneralPlayerInfo_t;

/*
 * Struct of all the bombs, only changed by the host
 * Include ENUM for different bomb types (Stretch goal)************************************************
 */
typedef struct
{
    int16_t positionX;
    int16_t positionY;
    Direction_t direction;
    uint16_t color;
    uint16_t kaboomTime;
    bool alive;
} Bomb_t;

/*
 * Struct to be sent from the host to the client
 */
typedef struct
{
    SpecificPlayerInfo_t player;
    GeneralPlayerInfo_t players[MAX_NUM_OF_PLAYERS];
    Bomb_t bombs[MAX_NUM_OF_BOMBS];
    bool clearClientBomb;
    bool clearHostBomb;
    bool hostDead;
    bool clientDead;
    bool winner;
    bool tie;
    bool gameDone;
    uint8_t overallScores[2];
} GameState_t;

bool bricks[9][7];

#pragma pack ( pop )


typedef struct
{
    int16_t positionX;
    int16_t positionY;
}PrevBomb_t;

/*
 * Struct of all the previous players locations, only changed by self for drawing
 */
//typedef struct
//{
//    int16_t positionX;
//    int16_t positionY;
//}PrevPlayer_t;

typedef struct
{
    int16_t positionX;
    int16_t positionY;
    Direction_t direction;
}PrevPlayer_t;
//Global variables for emptying Packet information
GameState_t             GState;         //Packet to be sent from Host to Client
SpecificPlayerInfo_t    hostSpecific;
SpecificPlayerInfo_t    clientSpecific; //Packet to be sent from Client to Host

PrevPlayer_t            prevHostLocation;
PrevBomb_t              prevHostBomb;

PrevPlayer_t            prevClientLocation;
PrevBomb_t              prevClientBomb;


bool                    movedHost;
bool                    movedClient;
bool                    endGameFlag;
char                    overallRedScore[16];
char                    overallBlueScore[16];
bool                    hostDroppedBomb;
bool                    globalHostBombClr;
bool                    globalClientBombClr;
bool                    hostCurrentlyExploding;
bool                    clientCurrentlyExploding;

/*********************************************** Data Structures ********************************************************************/


/*********************************************** Client Threads *********************************************************************/
/*
 * Thread for client to join game
 */
void JoinGame();

/*
 * Thread that receives game state packets from host
 */
void ReceiveDataFromHost();

/*
 * Thread that sends UDP packets to host
 */
void SendDataToHost();

/*
 * Thread to read client's joystick
 */
void ReadJoystickClient();

/*
 * End of game for the client
 */
void EndOfGameClient();
 
//bomb ISR


/*********************************************** Client Threads *********************************************************************/


/*********************************************** Host Threads *********************************************************************/
/*
 * Thread for the host to create a game
 */
void CreateGame();

/*
 * Thread that sends game state to client
 */
void SendDataToClient();

/*
 * Thread that receives UDP packets from client
 */
void ReceiveDataFromClient();


/*
 * Thread to read host's joystick
 */
void ReadJoystickHost();

/*
 * Thread to move a single ball
 */
void MoveBomb();

void MovePlayers();

/*
 * End of game for the host
 */
void EndOfGameHost();

/*********************************************** Host Threads *********************************************************************/


/*********************************************** Common Threads *********************************************************************/
/*
 * Idle thread
 */
void IdleThread();

/*
 * Thread to draw all the objects in the game
 */
void DrawObjects();

/*
 * Thread to update LEDs based on score
 */

void DebounceHost();

void DebounceClient();

/*********************************************** Common Threads *********************************************************************/


/*********************************************** Public Functions *********************************************************************/
/*
 * Returns either Host or Client depending on button press
 */
playerType GetPlayerRole();

/*
 * Draw players given center X center coordinate
 */
void DrawPlayer(GeneralPlayerInfo_t * player);

/*
 * Updates player's paddle based on current and new center
 */
void UpdatePlayerOnScreen(PrevPlayer_t * prevPlayerIn, GeneralPlayerInfo_t * outPlayer);

/*
 * Function updates ball position on screen
 */
void UpdateBombOnScreen(PrevBomb_t * previousBomb, Bomb_t * currentBomb, uint16_t outColor);

/*
 * Initializes and prints initial game state
 */
void InitBoardState();

/*
 * Host fills packet (GState) from proper global variables to be sent to the client
 */
void FillPacket();

/*
 * Client empties received packet (GState) from the host, and fills data into proper global variables
 */
void EmptyPacket();

/*
 * Host waits for button press to restart game
 */
void HOST_RESTART();

void ClientButton();

void HostButton();

void drawPlayer(uint8_t Player);

void Generateboard();

void GenerateboardClient();

void drawperm(int x, int y);

void drawvolatile(int x, int y);

//void DrawBomb(Bomb_t * bomb, uint16_t outColor);
void DrawBomb(int16_t positionX, int16_t positionY, uint16_t outColor);

void DrawFirst();

bool checkperm(int x, int y);

void Explosion();

void ExplodeHost();

void ExplodeClient();

void ExplodeHost_C();

void ExplodeClient_C();
/*********************************************** Public Functions *********************************************************************/


#endif /* GAME_H_ */
//that pizza was fire

