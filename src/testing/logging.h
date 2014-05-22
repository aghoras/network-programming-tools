/** 
 * @brief Collection of functions to simplify logging operations
 *
 * @date   Apr 17, 2014
 * @author ali
 * 
 * 
 */
#ifndef LOGGING_H_
#define LOGGING_H_

#include <boost/log/trivial.hpp>
#include <stack>

class LocalLogLevel{
public:
    LocalLogLevel(boost::log::trivial::severity_level level);
    ~LocalLogLevel();
    static boost::log::trivial::severity_level GetLogLevel();
protected:
   static std::stack<boost::log::trivial::severity_level> m_logStack;
};

extern boost::log::trivial::severity_level g_SeverityLevel;
#endif /* LOGGING_H_ */
