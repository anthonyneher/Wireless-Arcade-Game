/*
 * G8RTOS_Scheduler.c
 */

/*********************************************** Dependencies and Externs *************************************************************/
#include "G8RTOS.h"
/*
 * G8RTOS_Start exists in asm
 */
extern void G8RTOS_Start();

/* System Core Clock From system_msp432p401r.c */
extern uint32_t SystemCoreClock;

/*
 * Pointer to the currently running Thread Control Block
 */
extern tcb_t * CurrentlyRunningThread;
extern tcb_t * HighestPriorityHead;
extern tcb_t * LowestPriorityTail;
/*********************************************** Dependencies and Externs *************************************************************/


/*********************************************** Defines ******************************************************************************/

/* Status Register with the Thumb-bit Set */
#define THUMBBIT 0x01000000

/*********************************************** Defines ******************************************************************************/


/*********************************************** Data Structures Used *****************************************************************/

/* Thread Control Blocks
 *	- An array of thread control blocks to hold pertinent information for each thread
 */
static tcb_t threadControlBlocks[MAX_THREADS];

/* Thread Stacks
 *	- An array of arrays that will act as invdividual stacks for each thread
 */
static int32_t threadStacks[MAX_THREADS][STACKSIZE];

/* Periodic Event Threads
 * - An array of periodic events to hold pertinent information for each thread
 */
static ptcb_t Pthread[MAXPTHREADS];

//static int currentMaxPriority;
uint32_t IDCounter = 1;

/*********************************************** Data Structures Used *****************************************************************/


/*********************************************** Private Variables ********************************************************************/

/*
 * Current Number of Threads currently in the scheduler
 */
static uint32_t NumberOfThreads;

/*
 * Current Number of Periodic Threads currently in the scheduler
 */
static uint32_t NumberOfPthreads;

/*********************************************** Private Variables ********************************************************************/


/*********************************************** Private Functions ********************************************************************/

/*
 * Initializes the Systick and Systick Interrupt
 * The Systick interrupt will be responsible for starting a context switch between threads
 * Param "numCycles": Number of cycles for each systick interrupt
 */
static void InitSysTick(uint32_t numCycles)
{
    SysTick_Config(numCycles);       //Configure SysTick interrupt to trigger at numCycles
}

/*
 * Chooses the next thread to run.
 * Lab 2 Scheduling Algorithm:
 * 	- Simple Round Robin: Choose the next running thread by selecting the currently running thread's next pointer
 * 	- Check for sleeping and blocked threads
 */
void G8RTOS_Scheduler()
{
    tcb_t *TempNextThread = HighestPriorityHead;

    while((TempNextThread->Blocked) || (TempNextThread->Asleep))
    {
        TempNextThread = TempNextThread->NextTCB;
    }
        CurrentlyRunningThread = TempNextThread;
}


/*
 * SysTick Handler
 * The SysTick Handler now will increment the system time,
 * set the PendSV flag to start the scheduler,
 * and be responsible for handling sleeping and periodic threads
 */
void SysTick_Handler()
{
    SystemTime++;

    ptcb_t *Pptr = &Pthread[0];
    for(int i = 0; i<NumberOfPthreads; i++)
    {
        if(Pptr->ExecuteTime == SystemTime)
        {
            Pptr->ExecuteTime = Pptr->Period + SystemTime;
            Pptr->Handler();
        }
        Pptr = Pptr->NextPTCB;
    }

    tcb_t *ptr =  HighestPriorityHead;
    if(ptr->Asleep && ptr->SleepCount <= SystemTime)
    {
        ptr->Asleep = 0;
    }
    ptr = ptr->NextTCB;
    while(ptr != HighestPriorityHead)
    {
        if(ptr->Asleep && ptr->SleepCount <= SystemTime)
        {
            ptr->Asleep = 0;
        }
        ptr = ptr->NextTCB;
    }

    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;

}

/*********************************************** Private Functions ********************************************************************/


/*********************************************** Public Variables *********************************************************************/

/* Holds the current time for the whole System */
uint32_t SystemTime;

/*********************************************** Public Variables *********************************************************************/


/*********************************************** Public Functions *********************************************************************/

/*
 * Sets variables to an initial state (system time and number of threads)
 * Enables board for highest speed clock and disables watchdog
 */
void G8RTOS_Init()
{
    BSP_InitBoard();                            //Turn on all hardware on board, set max clock speed

    // Relocate vector table to SRAM to use aperiodic events
    uint32_t newVTORTable = 0x20000000;
    memcpy((uint32_t *)newVTORTable, (uint32_t *)SCB->VTOR, 57*4);
    // 57 interrupt vectors to copy
    SCB->VTOR = newVTORTable;

    NumberOfThreads = 0;                        //Initialize NumberOfThreads to zero
    NumberOfPthreads = 0;
}

/*
 * Starts G8RTOS Scheduler
 * 	- Initializes the Systick
 * 	- Sets Context to first thread
 * Returns: Error Code for starting scheduler. This will only return if the scheduler fails
 */
int G8RTOS_Launch()
{
   CurrentlyRunningThread = HighestPriorityHead;

    __NVIC_SetPriority(PendSV_IRQn, OSINT_PRIORITY);
    __NVIC_SetPriority(SysTick_IRQn, OSINT_PRIORITY);
    InitSysTick(ClockSys_GetSysFreq()/1000);            //SystemFreq / 1000 = period necessary for 1ms
    G8RTOS_Start();

    return 0;
}

/*
 * Adds threads to G8RTOS Scheduler
 * 	- Checks if there are stil available threads to insert to scheduler
 * 	- Initializes the thread control block for the provided thread
 * 	- Initializes the stack for the provided thread to hold a "fake context"
 * 	- Sets stack tcb stack pointer to top of thread stack
 * 	- Sets up the next and previous tcb pointers in a round robin fashion
 * Param "threadToAdd": Void-Void Function to add as preemptable main thread
 * Returns: Error code for adding threads
 */
int G8RTOS_AddThread(void (*threadToAdd)(void), uint8_t Priority, char *threadName)
{
    threadId_t uniqueThreadId;

    int32_t IBit_State = StartCriticalSection(); //enter critical section

    if(NumberOfThreads == MAX_THREADS)
    {
        EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts
        return THREAD_LIMIT_REACHED;
    }

    //Calculate threadID
    if(NumberOfThreads > 0)
    {
        for(int i = 0; i<=MAX_THREADS; i++)
        {
            if(i == MAX_THREADS)
            {
                EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts
                return THREADS_INCORRECTLY_ALIVE;
            }
            else if(threadControlBlocks[i].isAlive != 1)
            {
                uniqueThreadId = ((IDCounter)) ;
                IDCounter++;
                break;
            }
        }
    }

    if(NumberOfThreads == 0){       //1st thread, nothing matters
        threadControlBlocks[NumberOfThreads].NextTCB        = &threadControlBlocks[NumberOfThreads];
        threadControlBlocks[NumberOfThreads].PreviousTCB    = &threadControlBlocks[NumberOfThreads];
        threadControlBlocks[NumberOfThreads].ThreadID       = uniqueThreadId;
        threadControlBlocks[NumberOfThreads].StackPointer   = &threadStacks[NumberOfThreads][STACKSIZE-16];
        threadControlBlocks[NumberOfThreads].Asleep         = 0;
        threadControlBlocks[NumberOfThreads].isAlive        = 1;
        threadControlBlocks[NumberOfThreads].Priority       = Priority;

        for(int i = 16; i>2; i--)
        {
            threadStacks[NumberOfThreads][STACKSIZE-i] = 0;
        }
        threadStacks[NumberOfThreads][STACKSIZE-2]          = (int32_t)threadToAdd;
        threadStacks[NumberOfThreads][STACKSIZE-1]          = THUMBBIT; //location of the function

        for(int i = 0; i<MAX_NAME_LENGTH; i++)                      //Assign thread name
        {
            threadControlBlocks[NumberOfThreads].threadName[i] = *threadName;
            threadName++;
        }
        HighestPriorityHead = &threadControlBlocks[0];
        //NumberOfThreads++;
        //EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts
        //return NO_ERROR;
    }else{
        tcb_t *index_ptr = HighestPriorityHead->NextTCB;
        for(int i = 0; i < MAX_THREADS; i++)
        {
            if(threadControlBlocks[i].isAlive != 1) //empty thread found, insert new TCB here!
            {
                if(Priority < HighestPriorityHead->Priority)
                {
                    //Newest Highest priority
                    threadControlBlocks[i].NextTCB                = HighestPriorityHead;
                    threadControlBlocks[i].PreviousTCB            = HighestPriorityHead->PreviousTCB;
                    (HighestPriorityHead->PreviousTCB)->NextTCB   = &threadControlBlocks[i];
                    (HighestPriorityHead->PreviousTCB)            = &threadControlBlocks[i];
                    HighestPriorityHead = &threadControlBlocks[i];

                }else                                           //When Priority >= HighestPriorityHead->Priority
                {
                    while( (Priority > index_ptr->Priority) && (index_ptr->NextTCB != index_ptr))
                    {
                        index_ptr = index_ptr->NextTCB;
                    }
                    threadControlBlocks[i].NextTCB      = index_ptr;
                    threadControlBlocks[i].PreviousTCB  = index_ptr->PreviousTCB;
                    (index_ptr->PreviousTCB)->NextTCB   = &threadControlBlocks[i];
                    index_ptr->PreviousTCB              = &threadControlBlocks[i];
                }
                threadControlBlocks[i].ThreadID       = uniqueThreadId;
                threadControlBlocks[i].StackPointer   = &threadStacks[i][STACKSIZE-16];
                threadControlBlocks[i].Asleep         = 0;
                threadControlBlocks[i].isAlive        = 1;
                threadControlBlocks[i].Priority       = Priority;
                for(int j = 16; j>2; j--)
                {
                    threadStacks[i][STACKSIZE-j] = 0;
                }
                threadStacks[i][STACKSIZE-2]          = (int32_t)threadToAdd;
                threadStacks[i][STACKSIZE-1]          = THUMBBIT; //location of the function

                for(int j = 0; j<MAX_NAME_LENGTH; j++)                      //Assign thread name
                {
                    threadControlBlocks[i].threadName[j] = *threadName;
                    threadName++;
                }
                break;
            }
        }
    }
    NumberOfThreads++;
    EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts
    return NO_ERROR;
}

/*
 * Adds periodic threads to G8RTOS Scheduler
 * Function will initialize a periodic event struct to represent event.
 * The struct will be added to a linked list of periodic events
 * Param Pthread To Add: void-void function for P thread handler
 * Param period: period of P thread to add
 * Returns: Error code for adding threads
 */
int G8RTOS_AddPeriodicEvent(void (*PthreadToAdd)(void), uint32_t period)
{
    if(NumberOfPthreads >= MAX_THREADS){
        return THREAD_LIMIT_REACHED;
    }else if(NumberOfPthreads == 0){
        Pthread[NumberOfPthreads].NextPTCB      = &Pthread[NumberOfPthreads];
        Pthread[NumberOfPthreads].PreviousPTCB  = &Pthread[NumberOfPthreads];
    }else{
        Pthread[NumberOfPthreads].NextPTCB      = &Pthread[0];
        Pthread[NumberOfPthreads].PreviousPTCB  = &Pthread[NumberOfPthreads-1];

        Pthread[NumberOfPthreads-1].NextPTCB    = &Pthread[NumberOfPthreads];
        Pthread[0].PreviousPTCB                 = &Pthread[NumberOfPthreads];
    }

    Pthread[NumberOfPthreads].Handler           = PthreadToAdd;
    Pthread[NumberOfPthreads].Period            = period;
    Pthread[NumberOfPthreads].CurrentTime       = 0;      //init CurrentTime to zero


    /* Determine if any of the periods shared are multiples of each other.  This is used to stagger CurrentTime
     * (initial time the task begins). */
    for(int i = 0; i < NumberOfPthreads; i++)
    {
        if(Pthread[i].Period > Pthread[NumberOfPthreads].Period)            //Period of new task is less than previously inserted task
        {
            if(Pthread[i].Period % Pthread[NumberOfPthreads].Period == 0)
            {
                Pthread[NumberOfPthreads].CurrentTime++;
            }
        }else{                                                              //Period of new task is greater than previously inserted task
            if(Pthread[NumberOfPthreads].Period % Pthread[i].Period == 0)
            {
                Pthread[NumberOfPthreads].CurrentTime++;
            }
        }
    }
    Pthread[NumberOfPthreads].ExecuteTime       = period - Pthread[NumberOfPthreads].CurrentTime;

    NumberOfPthreads++;
    return NO_ERROR;
}

/*
 * Puts the current thread into a sleep state.
 *  param durationMS: Duration of sleep time in ms
 */
void sleep(uint32_t durationMS)
{
    CurrentlyRunningThread->Asleep = 1;
    CurrentlyRunningThread->SleepCount = durationMS + SystemTime;
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;                //set PendSV pending to perform context switch
   // while((SCB->ICSR & SCB_ICSR_PENDSVSET_Msk));
}

threadId_t G8RTOS_GetThreadId()
{
    return CurrentlyRunningThread->ThreadID;
}

sched_ErrCode_t G8RTOS_KillThread(threadId_t threadId)
{
    int32_t IBit_State = StartCriticalSection(); //enter critical section
    tcb_t *threadPtr = CurrentlyRunningThread;

    if(NumberOfThreads == 1)
    {
        EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts
        return CANNOT_KILL_LAST_THREAD;
    }

    //look for thread
    for(int i = 0; i<NumberOfThreads; i++)
    {
        if(threadPtr->ThreadID == threadId)     //thread found
        {
            threadPtr->isAlive = 0;
            (threadPtr->PreviousTCB)->NextTCB = (threadPtr->NextTCB);
            (threadPtr->NextTCB)->PreviousTCB = (threadPtr->PreviousTCB);
            threadPtr->Blocked = 0;
            NumberOfThreads--;

            EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts
            if(threadPtr == CurrentlyRunningThread)
            {
                SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;     //set PendSV pending to perform context switch
                return NO_ERROR;
            }
            return NO_ERROR;
        }
        threadPtr = threadPtr->NextTCB;
    }
    EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts
    return THREAD_DOES_NOT_EXIST ;
}

sched_ErrCode_t G8RTOS_KillAllOtherThreads()
{
    int32_t IBit_State = StartCriticalSection();    //enter critical section

    if(NumberOfThreads == 1)
    {
        EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts
        return ONE_THREAD_LEFT;
    }

    tcb_t *threadPtr = CurrentlyRunningThread->NextTCB;
    while(threadPtr != CurrentlyRunningThread)
    {
        threadPtr->isAlive = 0;
        threadPtr = threadPtr->NextTCB;
    }

    CurrentlyRunningThread->NextTCB     = CurrentlyRunningThread;
    CurrentlyRunningThread->PreviousTCB = CurrentlyRunningThread;
    NumberOfThreads = 1;

    EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts
    return NO_ERROR ;
}

sched_ErrCode_t G8RTOS_KillSelf()
{
    int32_t IBit_State = StartCriticalSection();    //enter critical section

    if(NumberOfThreads == 1)
    {
        EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts
        return CANNOT_KILL_LAST_THREAD;
    }

    if(CurrentlyRunningThread == HighestPriorityHead)
    {
        HighestPriorityHead = HighestPriorityHead->NextTCB;
    }

    CurrentlyRunningThread->isAlive = 0;
    (CurrentlyRunningThread->PreviousTCB)->NextTCB = (CurrentlyRunningThread->NextTCB);
    (CurrentlyRunningThread->NextTCB)->PreviousTCB = (CurrentlyRunningThread->PreviousTCB);

    NumberOfThreads--;

    EndCriticalSection(IBit_State);          //exit critical section, re-enabling interrupts

    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;     //set PendSV pending to perform context switch

    return NO_ERROR;
}

sched_ErrCode_t G8RTOS_AddAPeriodicEvent(void(*AthreadToAdd)(void), uint8_t priority, IRQn_Type IRQn)
{
    if( (IRQn < PSS_IRQn) || (IRQn > PORT6_IRQn) )
    {
       return IRQn_INVALID;
    }
    if(priority > 6)
    {
        return HWI_PRIORITY_INVALID;
    }
    __NVIC_SetVector(IRQn, (uint32_t)AthreadToAdd);
    __NVIC_SetPriority(IRQn, priority);
    __NVIC_EnableIRQ(IRQn);

    return NO_ERROR;
}

/*********************************************** Public Functions *********************************************************************/
