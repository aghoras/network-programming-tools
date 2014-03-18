#ifndef _WATCH_DOG_TIMER_H
#define _WATCH_DOG_TIMER_H

#include <queue>
#include <map>
#include <vector>
#include <pthread.h>
#include <math.h>
#include "WaitableObject.h"
#include "Lists.h"
#include <limits>
#ifdef WIN32
#   include <windows.h>
#else
# include <sys/time.h>
#endif

#ifndef BILLION
# define BILLION 1000000000
#endif

#ifndef MILLION
# define MILLION 1000000
#endif

/** 
* This class defines a series of functions to 
* create timers that can trigger events
*/
class CTimer
{
public:
    typedef void (*pTimer_Callback_t)(unsigned hTimer,void *hParam);
    typedef enum {TimerActive,TimerSuspended} State_t;

    CTimer(void);
    ~CTimer(void);
    /** Handle for invalid timers */
    enum {INVALID_HANDLE=0xFFFFFFFF};
    /** Maximum number of timer this class can handle */
    enum {MAX_TIMER_COUNT=100};
    static const unsigned long MAX_TIMER_INTERVAL;

    void StopTimer(unsigned hTimer,bool bTriggerSvcRoutine=false);
	void RestartTimer(unsigned hTimer,bool bTriggerSvcRoutine=false);
	void DeleteTimer(unsigned hTimer);
    bool IsTimerActive(unsigned hTimer);
    void DumpValidTimers();
    void DumpTimersQueue();
	unsigned CreateTimer(unsigned long uIntervalMs,pTimer_Callback_t pTimerFunc,void *pUser,State_t InitialState=TimerActive,bool bAutoReset=true);

    static void getTime(struct timespec &time );
    static int comp_timespec(struct timespec &a, struct timespec &b);
    static struct timespec add_timespec(const struct timespec &a, const struct timespec &b);
    static struct timespec diff_timespec(const struct timespec &a, const struct timespec &b);
    static unsigned long timespec2ms(const struct timespec &a);

protected:
    /** timer information */
    typedef struct TIMER_INFO_STRUCT{
        timespec                   ExpiredTime;
        timespec                   interval;
        State_t                    TimerState;
        pTimer_Callback_t          pTimerServiceFunc;
        bool                       bAutoReset;
        void*                      pUser;
        unsigned                   hTimer;
        struct TIMER_INFO_STRUCT   **pHeapContainer;
    }TimerInfo_t;

    //queue stl compare class
    class CompareTimerInfo{
    public:
        bool operator()(TimerInfo_t** x,TimerInfo_t** y);
    };
    //list of pending active timers
    typedef std::priority_queue<TimerInfo_t**,std::vector<TimerInfo_t**>,CompareTimerInfo> TimerQueue;
    typedef std::map<unsigned,int*> SUSPEND_COUNT_MAP;

    pthread_t m_timerServiceThreadId;
    bool m_bStopTimerSvc; //set to true to signal the service thread to exist
    bool m_bThreadExited; //set to true when the service thread exists
    pthread_mutex_t m_TimerListMutex;
    //TimerQueue m_ActiveTimerQueue;
    CWaitableObject<TimerQueue> m_ActiveTimerQueue;
    //a safe list of the all the timers
    Handle_List<TimerInfo_t>  m_TimerList;

    void* timerServiceFunc();

    friend void* CTimer_Thread_Helper(void *pUser);
};

void* CTimer_Thread_Helper(void *pUser);

#endif //_WATCH_DOG_TIMER_H

