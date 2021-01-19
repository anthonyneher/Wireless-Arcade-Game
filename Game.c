/*
 * Threads.c 
 *
 *  Created on: Feb 10, 2019
 *      Authors: David Rivera & Anthony Neher
 */

#include "Game.h"

/*
 * Thread for client to join game
 */

void JoinGame()
{
    clientSpecific.IP_address   = getLocalIP();
    clientSpecific.playerNumber = Client;       // player number
    clientSpecific.ready        = 0;
    clientSpecific.joined       = true;
    clientSpecific.acknowledge  = 0;

    /****************************Handshake********************************/
    GState.player.acknowledge = false;

    while(GState.player.acknowledge == false)                                   //wait for client to join host
    {
        SendData((uint8_t*)&clientSpecific, HOST_IP_ADDR, sizeof(clientSpecific));   //send acknowledgment)
        ReceiveData((uint8_t*)&GState, sizeof(GState));
    }

    clientSpecific.acknowledge      = true;
    SendData((uint8_t*)&clientSpecific, HOST_IP_ADDR, sizeof(clientSpecific));   //send acknowledgment)
    /****************************Handshake********************************/
    while(!bricks[1][1]){
        ReceiveData((uint8_t*)&bricks, sizeof(bricks));
    }

    clientSpecific.ready            = false;
    clientSpecific.acknowledge      = false;
    clientSpecific.joined           = false;

    GenerateboardClient();

    G8RTOS_AddThread(IdleThread, 255, "IDLE");
    G8RTOS_AddThread(SendDataToHost, 50, "SendDataToHost");
    G8RTOS_AddThread(ReceiveDataFromHost, 50, "RecieveDataFromHost");
    G8RTOS_AddThread(DrawObjects, 45, "DrawObjects");
    G8RTOS_AddThread(ReadJoystickClient, 47, "ReadJoystick");

    P5->IFG &= ~BIT4;       // P5.4 IFG cleared
    P5->IE  |= BIT4;        // Enable interrupt on P5.4
    P5->IES |= BIT4;        // high-to-low transition
    G8RTOS_AddAPeriodicEvent(ClientButton, 0, PORT5_IRQn);

    G8RTOS_KillSelf();
}

/*
 * Thread that receives game state packets from host
 */

void ReceiveDataFromHost()
{
    /*
     * Received UDP packet from client is a SpecificPlayer_t struct.
     * This is loaded into clientSpecific
     */
    while(1)
    {
        G8RTOS_WaitSemaphore(&WIFI_semaphore);
        ReceiveData((uint8_t*)&GState, sizeof(GState));
        G8RTOS_SignalSemaphore(&WIFI_semaphore);
        sleep(1);

        if(GState.bombs[1].alive == true){
            clientSpecific.clientDroppedBomb = false;   //client has already dropped bomb
        }

        if(GState.clearHostBomb == true && clientSpecific.hostCleared == false){
            clientSpecific.hostCleared = true;
            G8RTOS_AddThread(ExplodeHost_C, 40, "explodehost");
            G8RTOS_AddThread(Explosion, 50, "LED HOST");
            //globalHostBombClr = true;
        }

        if(GState.clearClientBomb == true && clientSpecific.clientCleared == false){
            clientSpecific.clientCleared = true;
            G8RTOS_AddThread(ExplodeClient_C, 40, "explodeclient");
            G8RTOS_AddThread(Explosion, 50, "LED HOST");
            //globalHostBombClr = true;
        }

        if(GState.gameDone){

            if(GState.clearHostBomb == true){
                G8RTOS_AddThread(ExplodeHost_C, 40, "explodehost");
            }
            if(GState.clearClientBomb == true){
                G8RTOS_AddThread(ExplodeClient_C, 40, "explodeclient");
            }
            sleep(50);
            G8RTOS_AddThread(EndOfGameClient, 1, "EndOfGameClient");
        }
        sleep(2);
    }
}

/*
 * Thread that sends UDP packets to host
 */
void SendDataToHost()
{
    while(1)
    {
        G8RTOS_WaitSemaphore(&WIFI_semaphore);
        SendData((uint8_t*)&clientSpecific, HOST_IP_ADDR, sizeof(clientSpecific));   //send packet to client
        G8RTOS_SignalSemaphore(&WIFI_semaphore);
        sleep(4);
    }
}

/*
 * Thread to read client's joystick
 */
void ReadJoystickClient()
{
    int16_t x, y;
    while(1)
    {
        GetJoystickCoordinates(&x, &y);

        /**********************Client**************************/
        if(x > 2500){
            clientSpecific.direction = LEFT;
        }else if(x < -3000){
            clientSpecific.direction = RIGHT;
        }else if(y > 3000){
            clientSpecific.direction = DOWN;
        }else if(y < -3000){
            clientSpecific.direction = UP;
        }else{
            clientSpecific.direction = NONE;
        }
       /**********************Client**************************/
        sleep(50);
    }
}


/*
 * End of game for the client
 */
void EndOfGameClient()
{
    P5->IE  &= ~BIT4;        //Disable interrupt on P5.4
    G8RTOS_WaitSemaphore(&WIFI_semaphore);
    G8RTOS_WaitSemaphore(&LCD_semaphore);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    G8RTOS_SignalSemaphore(&LCD_semaphore);
    G8RTOS_SignalSemaphore(&WIFI_semaphore);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    G8RTOS_KillAllOtherThreads();

    LP3943_ColorSet(RED, 0xFF, 0xFFFF);
    LP3943_ColorSet(GREEN, 0x30, 0xFFFF);

    LCD_Clear(LCD_WHITE);
    if(GState.tie){
        LCD_Text(100, 40, "YA BOTH EAT A$$", LCD_PURPLE);
    }else if(GState.winner){
        LCD_Text(125, 40, "YOU WIN!", LCD_RED);
    }else{
        LCD_Text(120, 40, "YOU LOST!", LCD_BLUE);
    }

    snprintf(overallBlueScore, 16, "BLUE SCORE %d", GState.overallScores[0]);
    snprintf(overallRedScore, 16, "RED SCORE %d", GState.overallScores[1]);

    GState.players[0].positionX = 2;
    GState.players[0].positionY = 2;
    GState.players[1].positionX = 6;
    GState.players[1].positionY = 2;

    UpdatePlayerOnScreen(0, &GState.players[0]);
    LCD_Text(50, 110, overallBlueScore, LCD_BLUE);

    UpdatePlayerOnScreen(0, &GState.players[1]);
    LCD_Text(190, 110, overallRedScore, LCD_RED);


    clientSpecific.ready = true;

    clientSpecific.ready        = false;
    clientSpecific.acknowledge  = true;
    clientSpecific.joined       = false;

    GState.player.acknowledge   = false;
    GState.player.ready         = false;

    //Reset all global game variables, except scores, bomb flags
    GState.players[0].positionX    = 0;    //player 0 is host
    GState.players[0].positionY    = 0;    //player 0 is host
    GState.players[0].direction    = NONE;
    GState.players[1].positionX    = 8;    //players[1] is client
    GState.players[1].positionY    = 6;    //players[1] is client
    GState.players[1].direction    = NONE;
    GState.hostDead                = false;
    GState.clientDead              = false;
    GState.clearClientBomb         = false;
    GState.clearHostBomb           = false;
    GState.gameDone                = false;
    GState.winner                  = false;
    GState.tie                     = false;

    clientSpecific.clientCleared    = false;
    clientSpecific.hostCleared      = false;


    movedHost                      = false;
    movedClient                    = false;
    hostDroppedBomb                = false;
    globalHostBombClr              = false;
    globalClientBombClr            = false;

    //put a receive here

    //waiting for bricks, should be enough time for board to be made on host end
    for(int x = 0; x<9; x++)
    {
        for(int y = 0; y<7; y++)
        {
            bricks[x][y] = false;
        }
    }
    clientSpecific.joined       = true;
    while(!bricks[1][1])
    {
        ReceiveData((uint8_t*)&bricks, sizeof(bricks));
        SendData((uint8_t*)&clientSpecific, HOST_IP_ADDR, sizeof(clientSpecific));  //send specific player info;
    }

    GenerateboardClient();
    LP3943_LedModeSet(GREEN, 0);
    LP3943_LedModeSet(RED, 0);

    G8RTOS_AddThread(IdleThread, 255, "IDLE");
    G8RTOS_AddThread(SendDataToHost, 50, "SendDataToHost");
    G8RTOS_AddThread(ReceiveDataFromHost, 50, "RecieveDataFromHost");
    G8RTOS_AddThread(DrawObjects, 45, "DrawObjects");
    G8RTOS_AddThread(ReadJoystickClient, 47, "ReadJoystick");

    P5->IFG &= ~BIT4;       // P5.4 IFG cleared
    P5->IE  |= BIT4;        // Enable interrupt on P5.4

    G8RTOS_KillSelf();
}

/*********************************************** Client Threads *********************************************************************/


/*********************************************** Host Threads *********************************************************************/
/*
 * Thread for the host to create a game
 */

void CreateGame()
{
    srand ( time(NULL) );
    hostSpecific.IP_address    = HOST_IP_ADDR;
    hostSpecific.playerNumber  = Host;
    hostSpecific.acknowledge   = 0;
    hostSpecific.joined        = 1;
    hostSpecific.ready         = 0;

//  player 0 is host
    GState.players[0].color = PLAYER_BLUE;
    GState.players[0].positionX = 0;
    GState.players[0].positionY = 0;

//  players[1] is client
    GState.players[1].color = PLAYER_RED;
    GState.players[1].positionX = 8;
    GState.players[1].positionY = 6;

    GState.overallScores[0]            = 0;
    GState.overallScores[1]            = 0;

    /****************************Handshake********************************/
    while(ReceiveData((uint8_t*)&clientSpecific, sizeof(clientSpecific)) < 0);    //wait for specific player
    GState.player.acknowledge = 1;

    LCD_Text(MAX_SCREEN_X>>1, MAX_SCREEN_Y>>1, "Connected_H", LCD_BLUE);

    SendData((uint8_t*)&GState, clientSpecific.IP_address, sizeof(GState));       //join host and player
    while(ReceiveData((uint8_t*)&clientSpecific, sizeof(clientSpecific)) < 0);    //wait for acknowledgemt from player
    GState.player.ready = 1;

    /****************************Handshake********************************/
    Generateboard();
    SendData((uint8_t*)&bricks, clientSpecific.IP_address, sizeof(bricks));       //join host and player

    hostSpecific = GState.player;
    GState.player.acknowledge   = false;
    GState.player.ready         = false;
    GState.player.joined        = false;

    G8RTOS_AddThread(IdleThread, 255, "IDLE");
    G8RTOS_AddThread(SendDataToClient, 50, "SendDataToClient");
    G8RTOS_AddThread(ReceiveDataFromClient, 50, "RecieveDataFromClient");
    G8RTOS_AddThread(ReadJoystickHost, 48, "ReadJoystickH");
    G8RTOS_AddThread(DrawObjects, 45, "DrawObjects");
    G8RTOS_AddThread(MovePlayers, 47, "MovePlayers");
    G8RTOS_AddThread(MoveBomb, 46, "MoveBomb");

    P4->IFG &= ~BIT4;       // P4.4 IFG cleared
    P4->IE  |= BIT4;        // Enable interrupt on P4.4
    P4->IES |= BIT4;        // high-to-low transition
    G8RTOS_AddAPeriodicEvent(HostButton, 0, PORT4_IRQn);

    G8RTOS_KillSelf();
}

/*
 * Thread that sends game state to client
 */
void SendDataToClient()
{
    while(1)
    {
        G8RTOS_WaitSemaphore(&WIFI_semaphore);
        SendData((uint8_t*)&GState, clientSpecific.IP_address, sizeof(GState));   //send packet to client
        G8RTOS_SignalSemaphore(&WIFI_semaphore);
        sleep(3);

        if(GState.gameDone)
        {
            if(GState.hostDead && GState.clientDead){
                GState.winner   = false;
                GState.tie      = true;
            }else if(GState.clientDead){
                GState.overallScores[0]++;
                GState.winner   = false;
                GState.tie      = false;
            }else if(GState.hostDead){
                GState.overallScores[1]++;
                GState.winner   = true;
                GState.tie      = false;
            }
            G8RTOS_AddThread(EndOfGameHost, 1, "EndOfGameHost");
            //sleep(50);
        }
    }
}

/*
 * Thread that receives UDP packets from client
 */
void ReceiveDataFromClient()
{
    /* Received UDP packet from client is a SpecificPlayer_t struct.
     * This is loaded into clientSpecific*/
    while(1)
    {
        G8RTOS_WaitSemaphore(&WIFI_semaphore);
        ReceiveData((uint8_t*)&clientSpecific, sizeof(clientSpecific));
        G8RTOS_SignalSemaphore(&WIFI_semaphore);

        GState.players[1].direction     = clientSpecific.direction;
        if(clientSpecific.hostCleared == true && GState.clearHostBomb == true){
            GState.clearHostBomb = false;
        }
        if(clientSpecific.clientCleared == true && GState.clearClientBomb == true){
            GState.clearClientBomb = false;
        }
        sleep(2);
    }
}

/*
 * Thread to read host's joystick
 */
void ReadJoystickHost()
{
    int16_t x = 0;
    int16_t y = 0;

    while(1)
    {
        /**********************HOST**************************/
        GetJoystickCoordinates(&x, &y);             //host

        if(x > 3500 ){
            GState.players[0].direction = LEFT;
        }else if(x < -3500){
            GState.players[0].direction = RIGHT;
        }
        else if(y > 3500){
            GState.players[0].direction = DOWN;
        }else if(y < -3500){
            GState.players[0].direction = UP;
        }else{
            GState.players[0].direction = NONE;
        }
        /**********************HOST**************************/

        sleep(100);
    }
}

/*
 * Thread to move a single player
 */
void MovePlayers()
{
    //Initial previous directions
    Direction_t prevDirectionHost   = NONE;
    Direction_t prevDirectionClient = NONE;

    while(1)
    {
        //Changing the location of the Host
        if(movedHost){
            if(GState.players[0].direction != prevDirectionHost){
                movedHost = false;
            }
        }else{
            if(GState.players[0].direction == UP){
                if(GState.players[0].positionY > 0 && (bricks[GState.players[0].positionX][GState.players[0].positionY-1] == false)){
                    for(int i=0;i<2;i++){
                        if(GState.bombs[i].positionX == GState.players[0].positionX && GState.bombs[i].positionY == GState.players[0].positionY-1 && GState.bombs[i].alive){
                            GState.bombs[i].direction = UP;
                            movedHost = true;
                        }
                    }
                    if(!movedHost){
                        GState.players[0].positionY--;
                        movedHost = true;
                    }
                }
                prevDirectionHost   = UP;
            }else if(GState.players[0].direction == DOWN){
                if(GState.players[0].positionY < 6 && (bricks[GState.players[0].positionX][GState.players[0].positionY + 1] == false))
                {
                    for(int i=0;i<2;i++){
                        if(GState.bombs[i].positionX == GState.players[0].positionX && GState.bombs[i].positionY == GState.players[0].positionY+1 && GState.bombs[i].alive){
                            GState.bombs[i].direction = DOWN;
                            movedHost = true;
                        }
                    }
                    if(!movedHost){
                        GState.players[0].positionY++;
                        movedHost = true;
                    }
                }
                prevDirectionHost   = DOWN;
            }else if(GState.players[0].direction == LEFT){
                if(GState.players[0].positionX > 0 && (bricks[GState.players[0].positionX - 1][GState.players[0].positionY] == false)){
                    for(int i=0;i<2;i++){
                        if(GState.bombs[i].positionX == GState.players[0].positionX-1 && GState.bombs[i].positionY == GState.players[0].positionY && GState.bombs[i].alive){
                            GState.bombs[i].direction = LEFT;
                            movedHost = true;
                        }
                    }
                    if(!movedHost){
                        GState.players[0].positionX--;
                        movedHost = true;
                    }
                }
                prevDirectionHost   = LEFT;
            }else if(GState.players[0].direction == RIGHT){
                if(GState.players[0].positionX < 8 && (bricks[GState.players[0].positionX + 1][GState.players[0].positionY] == false)){
                    for(int i=0;i<2;i++){
                        if(GState.bombs[i].positionX == GState.players[0].positionX+1 && GState.bombs[i].positionY == GState.players[0].positionY && GState.bombs[i].alive){
                            GState.bombs[i].direction = RIGHT;
                            movedHost = true;
                        }
                    }
                    if(!movedHost){
                        GState.players[0].positionX++;
                        movedHost = true;
                    }
                }
                prevDirectionHost   = RIGHT;
            }else{
                prevDirectionHost   = NONE;
            }
        }

        //Changing the location of the Client
        if(movedClient){
            if(GState.players[1].direction != prevDirectionClient){
                movedClient = false;
            }
        }else{
            if(GState.players[1].direction == UP){
                if(GState.players[1].positionY > 0 && bricks[GState.players[1].positionX][GState.players[1].positionY-1] == false){
                    for(int i=0;i<2;i++){
                        if(GState.bombs[i].positionX == GState.players[1].positionX && GState.bombs[i].positionY == GState.players[1].positionY-1 && GState.bombs[i].alive){
                            GState.bombs[i].direction = UP;
                            movedClient = true;
                        }
                    }
                    if(!movedClient){
                        GState.players[1].positionY--;
                        movedClient = true;
                    }
                }
                prevDirectionClient   = UP;
            }else if(GState.players[1].direction == DOWN){
                if(GState.players[1].positionY < 6 && bricks[GState.players[1].positionX][GState.players[1].positionY + 1] == false)
                {
                    for(int i=0;i<2;i++){
                        if(GState.bombs[i].positionX == GState.players[1].positionX && GState.bombs[i].positionY == GState.players[1].positionY+1 && GState.bombs[i].alive){
                            GState.bombs[i].direction = DOWN;
                            movedClient = true;
                        }
                    }
                    if(!movedClient){
                        GState.players[1].positionY++;
                        movedClient = true;
                    }
                }
                prevDirectionClient   = DOWN;
            }else if(GState.players[1].direction == LEFT ){
                if(GState.players[1].positionX > 0 && bricks[GState.players[1].positionX - 1][GState.players[1].positionY] == false){
                    for(int i=0;i<2;i++){
                        if(GState.bombs[i].positionX == GState.players[1].positionX-1 && GState.bombs[i].positionY == GState.players[1].positionY && GState.bombs[i].alive){
                            GState.bombs[i].direction = LEFT;
                            movedClient = true;
                        }
                    }
                    if(!movedClient){
                        GState.players[1].positionX--;
                        movedClient = true;
                    }
                }
                prevDirectionClient   = LEFT;
            }else if(GState.players[1].direction == RIGHT){
                if(GState.players[1].positionX < 8 && bricks[GState.players[1].positionX + 1][GState.players[1].positionY] == false){
                    for(int i=0;i<2;i++){
                        if(GState.bombs[i].positionX == GState.players[1].positionX+1 && GState.bombs[i].positionY == GState.players[1].positionY && GState.bombs[i].alive){
                            GState.bombs[i].direction = RIGHT;
                            movedClient = true;
                        }
                    }
                    if(!movedClient){
                        GState.players[1].positionX++;
                        movedClient = true;
                    }
                }
                prevDirectionClient   = RIGHT;
            }else{
                prevDirectionClient   = NONE;
            }
        }
        sleep(30);
    }
}

/*
 * Thread to move a single bomb
 */
void MoveBomb()
{
    Bomb_t *bombHost    = &GState.bombs[0];
    Bomb_t *bombClient  = &GState.bombs[1];

    while(1)
    {
        if(hostDroppedBomb && bombHost->alive == false){
            hostDroppedBomb     = false;
            bombHost->kaboomTime= 40;
            bombHost->alive     = true;
            bombHost->positionX = GState.players[0].positionX;
            bombHost->positionY = GState.players[0].positionY;
            bombHost->color     = LCD_BLUE;
        }
        if(bombHost->alive){
            bombHost->kaboomTime--;
            if(bombHost->kaboomTime == 0){
                bombHost->alive         = false;
                globalHostBombClr       = true;
                GState.clearHostBomb    = true;
                G8RTOS_AddThread(ExplodeHost, 40, "explodehost");
                G8RTOS_AddThread(Explosion, 50, "LED HOST");
                sleep(2);

            }
        }
        if(bombHost->direction != NONE){
            if(bombHost->direction == UP){
                for(int i=0; i<2; i++){
                    //this for loop checks to see if either player is in the way
                    if((bombHost->positionX == GState.players[i].positionX && bombHost->positionY-1 == GState.players[i].positionY)
                    || (bombHost->positionX == GState.bombs[1].positionX && bombHost->positionY-1 == GState.bombs[1].positionY  && GState.bombs[1].alive)){
                        bombHost->direction = NONE;
                    }
                }
                if(bricks[bombHost->positionX][bombHost->positionY-1] || (bombHost->positionY-1)<0){
                    //checks if there is a brick in way of path
                    bombHost->direction = NONE;
                }
                if(bombHost->direction == UP){//if there was nothing in the way, change position
                    bombHost->positionY -=1;
                }
            }else if(bombHost->direction == DOWN){
                for(int i=0; i<2; i++){
                    //this for loop checks to see if either player is in the way
                    if((bombHost->positionX == GState.players[i].positionX && bombHost->positionY+1 == GState.players[i].positionY)
                    || (bombHost->positionX == GState.bombs[1].positionX && bombHost->positionY+1 == GState.bombs[1].positionY  && GState.bombs[1].alive)){
                        bombHost->direction = NONE;
                    }
                }
                if(bricks[bombHost->positionX][bombHost->positionY+1] || (bombHost->positionY+1)>6){
                    //checks if there is a brick in way of path
                    bombHost->direction = NONE;
                }
                if(bombHost->direction == DOWN){//if there was nothing in the way, change position
                    bombHost->positionY +=1;
                }
            }else if(bombHost->direction == LEFT){
                for(int i=0; i<2; i++){
                    //this for loop checks to see if either player is in the way
                    if((bombHost->positionY == GState.players[i].positionY && bombHost->positionX-1 == GState.players[i].positionX)
                    || (bombHost->positionX-1 == GState.bombs[1].positionX && bombHost->positionY == GState.bombs[1].positionY  && GState.bombs[1].alive)){
                        bombHost->direction = NONE;
                    }
                }
                if(bricks[bombHost->positionX-1][bombHost->positionY] || (bombHost->positionX-1)<0){
                    //checks if there is a brick in way of path
                    bombHost->direction = NONE;
                }
                if(bombHost->direction == LEFT){//if there was nothing in the way, change position
                    bombHost->positionX -=1;
                }
            }else if(bombHost->direction == RIGHT){
                for(int i=0; i<2; i++){
                    //this for loop checks to see if either player is in the way
                    if((bombHost->positionY == GState.players[i].positionY && bombHost->positionX+1 == GState.players[i].positionX)
                    || (bombHost->positionX+1 == GState.bombs[1].positionX && bombHost->positionY == GState.bombs[1].positionY  && GState.bombs[1].alive)){
                        bombHost->direction = NONE;
                    }
                }
                if(bricks[bombHost->positionX+1][bombHost->positionY] || (bombHost->positionX+1)>8){
                    //checks if there is a brick in way of path
                    bombHost->direction = NONE;
                }
                if(bombHost->direction == RIGHT){//if there was nothing in the way, change position
                    bombHost->positionX +=1;
                }
            }
        }


        if(clientSpecific.clientDroppedBomb && bombClient->alive == false){   //change this
            GState.clearClientBomb = false;
            bombClient->kaboomTime    = 40;
            bombClient->alive         = true;
            bombClient->positionX     = GState.players[1].positionX;
            bombClient->positionY     = GState.players[1].positionY;
            bombClient->color         = LCD_RED;
        }
        if(bombClient->alive){
            bombClient->kaboomTime--;
            if(bombClient->kaboomTime == 0){
                bombClient->alive       = false;
                globalClientBombClr     = true;
                GState.clearClientBomb  = true;
                G8RTOS_AddThread(ExplodeClient, 40, "explodehost");
                G8RTOS_AddThread(Explosion, 50, "LED HOST");
                sleep(2);
            }
        }
        if(bombClient->direction != NONE){
            if(bombClient->direction == UP){
                for(int i=0; i<2; i++){
                    //this for loop checks to see if either player is in the way
                    if((bombClient->positionX == GState.players[i].positionX && bombClient->positionY-1 == GState.players[i].positionY)
                    || (bombClient->positionX == GState.bombs[0].positionX && bombClient->positionY-1 == GState.bombs[0].positionY && GState.bombs[0].alive)){
                        bombClient->direction = NONE;
                    }
                }
                if(bricks[bombClient->positionX][bombClient->positionY-1] || (bombClient->positionY-1)<0){
                    //checks if there is a brick in way of path
                    bombClient->direction = NONE;
                }
                if(bombClient->direction == UP){//if there was nothing in the way, change position
                    bombClient->positionY -=1;
                }
            }else if(bombClient->direction == DOWN){
                for(int i=0; i<2; i++){
                    //this for loop checks to see if either player is in the way
                    if((bombClient->positionX == GState.players[i].positionX && bombClient->positionY+1 == GState.players[i].positionY)
                    || (bombClient->positionX == GState.bombs[0].positionX && bombClient->positionY+1 == GState.bombs[0].positionY && GState.bombs[0].alive)){
                        bombClient->direction = NONE;
                    }
                }
                if(bricks[bombClient->positionX][bombClient->positionY+1] || (bombClient->positionY+1)>6){
                    //checks if there is a brick in way of path
                    bombClient->direction = NONE;
                }
                if(bombClient->direction == DOWN){//if there was nothing in the way, change position
                    bombClient->positionY +=1;
                }
            }else if(bombClient->direction == LEFT){
                for(int i=0; i<2; i++){
                    //this for loop checks to see if either player is in the way
                    if((bombClient->positionY == GState.players[i].positionY && bombClient->positionX-1 == GState.players[i].positionX)
                    || (bombClient->positionX-1 == GState.bombs[0].positionX && bombClient->positionY == GState.bombs[0].positionY && GState.bombs[0].alive)){
                        bombClient->direction = NONE;
                    }
                }
                if(bricks[bombClient->positionX-1][bombClient->positionY] || (bombClient->positionX-1)<0){
                    //checks if there is a brick in way of path
                    bombClient->direction = NONE;
                }
                if(bombClient->direction == LEFT){//if there was nothing in the way, change position
                    bombClient->positionX -=1;
                }
            }else if(bombClient->direction == RIGHT){
                for(int i=0; i<2; i++){
                    //this for loop checks to see if either player is in the way
                    if((bombClient->positionY == GState.players[i].positionY && bombClient->positionX+1 == GState.players[i].positionX)
                    || (bombClient->positionX+1 == GState.bombs[0].positionX && bombClient->positionY == GState.bombs[0].positionY && GState.bombs[0].alive)){
                        bombClient->direction = NONE;
                    }
                }
                if(bricks[bombClient->positionX+1][bombClient->positionY] || (bombClient->positionX+1)>8){
                    //checks if there is a brick in way of path
                    bombClient->direction = NONE;
                }
                if(bombClient->direction == RIGHT){//if there was nothing in the way, change position
                    bombClient->positionX +=1;
                }
            }
        }
        if(GState.clientDead || GState.hostDead){
            GState.gameDone = true;
        }

        sleep(35);
    }
}


/*
 * End of game for the host
 */
void EndOfGameHost()
{
    P4->IE &= ~BIT4;                                    //disable interrupt
    G8RTOS_WaitSemaphore(&LCD_semaphore);
    G8RTOS_WaitSemaphore(&WIFI_semaphore);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    G8RTOS_SignalSemaphore(&LCD_semaphore);
    G8RTOS_SignalSemaphore(&WIFI_semaphore);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    G8RTOS_KillAllOtherThreads();

    LP3943_ColorSet(RED, 0xFF, 0xFFFF);
    LP3943_ColorSet(GREEN, 0x30, 0xFFFF);

    LCD_Clear(LCD_WHITE);
    if(GState.tie){
        LCD_Text(100, 40, "YA BOTH EAT A$$", LCD_PURPLE);
    }else if(GState.winner == false){
        LCD_Text(125, 40, "YOU WIN!", LCD_BLUE);
    }else{
        LCD_Text(120, 40, "YOU LOST!", LCD_RED);
    }

    snprintf(overallBlueScore, 16, "BLUE SCORE %d", GState.overallScores[0]);
    snprintf(overallRedScore, 16, "RED SCORE %d", GState.overallScores[1]);

    GState.players[0].positionX = 2;
    GState.players[0].positionY = 2;
    GState.players[1].positionX = 6;
    GState.players[1].positionY = 2;

    UpdatePlayerOnScreen(0, &GState.players[0]);
    LCD_Text(50, 110, overallBlueScore, LCD_BLUE);

    UpdatePlayerOnScreen(0, &GState.players[1]);
    LCD_Text(190, 110, overallRedScore, LCD_RED);

    LCD_Text(80, 130, "Press Up to Re-match ", LCD_BLACK);

    //transmit data to client
    clientSpecific.acknowledge  = false;
    clientSpecific.ready        = false;
    clientSpecific.joined       = false;

    GState.player.acknowledge   = true;
    GState.player.ready         = false;
    GState.gameDone             = false;

    //Wait for Host to be ready
    endGameFlag = true;
    P4->IFG &= ~BIT4;               //Clear IFG flag
    P4->IE  |= BIT4;                //Enable interrupt on P4.4
    while(endGameFlag == true);
    endGameFlag = false;

    //Reset all global game variables, except scores, bomb flags
    GState.players[0].positionX    = 0;    //player 0 is host
    GState.players[0].positionY    = 0;    //player 0 is host
    GState.players[0].direction    = NONE;
    GState.players[1].positionX    = 8;    //players[1] is client
    GState.players[1].positionY    = 6;    //players[1] is client
    GState.players[1].direction    = NONE;
    GState.hostDead                = false;
    GState.clientDead              = false;
    GState.clearClientBomb         = false;
    GState.clearHostBomb           = false;
    GState.gameDone                = false;
    GState.winner                  = false;
    GState.tie                     = false;

    for(int i = 0; i < MAX_NUM_OF_BOMBS; i++)
    {
        GState.bombs[i].alive = false;
    }

    movedHost                      = false;
    movedClient                    = false;
    hostDroppedBomb                = false;
    globalHostBombClr              = false;
    globalClientBombClr            = false;


//    //transmit data to client
//    while(ReceiveData((uint8_t*)&clientSpecific, sizeof(clientSpecific)) < 0);    //wait for acknowledgemt from player
//
//    SendData((uint8_t*)&GState, clientSpecific.IP_address, sizeof(GState));       //join host and player
//    while(ReceiveData((uint8_t*)&clientSpecific, sizeof(clientSpecific)) < 0);    //wait for acknowledgemt from player
    GState.player.ready = 1;

    for(int x = 0; x<9; x++)
    {
        for(int y = 0; y<7; y++)
        {
            bricks[x][y] = false;
        }
    }

    clientSpecific.joined = false;
    Generateboard();
    while(clientSpecific.joined == false)
    {
        ReceiveData((uint8_t*)&clientSpecific, sizeof(clientSpecific));
    }
    SendData((uint8_t*)&bricks, clientSpecific.IP_address, sizeof(bricks));       //join host and player

    LP3943_LedModeSet(GREEN, 0);
    LP3943_LedModeSet(RED, 0);

    G8RTOS_AddThread(IdleThread, 255, "IDLE");
    G8RTOS_AddThread(SendDataToClient, 50, "SendDataToClient");
    G8RTOS_AddThread(ReceiveDataFromClient, 50, "RecieveDataFromClient");
    G8RTOS_AddThread(ReadJoystickHost, 48, "ReadJoystickH");
    G8RTOS_AddThread(DrawObjects, 45, "DrawObjects");
    G8RTOS_AddThread(MovePlayers, 47, "MovePlayers");
    G8RTOS_AddThread(MoveBomb, 46, "MoveBomb");

    P4->IFG &= ~BIT4;               //Clear IFG flag
    P4->IE  |= BIT4;                //Enable interrupt on P4.4
    G8RTOS_KillSelf();
}

/*********************************************** Host Threads *********************************************************************/


/*********************************************** Common Threads *********************************************************************/
/*
 * Idle thread
 */
void IdleThread()
{
    while(1);
}

/*
 * Thread to draw all the objects in the game
 */
void DrawObjects()
{
    prevHostLocation.positionX      = 0 ;  //initial center
    prevHostLocation.positionY      = 0 ;  //initial center
    prevClientLocation.positionX    = 8;  //initial center
    prevClientLocation.positionY    = 6;  //initial center

    //bombs out of sight
    GState.bombs[0].positionX       = -1;
    GState.bombs[0].positionY       = -1;
    GState.bombs[1].positionX       = -1;
    GState.bombs[1].positionY       = -1;
    prevHostBomb.positionX          = GState.bombs[0].positionX;
    prevHostBomb.positionY          = GState.bombs[0].positionY;
    prevClientBomb.positionX        = GState.bombs[1].positionX;
    prevClientBomb.positionY        = GState.bombs[1].positionY;

    while(1)
    {
        G8RTOS_WaitSemaphore(&LCD_semaphore);

        UpdateBombOnScreen(&prevHostBomb, &GState.bombs[0], GState.bombs[0].color);
        prevHostBomb.positionX = GState.bombs[0].positionX;
        prevHostBomb.positionY = GState.bombs[0].positionY;

        UpdateBombOnScreen(&prevClientBomb, &GState.bombs[1], GState.bombs[1].color);
        prevClientBomb.positionX = GState.bombs[1].positionX;
        prevClientBomb.positionY = GState.bombs[1].positionY;

        if(prevHostLocation.positionX != GState.players[0].positionX || prevHostLocation.positionY != GState.players[0].positionY){
            UpdatePlayerOnScreen(&prevHostLocation, &GState.players[0]);
        }else if(prevHostLocation.direction != GState.players[0].direction && GState.players[0].direction != NONE){
            UpdatePlayerOnScreen(&prevHostLocation, &GState.players[0]);
        }
        if(prevClientLocation.positionX != GState.players[1].positionX || prevClientLocation.positionY != GState.players[1].positionY){
            UpdatePlayerOnScreen(&prevClientLocation, &GState.players[1]);
        }else if(prevClientLocation.direction != GState.players[1].direction && GState.players[1].direction != NONE){
            UpdatePlayerOnScreen(&prevClientLocation, &GState.players[1]);
        }

        G8RTOS_SignalSemaphore(&LCD_semaphore);

        sleep(40);
    }
}

void DebounceHost()
{
    if(GState.bombs[0].alive == false)
    {
        hostDroppedBomb = true;
    }
    sleep(30);                      //30ms for debounce
    P4->IFG &= ~BIT4;
    P4->IE  |= BIT4;                //re-enable interrupt
    G8RTOS_KillSelf();
}

void DebounceClient()
{
    if(GState.bombs[1].alive == false){
        clientSpecific.clientDroppedBomb = true;
    }

    sleep(30);                      //30ms for debounce
    P5->IFG &= ~BIT4;
    P5->IE  |= BIT4;                //re-enable interrupt
    G8RTOS_KillSelf();
}
/*********************************************** Common Threads *********************************************************************/


/*********************************************** Public Functions *********************************************************************/
/*
 * Returns either Host or Client depending on button press
 */
playerType GetPlayerRole()
{
    LCD_Text(90, 100, "Press up for Host", LCD_WHITE);
    LCD_Text(90, 130, "Press down for Client", LCD_WHITE);

    while(1)
    {
        if(!GPIO_getInputPinValue(GPIO_PORT_P4, GPIO_PIN4))          //HOST
        {
            G8RTOS_AddThread(CreateGame, 1, "Host");
            LCD_DrawRectangle(90, 300, 100, 150, LCD_BLACK);
            LCD_Text(130, 100, "HOST", LCD_BLUE);
            return Host;
        }else if(!GPIO_getInputPinValue(GPIO_PORT_P5, GPIO_PIN4))    //CLIENT
        {
            G8RTOS_AddThread(JoinGame, 1, "Client");
            LCD_DrawRectangle(90, 300, 100, 150, LCD_BLACK);
            LCD_Text(130, 100, "CLIENT", LCD_RED);
            return Client;
        }
    }
}


/*
 * Updates player based on current and new center
 */
void UpdatePlayerOnScreen(PrevPlayer_t * prevPlayerIn, GeneralPlayerInfo_t * outPlayer)
{
    int x;
    int y;
    if(prevPlayerIn){
        y= prevPlayerIn->positionY*34 + 1;
        x= prevPlayerIn->positionX*34 + 7;
        if(outPlayer->color == LCD_BLUE && prevPlayerIn->positionX == GState.players[1].positionX && prevPlayerIn->positionY == GState.players[1].positionY){
            LCD_DrawRectangle(x+12, x+22, y+6, y+26, 0x84ca);
            UpdatePlayerOnScreen(0, &GState.players[1]);
        }else if(outPlayer->color == LCD_RED && prevPlayerIn->positionX == GState.players[0].positionX && prevPlayerIn->positionY == GState.players[0].positionY){
            LCD_DrawRectangle(x+12, x+22, y+6, y+26, 0x84ca);
            UpdatePlayerOnScreen(0, &GState.players[0]);
        }else{
            LCD_DrawRectangle(x+12, x+22, y+6, y+26, 0x84ca);
        }
    }
    x = outPlayer->positionX * 34 + 7;
    y = outPlayer->positionY * 34 + 1;
    if(outPlayer->direction == UP){
        LCD_DrawRectangle(x+14, x+20, y+6, y+12, 0xf652);//0x84ca
        LCD_DrawRectangle(x+13, x+21, y+10, y+19, outPlayer->color);//0x84ca
        LCD_DrawRectangle(x+13, x+21, y+19, y+22, LCD_BLACK);
        LCD_DrawRectangle(x+21, x+22, y+14, y+18, 0xf652);//0x84ca
        LCD_DrawRectangle(x+12, x+13, y+14, y+18, 0xf652);//0x84ca
        LCD_DrawRectangle(x+19, x+21, y+22, y+26, 0xf652);//0x84ca
        LCD_DrawRectangle(x+13, x+15, y+22, y+26, 0xf652);//0x84ca
    }else if(outPlayer->direction == RIGHT){
        LCD_DrawRectangle(x+14, x+20, y+10, y+19, outPlayer->color);//0x84ca
        LCD_DrawRectangle(x+14, x+20, y+19, y+22, LCD_BLACK);
        LCD_DrawRectangle(x+14, x+20, y+6, y+12, 0xf652);//face
        LCD_DrawRectangle(x+19, x+20, y+7, y+9, LCD_BLACK);//0x84ca
        LCD_DrawRectangle(x+16, x+18, y+14, y+18, 0xf652);//0x84ca
        LCD_DrawRectangle(x+16, x+18, y+22, y+26, 0xf652);//0x84ca
    }else if(outPlayer->direction == LEFT){
        LCD_DrawRectangle(x+14, x+20, y+10, y+19, outPlayer->color);//shirt
        LCD_DrawRectangle(x+14, x+20, y+19, y+22, LCD_BLACK);//pants
        LCD_DrawRectangle(x+14, x+20, y+6, y+12, 0xf652);//face
        LCD_DrawRectangle(x+14, x+15, y+7, y+9, LCD_BLACK);//eye
        LCD_DrawRectangle(x+16, x+18, y+14, y+18, 0xf652);//0x84ca
        LCD_DrawRectangle(x+16, x+18, y+22, y+26, 0xf652);//0x84ca
    }else{
        LCD_DrawRectangle(x+13, x+21, y+10, y+19, outPlayer->color);//0x84ca
        LCD_DrawRectangle(x+13, x+21, y+19, y+22, LCD_BLACK);
        LCD_DrawRectangle(x+14, x+20, y+6, y+12, 0xf652);//0x84ca
        LCD_DrawRectangle(x+15, x+16, y+7, y+9, LCD_BLACK);//0x84ca
        LCD_DrawRectangle(x+18, x+19, y+7, y+9, LCD_BLACK);//0x84ca
        LCD_DrawRectangle(x+21, x+22, y+14, y+18, 0xf652);//0x84ca
        LCD_DrawRectangle(x+12, x+13, y+14, y+18, 0xf652);//0x84ca
        LCD_DrawRectangle(x+19, x+21, y+22, y+26, 0xf652);//0x84ca
        LCD_DrawRectangle(x+13, x+15, y+22, y+26, 0xf652);//0x84ca
    }
    prevPlayerIn->positionX = outPlayer->positionX;
    prevPlayerIn->positionY = outPlayer->positionY;
    prevPlayerIn->direction = outPlayer->direction;
}

/*
 * Function updates ball position on screen
 */
void UpdateBombOnScreen(PrevBomb_t * previousBomb, Bomb_t * currentBomb, uint16_t outColor)
{
    if(currentBomb->alive == true){
        DrawBomb(previousBomb->positionX, previousBomb->positionY, 0x84ca);
        DrawBomb(currentBomb->positionX, currentBomb->positionY, currentBomb->color);
    }
}

//void DrawBomb(Bomb_t * bomb, uint16_t outColor)
void DrawBomb(int16_t positionX, int16_t positionY, uint16_t outColor)
{
    if(positionX != -1 && positionY != -1)
    {
        int x = (positionX * 34) + 7;
        int y = (positionY * 34) + 1;
        LCD_DrawRectangle(x+12, x+22, y+12, y+22, outColor);//0x84ca
        LCD_DrawRectangle(x+11, x+12, y+13, y+21, outColor);
        LCD_DrawRectangle(x+10, x+11, y+15, y+19, outColor);
        LCD_DrawRectangle(x+13, x+21, y+11, y+12, outColor);
        LCD_DrawRectangle(x+15, x+19, y+10, y+11, outColor);
        LCD_DrawRectangle(x+22, x+23, y+13, y+21, outColor);
        LCD_DrawRectangle(x+23, x+24, y+15, y+19, outColor);
        LCD_DrawRectangle(x+13, x+21, y+22, y+23, outColor);
        LCD_DrawRectangle(x+15, x+19, y+23, y+24, outColor);
    }
}

/*********************************************** Public Functions *********************************************************************/
void HostButton()
{
    if(endGameFlag == false)
    {
        P4->IE &= ~BIT4;                                //disable interrupt
        G8RTOS_AddThread(DebounceHost, 8, "Debounce");
    }else{
        endGameFlag = false;
        P4->IE &= ~BIT4;                                //disable interrupt
    }
}

void ClientButton()
{
    P5->IE &= ~BIT4;                    //disable interrupt
    G8RTOS_AddThread(DebounceClient, 8, "Debounce");
}

void Generateboard (){
    //srand (time(NULL));
    LCD_Clear(0x84ca);
    LCD_DrawRectangle(0, 320, 0, 1, 0);
    LCD_DrawRectangle(0, 320, 239, 240, 0);
    LCD_DrawRectangle(0, 7, 0, 240, 0);
    LCD_DrawRectangle(313,320, 0, 240, 0);
    int x = 7;
    for(int i = 0; i<9;i++){
        int y = 1;
        for(int j = 0; j<7;j++){
            if(!(x==7 && y==1) && !(x==7 && y==35) && !(x==41 && y==1) &&
               !(x==245 && y==205) && !(x==279 && y==205) && !(x==279 && y==171)){
                if(rand()%2){
                    bricks[i][j] = true;
                    drawvolatile(x,y);
                }
            }
            y += 34;
        }
        x += 34;
    }
    x = 7 + 34;
    int xx = 1;
    for(int i = 0;i<4;i++){//x x 4
        int y = 1 + 34;
        int yy = 1;
        for(int j=0;j<3;j++){//y x 3
            drawperm(x,y);
            bricks[xx][yy] = true;
            y += 2*34;
            yy += 2;
        }
        xx += 2;
        x += 2*34;
    }
    GState.players[0].positionX = 0;
    GState.players[0].positionY = 0;
    GState.players[0].color = LCD_BLUE;
    GState.players[1].color = LCD_RED;
    GState.players[1].positionX = 8;
    GState.players[1].positionY = 6;
    UpdatePlayerOnScreen(0, &GState.players[0]);
    UpdatePlayerOnScreen(0, &GState.players[1]);
}

void GenerateboardClient(){
    LCD_Clear(0x84ca);
    LCD_DrawRectangle(0, 320, 0, 1, 0);
    LCD_DrawRectangle(0, 320, 239, 240, 0);
    LCD_DrawRectangle(0, 7, 0, 240, 0);
    LCD_DrawRectangle(313,320, 0, 240, 0);
    int x = 7;
    for(int i = 0; i<9;i++){
        int y = 1;
        for(int j = 0; j<7;j++){
            if(!(x==7 && y==1) && !(x==7 && y==35) && !(x==41 && y==1) &&
               !(x==245 && y==205) && !(x==279 && y==205) && !(x==279 && y==171)){
                if(bricks[i][j]){
                    drawvolatile(x,y);
                }
            }
            y += 34;
        }
        x += 34;
    }
    x = 7 + 34;
    int xx = 1;
    for(int i = 0;i<4;i++){//x x 4
        int y = 1 + 34;
        int yy = 1;
        for(int j=0;j<3;j++){//y x 3
            if(bricks[xx][yy]){
                drawperm(x,y);
            }
            y += 2*34;
            yy += 2;
        }
        xx += 2;
        x += 2*34;
    }
    GState.players[0].positionX = 0;
    GState.players[0].positionY = 0;
    GState.players[0].color = LCD_BLUE;
    GState.players[1].color = LCD_RED;
    GState.players[1].positionX = 8;
    GState.players[1].positionY = 6;
    UpdatePlayerOnScreen(0, &GState.players[0]);
    UpdatePlayerOnScreen(0, &GState.players[1]);
//    Bomb_t bomb;
//    bomb.positionX = 7;
//    bomb.positionY = 6;
//    DrawBomb(&bomb);
}

void drawperm(int x, int y){
    LCD_DrawRectangle(x, x+34, y, y+34, 0x634c);
    LCD_DrawRectangle(x, x+2, y, y+34, 0x52aa);
    LCD_DrawRectangle(x, x+34, y+32, y+34, 0x52aa);//0x52aa
    LCD_DrawRectangle(x+32, x+34, y, y+34, 0x9cf3);
    LCD_DrawRectangle(x, x+34, y, y+2, 0x9cf3);//0x7d2e
}

void drawvolatile(int x, int y){
    //brick background color and separating bars
    LCD_DrawRectangle(x, x+34, y, y + 34, 0x83ed);//0x2c03
    LCD_DrawRectangle(x, x+34, y+11, y + 12, 0x4227);//0x2c03
    LCD_DrawRectangle(x, x+34, y+22, y + 23, 0x4227);
    LCD_DrawRectangle(x, x+1, y, y + 11, 0x4227);
    LCD_DrawRectangle(x+13, x+14, y+12, y + 22, 0x4227);
    LCD_DrawRectangle(x+24, x+25, y+ 23, y + 34, 0x4227);
    LCD_DrawRectangle(x, x+34, y, y + 1, 0x4227);
    LCD_DrawRectangle(x, x+34, y+33, y + 34, 0x4227);
    //upper brick shading
    LCD_DrawRectangle(x+2, x+34, y+1, y + 2, 0x94b1);
    LCD_DrawRectangle(x+33, x+34, y+1, y + 11, 0x94b1);
    LCD_DrawRectangle(x, x+13, y+12, y + 13, 0x94b1);
    LCD_DrawRectangle(x+15, x+34, y+12, y + 13, 0x94b1);
    LCD_DrawRectangle(x+12, x+13, y+12, y + 22, 0x94b1);
    LCD_DrawRectangle(x, x+24, y+23, y + 24, 0x94b1);
    LCD_DrawRectangle(x+12, x+13, y+12, y + 22, 0x94b1);
    LCD_DrawRectangle(x+23, x+24, y+23, y + 34, 0x94b1);
    LCD_DrawRectangle(x+26, x+34, y+23, y + 24, 0x94b1);
}


void Explosion(){
    //explosion animation thread
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0x0180);
    LP3943_ColorSet(GREEN, 0x30, 0x0180);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0x03C0);
    LP3943_ColorSet(GREEN, 0x30, 0x03C0);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0x07E0);
    LP3943_ColorSet(GREEN, 0x30, 0x07E0);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0x0FF0);
    LP3943_ColorSet(GREEN, 0x30, 0x0FF0);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0x1E78);
    LP3943_ColorSet(GREEN, 0x30, 0x1E78);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0x3C3C);
    LP3943_ColorSet(GREEN, 0x30, 0x3C3C);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0x781E);
    LP3943_ColorSet(GREEN, 0x30, 0x781E);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0xF00F);
    LP3943_ColorSet(GREEN, 0x30, 0xF00F);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0xE007);
    LP3943_ColorSet(GREEN, 0x30, 0xE007);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0xC003);
    LP3943_ColorSet(GREEN, 0x30, 0xC003);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0x8001);
    LP3943_ColorSet(GREEN, 0x30, 0x8001);
    G8RTOS_SignalSemaphore(&LED_semaphore);
    sleep(40);
    G8RTOS_WaitSemaphore(&LED_semaphore);
    LP3943_ColorSet(RED, 0xFF, 0x0000);
    LP3943_ColorSet(GREEN, 0x30, 0x0000);
    G8RTOS_SignalSemaphore(&LED_semaphore);

    G8RTOS_KillSelf();
}

void ExplodeHost(){
    //host is zero

//    if(globalHostBombClr == true){
        Bomb_t * bomb = &GState.bombs[0];
        int16_t bombPositionX = bomb->positionX;
        int16_t bombPositionY = bomb->positionY;
        int16_t x;
        int16_t y;
//        GState.bombs[0].positionX       = -1;
//        GState.bombs[0].positionY       = -1;
        hostCurrentlyExploding = true;
        G8RTOS_WaitSemaphore(&LCD_semaphore);
        if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
        if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
        x = bombPositionX * 34 +7;
        y = bombPositionY * 34 +1;
        LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
        if(checkperm(bombPositionX+1,bombPositionY) && bombPositionX+1<=8){
            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX+1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX+1][bombPositionY] = false;
        }
        if(checkperm(bombPositionX-1,bombPositionY) && bombPositionX-1>=0){
            if(bombPositionX-1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX-1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX-1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX-1][bombPositionY] = false;
        }
        if(checkperm(bombPositionX,bombPositionY+1) && bombPositionY+1<=6){
            if(bombPositionX == GState.players[0].positionX && bombPositionY+1 == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX == GState.players[1].positionX && bombPositionY+1 == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY+1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX][bombPositionY+1] = false;
        }
        if(checkperm(bombPositionX,bombPositionY-1) && bombPositionY-1>=0){
            if(bombPositionX == GState.players[0].positionX && bombPositionY-1 == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX == GState.players[1].positionX && bombPositionY-1 == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY-1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX][bombPositionY-1] = false;
        }
        G8RTOS_SignalSemaphore(&LCD_semaphore);
        sleep(100);
        G8RTOS_WaitSemaphore(&LCD_semaphore);
        x = bombPositionX * 34 +7;
        y = bombPositionY * 34 +1;
        LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) UpdatePlayerOnScreen(0,&GState.players[0]);
        if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) UpdatePlayerOnScreen(0,&GState.players[1]);
        if(checkperm(bombPositionX+1,bombPositionY) && bombPositionX+1<=8){
            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX+1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX-1,bombPositionY) && bombPositionX-1>=0){
            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX-1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX,bombPositionY+1) && bombPositionY+1<=6){
            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY+1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX,bombPositionY-1) && bombPositionY-1>=0){
            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY-1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        G8RTOS_SignalSemaphore(&LCD_semaphore);
        hostCurrentlyExploding = false;
//        globalHostBombClr =true;
//    }
    G8RTOS_KillSelf();
}

void ExplodeClient()
{
    //client is one
//    if(globalClientBombClr == true)
//    {
        Bomb_t * bomb = &GState.bombs[1];
        int16_t bombPositionX = bomb->positionX;
        int16_t bombPositionY = bomb->positionY;
        uint16_t x;
        uint16_t y;
//        GState.bombs[1].positionX       = -1;
//        GState.bombs[1].positionY       = -1;
        clientCurrentlyExploding = true;
        G8RTOS_WaitSemaphore(&LCD_semaphore);
        if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
        if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
        x = bombPositionX * 34 +7;
        y = bombPositionY * 34 +1;
        LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
        if(checkperm(bombPositionX+1,bombPositionY) && bombPositionX+1<=8){
            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX+1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX+1][bombPositionY] = false;
        }
        if(checkperm(bombPositionX-1,bombPositionY) && bombPositionX-1>=0){
            if(bombPositionX-1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX-1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX-1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX-1][bombPositionY] = false;
        }
        if(checkperm(bombPositionX,bombPositionY+1) && bombPositionY+1<=6){
            if(bombPositionX == GState.players[0].positionX && bombPositionY+1 == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX == GState.players[1].positionX && bombPositionY+1 == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY+1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX][bombPositionY+1] = false;
        }
        if(checkperm(bombPositionX,bombPositionY-1) && bombPositionY-1>=0){
            if(bombPositionX == GState.players[0].positionX && bombPositionY-1 == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX == GState.players[1].positionX && bombPositionY-1 == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY-1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX][bombPositionY-1] = false;
        }
        G8RTOS_SignalSemaphore(&LCD_semaphore);
        sleep(100);
        G8RTOS_WaitSemaphore(&LCD_semaphore);
        x = bombPositionX * 34 +7;
        y = bombPositionY * 34 +1;
        LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) UpdatePlayerOnScreen(0,&GState.players[0]);
        if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) UpdatePlayerOnScreen(0,&GState.players[1]);
        if(checkperm(bombPositionX+1,bombPositionY) && bombPositionX+1<=8){
            if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX+1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX-1,bombPositionY) && bombPositionX-1>=0){
            if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX-1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX,bombPositionY+1) && bombPositionY+1<=6){
            if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY+1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX,bombPositionY-1) && bombPositionY-1>=0){
            if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY-1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        G8RTOS_SignalSemaphore(&LCD_semaphore);
        clientCurrentlyExploding = false;
//        globalClientBombClr = true;
//    }
    G8RTOS_KillSelf();
}

void ExplodeHost_C(){
    //host is zero

//    if(globalHostBombClr == true){
        Bomb_t * bomb = &GState.bombs[0];
        int16_t bombPositionX = bomb->positionX;
        int16_t bombPositionY = bomb->positionY;
        int16_t x;
        int16_t y;
//        GState.bombs[0].positionX       = -1;
//        GState.bombs[0].positionY       = -1;
        hostCurrentlyExploding = true;
        G8RTOS_WaitSemaphore(&LCD_semaphore);
//        if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//        if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
        x = bombPositionX * 34 +7;
        y = bombPositionY * 34 +1;
        LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
        if(checkperm(bombPositionX+1,bombPositionY) && bombPositionX+1<=8){
            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX+1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX+1][bombPositionY] = false;
        }
        if(checkperm(bombPositionX-1,bombPositionY) && bombPositionX-1>=0){
//            if(bombPositionX-1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX-1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX-1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX-1][bombPositionY] = false;
        }
        if(checkperm(bombPositionX,bombPositionY+1) && bombPositionY+1<=6){
//            if(bombPositionX == GState.players[0].positionX && bombPositionY+1 == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX == GState.players[1].positionX && bombPositionY+1 == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY+1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX][bombPositionY+1] = false;
        }
        if(checkperm(bombPositionX,bombPositionY-1) && bombPositionY-1>=0){
//            if(bombPositionX == GState.players[0].positionX && bombPositionY-1 == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX == GState.players[1].positionX && bombPositionY-1 == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY-1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX][bombPositionY-1] = false;
        }
        G8RTOS_SignalSemaphore(&LCD_semaphore);
        sleep(100);
        G8RTOS_WaitSemaphore(&LCD_semaphore);
        x = bombPositionX * 34 +7;
        y = bombPositionY * 34 +1;
        LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) UpdatePlayerOnScreen(0,&GState.players[0]);
        if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) UpdatePlayerOnScreen(0,&GState.players[1]);
        if(checkperm(bombPositionX+1,bombPositionY) && bombPositionX+1<=8){
//            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX+1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX-1,bombPositionY) && bombPositionX-1>=0){
//            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX-1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX,bombPositionY+1) && bombPositionY+1<=6){
//            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY+1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX,bombPositionY-1) && bombPositionY-1>=0){
//            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY-1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        G8RTOS_SignalSemaphore(&LCD_semaphore);
        hostCurrentlyExploding = false;
//        globalHostBombClr =true;
//    }
        clientSpecific.hostCleared = false;
    G8RTOS_KillSelf();
}

void ExplodeClient_C()
{
    //client is one
//    if(globalClientBombClr == true)
//    {
        Bomb_t * bomb = &GState.bombs[1];
        int16_t bombPositionX = bomb->positionX;
        int16_t bombPositionY = bomb->positionY;
        uint16_t x;
        uint16_t y;
//        GState.bombs[1].positionX       = -1;
//        GState.bombs[1].positionY       = -1;
        clientCurrentlyExploding = true;
        G8RTOS_WaitSemaphore(&LCD_semaphore);
//        if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//        if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
        x = bombPositionX * 34 +7;
        y = bombPositionY * 34 +1;
        LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
        if(checkperm(bombPositionX+1,bombPositionY) && bombPositionX+1<=8){
            if(bombPositionX+1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
            if(bombPositionX+1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX+1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX+1][bombPositionY] = false;
        }
        if(checkperm(bombPositionX-1,bombPositionY) && bombPositionX-1>=0){
//            if(bombPositionX-1 == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX-1 == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX-1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX-1][bombPositionY] = false;
        }
        if(checkperm(bombPositionX,bombPositionY+1) && bombPositionY+1<=6){
//            if(bombPositionX == GState.players[0].positionX && bombPositionY+1 == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX == GState.players[1].positionX && bombPositionY+1 == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY+1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX][bombPositionY+1] = false;
        }
        if(checkperm(bombPositionX,bombPositionY-1) && bombPositionY-1>=0){
//            if(bombPositionX == GState.players[0].positionX && bombPositionY-1 == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX == GState.players[1].positionX && bombPositionY-1 == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY-1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0xfc00);
            bricks[bombPositionX][bombPositionY-1] = false;
        }
        G8RTOS_SignalSemaphore(&LCD_semaphore);
        sleep(100);
        G8RTOS_WaitSemaphore(&LCD_semaphore);
        x = bombPositionX * 34 +7;
        y = bombPositionY * 34 +1;
        LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) UpdatePlayerOnScreen(0,&GState.players[0]);
        if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) UpdatePlayerOnScreen(0,&GState.players[1]);
        if(checkperm(bombPositionX+1,bombPositionY) && bombPositionX+1<=8){
//            if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX+1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX-1,bombPositionY) && bombPositionX-1>=0){
//            if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX-1) * 34 +7;
            y = (bombPositionY) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX,bombPositionY+1) && bombPositionY+1<=6){
//            if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY+1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        if(checkperm(bombPositionX,bombPositionY-1) && bombPositionY-1>=0){
//            if(bombPositionX == GState.players[0].positionX && bombPositionY == GState.players[0].positionY) GState.hostDead = true;
//            if(bombPositionX == GState.players[1].positionX && bombPositionY == GState.players[1].positionY) GState.clientDead = true;
            x = (bombPositionX) * 34 +7;
            y = (bombPositionY-1) * 34 +1;
            LCD_DrawRectangle(x, x+34, y, y+34, 0x84ca);
        }
        G8RTOS_SignalSemaphore(&LCD_semaphore);
        clientCurrentlyExploding = false;
//        globalClientBombClr = true;
//    }
        clientSpecific.clientCleared = false;

    G8RTOS_KillSelf();
}



bool checkperm(int x, int y){
    //if grid position is perm it returns false
    if(x%2 && y%2){
        return false;
    }
    return true;
}
