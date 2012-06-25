#include <iostream>
#include <fstream>

#include "monitoringAgent.h"

namespace rax 
{
  MonitoringAgentSvc::MonitoringAgentSvc(void)
  {
  }


  MonitoringAgentSvc::~MonitoringAgentSvc(void)
  {
  }

  bool MonitoringAgentSvc::Start()
  {
    virgo_error_t *err;

    err = virgo_create(&mVirgoCfg, "monitoring");

    if (err) {
      HandleError("Error in startup", err);
      return false;
    }

    /* default filename */
    err = virgo_conf_lua_load_path(mVirgoCfg, VIRGO_DEFAULT_ZIP_FILENAME);
    if (err) {
      HandleError("Error in setting lua load path", err);
      return false;
    }

    mThreadHandle = CreateThread(NULL, 0, VirgoThread, (LPVOID)this, 0, &mThreadId);

    return NULL != mThreadHandle;
  }

  void MonitoringAgentSvc::Stop()
  {
    virgo_destroy(mVirgoCfg);
  }

  DWORD WINAPI MonitoringAgentSvc::VirgoThread(const LPVOID lpParameter)
  {
    MonitoringAgentSvc* _this = (MonitoringAgentSvc*)lpParameter;

    _this->mVirgoRunReturn = virgo_run(_this->mVirgoCfg);
    if (_this->mVirgoRunReturn) {
      //if (err->err == VIRGO_EHELPREQ) {
      //  show_help();
      //  virgo_error_clear(err);
      //  return 1;
      //}
      //else if (err->err == VIRGO_EVERSIONREQ) {
      //  show_version();
      //  virgo_error_clear(err);
      //  return 1;
      //}
      //else {
      //  HandleError("Runtime Error", err);
      //}
      return EXIT_FAILURE;
    }

    return 0;
  }

  void MonitoringAgentSvc::HandleError(const char *msg, virgo_error_t *err)
  {
    char buf[256];

    snprintf(buf, sizeof(buf), "%s: %s", msg, "[%s:%d] (%d) %s");
    fprintf(stderr, buf, err->file, err->line, err->err, err->msg);
    fputs("\n", stderr);
    fflush(stderr);
    virgo_error_clear(err);
  }
}
