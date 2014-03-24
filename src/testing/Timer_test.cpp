/**
 * @file Timer_test.cpp
 *
 * This file defines the unit tests for verifying the operation
 * of the CTimer class
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

    void clear() {
        pthread_mutex_lock(&m_mutex);
        m_TimeHacks.clear();
        pthread_mutex_unlock(&m_mutex);
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

static void basicTimerCallback(unsigned id,void *pUser){
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

/**
 * Test the operation of multiple timers at the same time
 *
 */
TEST(Timer,multipleTimers) {
    CTimer timerManager;
    timespec start_time, delta;
    const unsigned nTimers = 8;
    unsigned int timer[nTimers];
    CTimeList timeList[nTimers]; // a list of times that the timer was invoked
    const unsigned testIntervalMs[nTimers] = { 10,30,90,270,500,800,1000,2000 };  // how far apart should the tests be scheduled
    const unsigned numTestEnvents = 5; //number of events for the longest timer
    const unsigned MaxAllowedError = 3; //number of allowed timer error in mS
    std::list<timespec>::iterator it, last_it;
    std::list<timespec> tempTimeList;

    CTimer::getTime(start_time);
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

/**
 * Test the operation of creation timers after
 * a long timer is running
 *
 */
TEST(Timer,lateCreation) {
    const unsigned nTimers = 2;
    CTimer timerManager;
    timespec start_time[nTimers], delta;
    unsigned safety_counter;
    unsigned max_saftey_counter=10000;

    unsigned int timer[nTimers];
    CTimeList timeList[nTimers]; // a list of times that the timer was invoked
    const unsigned testIntervalMs[nTimers] = { 2000,100 };  // how far apart should the tests be scheduled
    const unsigned MaxAllowedError = 1; //number of allowed timer error
    std::list<timespec> tempTimeList;

    CTimer::getTime(start_time[0]);
    timer[0]=timerManager.CreateTimer(testIntervalMs[0],basicTimerCallback,&timeList[0]);
    mSleep(200); //wait a little
    //there should be no pending events
    EXPECT_TRUE(timeList[0].empty());
    EXPECT_TRUE(timeList[1].empty());

    CTimer::getTime(start_time[1]);
    timer[1]=timerManager.CreateTimer(testIntervalMs[1],basicTimerCallback,&timeList[1]);
    //still, there should be no events
    EXPECT_TRUE(timeList[0].empty());
    EXPECT_TRUE(timeList[1].empty());

    //wait for the short timer to trip
    safety_counter=0;
    while (timeList[1].empty() && safety_counter++ < max_saftey_counter) {
        mSleep(1);
    }
    //the slow timer should not have tripped
    EXPECT_TRUE(timeList[0].empty());
    //the fast timer should have tripped only once
    EXPECT_FALSE(timeList[1].empty());
    EXPECT_EQ(timeList[1].size(),(unsigned)1);
    tempTimeList = timeList[1].get_list();
    delta = CTimer::diff_timespec(tempTimeList.front(),start_time[1]);
    EXPECT_NEAR(CTimer::timespec2ms(delta),testIntervalMs[1],MaxAllowedError);
    EXPECT_TRUE(timerManager.IsTimerActive(timer[1]));
    timerManager.StopTimer(timer[1]);
    EXPECT_FALSE(timerManager.IsTimerActive(timer[1]));

    //wait for the long timer to trip
    safety_counter=0;
    while (timeList[0].empty() && safety_counter++ < max_saftey_counter) {
        mSleep(1);
    }
    EXPECT_TRUE(timerManager.IsTimerActive(timer[0]));
    //both lists should still only have one element
    EXPECT_FALSE(timeList[0].empty());
    EXPECT_EQ(timeList[0].size(),(unsigned)1);
    EXPECT_EQ(timeList[1].size(),(unsigned)1);

    tempTimeList = timeList[0].get_list();
    delta = CTimer::diff_timespec(tempTimeList.front(),start_time[0]);
    EXPECT_NEAR(CTimer::timespec2ms(delta),testIntervalMs[0],MaxAllowedError);

}


/**
 * Test the operation of one shot timers
 *
 */
TEST(Timer,oneShot) {
    const unsigned nTimers = 2;
    CTimer timerManager;
    timespec start_time[nTimers], delta;

    unsigned int timer[nTimers];
    CTimeList timeList[nTimers]; // a list of times that the timer was invoked
    const unsigned testIntervalMs[nTimers] = { 2000, 100 };  // how far apart should the tests be scheduled
    const unsigned MaxAllowedError = 1; //number of allowed timer error
    std::list<timespec> tempTimeList;
    unsigned safety_counter;
    unsigned max_saftey_counter=10000;

    CTimer::getTime(start_time[0]);
    timer[0] = timerManager.CreateTimer(testIntervalMs[0], basicTimerCallback, &timeList[0],CTimer::TimerActive,false);
    mSleep(200); //wait a little
    //there should be no pending events
    EXPECT_TRUE(timeList[0].empty());
    EXPECT_TRUE(timeList[1].empty());
    EXPECT_TRUE(timerManager.IsTimerActive(timer[0]));

    CTimer::getTime(start_time[1]);
    timer[1] = timerManager.CreateTimer(testIntervalMs[1], basicTimerCallback, &timeList[1],CTimer::TimerActive,false);
    //still, there should be no events
    EXPECT_TRUE(timeList[0].empty());
    EXPECT_TRUE(timeList[1].empty());
    EXPECT_TRUE(timerManager.IsTimerActive(timer[1]));

    //wait for the short timer to trip
    safety_counter=0;
    while (timeList[1].empty() && safety_counter++ < max_saftey_counter) {
        mSleep(1);
    }
    EXPECT_FALSE(timerManager.IsTimerActive(timer[1]));
    //the slow timer should not have tripped
    EXPECT_TRUE(timeList[0].empty());
    //the fast timer should have tripped only once
    EXPECT_FALSE(timeList[1].empty());
    EXPECT_EQ(timeList[1].size(), (unsigned )1);
    tempTimeList = timeList[1].get_list();
    delta = CTimer::diff_timespec(tempTimeList.front(), start_time[1]);
    EXPECT_NEAR(CTimer::timespec2ms(delta), testIntervalMs[1], MaxAllowedError);

    //wait for the long timer to trip
    safety_counter=0;
    while (timeList[0].empty() && safety_counter++ < max_saftey_counter) {
        mSleep(1);
    }
    EXPECT_FALSE(timerManager.IsTimerActive(timer[0]));
    //both lists should still only have one element
    EXPECT_FALSE(timeList[0].empty());
    EXPECT_EQ(timeList[0].size(), (unsigned )1);
    EXPECT_EQ(timeList[1].size(), (unsigned )1);

    tempTimeList = timeList[0].get_list();
    delta = CTimer::diff_timespec(tempTimeList.front(), start_time[0]);
    EXPECT_NEAR(CTimer::timespec2ms(delta), testIntervalMs[0], MaxAllowedError);

}


/**
 * Test the operation of restarting one shot timers
 *
 */
TEST(Timer,oneShotRestart) {
    const unsigned nTimers = 1;
    CTimer timerManager;
    timespec start_time[nTimers], delta;
    unsigned int timer[nTimers];
    CTimeList timeList[nTimers]; // a list of times that the timer was invoked
    const unsigned testIntervalMs[nTimers] = { 100 };  // how far apart should the tests be scheduled
    const unsigned MaxAllowedError = 1; //number of allowed timer error
    std::list<timespec> tempTimeList;
    unsigned safety_counter;
    unsigned max_saftey_counter=10000;

    CTimer::getTime(start_time[0]);
    timer[0] = timerManager.CreateTimer(testIntervalMs[0], basicTimerCallback, &timeList[0],CTimer::TimerActive,false);
    mSleep(testIntervalMs[0]/2 ); //wait a little
    //there should be no pending events
    EXPECT_TRUE(timeList[0].empty());
    EXPECT_TRUE(timerManager.IsTimerActive(timer[0]));

    //wait for the short timer to trip
    safety_counter=0;
    while (timeList[0].empty() && safety_counter++ < max_saftey_counter) {
        mSleep(1);
    }
    EXPECT_FALSE(timerManager.IsTimerActive(timer[0]));
    EXPECT_EQ(timeList[0].size(), (unsigned )1);

    tempTimeList = timeList[0].get_list();
    delta = CTimer::diff_timespec(tempTimeList.front(), start_time[0]);
    EXPECT_NEAR(CTimer::timespec2ms(delta), testIntervalMs[0], MaxAllowedError);

    //wait a little to make sure the timer is not starting
    mSleep(testIntervalMs[0]*3 );
    EXPECT_FALSE(timerManager.IsTimerActive(timer[0]));
    EXPECT_EQ(timeList[0].size(), (unsigned )1);

    //restart the timer
    timeList[0].clear();
    CTimer::getTime(start_time[0]);
    timerManager.RestartTimer(timer[0]);
    safety_counter=0;
    while (timeList[0].empty() && safety_counter++ < max_saftey_counter) {
            mSleep(1);
    }
    EXPECT_FALSE(timerManager.IsTimerActive(timer[0]));
    EXPECT_EQ(timeList[0].size(), (unsigned )1);
    tempTimeList = timeList[0].get_list();
    delta = CTimer::diff_timespec(tempTimeList.front(), start_time[0]);
    EXPECT_NEAR(CTimer::timespec2ms(delta), testIntervalMs[0], MaxAllowedError);


}

TEST(Timer,deleteTimer) {
    const unsigned nTimers = 1;
    CTimer timerManager;
    unsigned int timer[nTimers];
    CTimeList timeList[nTimers]; // a list of times that the timer was invoked
    const unsigned testIntervalMs[nTimers] = { 100 };  // how far apart should the tests be scheduled


    timer[0] = timerManager.CreateTimer(testIntervalMs[0], basicTimerCallback, &timeList[0],CTimer::TimerActive,false);
    mSleep(testIntervalMs[0]/2 ); //wait a little
    //there should be no pending events
    EXPECT_TRUE(timeList[0].empty());
    EXPECT_TRUE(timerManager.IsTimerActive(timer[0]));

    timerManager.DeleteTimer(timer[0]);

    mSleep(testIntervalMs[0]*2 ); //wait a little
    //there should be no pending events
    EXPECT_TRUE(timeList[0].empty());

}

TEST(Timer,stopTimer) {
    const unsigned nTimers = 1;
    CTimer timerManager;
    unsigned int timer[nTimers];
    CTimeList timeList[nTimers]; // a list of times that the timer was invoked
    const unsigned testIntervalMs[nTimers] = { 100 };  // how far apart should the tests be scheduled


    timer[0] = timerManager.CreateTimer(testIntervalMs[0], basicTimerCallback, &timeList[0],CTimer::TimerActive,false);
    mSleep(testIntervalMs[0]/2 ); //wait a little
    //there should be no pending events
    EXPECT_TRUE(timeList[0].empty());
    EXPECT_TRUE(timerManager.IsTimerActive(timer[0]));

    timerManager.StopTimer(timer[0]);

    mSleep(testIntervalMs[0]*2 ); //wait a little
    //there should be no pending events
    EXPECT_TRUE(timeList[0].empty());
    EXPECT_FALSE(timerManager.IsTimerActive(timer[0]));

}

/**
 * Test the operation of dump functions used for debugging
 *
 */
TEST(Timer,dumpFunctions) {
    CTimer timerManager;
    const unsigned nTimers = 8;
    unsigned int timer[nTimers];
    CTimeList timeList[nTimers]; // a list of times that the timer was invoked
    const unsigned testIntervalMs[nTimers] = { 10,30,90,270,500,800,1000,2000 };  // how far apart should the tests be scheduled
    const unsigned numTestEnvents = 5; //number of events for the longest timer

    for (unsigned idxTimer = 0; idxTimer < nTimers; idxTimer++) {
        timer[idxTimer] = timerManager.CreateTimer(testIntervalMs[idxTimer], basicTimerCallback, &timeList[idxTimer]);
        ASSERT_NE(timer[idxTimer], CTimer::INVALID_HANDLE);
    }
    mSleep(testIntervalMs[nTimers - 1] * (numTestEnvents + 1));

    timerManager.DumpTimersQueue();
    timerManager.DumpValidTimers();

}


