#pragma once

#include <windows.h>

#include <virgo.h>

namespace rax
{
  class MonitoringAgentSvc // Previously defined as a final class.
  {
  public:
    MonitoringAgentSvc(void);
    ~MonitoringAgentSvc(void);

    bool Start();
    void Stop();

    /// Returns true if the agent wants to shut down
    bool AbortRequested() { return false; }

    static DWORD WINAPI MonitoringAgentSvc::VirgoThread(const LPVOID lpParameter);
    static void HandleError(const char *msg, virgo_error_t *err);

  private:
    // Equivalent to boost::noncopyable.
    MonitoringAgentSvc(MonitoringAgentSvc const& rhs) {}
    void operator=(MonitoringAgentSvc const& rhs) {}

    HANDLE mThreadHandle;
    DWORD mThreadId;
    virgo_error_t* mVirgoRunReturn;
    virgo_t* mVirgoCfg;
  };
}
