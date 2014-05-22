#include "logging.h"
#include <stack>
//////////////////////////////////////////////////
// Log management
namespace logging = boost::log;

logging::trivial::severity_level g_SeverityLevel = logging::trivial::info;
std::stack<boost::log::trivial::severity_level> LocalLogLevel::m_logStack;

/**
 * Sets the log level and saves the previous level
 */
LocalLogLevel::LocalLogLevel(boost::log::trivial::severity_level level) {
    m_logStack.push(g_SeverityLevel);
    g_SeverityLevel=level;
}

/**
 * Restores the previous log level
 */
LocalLogLevel::~LocalLogLevel() {
    if(!m_logStack.empty()){
        g_SeverityLevel=m_logStack.top();
        m_logStack.pop();
    }
}

/**
 * Gets the current log level
 */
boost::log::trivial::severity_level LocalLogLevel::GetLogLevel() {
    return g_SeverityLevel;
}
//////////////////////////////////////////////////
