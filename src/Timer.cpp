#include "Timer.h"
#include "TRACE.h"
#include <assert.h>
#include <iostream>

//Enable to do accuracy check
#define _DEBUG_TIMER
//percent error ceiling on all timers
//a warning message will be displayed if the relative
//error exceeds this amount
#define MAX_TIMER_ACCURACY 0.25 

const unsigned long CTimer::MAX_TIMER_INTERVAL=std::numeric_limits<unsigned long int>::max()/2-1;

CTimer::CTimer(void):m_TimerList(MAX_TIMER_COUNT)
{
    m_bStopTimerSvc=false;
    m_bThreadExited=true;
    pthread_mutex_init(&m_TimerListMutex,NULL);
    if(pthread_create(&m_timerServiceThreadId,NULL,CTimer_Thread_Helper,this)){
        throw "Could not start thread";
    }
}

CTimer::~CTimer(void)
{
    unsigned counter=0;
    m_bStopTimerSvc=true;
    m_ActiveTimerQueue.Signal();

    while(m_bThreadExited == false && counter < 30){
        usleep(10*1000);
        counter++;
    }
    if(m_bThreadExited == false) {
        PERROR("Service thread did not exit. Cancelling thread.\n");
        pthread_cancel(m_timerServiceThreadId);
    }
    pthread_mutex_lock(&m_TimerListMutex);
    pthread_mutex_destroy(&m_TimerListMutex);
}

/**
* Gets the current time based on the operating system
*/
void CTimer::getTime(struct timespec &time){
    //////////////
    //win32
#ifdef WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER count;
    double fResults, int_part, float_part;

    QueryPerformanceFrequency( &frequency );
    QueryPerformanceCounter(&count);

    fResults = (double)count.QuadPart / (double)frequency.QuadPart;
    float_part = modf(fResults, &int_part);

    time.tv_sec = (long)(int_part);
    time.tv_nsec = (long)(float_part * BILLION);


    /////////////
    //Solaris
#elif defined(__sun)
    hrtime_t now = gethrtime();

    time.tv_nsec = (long) (now % BILLION);
    time.tv_sec  = (long) (now / BILLION);
#elif defined(__linux__)
#   include <time.h>
    clock_gettime(CLOCK_REALTIME,&time);
#else
#   error("Unsupported OS");
#endif
}

/**
 * Converts a timespec to a miliseconds
 * @param a timespec to convert
 * @return value of timespec in miliseconds
 */
unsigned long  CTimer::timespec2ms(const struct timespec &a){
    return (a.tv_sec*1000+a.tv_nsec/MILLION);
}

/**
* Compares two time specs
* @param a compare argument
* @param b compare argument
* @retval  0  if a == b
* @retval  1  if a  > b
* @retval -1  if a  < b
*/
int CTimer::comp_timespec(struct timespec &a, struct timespec &b) {
    if ( (a.tv_sec > b.tv_sec) ||
        (a.tv_sec == b.tv_sec && a.tv_nsec > b.tv_nsec)) {
            return 1;
    } else if (a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec) {
        return 0;
    }

    return -1;
}
/**
* returns the difference between a and b
* @param a first argument
* @param b second argument
* @return The difference between a and b
*/
struct timespec CTimer::diff_timespec(const struct timespec &a, const struct timespec &b) {
    struct timespec r;

    if (a.tv_nsec < b.tv_nsec) {
        r.tv_nsec = BILLION + a.tv_nsec - b.tv_nsec;
        r.tv_sec  = a.tv_sec - 1 - b.tv_sec;
    } else {
        r.tv_nsec = a.tv_nsec - b.tv_nsec;
        r.tv_sec  = a.tv_sec - b.tv_sec;
    }

    return r;
}

/**
* returns the sum of a and b
* @param a first argument
* @param b second argument
* @return The sum of a and b
*/
struct timespec CTimer::add_timespec(const struct timespec &a, const struct timespec &b) {
    struct timespec r;

    r.tv_sec = a.tv_sec + b.tv_sec + (a.tv_nsec + b.tv_nsec) / BILLION;
    r.tv_nsec = (a.tv_nsec + b.tv_nsec) % BILLION;
    return r;
}

/**
* Suspends a timer
*  hTimer timer handle to suspend
* bTriggerSvcRoutine Wet to true if the service function should be 
*                          triggered after stopping the timer 
*/
void CTimer::StopTimer(unsigned hTimer,bool bTriggerSvcRoutine){
    unsigned uTimer=(unsigned)hTimer;
    if(!m_TimerList.IsGoodHandle(uTimer)){
        assert(0);
        PERROR("StopTimer received bad handle\n");
        return;
    }
    else{
        pthread_mutex_lock(&m_TimerListMutex);
        if (m_TimerList[uTimer].TimerState != TimerSuspended) {
            
            m_TimerList[uTimer].TimerState=TimerSuspended;
            //mark for deletion
            if (m_TimerList[uTimer].pHeapContainer != NULL) {
                *m_TimerList[uTimer].pHeapContainer=NULL;
            }
            m_TimerList[uTimer].pHeapContainer=NULL;
            //if we need to trigger the service routine
            if(bTriggerSvcRoutine){
                (m_TimerList[uTimer].pTimerServiceFunc)(m_TimerList[uTimer].hTimer,m_TimerList[uTimer].pUser);
            }          
            
        }
        pthread_mutex_unlock(&m_TimerListMutex);
    }
}

/**
* Restarts a timer 
* @param hTimer timer handle
* @param bTriggerSvcRoutine Set to true if the service function is to be called
*                        after resetting the timer
**/
void CTimer::RestartTimer(unsigned hTimer,bool bTriggerSvcRoutine){
    unsigned uTimer=(unsigned)hTimer;
    struct TIMER_INFO_STRUCT   **pHeapContainer;

    if(!m_TimerList.IsGoodHandle(uTimer)){
        assert(0);
        PERROR("RestartTimer received bad handle\n");
        return;
    }
    else{
        pthread_mutex_lock(&m_TimerListMutex);

        //mark this timer to be removed from the active queue
        //If timer is not stopped
        if (m_TimerList[uTimer].TimerState != TimerSuspended &&
            m_TimerList[uTimer].pHeapContainer != NULL) {
                *m_TimerList[uTimer].pHeapContainer=NULL;
        }
        struct timespec currentTime;
        getTime(currentTime);
        m_TimerList[uTimer].ExpiredTime=add_timespec(currentTime,m_TimerList[uTimer].interval);        

        m_TimerList[uTimer].TimerState=TimerActive;
        //add it to the active queue
        //set up self referencing queue container
        m_TimerList[uTimer].pHeapContainer=new TimerInfo_t*;
        *m_TimerList[uTimer].pHeapContainer=&m_TimerList[uTimer];
        pHeapContainer=m_TimerList[uTimer].pHeapContainer;

        //if we need to trigger the service routine
        if(bTriggerSvcRoutine){
            (m_TimerList[uTimer].pTimerServiceFunc)(m_TimerList[uTimer].hTimer,m_TimerList[uTimer].pUser);   
        }
        pthread_mutex_unlock(&m_TimerListMutex);

        m_ActiveTimerQueue.LockMutex();
        m_ActiveTimerQueue.m_Object.push(pHeapContainer);
        m_ActiveTimerQueue.UnlockMutex();
        m_ActiveTimerQueue.Signal();
    }
}

/**
* Deletes a timer
* @param hTimer timer handle
*/
void CTimer::DeleteTimer(unsigned hTimer){
    unsigned uTimer=(unsigned)hTimer;
    if(!m_TimerList.IsGoodHandle(uTimer)){
        assert(0);
        PERROR("DeleteTimer received bad handle\n");
        return;
    }
    else{
         pthread_mutex_lock(&m_TimerListMutex);
        if (m_TimerList[uTimer].pHeapContainer != NULL) {
            *m_TimerList[uTimer].pHeapContainer=NULL;
        }
        m_TimerList.FreeHandle(uTimer);
        pthread_mutex_unlock(&m_TimerListMutex);
    }
}

/** 
* Returns true if the timer is active
* @param hTimer timer handle
* @retval true  if the timer is active
* @retval flase  if the timer is stopped
**/
bool CTimer::IsTimerActive(unsigned hTimer){
    bool bResults=false;
    unsigned uTimer=(unsigned)hTimer;

    if(!m_TimerList.IsGoodHandle(uTimer)){
        assert(0);
        PERROR("IsTimerActive received bad handle\n");
        return false;
    }
    else{
        pthread_mutex_lock(&m_TimerListMutex);
        bResults = (m_TimerList[uTimer].TimerState == TimerActive);
        pthread_mutex_unlock(&m_TimerListMutex);
    }

    return bResults;
}


/***
* Creates a timer
* @param uIntervalMs Timer inverval in mS
* @pTimerFunc  Pointer to service function
* @InitialState Initial state of the timer
* @bAutoReset If true the timer will automatically reset after 
*             a call to handler. If false, the counter is one shot 
* @return timer handle.
* @retval INVALID_TIMER_HANDLE if a timer cannot be created
**/
unsigned CTimer::CreateTimer(unsigned long uIntervalMs,pTimer_Callback_t pTimerFunc,void *pUser,State_t InitialState,bool bAutoReset){
    unsigned int uTimer=0;

    assert(pTimerFunc != NULL);
    //check the timer value
    if(uIntervalMs > MAX_TIMER_INTERVAL){
        assert(!"Interval Too Long");
        return INVALID_HANDLE; 
    }

    pthread_mutex_lock(&m_TimerListMutex);

    uTimer=m_TimerList.GetNewHandle();

    //if no timers are available
    if(uTimer == (unsigned)-1){
        uTimer=-1;
        PERROR("No Timers available\n");
        assert(0);
    }
    else{
         struct timespec currentTime;
        getTime(currentTime);
        
        m_TimerList[uTimer].pTimerServiceFunc=pTimerFunc;
        m_TimerList[uTimer].TimerState=InitialState;
        m_TimerList[uTimer].interval.tv_sec=uIntervalMs/1000;        
        m_TimerList[uTimer].interval.tv_nsec=(uIntervalMs%1000)*MILLION;        
        m_TimerList[uTimer].ExpiredTime=add_timespec(currentTime,m_TimerList[uTimer].interval);        
        m_TimerList[uTimer].bAutoReset=bAutoReset;
        m_TimerList[uTimer].pUser=pUser;
        m_TimerList[uTimer].hTimer=uTimer;
        //only active timers need the circular reference
        if(InitialState == TimerActive){
            //set up a circular reference to the heap pointer
            m_TimerList[uTimer].pHeapContainer=new TimerInfo_t*;
            //printf("Created heap container %p\n",m_TimerList[uTimer].pHeapContainer);
            *m_TimerList[uTimer].pHeapContainer=&m_TimerList[uTimer];
        }
        else{
            m_TimerList[uTimer].pHeapContainer=NULL;
        }
    }
    pthread_mutex_unlock(&m_TimerListMutex);
    
    //if this timer is active add it to the queue
    if(InitialState == TimerActive){
        //add it to the active queue      
        m_ActiveTimerQueue.LockMutex();
        m_ActiveTimerQueue.m_Object.push(m_TimerList[uTimer].pHeapContainer);
        m_ActiveTimerQueue.UnlockMutex();
        m_ActiveTimerQueue.Signal();
    }

    return uTimer;
}

/**
* This function is runs inside a thread 
* to service the timers
* @return not used
*/
void* CTimer::timerServiceFunc(){
    TimerInfo_t  *pTimerInfo,**ppTimerInfo;
    struct timespec    now;
    bool   bQueueEmpty=false;

    PTRACE("Timer Handler Thread Started\n");
    m_bThreadExited=false;
    //lock the timer list
    m_ActiveTimerQueue.LockMutex();
    while(!m_bStopTimerSvc){        
        //test the queue
        bQueueEmpty=m_ActiveTimerQueue.m_Object.empty();
        m_ActiveTimerQueue.UnlockMutex();

        //check for empty queue
        if(bQueueEmpty){
            PTRACE("Timer Queue is empty\n");
            //wait for wake signal
            m_ActiveTimerQueue.LockMutex();
            m_ActiveTimerQueue.WaitOnObject();
            //if we are supposed to exit, get out
            if(m_bStopTimerSvc){
                m_ActiveTimerQueue.UnlockMutex();
                m_bThreadExited=true;
                PTRACE("Timer Handler Thread exited inside processing loop\n");
                return 0;
            }
        }
        else{
            //lock the timer list
            m_ActiveTimerQueue.LockMutex();
        }
        //get the first item off the queue
        ppTimerInfo=m_ActiveTimerQueue.m_Object.top();
        pTimerInfo=*ppTimerInfo;
        //get current time
        getTime(now);

        if(pTimerInfo == NULL){
            ///////////////////////
            //marked to be deleted
            //////////////////////
            m_ActiveTimerQueue.m_Object.pop();
            //free up the memory for it
            delete ppTimerInfo;
            m_ActiveTimerQueue.UnlockMutex();
        }
        //else if(pTimerInfo->ExpiredTime<=Now){ 
        else if(comp_timespec(pTimerInfo->ExpiredTime,now) != 1){ 
            //expired timer
            m_ActiveTimerQueue.m_Object.pop();
            //at this point we should always have an active timer
            assert(pTimerInfo->TimerState == TimerActive);
            //In Debug mode check % error
#ifdef _DEBUG_TIMER            
            struct timespec diff=diff_timespec(now,pTimerInfo->ExpiredTime);
            unsigned long dwError=diff.tv_nsec/MILLION+diff.tv_sec*1000;   
            unsigned long intervalMs=pTimerInfo->interval.tv_nsec/MILLION+pTimerInfo->interval.tv_sec*1000;   

            if(diff.tv_sec >=0 && dwError > (unsigned long)((1+MAX_TIMER_ACCURACY)*intervalMs)){
                PTRACE2(
                    "Timer[%d] accuracy compermised!! %%Error=%3.2f\n",
                    pTimerInfo->hTimer,
                    ((float)(dwError)/intervalMs-1)*100);
            }
#endif
            //if the timer is auto reset
            if(pTimerInfo->bAutoReset){
                struct timespec currentTime;
                getTime(currentTime);
                pTimerInfo->ExpiredTime=add_timespec(currentTime,pTimerInfo->interval);
                //put the timer back on the queue
                m_ActiveTimerQueue.m_Object.push(ppTimerInfo);
            }
            else{
                //one shot timers are suspended automatically after they are 
                //serviced
                pTimerInfo->TimerState=TimerSuspended; 
            }
            ////////////////////
            // Call Back
            ////////////////////
            assert(pTimerInfo->pTimerServiceFunc !=NULL);

            m_ActiveTimerQueue.UnlockMutex();
            (pTimerInfo->pTimerServiceFunc)(pTimerInfo->hTimer,pTimerInfo->pUser);
        }
        else{
            //////////////////////////
            // No more expired timers
            //////////////////////////
            struct timespec diff=diff_timespec(pTimerInfo->ExpiredTime,now);
            unsigned int uSleepTime=diff.tv_nsec/1000+diff.tv_sec*MILLION; //in uS    
            //compute wake time
#ifdef _DEBUG_TIMER
            //PTRACE1("Resetting alarm for %u uS\n",uSleepTime);
#endif                   
            m_ActiveTimerQueue.WaitOnObject(uSleepTime);
        }
    }
    m_bThreadExited=true;
    PTRACE("Timer Handler Exited Started\n");
    return 0;
}


/**
*
* Dumps a list of valid timers
**/
void CTimer::DumpValidTimers(){
    int i;

    PTRACE("-----------------------------------------\n");
    //PTRACE1("Now: %ld\n",GetCurrentTime());
    //make a copy of the main timer list
    pthread_mutex_lock(&m_TimerListMutex);
    for(i=0;i<MAX_TIMER_COUNT;i++){
        if(m_TimerList.IsGoodHandle(i)){
            PTRACE3("----\nTimer Id: %8d\tParam: %8p\tAutoReset: %d\n",m_TimerList[i].hTimer,
                m_TimerList[i].pUser,
                m_TimerList[i].bAutoReset);    
            PTRACE1("Func Ptr: %8p\t",m_TimerList[i].pTimerServiceFunc);
            PTRACE2("Intvl: %8ld.%09ld\t",m_TimerList[i].interval.tv_sec,m_TimerList[i].interval.tv_nsec);
            PTRACE2("Exp Time : %8ld.%09ld\t",m_TimerList[i].ExpiredTime.tv_sec,m_TimerList[i].ExpiredTime.tv_nsec);
            PTRACE1("State: %s\n",m_TimerList[i].TimerState == TimerActive ? "Active":"Suspended");
        }
    }
    pthread_mutex_unlock(&m_TimerListMutex);
    PTRACE("-----------------------------------------\n");

}
/**
* Dumps all active timers in the queue
**/
void CTimer::DumpTimersQueue(){
    size_t i,nElements=m_ActiveTimerQueue.m_Object.size();
    TimerInfo_t **pTimerInfoList[MAX_TIMER_COUNT];
    struct timespec now;

    getTime(now);
    //make a local copy of the timers
    m_ActiveTimerQueue.LockMutex();
    for(i=0;i<nElements;i++){
        pTimerInfoList[i]=m_ActiveTimerQueue.m_Object.top();
        m_ActiveTimerQueue.m_Object.pop();      
    }
    //put the timers back on the list
    for(i=0;i<nElements;i++){
        m_ActiveTimerQueue.m_Object.push(pTimerInfoList[i]);      
    }
    m_ActiveTimerQueue.UnlockMutex();

    PTRACE("-----------------------------------------\n");    
    PTRACE2("Now: %ld.%09ld\n",now.tv_sec,now.tv_nsec);
    for(i=0;i<nElements;i++){

        if(pTimerInfoList[i]!=NULL){           
            PTRACE3("----\nTimer Id: %8d\tParam: %8p\tAutoReset: %d\n",(*pTimerInfoList[i])->hTimer,
                (*pTimerInfoList[i])->pUser,
                (*pTimerInfoList[i])->bAutoReset);    
            PTRACE1("Func Ptr: %8p\t",(*pTimerInfoList[i])->pTimerServiceFunc);
            PTRACE2("Intvl: %8ld.%09ld\t",(*pTimerInfoList[i])->interval.tv_sec,(*pTimerInfoList[i])->interval.tv_nsec);
            PTRACE2("Exp Time : %8ld.%09ld\t",(*pTimerInfoList[i])->ExpiredTime.tv_sec,(*pTimerInfoList[i])->ExpiredTime.tv_nsec);
            PTRACE1("State: %s\n",(*pTimerInfoList[i])->TimerState == TimerActive ? "Active":"Suspended");
        }
        else{
            PTRACE("----\nTimer Id: xxxxxxxx\tParam: xxxxxxxx\tAutoReset: x\n");
            PTRACE("Func Ptr: xxxxxxxx\tIntvl: xxxxxxxx\tExp Time : xxxxxxxx\tState: xxxxxxx\n");
        }

    }
    PTRACE("-----------------------------------------\n");
}


void* CTimer_Thread_Helper(void *pUser){
    CTimer *pTimer=(CTimer *)pUser;

    return pTimer->timerServiceFunc();
}

/**
* Used by the STL priority queue to sort items
* @param x  first argument
* @param y  second argument
* @retval true y should be placed before x
* @retval false y should NOT be placed before x
**/
bool CTimer::CompareTimerInfo::operator()(TimerInfo_t** x, TimerInfo_t** y){

    if(*x == NULL && *y!=NULL)
        return false;
    else if (*x != NULL && *y==NULL)
        return true;
    else if (*x == NULL && *y==NULL)
        return true;
    //else if((*y)->ExpiredTime < (*x)->ExpiredTime )
    else if(comp_timespec((*y)->ExpiredTime,(*x)->ExpiredTime) == -1)
        return true;    
    else
        return false;          
}
