/**
 * This file defines a template based waitable object
 * that can be used to signal between threads using
 * pthread_cond_wait or pthread_cond_wait_timeout
 *
 * @author AG
 * @date 4-4-2008
 */

#ifndef _WATIABLEOBJECT_H
#define _WATIABLEOBJECT_H

#include <pthread.h>
#include <sys/time.h>
#include <boost/log/trivial.hpp>
/**
 * A template based waitable object
 * that can be used to signal between threads using
 * pthread_cond_wait or pthread_cond_wait_timeout
 */
template<class T>
class WaitableObject {
public:
    /** The object we are throwing */
    T m_Object;

    /** class constructor with mutex and condition attributes used to initialize the class
      * @param[in] pMutex_attr Mutex attributes
      * @param[in] pCond_attr Condition attributes
      */
    WaitableObject(const pthread_mutexattr_t *pMutex_attr, const pthread_condattr_t *pCond_attr) {
        pthread_mutex_init(&m_ClassMutex,0);
        pthread_mutex_init(&m_ConditionMutex, pMutex_attr);
        pthread_cond_init(&m_Condition, pCond_attr);
        m_bTriggered = false;
        m_WaiterCount = 0;
    }
    /** default constructor */
    WaitableObject() {
        pthread_mutex_init(&m_ClassMutex, NULL);
        pthread_mutex_init(&m_ConditionMutex, NULL);
        pthread_cond_init(&m_Condition, NULL);
        m_bTriggered = false;
        m_WaiterCount = 0;
    }
    /** destructor */
    ~WaitableObject() {
        pthread_mutex_destroy(&m_ClassMutex);
        pthread_mutex_destroy(&m_ConditionMutex);
        pthread_cond_destroy(&m_Condition);
    }
    /** locks the mutex and returns the results of the lock operation
      * @return mutex lock operation results
      */
    int LockMutex() {
        return pthread_mutex_lock(&m_ConditionMutex);
    }
    /** unlocks the mutex */
    int UnlockMutex() {
        return pthread_mutex_unlock(&m_ConditionMutex);
    }
    /** sends a signal to one the waiters*/
    int Signal() {
        pthread_mutex_lock(&m_ClassMutex);
        m_bTriggered = true;
        pthread_mutex_unlock(&m_ClassMutex);

        return pthread_cond_signal(&m_Condition);
    }
    /** sends a signal to all the waiters */
    int SignalBroadcast() {
        pthread_mutex_lock(&m_ClassMutex);
        m_bTriggered = true;
        pthread_mutex_unlock(&m_ClassMutex);

        return pthread_cond_broadcast(&m_Condition);
    }
    /** wait on a signal for ever
      * @note the mutex must already be locked before calling this function
      */
    int WaitOnObject() {
        int status = 0;

        pthread_mutex_lock(&m_ClassMutex);
        m_WaiterCount++;
        pthread_mutex_unlock(&m_ClassMutex);

        do {
            status = pthread_cond_wait(&m_Condition, &m_ConditionMutex);
        } while ( status == 0 && m_bTriggered == false);

        pthread_mutex_lock(&m_ClassMutex);
        m_WaiterCount--;
        if (m_WaiterCount == 0) {
            m_bTriggered = false;
        }
        pthread_mutex_unlock(&m_ClassMutex);

        return status;
    }
    /** wait on a signal for a specified time
      * @param waitTime number of uS to wait for the object
      * @note the mutex must already be locked before calling this function
      * @retval ETIMEDOUT The time specified by abstime to pthread_cond_timedwait() has passed.
      * @retval EINVAL The value specified by abstime is invalid.
      * @retval EINVAL The value specified by cond or mutex is invalid.
      * @retval EPERM The mutex was not owned by the current thread at the time of the call.
      */
    int WaitOnObject(unsigned waitTime) {
        int status = 0;
        struct timeval now;
        struct timespec timeout;
        const unsigned million = 1000000;

        gettimeofday(&now, NULL);
        timeout.tv_sec  =  now.tv_sec + (now.tv_usec + waitTime) / million;
        timeout.tv_nsec = ((now.tv_usec + waitTime) % million) * 1000;

        pthread_mutex_lock(&m_ClassMutex);
        m_WaiterCount++;
        pthread_mutex_unlock(&m_ClassMutex);

        do {
            status = pthread_cond_timedwait(&m_Condition, &m_ConditionMutex, &timeout);
            if (status == 0 && m_bTriggered == false) {
                BOOST_LOG_TRIVIAL(trace) << "!!!!!!!!Spurious wake up detected!!!!!!!!!!\n";
            }
        } while ( status == 0 && m_bTriggered == false);

        pthread_mutex_lock(&m_ClassMutex);
        m_WaiterCount--;
        if (m_WaiterCount == 0) {
            m_bTriggered = false;
        }
        pthread_mutex_unlock(&m_ClassMutex);

        return status;
    }
protected:
    /** this mutex is used within the class */
    pthread_mutex_t m_ClassMutex;
    /** this mutex is used in conjunction with the condition variable */
    pthread_mutex_t m_ConditionMutex;
    /** condition variable */
    pthread_cond_t  m_Condition;
    /** set to true when the condition is signaled */
    bool m_bTriggered;
    /** number of waiters. i.e. number of threads waiting on this object */
    unsigned int m_WaiterCount;

};

#endif //_WATIABLEOBJECT_H
