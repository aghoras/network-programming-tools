/**
 * @file Timer_test.cpp
 *
 * @date   Mar 17, 2014
 * @author ali
 */


#include "Timer.h"
#include "TRACE.h"
#include "gtest.h"
#include <list>

#define mSleep(x) (usleep(x*1000))


class CTimeList{
protected:
    std::list<timespec> m_TimeHacks;
    pthread_mutex_t m_mutex;

public:
    CTimeList(){
        pthread_mutex_init(&m_mutex,NULL);
    }

    ~CTimeList(){
         pthread_mutex_destroy(&m_mutex);
     }

    void push_back(timespec t){
        pthread_mutex_lock(&m_mutex);
        m_TimeHacks.push_back(t);
        pthread_mutex_unlock(&m_mutex);
    }

    void push_front(timespec t){
        pthread_mutex_lock(&m_mutex);
        m_TimeHacks.push_front(t);
        pthread_mutex_unlock(&m_mutex);
    }

    unsigned size() {
        int results;
        pthread_mutex_lock(&m_mutex);
        results = m_TimeHacks.size();
        pthread_mutex_unlock(&m_mutex);
        return results;
    }

    unsigned empty() {
        bool bResults;;
        pthread_mutex_lock(&m_mutex);
        bResults = m_TimeHacks.empty();
        pthread_mutex_unlock(&m_mutex);
        return bResults;
    }

    std::list<timespec> get_list() {
        pthread_mutex_lock(&m_mutex);
        std::list<timespec> results(m_TimeHacks);
        pthread_mutex_unlock(&m_mutex);
        return results;
    }

};

/**
 * Basic Time Spec operaions
 */
TEST(Timer,diff_timespec){
    timespec a,b,c;

    //zero test
    a.tv_sec=0;
    a.tv_nsec=0;
    b.tv_sec=0;
    b.tv_nsec=0;
    c=CTimer::diff_timespec(a,b);
    EXPECT_EQ(c.tv_sec,0);
    EXPECT_EQ(c.tv_nsec,0);

    //ns diff test
    a.tv_sec=0;
    a.tv_nsec=BILLION-1;
    b.tv_sec=0;
    b.tv_nsec=BILLION-1;
    c=CTimer::diff_timespec(a,b);
    EXPECT_EQ(c.tv_sec,0);
    EXPECT_EQ(c.tv_nsec,0);

    a.tv_sec=1;
    a.tv_nsec=0;
    b.tv_sec=0;
    b.tv_nsec=BILLION-1;
    c=CTimer::diff_timespec(a,b);
    EXPECT_EQ(c.tv_sec,0);
    EXPECT_EQ(c.tv_nsec,1);

    a.tv_sec=1;
    a.tv_nsec=0;
    b.tv_sec=1;
    b.tv_nsec=0;
    c=CTimer::diff_timespec(a,b);
    EXPECT_EQ(c.tv_sec,0);
    EXPECT_EQ(c.tv_nsec,0);

    a.tv_sec=2;
    a.tv_nsec=0;
    b.tv_sec=1;
    b.tv_nsec=0;
    c=CTimer::diff_timespec(a,b);
    EXPECT_EQ(c.tv_sec,1);
    EXPECT_EQ(c.tv_nsec,0);

    a.tv_sec=1;
    a.tv_nsec=0;
    b.tv_sec=0;
    b.tv_nsec=1;
    c=CTimer::diff_timespec(a,b);
    EXPECT_EQ(c.tv_sec,0);
    EXPECT_EQ(c.tv_nsec,BILLION-1);

    a.tv_sec=1;
    a.tv_nsec=400000000;
    b.tv_sec=0;
    b.tv_nsec=900000000;
    c=CTimer::diff_timespec(a,b);
    EXPECT_EQ(c.tv_sec,0);
    EXPECT_EQ(c.tv_nsec,500000000);
}

void basicTimerCallback(unsigned id,void *pUser){
    CTimeList *pTimeList=( CTimeList *) pUser;
    timespec now;
    CTimer::getTime(now);

    pTimeList->push_back(now);
}

/**
 * Single Timer Tests
 */
TEST(Timer,singleTimer){
    CTimer timer;
    timespec start_time,delta;
    unsigned int timer1;
    CTimeList  timeList; // a list of times that the timer was invoked
    const unsigned testIntervalMs=10;  // how far apart should the tests be scheduled
    const unsigned numTestEnvents=200;
    const unsigned MaxAllowedError=2; //number of allowed timer error
    std::list<timespec>::iterator it,last_it;
    std::list<timespec> tempTimeList;

    CTimer::getTime(start_time);    ;
    timer1=timer.CreateTimer(testIntervalMs,basicTimerCallback,&timeList);
    ASSERT_NE(timer1,CTimer::INVALID_HANDLE);

    mSleep(testIntervalMs*(numTestEnvents+2));

    timer.StopTimer(timer1);
    //did we get the write number of events
    EXPECT_GE(timeList.size(),numTestEnvents);
    //make sure the list has something in it
    EXPECT_FALSE(timeList.empty());

    timeList.push_front(start_time);
    tempTimeList=timeList.get_list();
    //check the time differences
    it=tempTimeList.begin();
    do{
        last_it=it;
        it++;
        if(it!=tempTimeList.end()){
            delta=CTimer::diff_timespec(*it,*last_it);
            EXPECT_NEAR(CTimer::timespec2ms(delta),testIntervalMs,MaxAllowedError);
        }
    }while(it!=tempTimeList.end());
}


#define CHECK_TIMER(l,e)   EXPECT_NE( l.begin(),  l.end() );\
                   it=l.begin();                                                     \
                   do{                                                               \
                       last_it=it;                                                   \
                       it++;                                                         \
                       if(it!=l.end()){                                              \
                           delta=CTimer::diff_timespec(*it,*last_it);                \
                           EXPECT_NEAR(CTimer::timespec2ms(delta),e,MaxAllowedError);\
                       }                                                             \
                   }while(it!=l.end());                                              \


TEST(Timer,multipleTimers) {
    CTimer timerManager;
    timespec start_time, delta;
    const unsigned nTimers = 8;
    unsigned int timer[nTimers];
    CTimeList timeList[nTimers]; // a list of times that the timer was invoked
    const unsigned testIntervalMs[nTimers] = { 10,30,90,270,500,800,1000,2000 };  // how far apart should the tests be scheduled
    const unsigned numTestEnvents = 5; //number of events for the longest timer
    const unsigned MaxAllowedError = 2; //number of allowed timer error
    std::list<timespec>::iterator it, last_it;
    std::list<timespec> tempTimeList;

    CTimer::getTime(start_time);
    ;
    for (unsigned idxTimer = 0; idxTimer < nTimers; idxTimer++) {
        timer[idxTimer] = timerManager.CreateTimer(testIntervalMs[idxTimer], basicTimerCallback, &timeList[idxTimer]);
        ASSERT_NE(timer[idxTimer], CTimer::INVALID_HANDLE);
    }
    mSleep(testIntervalMs[nTimers - 1] * (numTestEnvents + 1));

    for (unsigned idxTimer = 0; idxTimer < nTimers; idxTimer++) {
        timerManager.StopTimer(timer[idxTimer]);

    }

    //did we get the write number of events
    EXPECT_GE(timeList[nTimers - 1].size(), numTestEnvents);
    //add the test start times and check the values
    for (unsigned idxTimer = 0; idxTimer < nTimers; idxTimer++) {
        tempTimeList=timeList[idxTimer].get_list();
        CHECK_TIMER(tempTimeList, testIntervalMs[idxTimer]);
    }

}
