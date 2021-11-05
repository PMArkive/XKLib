#include "pch.h"

#include "processbase.h"

using namespace XKLib;

auto ProcessBase::self() -> ProcessBase
{
#ifdef WINDOWS
    return ProcessBase(GetCurrentProcessId());
#else
    return ProcessBase(getpid());
#endif
}

ProcessBase::ProcessBase(pid_t pid) : _pid(pid)
{
}

auto ProcessBase::setPID(pid_t pid)
{
    _pid = pid;
}

auto ProcessBase::id() -> pid_t
{
    return _pid;
}
