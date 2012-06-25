/*
Rackspace (c) 2011,2012
*/

#pragma comment(lib, "advapi32.lib")

#include "WinService.h"
#include "monitoringAgent.h"

#define SVCNAME TEXT("Bloop")
#define AUDIT_POLL 1*1*1000 ///@todo currently let service audit run every this-many milliseconds
/// \todo Move all engine and application policy out of service cpp module and use whatever typedef or other implementation that is instantiated for the server to use at runtime 

VOID SvcInstall(void);
VOID WINAPI SvcCtrlHandler(DWORD); 
VOID WINAPI SvcMain(DWORD, LPWSTR *); 

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPWSTR *); 

VOID SvcReportEvent(LPWSTR, bool NonError = false);

namespace local 
{ namespace
  {
    struct ServiceControl
    {
      ServiceControl() : mhandleSvcStopEvent(NULL)
      {
        mSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
        mSvcStatus.dwServiceSpecificExitCode = 0;    
      }

      SERVICE_STATUS          mSvcStatus; 
      SERVICE_STATUS_HANDLE   mSvcStatusHandle; 
      HANDLE                  mhandleSvcStopEvent;
    };

    rax::MonitoringAgentSvc agent;
    ServiceControl service_control;
  }
}


BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
  bool shut_down = false;
  switch(CEvent)
  {
  case CTRL_BREAK_EVENT:    
//    RAXLOG_INFO("Got BREAK event. Shutting down.");
    shut_down = true;
    break;
  case CTRL_C_EVENT:
//    RAXLOG_INFO("Got CTRL+C event. Shutting down.");
    shut_down = true;
    break;
  case CTRL_CLOSE_EVENT:
//    RAXLOG_INFO("Got CLOSE event. Shutting down.");
    shut_down = true;
    break;
  case CTRL_LOGOFF_EVENT:
    ///@todo Do we need to handle this?
    break;
  case CTRL_SHUTDOWN_EVENT:
    // Should be handled by the svc loop
    break;

  }

  if (shut_down)
  {
//    local::agent.Stop();
  }

  return TRUE;
}

///
/// Purpose: 
///   Process CLI entry point
///   SCM, however uses our registered SvMain
///
/// Parameters:
///   None
/// 
/// Return value:
///   None
///
int wmain(int argc, wchar_t* argv[])
{
  if (argc > 1)
  {
    if(_wcsicmp(argv[1], TEXT("standalone")) == 0) /// optionally: add commandline to debugger options in Visual Studio
    {
      SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE);
      SvcInit(argc, argv); ///< creates our app object frame(s) and Windows Service Manager callback mechanisms        
      return 0;
    }

    /// Install service and exit - run from SCM
    if(_wcsicmp(argv[1], TEXT("install")) == 0)
    {
      SvcInstall();
      return 0;
    }
  }
#ifdef RAX_DEBUG
  else
  {
    // Just run standalone for now (aids in debugging)
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE);
    SvcInit(argc, argv);
    return 0;
  }
#endif

  /// sc  delete CloudFilesService <presumes that is service name>
  /// sc GetDisplayname CloudFilesService
  /// sc start CloudFilesService
  /// sc stop CloudFilesService
  /*
  qc--------------Queries the configuration information for a service.
  qdescription----Queries the description for a service.
  qfailure--------Queries the actions taken by a service upon failure.
  qfailureflag----Queries the failure actions flag of a service.
  qsidtype--------Queries the service SID type of a service.
  qprivs----------Queries the required privileges of a service.
  qtriggerinfo----Queries the trigger parameters of a service.
  qpreferrednode--Queries the preferred NUMA node of a service.
  */

  /// Multiple services can be registered via this table, we're single-instanced
  SERVICE_TABLE_ENTRY DispatchTable[] = 
  { 
      { SVCNAME, (LPSERVICE_MAIN_FUNCTION) SvcMain }, //our "main" callback
      { NULL, NULL } 
  }; 

  /// This system call will end-up calling into our SvcMain-registered-callback, below
  if (!StartServiceCtrlDispatcher(DispatchTable)) 
  { 
      SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
  } 

  return 1;
} 


/// SvcInstall ONLY registers this binary with Windows Service Controller Module registry(database)
VOID SvcInstall()
{
    TCHAR szPath[MAX_PATH] = {0};

    if(!GetModuleFileName(NULL, szPath, MAX_PATH))
    {
        SvcReportEvent(TEXT("SvcInstall GetModuleFileName ERROR"));
        return;
    }

    /// Get handle to SCM database
    SC_HANDLE schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager) 
    {
        SvcReportEvent(TEXT("SvcInstall OpenSCManager ERROR"));
        return;
    }

    /// Create service
    SC_HANDLE schService = CreateService(
        schSCManager,              // SCM database 
        SVCNAME,				   // name of service 
        SVCNAME,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_DEMAND_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        szPath,                    // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 

    if (schService == NULL) 
    {
        SvcReportEvent(TEXT("SvcInstall CreateService ERROR")); 
        CloseServiceHandle(schSCManager);
        return;
    }
    
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Entry point for service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//

/// SvcMain invoked by SCM, ala CLI: service start/stop "Service Name"
VOID WINAPI SvcMain(DWORD dwArgc, LPWSTR *lpszArgv)
{
  /// SCM commands will be routed to our registered service-command-handler
  local::service_control.mSvcStatusHandle = RegisterServiceCtrlHandler(
    SVCNAME, 
    SvcCtrlHandler);

  SvcReportEvent(TEXT("Hello")); 

  ///Verify our registration?
  if(!local::service_control.mSvcStatusHandle)
  { 
    SvcReportEvent(TEXT("SvcMain RegisterServiceCtrlHandler ERROR")); 
    return; 
  } 

  /// SCM Status Report: initial status
  ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 1000);

  /// Perform service-specific initialization (only our one instance)
  /// and run our service loop, all application-domained objects 
  /// are housed therein
  SvcInit(dwArgc, lpszArgv);
}

/// SvcInit - a couple of frames after runtime startup, ala our service's registered-callback:SvcMain, 
/// this is service's main loop.
///
/// Note parameters can be supplied to this binary via services.msc, so this is logically
/// equivalent to regular C-Runtime's int main(...)
/// Parameters:
///   dwArgc - Number of arguments in the lpszArgv array
///   lpszArgv - Array of CLI (CommandLineInterface) strings
///
/// Return value:
///   None
///

VOID SvcInit(DWORD dwArgc, LPWSTR *lpszArgv)
{     
  // Must periodically call ReportSvcStatus() with
  // SERVICE_START_PENDING. If initialization fails, call
  // ReportSvcStatus with SERVICE_STOPPED.

  // Create "stop service" event. SvcCtrlHandler
  // signals this event received via SCM (CLI or services.msc)
  local::service_control.mhandleSvcStopEvent = CreateEvent(
    NULL,    // default security attributes
    TRUE,    // manual reset event
    FALSE,   // not signaled
    NULL);   // no name

  if (NULL == local::service_control.mhandleSvcStopEvent)
  {
    SvcReportEvent(SVCNAME L" CreateStopEvent ERROR - EXITING");
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
    return;
  }

  // SCM interface's mandatory report "we are running" status upon initialization completion    
  ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 2000);

  SvcReportEvent(SVCNAME L"service starting", true/*==non error*/);

  if (!local::agent.Start())
  {
    SvcReportEvent(SVCNAME L" Engine OnStartup ERROR - exiting");
    ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 2000); //post status to scm 
    ReportSvcStatus(SERVICE_STOPPED, ERROR_ACCESS_DENIED, 0); //post status to scm 
    return;
  }

  ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

//  RAXLOG_INFO("Process started");

  // Check for signaled stop event
  bool keep_running(true);
  while(keep_running)
  {
    DWORD result = WaitForSingleObject(local::service_control.mhandleSvcStopEvent, (AUDIT_POLL));

    // NOTE: 3-out-of-4 cases can mutate, so just update it once now 
    keep_running = false;

    switch(result)
    {
    case WAIT_OBJECT_0:
//      RAXLOG_INFO("Stopped by SCM. Shutting down.");
      SvcReportEvent(SVCNAME L" Stopped by SCM", true/*==non error*/);
      break; ///< stops loop: bServiceLoop was set(false)
    case WAIT_ABANDONED:
//      RAXLOG_INFO("Stopped by WAIT_ABANDONED. Shutting down.");
      SvcReportEvent(SVCNAME L" Stopped by ERROR");
      break; ///< stops loop: bServiceLoop was set(false)
    case WAIT_TIMEOUT:
       //Normal timeout case. Do NOT run WaitForSingleObject w/INFINITE timeout, else no health-heartbeats can be audited here.
      //engine.OnAudit("Service running");
      keep_running = !local::agent.AbortRequested();
      break;
    case WAIT_FAILED:
//      RAXLOG_INFO("Stopped by WAIT_FAILED. Shutting down.");
      SvcReportEvent(SVCNAME L" Stopped by ERROR");
      break; ///< stops loop: bServiceLoop was set(false)
    }
  }

  // Service is shutting down
//  RAXLOG_INFO("Process exiting");

  ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);    
  local::agent.Stop();
  ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);

  return;
}

///
/// Purpose: 
///   Sets current service status and reports it to SCM
///   When SCM issues a control code, start, stop,.pause, all services
///   are expected to report their progress-status. After an elapsed 
///   time period, if the service doesn't stop, SCM has the authority to 
///   forcibly terminate the service. Goal: try to shutdown/pause as cleanly 
///   as possible when so commanded
///
///
/// Parameters:
///   dwCurrentState - current state (see SERVICE_STATUS)
///   dwWin32ExitCode - system error code
///   dwWaitHint - Estimated time for pending operation, 
///     in milliseconds
/// 
/// Return value:
///   None
///
VOID ReportSvcStatus(
  DWORD dwCurrentState,
  DWORD dwWin32ExitCode,
  DWORD dwWaitHint)
{
    static DWORD dwCheckPoint(1); ///Used by SCM to determine progression/status

    /// Fill in SERVICE_STATUS structure.
    local::service_control.mSvcStatus.dwCurrentState = dwCurrentState;
    local::service_control.mSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    local::service_control.mSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
    {
        local::service_control.mSvcStatus.dwControlsAccepted = 0;
    }
    else
    {
        local::service_control.mSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    }

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
    {
        local::service_control.mSvcStatus.dwCheckPoint = 0;
    }
    else
    {
        local::service_control.mSvcStatus.dwCheckPoint = dwCheckPoint++;
    }

    /// Report service status to SCM
    SetServiceStatus(local::service_control.mSvcStatusHandle, &local::service_control.mSvcStatus);
}

///
/// Purpose: 
///   Called by SCM whenever a control code is sent to  service
///   using ControlService function
///
/// Parameters:
///   dwCtrl - control code
/// 
/// Return value:
///   None
///
VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
    /// Handle control code SCM has issued to us

    switch(dwCtrl) 
    {  
    case SERVICE_CONTROL_STOP: 
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        /// Terminate service
        SetEvent(local::service_control.mhandleSvcStopEvent);
        ReportSvcStatus(local::service_control.mSvcStatus.dwCurrentState, NO_ERROR, 0);
        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default: 
        break;
    } 
}

///
/// Purpose: 
///   Logs messages to event log
///
/// Parameters:
///   szFunction - name of function that failed
/// 
/// Return value:
///   None
///
/// Remarks:
///   Service must have an entry in Application event log.
///
VOID SvcReportEvent(LPWSTR szFunction, bool NonError/*= false*/) 
{
    HANDLE hEventSource(RegisterEventSource(NULL, SVCNAME));

    if(NULL != hEventSource)
    {
        TCHAR Buffer[80] = {0};
        
        LPCTSTR lpszStrings[2] =  { SVCNAME, Buffer};

        if (!NonError) //clumsy but using a default w/existing code : Not a "non-error" means error
        {
            // _snwprintf(Buffer, 80, TEXT("%s failed with %s (%d)"), szFunction, GetErrorMessage(GetLastError()), GetLastError());

            ReportEvent(hEventSource,        // event log handle
            EVENTLOG_ERROR_TYPE, // event type
            0,                   // event category
            SVC_ERROR,           // event identifier
            NULL,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            NULL);               // no binary data
        }
        else //NonError means no error
        {
            // _snwprintf(Buffer, 80, TEXT("Information:%s"), szFunction);

            ReportEvent(hEventSource,        // event log handle
            EVENTLOG_INFORMATION_TYPE, // event type
            0,                   // event category
            SVC_INFO,           // event identifier
            NULL,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            NULL);               // no binary data
        }       

        DeregisterEventSource(hEventSource);
    }
}

