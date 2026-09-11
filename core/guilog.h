#ifndef GUI_LOG_H
#define GUI_LOG_H 1

/* guilog.h v0.2
   Copyright: Tomasz Pawlak (C) 2014-2025
   License:   GPLv3+

   Core-side log sink: the parser pushes status/log lines here.
   The GTK4 app installs a sink callback; without one, DoLog()
   forwards to stdout, so the core stays usable headless (tests).
*/

#include "wxcompat.h"
#include "rcode/rcode.h"

enum {
   msg_buf_sz = 1024
};

//log sink: receives assembled log lines (may be NULL -> stdout)
typedef void (*guilog_sink_fn)(const char* msg, void* user_data);

class guilog_cl {
   protected:
      int          status;

   public:
      wxc_String   slog;
      char         rcd_msg_buff[msg_buf_sz];

      guilog_sink_fn sink_fn;
      void*          sink_data;

      inline bool isReady() {return (status>0);};

      void RcodeToString(rcode retU, wxc_String& str);
      void PrintRcode(rcode retU);

      void DoLog();
      void DoLog(const wxc_String& msg);

      //sink registration (replaces upstream wxLogWindow::Create)
      void SetSink(guilog_sink_fn fn, void* user_data = NULL);

   guilog_cl() : status(0), sink_fn(NULL), sink_data(NULL) {};

   ~guilog_cl() {};
};

#endif /* GUI_LOG_H */
