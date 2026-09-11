/***************************************************************
 * Name:      wxEDID_App.h
 * Purpose:   Defines Application Class
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2014-2025
 * License:   GPLv3+
 **************************************************************/

#ifndef wxEDIDAPP_H
#define wxEDIDAPP_H 1

#include <wx/app.h>
#include <wx/cmdline.h>

class wxEDID_App : public wxApp {
   public:
      virtual bool OnInit();
      virtual int  OnExit();

      bool  CmdLineArgs();

      void  SaveConfig();
      void  LoadConfig();
};

#endif // wxEDIDAPP_H
