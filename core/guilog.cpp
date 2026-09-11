
#include "rcdunits.h"
#ifndef idGUI_LOG
   #error "guilog.cpp: missing unit ID"
#endif
#define RCD_UNIT idGUI_LOG
#include "rcode/rcode.h"

#include "wxedid_rcd_scope.h"

#include "guilog.h"

#include <stdio.h>

void guilog_cl::RcodeToString(rcode retU, wxc_String& str) {
   //assemble the message
   wxedid_RCD_GET_MSG(retU, rcd_msg_buff, msg_buf_sz);

   str = rcd_msg_buff;
}

void guilog_cl::PrintRcode(rcode retU) {
   wxedid_RCD_GET_MSG(retU, rcd_msg_buff, msg_buf_sz);
   DoLog(rcd_msg_buff);
}

void guilog_cl::DoLog() {
   DoLog(slog);
   slog.Empty();
};

void guilog_cl::DoLog(const wxc_String& msg) {
   if (sink_fn != NULL) {
      sink_fn(msg.c_str(), sink_data);
      return;
   }
   //headless fallback: stdout
   printf("%s\n", msg.c_str());
};

void guilog_cl::SetSink(guilog_sink_fn fn, void* user_data) {
   sink_fn   = fn;
   sink_data = user_data;
   status    = 1;
};
