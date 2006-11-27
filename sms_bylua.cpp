#include <windows.h>
#include <wininet.h>
#include <stdstring.h>

#include <map>

#define LUAEX_PARAM_TYPE cSILua
#include "..\lua\luaex.h"

//#pragma comment(lib , "..\\__libs\\lua.lib")

using namespace std;
#include "konnekt/plug_export.h"

#include "sms_interface.h"
#include "../include/preg.h"
#include "../include/func.h"

#define GC_INTERNET ((void*)1)

std::string urlEncode(std::string str);
std::string urlDecode(std::string str);

class cSILua: public cSMSScriptInterface {
public:
    cSILua(cSMSSend * owner):cSMSScriptInterface(owner) {}

    void Init(LuaState * state) {
     
        LuaObject global = state->GetGlobals();
        global.Register("alert" , makeFunctor(LSFunctorType(), *this,LS_alert));
        global.Register("confirm" , makeFunctor(LSFunctorType(), *this,LS_confirm));
        global.Register("getToken" , makeFunctor(LSFunctorType(), *this,LS_gettoken));
    //    state->Register("input" , LS_input);
    //    state->Register("getLogin" , LS_getLogin);

    //    state->Register("setProgress" , LS_setProgress);
        global.Register("setInfo" , makeFunctor(LSFunctorType(), *this,LS_setInfo));
        global.Register("setError" , makeFunctor(LSFunctorType(), *this,LS_setError));
        global.Register("setSuccess" , makeFunctor(LSFunctorType(), *this,LS_setSuccess));
        global.Register("setParam" , makeFunctor(LSFunctorType(), *this,LS_setParam));
        global.Register("setVar" , makeFunctor(LSFunctorType(), *this,LS_setVar));
        global.Register("getVar" , makeFunctor(LSFunctorType(), *this,LS_getVar));
		global.Register("refreshStatus" , makeFunctor(LSFunctorType(), *this,LS_refreshStatus));
//        global.Register("LOG" , LS_LOG);
//        global.Register("include" , LS_include);

    }

    int LS_gettoken(LuaState* state, LuaStackObject* args);
    int LS_input(LuaState* state, LuaStackObject* args);
    int LS_getLogin(LuaState* state, LuaStackObject* args);
    int LS_setInfo(LuaState* state, LuaStackObject* args);
    int LS_getVar(LuaState* state, LuaStackObject* args);
    int LS_setVar(LuaState* state, LuaStackObject* args);
    int LS_setParam(LuaState* state, LuaStackObject* args);
    int LS_setProgress(LuaState* state, LuaStackObject* args);
    int LS_setError(LuaState* state, LuaStackObject* args);
    int LS_setSuccess(LuaState* state, LuaStackObject* args);
    int LS_refreshStatus(LuaState* state, LuaStackObject* args);
    int LS_alert(LuaState* state, LuaStackObject* args);
    int LS_confirm(LuaState* state, LuaStackObject* args);
};

void CBError (cLuaEx * ex , const cLUAErrorInfo * nfo) {
    LuaExP(ex)->owner->setScriptError(nfo->txtInfo);
}
void CBLog (cLuaEx * ex , const char * txt){
    LuaExP(ex)->owner->LOG(txt);
}



// gettoken(info , file)=string
int cSILua::LS_gettoken(LuaState* state, LuaStackObject* args) {
    LuaStackObject & info = args[1];
    LuaStackObject & file = args[2];
    if (info.IsString() && file.IsString())
        state->PushString(this->owner->gettoken(info.GetString() , file.GetString()));
        else state->PushBoolean(false);
    return 1; // Liczba zwracanych wartoœci
}


// input(title , info)
int cSILua::LS_input(LuaState* state, LuaStackObject* args) {
    LuaStackObject & p1 = args[1];
    LuaStackObject & p2 = args[2];
    return 1; // Liczba zwracanych wartoœci
}

// getLogin(title , info , login , haslo)
int cSILua::LS_getLogin(LuaState* state, LuaStackObject* args) {
    LuaStackObject & title = args[1];
    LuaStackObject & info = args[2];
    LuaStackObject & login = args[3];
    LuaStackObject & haslo = args[4];
    return 2; // Liczba zwracanych wartoœci
}



int cSILua::LS_setInfo(LuaState* state, LuaStackObject* args) {
    LuaStackObject & msg = args[1];
    LuaStackObject & type = args[2];
    owner->setInfo(msg.GetString() , type.IsNumber()?(SMSInfoType)type.GetInteger():SIT_INFO);
    return 0; // Liczba zwracanych wartoœci
}



// setParam(name , value)
int cSILua::LS_setParam(LuaState* state, LuaStackObject* args) {
    LuaStackObject & name = args[1];
    LuaStackObject & value = args[2];
    if (!name.IsString() || !value.IsString()
        || !*name.GetString()
        ) {state->PushBoolean(false); return 1;}
    state->PushBoolean(
        owner->setParam(name.GetString() , value.GetString())
    );
    return 1; // Liczba zwracanych wartoœci
}


// setVar(name , value)
int cSILua::LS_setVar(LuaState* state, LuaStackObject* args) {
    LuaStackObject & name = args[1];
    LuaStackObject & value = args[2];
    if (!name.IsString() || !*name.GetString()
        ) {state->Error("Name must be a string!"); return 0;}
    state->PushBoolean(
        (value.IsString())?
        owner->setVar(name.GetString() , value.GetString())
        :
        owner->eraseVar(name.GetString())
    );
    return 1; // Liczba zwracanych wartoœci
}
// getVar(name)
int cSILua::LS_getVar(LuaState* state, LuaStackObject* args) {
    LuaStackObject & name = args[1];
    if (!name.IsString() || !*name.GetString()
        ) {state->Error("Name must be a string!"); return 0;}
    state->PushString(
        owner->getVar(name.GetString())
    );
    return 1; // Liczba zwracanych wartoœci
}

int cSILua::LS_refreshStatus(LuaState* state, LuaStackObject* args) {
	owner->refreshStatus();
	return 0;
}


int cSILua::LS_setProgress(LuaState* state, LuaStackObject* args) {
    LuaStackObject & param1 = args[1];
//    if (param1.IsNumber())
//        printf(" %d %%" , param1.GetInteger());
    return 0; // Liczba zwracanych wartoœci
}
int cSILua::LS_setError(LuaState* state, LuaStackObject* args) {
    LuaStackObject & param1 = args[1];
    owner->setError(param1.GetString());
    return 0; // Liczba zwracanych wartoœci
}
int cSILua::LS_setSuccess(LuaState* state, LuaStackObject* args) {
    LuaStackObject & param1 = args[1];
    owner->setSuccess(param1.GetString());
    return 0; // Liczba zwracanych wartoœci
}


// ---00000000000-------------------------------------------

int cSILua::LS_alert(LuaState* state, LuaStackObject* args) {
    LuaStackObject & txt = args[1];
    LuaStackObject & title = args[2];
    if (!txt.IsString()) {state->Error("NIL passed!");return 0;}
    owner->alert(txt.GetString() , title.IsString()?title.GetString():"LUA.script");
    return 0; // Liczba zwracanych wartoœci
}

// confirm(txt,title)=bool
int cSILua::LS_confirm(LuaState* state, LuaStackObject* args) {
    LuaStackObject & param1 = args[1];
    LuaStackObject & param2 = args[2];
    if (param1.IsString())
        if (owner->confirm(param1.GetString() , param2.IsString()?param2.GetString():"LUA.script"))
            state->PushBoolean(true);
        else state->PushBoolean(false);
    return 1; // Liczba zwracanych wartoœci
}


// --------------------------------------------------------------------




void sendSMS_byLUA(cSMSSend * sms) {


    LuaStateOwner owner(true);
    LuaState * state = owner.Get();
    sms->SI = new cSILua(sms);
    cLuaEx EX(state , LEI_DEFAULT , sms->SI);
    EX.setErrorCallBack(CBError);
    EX.setLogCallBack(CBLog);
	LuaObject oPlus = state->GetGlobals()["LuaPlusEx"];
    oPlus.SetLightUserData("HWND",(void*)sms->getHWND(sms->winID));
	if (GETINT(CFG_PROXY)) {
		oPlus.SetNumber("proxyType" , INTERNET_OPEN_TYPE_PROXY);
		oPlus.SetString("proxyHost" , GETSTR(CFG_PROXY_HOST) + CStdString(":") + GETSTR(CFG_PROXY_PORT));
		if (GETINT(CFG_PROXY_AUTH)) {
			oPlus.SetString("proxyLogin" , GETSTR(CFG_PROXY_LOGIN));
			oPlus.SetString("proxyPass" , GETSTR(CFG_PROXY_PASS));
		}
	} else 
		oPlus.SetNumber("proxyType" , INTERNET_OPEN_TYPE_DIRECT);

    ((cSILua*)sms->SI)->Init(state);
//    state->DumpGlobals("lua.dmp" , true , -1 , true); 
    sms->result = 0;
    
    if (!state->DoFile(sms->scriptFile)) {
        LuaObject func = state->GetGlobal("sendSMS");
        LuaObject info(state);
        info.AssignNewTable();
        info.SetString("msg" , sms->msg);
        info.SetString("to" , sms->to);
        info.SetString("from" , sms->from);
    // Obsolete
        info.SetString("toNr" , sms->to);
        info.SetString("fromName" , sms->from);
        info.SetString("fromEmail" , "");
        info.SetString("fromNr" , "");

        for (cSMSSend::params_it_t param = sms->params.begin(); param != sms->params.end(); param++) {
            info.SetString(param->first , param->second);
        }
        func() << info << LuaRun(0);
    } else {if (!sms->result) sms->setScriptError("Nie znalaz³em " + sms->scriptFile);}
    delete sms->SI;
}