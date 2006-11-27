#pragma once
#ifndef SMS_INTERFACE_H
#define SMS_INTERFACE_H
#include "..\include\garbage.h"

enum SMSInfoType {
    SIT_INFO , 
    SIT_WARN , 
    SIT_ERROR,
    SIT_START , 
    SIT_FINISH , 
};


class cSMSEnv { // Podstawowe, wspolne dla wszystkich sms'ow ustawienia
    public:
};

class cSMSScriptInterface {
public:
    cSMSScriptInterface(class cSMSSend * owner):owner(owner) {}
    cSMSSend * owner;
};

class cSMSSend {
    public:
        struct cSMSGate * gate;
        cGarbageCollection GC;

        CStdString XMLFile;
        CStdString scriptFile;

        CStdString to;
        CStdString from;
        CStdString msg;
        // powroty
#define SMS_RESULT_UNSENT       1
#define SMS_RESULT_SCRIPTERROR  2
        int result; /* 0 - uda³o sie, 1 - nie uda³o siê wysy³anie, 2 - b³¹d w skrypcie */
        CStdString errorMsg;

        cSMSEnv * env;
        cSMSScriptInterface * SI;
        int winID;

        unsigned int msgID; // ID wiadomosci w kolejce

        std::map <CStdString , CStdString> params;
        typedef std::map <CStdString , CStdString>::iterator params_it_t;

        cSMSSend(cSMSEnv * _env , cSMSGate * _gate) {env=_env;result=0;gate = _gate;}

        void setInfo(CStdString msg , SMSInfoType type = SIT_INFO);
        HWND getHWND(int winID);
        void setError(CStdString msg);
        void setScriptError(CStdString msg);
        void setSuccess(CStdString msg);
        void LOG(CStdString msg);
        bool setParam(CStdString name , CStdString value);
        bool setVar(CStdString name , CStdString value);
        void refreshStatus();
        bool eraseVar(CStdString name);
        CStdString getVar(CStdString name);
        void alert(CStdString txt , CStdString title);
        bool confirm(CStdString txt, CStdString title);
        CStdString gettoken(CStdString info, CStdString file);

};

#endif