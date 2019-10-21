#ifndef RECASTCONTEXT_H
#define RECASTCONTEXT_H

#include <Godot.hpp>
#include <Recast.h>
#include <map>

/**
 * @brief Provides recast an interface for logging, performance timers, etc.
 */
class RecastContext : public rcContext
{
public:
    RecastContext();

protected:
    virtual void doResetLog();
    virtual void doLog(const rcLogCategory category, const char* msg, const int len);
    virtual void doResetTimers();
    virtual void doStartTimer(const rcTimerLabel label);
    virtual void doStopTimer(const rcTimerLabel label);
    virtual int doGetAccumulatedTime(const rcTimerLabel label) const;

private:
    std::map<rcTimerLabel, int64_t> _timers;
    std::map<rcTimerLabel, int64_t> _accumulatedTime;
};

#endif // RECASTCONTEXT_H
