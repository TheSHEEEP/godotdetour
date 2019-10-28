#include "recastcontext.h"
#include <OS.hpp>

using namespace godot;

RecastContext::RecastContext()
{
    doResetTimers();
}

void
RecastContext::doResetLog()
{
    // ???
}

void
RecastContext::doLog(const rcLogCategory category, const char* msg, const int len)
{
    // Sanity check
    if (!len || msg == 0)
    {
        ERR_PRINT("Log message from recast invalid: ");
        return;
    }

    // Assemble the message
    String message = "recast: {0}: {1}";
    String catMsg = "";
    switch (category)
    {
    case RC_LOG_PROGRESS:
        catMsg = "progress";
        break;

    case RC_LOG_WARNING:
        catMsg = "warning";
        break;

    case RC_LOG_ERROR:
        catMsg = "error";
        break;

    default:
        catMsg = "unknown";
        break;
    }
    message = message.format(Array::make(catMsg, String(msg)));

    // Print
    Godot::print(message);
}

void
RecastContext::doResetTimers()
{
    for (int i = 0; i < RC_MAX_TIMERS; ++i)
    {
        _timers[(rcTimerLabel)i] = -1;
        _accumulatedTime[(rcTimerLabel)i] = -1;
    }
}

void
RecastContext::doStartTimer(const rcTimerLabel label)
{
    _timers[label] = OS::get_singleton()->get_ticks_msec();
}

void
RecastContext::doStopTimer(const rcTimerLabel label)
{
    int64_t now = OS::get_singleton()->get_ticks_msec();
    int64_t deltaTime = now - _timers[label];
    if (_accumulatedTime[label] == -1)
    {
        _accumulatedTime[label] = deltaTime;
    }
    else
    {
        _accumulatedTime[label] += deltaTime;
    }
}

int
RecastContext::doGetAccumulatedTime(const rcTimerLabel label) const
{
    return _accumulatedTime.at(label);
}
