// sms.cpp : Defines the entry point for the DLL application.
//

#include <windows.h>
#include <commctrl.h>
#include <stdstring.h>
#include <map>

//#define IL_STATIC_LIB

/*#include <IL/il.h>
#include <IL/ilu.h>
#include <IL/ilut.h>
*/
//#pragma comment(lib , "devil.lib");
//#pragma comment(lib , "ilu.lib");
//#pragma comment(lib , "ilut.lib");

using namespace std;

#include <io.h>
#include <math.h>
#include <process.h>

#include "include\func.h"
#include "include\time64.h"
#include "include\critical_section.h"

#include "konnekt/plug_export.h"
#include "konnekt/ui.h"
#include "konnekt/plug_func.h"
#include "konnekt/obsolete.h"
#include "konnekt/sms.h"
#include "sms/sms_interface.h"
#include "include/simxml.h"
#include "plug_defs/lib.h"

#include "res/sms/resource1.h"



#define USE_LUA

#ifdef USE_LUA
    void sendSMS_byLUA(cSMSSend * );
#endif

#define SMS_DIR "sms\\"
#define SMS_CFGID "SMS_"
#define SMS_CFGENABLEDID "SMSENABLED_"
#define SMS_HISTORY_DIR "sms\\"

#define SMS_PART_LENGTH 5

#define IMIG_SMS 14000
#define IMIG_SMS_GATE 14001

#define IMIA_SENDSMS 14002

// 14100 - 14200 - grupy bramek

#define CFG_SMS_REUSEWINDOW 14000
#define CFG_SMS_USELASTGATE 14001  // Opcja
#define CFG_SMS_GATEUSAGE   14002  // Liczba uzyc bramki
#define CFG_SMS_DELETEBODY 14003 // usuwa tresc wiadomosci
#define CFG_SMS_DELMSG_ONFAILURE 14008 // usuwa wiadomosc jak sie cos nie powiedzie...

#define CFG_SMS_SIG         14004
#define CFG_SMS_SIG_SAVE    14007


#define CNT_SMS_GATE        14003  // Bramka ostatnio uzyta z tym kontaktem



INT_PTR CALLBACK SMSWindowProc(HWND hwnd , UINT message, WPARAM wParam, LPARAM lParam);



struct cSMSInfo {
   struct cSMSGate * gate;
   int winID;
   string digest;
   SMSInfoType type;
   string info;

   cSMSInfo(string info , cSMSGate * gate , string digest , int winID , SMSInfoType type=SIT_INFO):info(info),gate(gate),digest(digest),winID(winID),type(type) {}
   cSMSInfo(string info , SMSInfoType type=SIT_INFO , int w=-1):info(info),gate(0),digest(""),winID(winID),type(type) {}

   cSMSInfo & reuse(string info , SMSInfoType type=SIT_INFO) {
       this->info = info; 
       this->type = type;
       return *this;
   }
};

struct cSMSGateParam {
    CStdString name;
    CStdString nameID; // nazwa w tablicy
    CStdString def;
    CStdString type;
    CStdString text;
    CStdString tip;
    int ID;
    string xmlNode; // fragment xml'a opisujacego element
    struct cSMSGate * owner;
    void addParam(string param);
    void configAdd();
    void configRegister();
};

class cSMSVar {
public:
	bool protect;
	cSMSVar() {protect = false;}
	virtual ~cSMSVar() {}
	virtual cSMSVar & operator = (CStdString & val) {return *this;}
	virtual cSMSVar & operator = (string & val) {return *this;}
	virtual cSMSVar & operator = (int val) {return *this;}

	virtual operator int() {
		return 0;
	}
	virtual operator CStdString() {return "";}
};

// klasa, która trzyma w sobie odpowiedni wskaŸnik...
class cSMSVarContainer {
private:
	cSMSVar* p;
public:
	static cSMSVar base; // pusty wskaŸnik...
	cSMSVarContainer() {
		p = &base;
//		OutputDebugString("Container create\n");
	}
	cSMSVarContainer(cSMSVar * v) {
//		OutputDebugString("Container create2\n");
		p = v;
	}
	cSMSVarContainer(const cSMSVarContainer & v) {
//		OutputDebugString("Container copy-create\n");
		p = &base;
	}

	~cSMSVarContainer() {
//		OutputDebugString("Container destroy\n");
		if (p && p!=&base) delete p;
		p = 0;
	}

	cSMSVar & operator * () {return *p;}
	cSMSVar * operator -> () {return p;}
	cSMSVar & operator = (cSMSVar * v) {
//		OutputDebugString("Container switch\n");
		if (p && p!=&base) delete p;
		p = v;
		return *p;
	}
	cSMSVar & operator = (cSMSVar & v) {
		return *p;
	}
	cSMSVar & operator = (CStdString & val) {return *p = val;}
	cSMSVar & operator = (string & val) {return *p = val;}
	cSMSVar & operator = (int val) {return *p = val;}
	operator int() {return (int)*p;}
	operator CStdString() {return (CStdString)*p;}

};
cSMSVar cSMSVarContainer::base = cSMSVar();

class cSMSVar_int: public cSMSVar {
private:
	int _val;
public:
	cSMSVar_int() { _val = 0; }
	cSMSVar_int(CStdString & val) { *this=val; }
	cSMSVar_int(string & val) {*this=val;}
	cSMSVar_int(int val) { _val=val; }
	cSMSVar & operator = (string & val) {
		return *this = (CStdString&)val;
	}
	cSMSVar & operator = (CStdString & val) {
		_val = atoi(val.c_str());
		return *this;
	}
	cSMSVar & operator = (int val) {
		_val = val;
		return *this;
	}
	operator int() {
		return _val;
	}
	operator CStdString(){
		CStdString r;
		itoa(_val , r.GetBuffer(60) , 10);
		r.ReleaseBuffer();
		return r;
	}
};
class cSMSVar_string: public cSMSVar {
private:
	CStdString _val;
public:
	cSMSVar_string() { _val = ""; }
	cSMSVar_string(CStdString & val) { _val=val; }
	cSMSVar_string(string & val) {_val = val;}
	cSMSVar_string(int val) { *this=val; }
	cSMSVar & operator = (string & val) {
		return *this = (CStdString&)val;
	}
	cSMSVar & operator = (CStdString & val) {
		_val = val;
		return *this;
	}
	cSMSVar & operator = (int val) {
		itoa(val , _val.GetBuffer(60) , 10);
		_val.ReleaseBuffer();
		return *this;
	}
	operator int() {
		return atoi(_val);
	}
	operator CStdString(){
		return _val;
	}
};


struct cSMSGate {
	bool enabled;
    cCriticalSection_ cSection;

    CStdString xmlFile; // xml
    CStdString id;

    CStdString name;
    int iconIndex;
    CStdString icon;
//    CStdString author;
//    CStdString info;

    CStdString acceptNumber; // przyjmowane numery
    CStdString scriptFile; // sciezka do skryptu

//    CStdString sigInfo; // Informacja do sygnaturki
//    unsigned sigEnabled : 1;
//    int maxChars;
//    int maxSendCount;
//    int maxParts;
//    int maxCharsOnServer;
    deque <cSMSGateParam> params;
    typedef deque <cSMSGateParam>::iterator params_it_t;
    map <CStdString,cSMSVarContainer> vars;

//    int usageCount; // Ilosc uzyc
//    int sentCount;

    int configGroup;

    cSMSGate(string fileName);
    ~cSMSGate();

    void configStart();
    void configMake();
    void configRegister();
    void configSet();
    void send(CStdString number , CStdString body , CStdString ext , int winID);
    unsigned int getMaxChars();

    void msgSend(cMessage * m);

    static unsigned int __stdcall msgSendThread(void * param);

    static bool compare(cSMSGate * a , cSMSGate * b) {return a->vars["usageCount"] > b->vars["usageCount"];}
};

struct cSMSWindow { // Okno wysylania
    void setStatus(string status , int part = 1);
    void setInfo(cSMSInfo nfo);
    void open(unsigned int cnt = 0);
    void send();

    void gateChosen();
    void numberChanged();
    void editChanged();
    void numberSelected();

    void fillNrList();
    void saveNrList(CStdString current);

    void setGateInfo(string info , int iconID , string iconFile);
    static INT_PTR CALLBACK SMSWindowProc(HWND hwnd , UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK StatusComboProc(HWND hwnd , UINT message, WPARAM wParam, LPARAM lParam);

    static WNDPROC _stdComboProc;

    int predictedCount;
    vector <cSMSGate *> gates;
    cSMSGate * selected;
    cSMSWindow(unsigned int cnt = 0);
    ~cSMSWindow();
    HWND _hwnd;
    int id;
    HWND _statusWnd;
    CStdString _last;
};

WNDPROC cSMSWindow::_stdComboProc = 0;

typedef vector <cSMSGate*>::iterator gate_it_t;  // bramki xmlowe
typedef vector <cSMSWindow*>::iterator wnd_it_t;  // bramki xmlowe

struct cSMS { // G³ówna struktura
	int currentGroupIndex;
    cCriticalSection cSection;
    cSMSEnv env;
    vector <cSMSGate*> gate;  // bramki xmlowe
    vector <cSMSWindow*> wnd; // okna
    HIMAGELIST iml;
    map <int , int> icon; // Indexy icon

    void UIset(); // ustawia cache'owane wartoœci
    void prepare(); // laduje SET'y
    void UIprepare();
    void CFGprepare();
    void CFGset();

    void setInfo(cSMSInfo & nfo);
	HWND getHWND(int winID);

    void saveUsage();

    cSMSGate * findGate(string id);

    void openWnd(unsigned int cnt);
    cSMS();
    ~cSMS();

    
} SMS;
// -----------------------------------------------------

void cSMSGateParam::addParam(string param) {
    cXML XML;
    XML.loadSource(param.c_str());
    XML.prepareNode("param" , true);
    this->text = XML.getText();
    this->name = XML.getAttrib("name");
    if (this->name.empty()) throw "Niepe³na specyfikacja parametru bramki!";
    this->def = XML.getAttrib("default");
    this->type = XML.getAttrib("type");
    this->tip = XML.getAttrib("tip");
    this->nameID = SMS_CFGID + this->owner->id + '_' + this->name;
    this->xmlNode = param;
}
void cSMSGateParam::configAdd() {
    CStdString tip = this->tip.empty()? "" : CFGTIP + this->tip;
    if (this->type == "string") {
        this->owner->configStart();
		UIActionAdd(owner->configGroup , 0 , ACTT_EDIT | (this->text.empty() ? ACTSC_FULLWIDTH : ACTSC_INLINE) , tip , this->ID);
		if (!this->text.empty())
			UIActionAdd(owner->configGroup , 0 , ACTT_COMMENT , this->text.c_str());
    } else if (this->type == "int") {
        this->owner->configStart();
        UIActionAdd(owner->configGroup , 0 , ACTT_EDIT | ACTSC_INLINE | ACTSC_INT , tip , this->ID, 30);
        UIActionAdd(owner->configGroup , 0 , ACTT_COMMENT , this->text.c_str());
    } else if (this->type == "pass") {
        this->owner->configStart();
        UIActionAdd(owner->configGroup , 0 , ACTT_PASSWORD | ACTSC_INLINE , tip , this->ID);
        UIActionAdd(owner->configGroup , 0 , ACTT_COMMENT , this->text.c_str());
    } else if (this->type == "bool") {
        this->owner->configStart();
        UIActionAdd(owner->configGroup , 0 , ACTT_CHECK , this->text + tip , this->ID);
    } else if (this->type == "text") {
        this->owner->configStart();
        UIActionAdd(owner->configGroup , 0 , ACTT_COMMENT , this->text.c_str());
        UIActionAdd(owner->configGroup , 0 , ACTT_TEXT , tip , this->ID);
    } else if (this->type == "info") {
        this->owner->configStart();
		UIActionAdd(owner->configGroup , 0 , ACTT_INFO | ACTSC_FULLWIDTH , SetActParam(this->text,AP_MINWIDTH, "350").c_str());
    } else if (this->type == "html") {
        this->owner->configStart();
		CStdString txt = this->text;
		txt.Replace("\n", "<br/>");
		UIActionAdd(owner->configGroup , 0 , ACTT_HTMLINFO | ACTSC_FULLWIDTH , SetActParam(txt,AP_MINWIDTH, "350").c_str());
    } else if (this->type == "session") {

    } else if (this->type == "hidden") {

    } else {
        this->owner->configStart();
        UIActionAdd(owner->configGroup , 0 , ACTT_COMMENT , (this->name + " - nieznany typ").c_str());
    }

}
void cSMSGateParam::configRegister() {
	if (this->type == "info") return;
    int type = DT_CT_PCHAR|DT_CF_CXOR;
    if (this->type == "session") type|=DT_CF_NOSAVE;
	if (this->type == "pass") type|=DT_CF_SECRET;
    sSETCOL sc(-1 , type , (int)this->def.c_str() , this->nameID.c_str());
    this->ID = ICMessage(IMC_CFG_SETCOL , (int)&sc);    
}

/* ________________________ */
cSMSGate::cSMSGate(string fileName) {
    string dir = SMS_DIR;
    configGroup = 0;
    cXML XML;
    XML.loadFile(fileName.c_str());
    this->xmlFile = fileName;
    this->id = XML.getText("sms/id");
    this->name = XML.getText("sms/name");
	this->vars["author"] = new cSMSVar_string(XML.getText("sms/author"));
    this->vars["info"] = new cSMSVar_string(XML.getText("sms/info"));
    this->icon = XML.getText("sms/logo");
    if (!this->icon.empty()) this->icon = dir + this->icon;
    this->iconIndex = ImageList_AddIcon(SMS.iml , (HICON)LoadImage(0 , this->icon.c_str() , IMAGE_ICON , 16 , 16 , LR_LOADFROMFILE));
    this->acceptNumber = XML.getText("sms/acceptNumber");
    if (this->acceptNumber.size() > 1 && this->acceptNumber[0]!='/')
        this->acceptNumber = "/^" + this->acceptNumber + "$/i";
    this->scriptFile = dir + XML.getText("sms/scriptFile");
    this->vars["maxChars"] = new cSMSVar_int(XML.getText("sms/maxChars"));
    this->vars["maxCharsOnServer"] = new cSMSVar_int(XML.getText("sms/maxCharsOnServer"));
    this->vars["maxParts"] = new cSMSVar_int(XML.getText("sms/maxParts"));
    this->vars["maxSendCount"] = new cSMSVar_int(XML.getText("sms/maxSendCount"));
    this->vars["sigInfo"] = new cSMSVar_string(XML.getText("sms/signature"));
    XML.prepareNode("sms/signature");
    this->vars["sigEnabled"] = new cSMSVar_int(XML.getAttrib("disabled")!=""? 0 : 1);
    if (this->id.empty()) throw "Bramka nie ma identyfikatora!";
    cPreg Preg;
    Preg.match(this->acceptNumber , "");
    if (Preg.lastError && *Preg.lastError) throw "Pole acceptNumber \""+string(this->acceptNumber)+"\" zawiera b³¹d: " + Preg.lastError;

	this->vars["usageCount"] = new cSMSVar_int(0);
	this->vars["sentCount"] = new cSMSVar_int(0);

    if (!this->vars["maxParts"]) this->vars["maxParts"] = 1;
    if (this->vars["maxParts"]==1 && !this->vars["maxCharsOnServer"]) this->vars["maxCharsOnServer"] = this->vars["maxChars"];
	if (!this->vars["maxChars"]) this->vars["maxChars"] = 1000;

    string par = XML.getContent("sms/params");
    if (!par.empty()) {
        cXML XML;
        XML.loadSource(par.c_str());
        string node;
        do {
            node = XML.getNode("param");
            XML.next();
            if (!node.empty()) {
                cSMSGateParam param;
                param.owner = this;
                try {
                    param.addParam(node);
                    this->params.push_back(param);
                } catch (char * c) {}
            }
        } while (!node.empty());
        
    }

    SMS.gate.push_back(this); // Rejestrujemy sie w kolejce
}

cSMSGate::~cSMSGate() {
}

void cSMSGate::configStart() {
    if (this->configGroup) return;
	this->configGroup = SMS.currentGroupIndex++;
	UIGroupAdd(IMIG_SMS , this->configGroup , 0 , this->name.c_str() , UIIcon(IT_MESSAGE , NET_NONE , MT_SMS , 0));
    //UIActionAdd(this->configGroup , 0 , ACTT_GROUP , this->name.c_str());

	UIActionAdd(this->configGroup , 0 , ACTT_GROUP , "");

	if (this->icon.empty() == false) {
		UIActionAdd(this->configGroup , 0 , ACTT_IMAGE | ACTSC_INLINE , ("file://" + this->icon).c_str(), 0, 32, 32);
	}
	CStdString info = this->vars["htmlInfo"];
	if (info.empty())
		info = EncodeEntities(this->vars["info"]);
	info += "<br/><br/>Wersja:\t<b>";
	info += EncodeEntities(this->vars["version"]);
	info += "</b><br/>Autor:\t<b>";
	info += EncodeEntities(this->vars["author"]);
	info += "</b>   ";
	info += EncodeEntities(this->vars["authorUrl"]);
	if (ShowBits::checkLevel(ShowBits::levelAdvanced)) {
		info += "<br/>Lokalizacja:\t";
		info += EncodeEntities(this->xmlFile);
		info += "<br/>Skrypt:\t";
		info += EncodeEntities(this->scriptFile);
		info += "<br/>Numery:\t";
		info += EncodeEntities(this->acceptNumber);
		info += "<br/>Znaków:\t";
		info += EncodeEntities(this->vars["maxChars"]);
	}
	
	UIActionAdd(this->configGroup , 0 , ACTT_HTMLINFO , info.c_str(), 0, 320);
	UIActionAdd(this->configGroup , 0 , ACTT_GROUPEND , "");
    UIActionAdd(this->configGroup , 0 , ACTT_GROUP , "");
}
void cSMSGate::configRegister(){
	CStdString id =  SMS_CFGENABLEDID + this->id;
	sSETCOL sc(-1 , DT_CT_INT , 1 , id);
    ICMessage(IMC_CFG_SETCOL , (int)&sc);    

    for (unsigned int i=0; i<params.size(); i++) {
        params[i].configRegister();
    }
}
void cSMSGate::configSet(){
	enabled = GETINT(Ctrl->DTgetNameID(DTCFG , SMS_CFGENABLEDID + this->id))?true:false;
}
void cSMSGate::configMake(){
    // Wczytujemy gateUsage
    this->vars["usageCount"] = 0;
    string usage = GETSTR(CFG_SMS_GATEUSAGE);
    usage = "\n" + usage;
    if (!usage.empty()) {
        int found = usage.find("\n" + this->id + "=");            
        if (found != usage.npos) {
            this->vars["usageCount"] = atoi(usage.substr(found + id.length() + 2 , 10).c_str());
        }
    }
    for (unsigned int i=0; i<params.size(); i++) {
        params[i].configAdd();
    }
    if (!this->configGroup) return;
//    UIActionAdd(IMIG_SMS_GATE , 0 , ACTT_GROUPEND);

}

unsigned int cSMSGate::getMaxChars() {
    return vars["maxChars"] * vars["maxParts"] - ((vars["maxParts"]>1)?vars["maxParts"]*SMS_PART_LENGTH:0);
}

void cSMSGate::send(CStdString number , CStdString body , CStdString ext , int winID){
	int sigCount = GetExtParam(ext , Sms::extFrom).size();
	ext = SetExtParam(ext , Sms::extGate , this->id);
	ext = SetExtParam(ext , Sms::extWindowID , inttoch(winID));
    cMessage m;
    sMESSAGESELECT ms;
    m.net = NET_SMS;
    m.type = MT_SMS;
    m.fromUid = "";
    m.body = (char*)body.c_str();
    m.toUid = (char*)number.c_str();
    m.ext = (char*)ext.c_str();
    m.flag = MF_SEND;
    m.time = cTime64(true);

    sHISTORYADD ha;
    ha.m = &m;
    ha.dir = SMS_HISTORY_DIR;
    ha.session = 0;
    ha.cnt = 0;
    // Szukamy kontaktu powiazanego z numerem
    int c = ICMessage(IMC_CNT_COUNT);
    for (int j = 0; j<c; j++) {
        if (number == GETCNTC(j , CNT_CELLPHONE)) ha.cnt = j;
    }
    if (!ha.cnt) ha.name = number;

    int parts = ((body.length() + sigCount) / (vars["maxChars"]+1)) + 1;
    if (parts > 1)
        parts = ((body.length() + parts*sigCount + parts*SMS_PART_LENGTH) / (vars["maxChars"]+1))+1;

    unsigned int pos = 0;
    int i = 1;
    while (pos < body.length()) {
		ext = SetExtParam(ext , Sms::extPart , inttoch(i));
		m.ext = (char*)ext.c_str();
        CStdString msg;
        if (parts>1) msg = '(' + string(inttoch(i)) + '/' + string(inttoch(parts)) + ')';
        int msgLength = msg.length();
        msg += body.substr(pos , vars["maxChars"] - msgLength - sigCount);
        pos += vars["maxChars"] - msgLength - sigCount;
        m.body = (char*)msg.c_str();
        ms.id = ICMessage(IMC_NEWMESSAGE , (int)&m);
        if (ms.id) ICMessage(IMC_MESSAGEQUEUE , (int)&ms);
        ICMessage(IMI_HISTORY_ADD , (int)&ha);
        ha.session = 1; // Wszystkie nastepne zostana zapisane w sesji...
        i++;
        if (parts==1) pos = body.length();
    }

    // Ustawiamy usageCount na najwyzszy
    for (gate_it_t gate = SMS.gate.begin(); gate!=SMS.gate.end(); gate++) {
        if ((*gate)->vars["usageCount"] >= this->vars["usageCount"]) this->vars["usageCount"] = (*gate)->vars["usageCount"]+1;
    }
    this->vars["sentCount"] = this->vars["sentCount"] + parts;
}

struct cSMSGateThread {
    cSMSGate * gate;
    cMessage * m;
};

unsigned int __stdcall cSMSGate::msgSendThread(void * param) {
    cSMSGateThread * thread = (cSMSGateThread*)param;
    // Blokujemy dostêp tylko do JEDNEJ bramki!
    thread->gate->cSection.lock();
    string digest = string(thread->m->body).substr(0,10);
    sMESSAGESELECT ms;
    ms.id = thread->m->id;
    cSMSSend SS(&SMS.env , thread->gate);
	SS.winID = atoi(GetExtParam(thread->m->ext , Sms::extWindowID).c_str());
	SS.from = GetExtParam(thread->m->ext , Sms::extFrom);
    SS.msg = thread->m->body;
    SS.msgID = thread->m->id;
    SS.to = thread->m->toUid;
    SS.XMLFile = thread->gate->xmlFile;
    SS.scriptFile = thread->gate->scriptFile;

    cSMSInfo info("" , thread->gate , digest , SS.winID);

    SMS.setInfo(info.reuse("Wysy³anie rozpoczête" , SIT_START));

    for (params_it_t param = thread->gate->params.begin(); param!=thread->gate->params.end(); param++) {
        SS.params[param->name] = GETSTR(param->ID);
    }
    sendSMS_byLUA(&SS);
    if (!SS.result) { // wszystko OK!
        SMS.setInfo(info.reuse("Wys³a³em, " + SS.errorMsg , SIT_FINISH));
        ICMessage(IMC_MESSAGEREMOVE , (int)&ms);
    } else {
        SMS.setInfo(info.reuse(SS.errorMsg, SIT_ERROR));
        if (GETINT(CFG_SMS_DELMSG_ONFAILURE))
            ICMessage(IMC_MESSAGEREMOVE , (int)&ms);
        else
            ICMessage(IMC_MESSAGEPROCESSED , ms.id);
    }
    // Zapisuje parametry
/*    for (params_it_t param = thread->gate->params.begin(); param!=thread->gate->params.end(); param++) {
        SETSTR(param->ID , SS.params[param->name].c_str());
    }
*/
    size_t GCcount = SS.GC.count(0);
    if (GCcount) IMLOG("SMS::%s.GarbageCollection: %d items in %d collections" , thread->gate->name.c_str()  , GCcount , SS.GC.count(1)-1);
    messageFree(thread->m);
    thread->gate->cSection.unlock();
    delete thread;
    return 0;
}


void cSMSGate::msgSend(cMessage * m) {
    cSMSGateThread * thread = new cSMSGateThread;
    thread->gate = this;
    thread->m = m;
//    int threadID;
	CloseHandle(Ctrl->BeginThread(0 , 0 , msgSendThread , (void*)thread , 0 , 0));
}


/*_______________________*/

void cSMS::prepare() {
    HICON ico;               
    HINSTANCE test = GetModuleHandle("ui.dll");
    ico = LoadIcon(GetModuleHandle("ui.dll") , "BLANK");
    ImageList_AddIcon(iml ,  ico);
    DestroyIcon(ico);
    ico = LoadIcon(Ctrl->hDll() , MAKEINTRESOURCE(IDI_OVR_ERROR));
    ImageList_AddIcon(iml ,  ico);
    DestroyIcon(ico);
    ico = LoadIcon(Ctrl->hDll() , MAKEINTRESOURCE(IDI_OVR_WARN));
    ImageList_AddIcon(iml ,  ico);
    DestroyIcon(ico);
    ico = LoadIcon(Ctrl->hDll() , MAKEINTRESOURCE(IDI_OVR_START));
    ImageList_AddIcon(iml ,  ico);
    DestroyIcon(ico);
    ico = LoadIcon(Ctrl->hDll() , MAKEINTRESOURCE(IDI_OVR_FINISH));
    ImageList_AddIcon(iml ,  ico);
    DestroyIcon(ico);
    ImageList_SetOverlayImage(iml , 1 , 1);
    ImageList_SetOverlayImage(iml , 2 , 2);
    ImageList_SetOverlayImage(iml , 3 , 3);
    ImageList_SetOverlayImage(iml , 4 , 4);
    icon[IDI_OVR_ERROR] = 1;
    icon[IDI_OVR_WARN] = 2;
    icon[IDI_OVR_START] = 3;
    icon[IDI_OVR_FINISH] = 4;

    ico = LoadIcon(Ctrl->hDll() , MAKEINTRESOURCE(IDI_SMS_LOGO));
    icon[IDI_SMS_LOGO] = ImageList_AddIcon(iml ,  ico);
    DestroyIcon(ico);


    // Wczytujemy po kolei wszystkie XMLe
    WIN32_FIND_DATA fd;
    HANDLE hFile;
    BOOL found;
    string dir = string(SMS_DIR) + "*.xml";
    found = ((hFile = FindFirstFile(dir.c_str(),&fd))!=INVALID_HANDLE_VALUE);
    while (found) {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            cSMSGate * tmp;
            try {
                tmp = new cSMSGate(SMS_DIR + string(fd.cFileName)); // Tworzymy obiekt bramki, ktory juz sam sie zarejestruje
            } 
            catch (char * c) {
                ICMessage(IMI_ERROR , (int)(fd.cFileName + string(" spowodowa³ b³¹d: ") + c).c_str());
            }
            catch (string s) {
                ICMessage(IMI_ERROR , (int)(fd.cFileName + string(" spowodowa³ b³¹d: ") + s).c_str());
            }
        }
        found = FindNextFile(hFile , &fd);
    }
    FindClose(hFile);   
}
void cSMS::UIprepare() {
	CFGset();
	// Wstawiamy opcje w³¹czania/wy³¹czania bramek
    UIActionAdd(IMIG_SMS_GATE , 0 , ACTT_GROUP , "Aktywne bramki");
    for (unsigned int i = 0; i<gate.size(); i++) {
		UIActionAdd(IMIG_SMS_GATE , 0 , ACTT_CHECK , gate[i]->name , Ctrl->DTgetNameID(DTCFG , SMS_CFGENABLEDID + gate[i]->id));
        gate[i]->configMake();
    }
	UIActionAdd(IMIG_SMS_GATE , 0 , ACTT_GROUPEND , "");
}
void cSMS::CFGprepare() {
    for (unsigned int i = 0; i<gate.size(); i++) {
        gate[i]->configRegister();
    }
}
void cSMS::CFGset() {
    for (unsigned int i = 0; i<gate.size(); i++) {
        gate[i]->configSet();
    }
}
void cSMS::UIset() {

}

cSMS::cSMS() {
    iml = ImageList_Create(16 , 16 , ILC_COLOR32|ILC_MASK , 5 , 5);
	currentGroupIndex = 14100;
}              

cSMS::~cSMS() {
    while(wnd.size()) {
		DestroyWindow(wnd.back()->_hwnd);
	}
    while(gate.size()) {
		delete gate.back(); gate.pop_back();
	}
}

void cSMS::openWnd(unsigned int cnt) {
    if (GETINT(CFG_SMS_REUSEWINDOW)) {
        if (this->wnd.size()) {wnd[0]->open(cnt);return;}
    }
    new cSMSWindow(cnt);
}

HWND cSMS::getHWND(int winID) {
	if (!wnd.size()) return (HWND)UIGroupHandle(sUIAction(0 , IMIG_MAINWND));
    for (unsigned int i=0; i<wnd.size(); i++)
        if (winID == -1 || wnd[i]->id == winID) {
	        return wnd[i]->_hwnd;
        }
}

void cSMS::setInfo(cSMSInfo & nfo){
    int count = 0;
    for (unsigned int i=0; i<wnd.size(); i++)
        if (nfo.winID == -1 || wnd[i]->id == nfo.winID) {
            wnd[i]->setInfo(nfo);
            count++;
        }
    if (nfo.winID != -1 && count==0) {nfo.winID = -1; setInfo(nfo);}
}

void cSMS::saveUsage() {
    string usage = "";
    for (unsigned int i=0; i<gate.size(); i++)
        usage += gate[i]->id + "=" + string(inttoch(gate[i]->vars["usageCount"])) + "\n";
    SETSTR(CFG_SMS_GATEUSAGE , usage.c_str());
}


cSMSGate * cSMS::findGate(string id) {
    for (unsigned int i=0; i<gate.size(); i++)
        if (gate[i]->id == id) return gate[i];
    return 0;        
}

/*_____________________*/

void cSMSSend::setInfo(CStdString msg , SMSInfoType type) {
    SMS.cSection.lock();
    string digest = this->msg.substr(0,10);
    IMLOG(("SMS::" + this->gate->id + "[" + digest + "].info = " + msg).c_str());
    SMS.setInfo(cSMSInfo(msg , this->gate , digest , this->winID , type));
    SMS.cSection.unlock();
}
void cSMSSend::setError(CStdString msg) {
    SMS.cSection.lock();
    IMLOG(("SMS::" + this->gate->id + "["+this->msg.substr(0,10)+"].ERROR = " + msg).c_str());
    this->errorMsg = msg;
    this->result = SMS_RESULT_UNSENT;
    SMS.cSection.unlock();
}
void cSMSSend::setScriptError(CStdString msg) {
    SMS.cSection.lock();
    IMLOG(("SMS::" + this->gate->id + "["+this->msg.substr(0,10)+"].scriptError = " + msg).c_str());
    SMS.cSection.unlock();
    MessageBox(0 , gate->scriptFile + " spowodowa³ b³¹d:\r\n\r\n" + msg , "" , MB_ICONERROR | MB_OK);
    this->errorMsg = msg;
    this->result = SMS_RESULT_SCRIPTERROR;
}
void cSMSSend::setSuccess(CStdString msg) {
    SMS.cSection.lock();
    IMLOG(("SMS::" + this->gate->id + "["+this->msg.substr(0,10)+"].success = " + msg).c_str());
    this->errorMsg = msg;
    this->result = 0;
    SMS.cSection.unlock();
}
void cSMSSend::LOG(CStdString msg) {
    SMS.cSection.lock();
    IMLOG("LUAsays: \"%s\"" , msg.c_str());
    SMS.cSection.unlock();
}
bool cSMSSend::setParam(CStdString name , CStdString value) {
    SMS.cSection.lock();
    int id = Ctrl->DTgetNameID(DTCFG , SMS_CFGID + gate->id + '_' + name);
    if (id == -1) return false;
    SETSTR(id , value);
    SMS.cSection.unlock();
    return true;
}
HWND cSMSSend::getHWND(int winID) {
	return SMS.getHWND(winID);
}

bool cSMSSend::setVar(CStdString name , CStdString value) {
    SMS.cSection.lock();
	if (gate->vars.find(name)!=gate->vars.end())
		gate->vars[name] = value;
	else 
		gate->vars[name] = new cSMSVar_string(value);
    SMS.cSection.unlock();
    return true;
}
CStdString cSMSSend::getVar(CStdString name) {
    SMS.cSection.lock();
    CStdString result = (gate->vars.find(name)!=gate->vars.end())? (CStdString)gate->vars[name] : "";
    SMS.cSection.unlock();
    return result;
}
bool cSMSSend::eraseVar(CStdString name) {
    SMS.cSection.lock();
    bool result = true;
    if (gate->vars.find(name)==gate->vars.end() || gate->vars[name]->protect) result = false;
    else gate->vars.erase(name);
    SMS.cSection.unlock();
    return result;
}



void cSMSSend::alert(CStdString txt , CStdString title) {
    MessageBox(0 , txt , title , MB_ICONWARNING | MB_OK);
}
bool cSMSSend::confirm(CStdString txt, CStdString title) {
    return MessageBox(0 , txt , title , MB_ICONQUESTION | MB_YESNO) == IDYES;
}

CStdString cSMSSend::gettoken(CStdString info, CStdString file) {
	sDIALOG_token tk;
	tk.info = info;
	tk.title = "SMS - Pobierz TOKEN";
	file = "file://" + file;
	tk.imageURL = file;
	ICMessage(IMI_DLGTOKEN , (int) &tk);
    return tk.token;
}

void cSMSSend::refreshStatus() {
	for (int i=0; i<SMS.wnd.size(); i++)
		SMS.wnd[i]->editChanged();
}


/*_____________________*/



void cSMSWindow::setStatus(string status , int part) {
    SendMessage(_statusWnd , SB_SETTEXT , part , (LPARAM)status.c_str());
    SendMessage(_statusWnd , SB_SETTIPTEXT , part , (LPARAM)status.c_str());
}

void cSMSWindow::setInfo(cSMSInfo nfo) {
    //SendMessage(_statusWnd , SB_SETTEXT , part , (LPARAM)status.c_str());
    COMBOBOXEXITEM cbi;
    memset(&cbi , 0 , sizeof(cbi));
    cbi.mask = CBEIF_IMAGE|CBEIF_INDENT|CBEIF_OVERLAY|CBEIF_SELECTEDIMAGE|CBEIF_TEXT;
    cbi.iItem = -1;
    CStdString text="";
    if (!nfo.digest.empty()) {
        if (nfo.digest.size() > 8) nfo.digest.erase(8);
        if (nfo.digest.find_first_of("\r\n")!=-1) nfo.digest.erase(nfo.digest.find_first_of("\r\n"));
        text+=" [" + nfo.digest + "] ";
    }
    text += nfo.info;
    cbi.iSelectedImage = cbi.iImage = nfo.gate? nfo.gate->iconIndex : SMS.icon[IDI_SMS_LOGO];
    cbi.iIndent = 0;
    cbi.iOverlay = (nfo.type == SIT_WARN)?SMS.icon[IDI_OVR_WARN] 
        :(nfo.type == SIT_ERROR)?SMS.icon[IDI_OVR_ERROR]
        :(nfo.type == SIT_START)?SMS.icon[IDI_OVR_START]
        :(nfo.type == SIT_FINISH)?SMS.icon[IDI_OVR_FINISH]
        :0;
    text.Replace('\r' , '\0');
    text.Replace('\n' , '\0');
    text.Replace('\t' , ' ');
    const char * ch = text.c_str();
    cbi.pszText = (LPSTR)ch;
#define SCB_MAXLEN 65
    char cut = 0;
    char * cutPos = 0;
    while (cbi.pszText && cbi.pszText < ch+text.size()) {
        if (*cbi.pszText) {
            if (strlen(cbi.pszText) > SCB_MAXLEN) {
                // Bedziemy szukaæ wœród ostatnich 15 znaków, wiec zerujemy ostatni znak z przedzialu
                cut = cbi.pszText[SCB_MAXLEN];
                cbi.pszText[SCB_MAXLEN] = 0;
                cutPos=strrchr(cbi.pszText + SCB_MAXLEN - 15 , ' ');
                if (!cutPos) {cutPos = cbi.pszText + SCB_MAXLEN;} // Ciachnêliœmy w dobrym miejscu
                else { // Tniemy na nowo...
                    cbi.pszText[SCB_MAXLEN] = cut;
                    cut = *cutPos;
                    *cutPos=0;
                }
            }
            int pos = SendDlgItemMessage(_hwnd , IDC_STATUS ,  CBEM_INSERTITEM , 0 , (LPARAM)&cbi);
            if (!cbi.iIndent) SendDlgItemMessage(_hwnd , IDC_STATUS , CB_SETCURSEL , pos ,0);
            cbi.iIndent = 1;
            cbi.iImage = cbi.iSelectedImage = cbi.iOverlay;
            cbi.mask &=~(CBEIF_OVERLAY);
        }
        if (cut && cutPos) {
            *cutPos=cut;
            cbi.pszText=cutPos;
            if (cut == ' ')
                cbi.pszText ++; // Omijamy uciêt¹ spacjê.
            cut = 0;
            cutPos = 0;
        } else
            cbi.pszText = strchr(cbi.pszText , 0)+1;
    }

}


void cSMSWindow::open(unsigned int cnt) {
    if (cnt) {
        const char * phone = GETCNTC(cnt , CNT_CELLPHONE);
        if (* phone) {SetWindowText(GetDlgItem(_hwnd , IDC_NUMBER) , phone);}

    }
    ShowWindow(_hwnd , SW_SHOW);
    SetForegroundWindow(_hwnd);
}
cSMSWindow::cSMSWindow(unsigned int cnt) {
    selected = 0;
    _hwnd = CreateDialogParam(Ctrl->hDll() , MAKEINTRESOURCE(IDD_SMS) , 0 , SMSWindowProc , (LPARAM)this);
    id = (int) _hwnd;
    SendMessage(GetDlgItem(_hwnd , IDC_GATE) , CBEM_SETIMAGELIST , 0 , (LPARAM)SMS.iml);
    SendMessage(GetDlgItem(_hwnd , IDC_NUMBER) , CBEM_SETIMAGELIST , 0 , (LPARAM)ICMessage(IMI_GETICONLIST , IML_16));
    SendMessage(GetDlgItem(_hwnd , IDC_STATUS) , CBEM_SETIMAGELIST , 0 , (LPARAM)SMS.iml);
    HWND item = GetDlgItem(_hwnd , IDC_NUMBER);

    // Wypelnia liste numerow
    fillNrList();
    // Tworzy status
    RECT rc , rcs;
    _statusWnd = CreateWindow(STATUSCLASSNAME , "Gotowe" , SBARS_TOOLTIPS | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0 , 0 , 0 , 0 , _hwnd , 0 , 0 , 0);
    GetClientRect(_statusWnd , &rcs);
    GetWindowRect(_hwnd , &rc);
    // Powiekszamy okienko o wysokosc status barrrrra
    SetWindowPos(_hwnd , 0 , 0 , 0 , rc.right - rc.left , rc.bottom - rc.top + rcs.bottom , SWP_NOZORDER|SWP_NOMOVE);
    SendMessage(_statusWnd , WM_SIZE , 0 , 0);
    int sbw [3]={200 , -1};
    SendMessage(_statusWnd , SB_SETPARTS , 2 , (LPARAM)sbw);
    // Wczytuje podpis
    SetDlgItemText(_hwnd , IDC_MYSIG , GETSTR(CFG_SMS_SIG));
    CheckDlgButton(_hwnd , IDC_SAVESIG , GETINT(CFG_SMS_SIG_SAVE)?BST_CHECKED:BST_UNCHECKED);
    open(cnt);
    SMS.wnd.push_back(this); // Rejestrujemy sie w kolejce
    gateChosen();
    item = GetDlgItem(_hwnd , IDC_STATUS);
    SendDlgItemMessage(_hwnd , IDC_STATUS , CBEM_SETEXTENDEDSTYLE , CBES_EX_NOSIZELIMIT , CBES_EX_NOSIZELIMIT);
    GetChildRect(item , &rc);
    SetWindowPos(item , HWND_TOP 
        , rc.left , rc.top+(rcs.bottom + rc.bottom - rc.top - 4)/2+4 , rc.right - rc.left , rc.bottom - rc.top -4 , 0);
    _stdComboProc = (WNDPROC) SetWindowLong(item , GWL_WNDPROC , (LONG)StatusComboProc); 

    item = (HWND)SendMessage(item , CBEM_GETCOMBOCONTROL , 0 , 0);
    GetChildRect(item , &rc);
    SetWindowPos(item , 0 , rc.left-2 , rc.top-2 , rc.right-rc.left+3 , rc.bottom-rc.top+4 , SWP_NOZORDER);
    // Wrzucamy krótkie mi³e info :)
    setInfo(cSMSInfo("Status przesy³anych SMSów" , SIT_INFO , this->id));
}
cSMSWindow::~cSMSWindow(){
    SMS.wnd.erase(find(SMS.wnd.begin() , SMS.wnd.end() , this)); // Rejestrujemy sie w kolejce
}


void cSMSWindow::gateChosen(){
    COMBOBOXEXITEM cbi;
    memset(&cbi , 0 , sizeof(cbi));
    cbi.iItem = SendMessage(GetDlgItem(_hwnd , IDC_GATE), CB_GETCURSEL , 0 , 0);
    cbi.mask = CBEIF_LPARAM;
    if (cbi.iItem != CB_ERR) SendMessage(GetDlgItem(_hwnd , IDC_GATE) , CBEM_GETITEM , 0 , (LPARAM)&cbi);
    selected = cbi.iItem!=CB_ERR?(cSMSGate*)cbi.lParam : 0;
    if (!selected) {
        this->setGateInfo("Podaj numer telefonu i wybierz bramkê. \r\nPowinieneœ te¿ wype³niæ jedno z pól podpisu." , (int)IDI_WARNING , "");
        editChanged();
        return;
    }
    string info = selected->name + "  " + (CStdString)selected->vars["info"] + "\r\n  by: " + (CStdString)selected->vars["author"];
    setGateInfo(info , (int)IDI_INFORMATION , selected->icon);
    SendDlgItemMessage(_hwnd , IDC_SMS , EM_SETLIMITTEXT , selected->getMaxChars() , 0);
    EnableWindow(GetDlgItem(_hwnd , IDC_MYSIG) , selected->vars["sigEnabled"]);
    EnableWindow(GetDlgItem(_hwnd , IDC_SAVESIG) , selected->vars["sigEnabled"]);
    SetWindowText(GetDlgItem(_hwnd , IDC_SIGGROUP) , ((CStdString)selected->vars["sigInfo"]).empty()?"Podpis" : (CStdString)selected->vars["sigInfo"]);
    int check = SendDlgItemMessage(_hwnd , IDC_SMS , EM_GETLIMITTEXT , 0 , 0);
    editChanged();
}
void cSMSWindow::numberChanged(){
    CStdString number;
    COMBOBOXEXITEM cbi;
    memset(&cbi , 0 , sizeof(cbi));
    GetWindowText(GetDlgItem(_hwnd , IDC_NUMBER) , number.GetBuffer(200) , 200);
    number.ReleaseBuffer();
    // Zapamietujemy, ktora jest teraz wybrana
    HWND item = GetDlgItem(_hwnd , IDC_GATE);
    cbi.iItem = SendMessage(item, CB_GETCURSEL , 0 , 0);
    cbi.mask = CBEIF_LPARAM;
    if (cbi.iItem != CB_ERR) SendMessage(item , CBEM_GETITEM , 0 , (LPARAM)&cbi);
    selected = cbi.iItem!=CB_ERR?(cSMSGate*)cbi.lParam : 0;
                                              
    // Sprawdzamy, czy wsrod bramek na liscie wszystkie spelniaja kryteria.
    bool refill = false;
    cPreg Preg;
    Preg.setSubject(number);
/*    if (number.empty() || !gates.size()) refill = true;
    else {
        for (gate_it_t gates_it = gates.begin(); gates_it != gates.end(); gates_it++) {
            if (!Preg.match((*gates_it)->acceptNumber)) {refill = true; break;}
        }
    }
*/    
//    if (!refill) return;
//    gates.clear(); // Czyscimy liste bramek.
    // Aktualizujemy liste bramek
    if (!number.empty()) {
        for (gate_it_t gate_it = SMS.gate.begin(); gate_it != SMS.gate.end(); gate_it++) {
            bool matched =  (*gate_it)->enabled && Preg.match((*gate_it)->acceptNumber);
            gate_it_t found = find(gates.begin() , gates.end() , *gate_it);
            if (matched && found==gates.end()) {gates.push_back(*gate_it);refill=true;continue;}
            if (!matched && found!=gates.end()) {gates.erase(found);refill=true;continue;}
        }
        sort(gates.begin() , gates.end() , cSMSGate::compare); // Sortujemy wg. uzywalnosci
    }
    // Wypelniamy prawdziwa liste
    SendMessage(item , CB_RESETCONTENT , 0 , 0);
    memset(&cbi , 0 , sizeof(cbi));
    cbi.iItem = -1;
    cbi.mask = CBEIF_TEXT | CBEIF_LPARAM | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
    for (gate_it_t gates_it = gates.begin(); gates_it != gates.end(); gates_it++) {
        cbi.iImage = cbi.iSelectedImage = (*gates_it)->iconIndex;
        cbi.pszText = (char*)(*gates_it)->name.c_str();
        cbi.lParam = (LPARAM)(*gates_it);
        SendMessage(item , CBEM_INSERTITEM , 0 , (LPARAM)&cbi);
    }
/*    if (selected) {
        gate_it_t gate = find(gates.begin() , gates.end() , selected);
        cbi.iItem = gate!=gates.end() ? gate - gates.begin() : 0;
    } else
    */
        cbi.iItem = 0;
    SendMessage(item , CB_SETCURSEL , cbi.iItem , 0);
    gateChosen();
}
void cSMSWindow::editChanged(){
    GetDlgItemText(_hwnd , IDC_MYNAME , TLS().buff , MAX_STRING);
    int countName = strlen(TLS().buff) + 1;
    GetDlgItemText(_hwnd , IDC_MYPHONE , TLS().buff , MAX_STRING);
    int countPhone = strlen(TLS().buff);
    GetDlgItemText(_hwnd , IDC_SMS , TLS().buff , MAX_STRING);
    int count = strlen(TLS().buff);
    EnableWindow(GetDlgItem(_hwnd , IDOK) , count>0 && selected);
    if (!selected)
        {setStatus("Wybierz operatora i wpisz wiadomoœæ" , 0);return;}
    int parts = 1;
    if (selected->vars["maxParts"] == 1) {
        parts = ((count + countPhone + countName) / (selected->vars["maxCharsOnServer"]+1))+1;
    } else {
        parts = ((count + countPhone + countName) / (selected->vars["maxChars"]+1))+1;
        if (parts>1)
            parts = ((count + parts*countName + parts*countPhone + parts*SMS_PART_LENGTH) / (selected->vars["maxChars"]+1))+1;
    }
    int countAdded = parts*countName + parts*countPhone + (parts>1?parts:0)*SMS_PART_LENGTH;
    count += countAdded;
    string info = inttoch(count , 10);
    info += "/";
    info += inttoch(selected->vars["maxChars"] * selected->vars["maxParts"] - ((selected->vars["maxParts"]>1)?selected->vars["maxParts"]*SMS_PART_LENGTH:0)) + string(" znaków. ");
    if (parts > 1) {
        info += inttoch(parts);
        if (selected->vars["maxParts"]>1) info += string("/") + inttoch(selected->vars["maxParts"]) + string(" smsów. ");
        else info+= " smsy. ";
    }
    if (selected->vars["maxSendCount"] > 0) {
        info += "Zosta³o ";
        info += inttoch(max(0,selected->vars["maxSendCount"] - selected->vars["sentCount"])) + string(" smsów. ");
    }
    predictedCount = count;

    SendDlgItemMessage(_hwnd , IDC_SMS , EM_LIMITTEXT , selected->getMaxChars() - countAdded , 0);

    setStatus(info , 0);
}

void cSMSWindow::setGateInfo(string info , int iconID , string iconFile) {
    HICON icon;
    if (iconFile.empty()) 
        icon = LoadIcon(0 , MAKEINTRESOURCE(iconID));
    else
        icon = (HICON)LoadImage(0 , iconFile.c_str() , IMAGE_ICON , 32 , 32 , LR_LOADFROMFILE);
    SetDlgItemText(_hwnd , IDC_INFO , info.c_str());
    icon = (HICON)SendDlgItemMessage(_hwnd , IDC_INFOICON , STM_SETICON , (WPARAM)icon , 0);
    if (icon) DestroyIcon(icon);
}

void cSMSWindow::send() {
    if (!selected) return;
    CStdString body;
    CStdString sig="";
    CStdString number;
    GetDlgItemText(_hwnd , IDC_SMS , body.GetBuffer(2000) , 2000);
    body.ReleaseBuffer();
    GetDlgItemText(_hwnd , IDC_NUMBER , number.GetBuffer(2000) , 2000);
    number.ReleaseBuffer();
    if (IsWindowEnabled(GetDlgItem(_hwnd , IDC_MYSIG))) {
        GetDlgItemText(_hwnd , IDC_MYSIG , sig.GetBuffer(600) , 600);
        sig.ReleaseBuffer();
    }

    if (predictedCount > selected->getMaxChars())
        if (MessageBox(_hwnd , "Przekroczy³eœ dopuszczaln¹ liczbê znaków. Popraw swoj¹ wiadomoœæ!" , "Wysy³anie SMSów" , MB_OKCANCEL | MB_ICONWARNING)==IDOK) return;
//    if (sig_name.empty() && sig_phone.empty())
//        if (MessageBox(_hwnd , "Powinieneœ podaæ jakiœ podpis!" , "Wysy³anie SMSów" , MB_OKCANCEL | MB_ICONWARNING)==IDOK) return;
    SETINT(CFG_SMS_SIG_SAVE , SendDlgItemMessage(_hwnd , IDC_SAVESIG , BM_GETCHECK , 0 , 0)==BST_CHECKED?1:0);
    if (GETINT(CFG_SMS_SIG_SAVE) && IsWindowEnabled(GetDlgItem(_hwnd , IDC_MYSIG))) {
        SETSTR(CFG_SMS_SIG , sig.c_str());
    }

	CStdString ext = SetExtParam("" , Sms::extFrom , sig);

    selected->send(number , body , ext , this->id);

    if (GETINT(CFG_SMS_DELETEBODY))
        SetDlgItemText(_hwnd , IDC_SMS , "");
    else {
        SendDlgItemMessage(_hwnd , IDC_SMS , EM_SETSEL , 0 , -1);
        SetFocus(GetDlgItem(_hwnd , IDC_SMS));
    }
    editChanged();
    saveNrList(number);
    if (selected->vars["maxSendCount"] > 0 && selected->vars["sentCount"] >= selected->vars["maxSendCount"]) {
        setGateInfo("Prawdopodobnie wykorzysta³eœ limit tej bramki.\r\nNastêpne SMSy mog¹ ju¿ dzisiaj nie zostaæ pomyœlnie wys³ane!" , (int)IDI_WARNING , "");
    }

}

#define SMS_MRU_NAME "SMS_contactList"
#define SMS_MRU_COUNT 50
#define SMS_BUFF_SIZE 2000
void cSMSWindow::fillNrList() {
    sMRU MRU;
    char buff [SMS_BUFF_SIZE];
    int cntList [SMS_MRU_COUNT]; // Lista kontaktów przypisanych do numerów na liœcie
    memset(cntList , 0 , 4*SMS_MRU_COUNT);
    MRU.flags = MRU_GET_ONEBUFF;
    MRU.name = SMS_MRU_NAME;
    MRU.count = SMS_MRU_COUNT;
    MRU.buffer = buff;
    MRU.buffSize = SMS_BUFF_SIZE;
    int lstC = Ctrl->IMessage(&sIMessage_MRU(IMC_MRU_GET , &MRU));
    // Zapelniamy liste numerkami z Listy kontaktow i sprawdzamy ju¿ ustniej¹ce kontakty
    int cntC = ICMessage(IMC_CNT_COUNT);
    int i;
    int lstCMRU = lstC;
    for (i = 1; i<cntC; i++) {
        const char * number = GETCNTC(i , CNT_CELLPHONE);
        if (!*number) continue;
        // Znajdujemy ten numer na liœcie
        for (int j=0; j<lstCMRU; j++)
            if (MRU.values[j] && !strcmp(number , MRU.values[j])) {cntList[j] = i; goto next;}
        if (lstC < SMS_MRU_COUNT)  // Jezeli nie ma go juz na liscie, to go sobie dodajemy...
            {cntList[lstC] = i;MRU.values[lstC]=0;lstC++;}
        next:;
    }
    HWND item = GetDlgItem(_hwnd , IDC_NUMBER);
    SendMessage(item , CB_RESETCONTENT , 0 , 0);

    // Wypelniamy liste
    COMBOBOXEXITEM cbi;
    memset(&cbi , 0 , sizeof(cbi));
    cbi.iItem = -1;
    cbi.mask = CBEIF_TEXT | CBEIF_LPARAM | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
    int imgCnt = ICMessage(IMI_GETICONINDEX , UIIcon(IT_LOGO , 0 , 0 , 0) , IML_16);
    int imgSms = ICMessage(IMI_GETICONINDEX , UIIcon(IT_MESSAGE , NET_NONE , MT_SMS , 0) , IML_16);
    for (i = 0; i<lstC; i++) {
        cbi.lParam = 0;
        CStdString phone = "";
        if (cntList[i]) {
            phone = GETCNTC(cntList[i] , CNT_DISPLAY);
            phone += " | ";
            cbi.lParam = phone.length();
            phone += MRU.values[i]?MRU.values[i] : GETCNTC(cntList[i] , CNT_CELLPHONE);
            cbi.iImage = cbi.iSelectedImage = imgCnt;
        } else {
            phone = MRU.values[i];
            cbi.iImage = cbi.iSelectedImage = imgSms;
        }
        cbi.pszText = (char*)phone.c_str();
        SendMessage(item , CBEM_INSERTITEM , 0 , (LPARAM)&cbi);
    }
}
void cSMSWindow::saveNrList(CStdString current){
    if (current != _last) {
        sMRU MRU;
        MRU.flags = MRU_GET_USETEMP | MRU_SET_LOADFIRST;
        MRU.name = SMS_MRU_NAME;
        MRU.count = SMS_MRU_COUNT;
        MRU.current = current;
        int lstC = Ctrl->IMessage(&sIMessage_MRU(IMC_MRU_SET , &MRU));
        fillNrList();
        _last = current;
    }
}

void cSMSWindow::numberSelected() {
    HWND item = GetDlgItem(_hwnd , IDC_NUMBER);
    COMBOBOXEXITEM cbi;
    memset(&cbi , 0 , sizeof(cbi));
    cbi.iItem = SendMessage(item , CB_GETCURSEL , 0 , 0);
    if (cbi.iItem == -1) return;
    cbi.mask = CBEIF_LPARAM | CBEIF_TEXT;
    char buff [201];
    cbi.pszText = buff;
    cbi.cchTextMax = 200;
    SendMessage(item , CBEM_GETITEM , 0 , (LPARAM)&cbi);
    if (!cbi.lParam) return;
    SetWindowText(item , buff + cbi.lParam);
}


// ______________________________________________________________________

INT_PTR CALLBACK cSMSWindow::SMSWindowProc(HWND hwnd , UINT message, WPARAM wParam, LPARAM lParam){
    cSMSWindow * sw = (cSMSWindow *)GetProp(hwnd , "cSMSWindow");
    switch (message) {
    case WM_INITDIALOG:
       SetProp(hwnd , "cSMSWindow" , (HANDLE)lParam);
       sw = (cSMSWindow*)lParam;
       sw->_hwnd = hwnd;
       SetFocus(GetDlgItem(hwnd , IDC_SMS));
       return 0;
    case WM_DESTROY:
       delete sw;
       SetProp(hwnd , "cSMSWindow" , (HANDLE)0);
       return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);return 0;
    case WM_ACTIVATE:
        SetWindowPos(GetDlgItem(hwnd , IDC_STATUS) , HWND_TOP , 0 , 0 , 0 , 0 , SWP_NOMOVE|SWP_NOSIZE);
        return 0;

    case WM_CTLCOLORLISTBOX:
        break;

    case WM_COMMAND:
        switch(HIWORD(wParam)) {
        case BN_CLICKED:{
            switch (LOWORD(wParam)) {
            case IDOK: sw->send(); return 0;
            }
            break;}
        case CBN_EDITCHANGE:
            if (LOWORD(wParam) == IDC_NUMBER) sw->numberChanged();
            break;
        case CBN_SELCHANGE:
            if (LOWORD(wParam) == IDC_GATE) sw->gateChosen();
            if (LOWORD(wParam) == IDC_NUMBER) sw->numberSelected();
            break;
        case CBN_DROPDOWN:
            if (LOWORD(wParam)== IDC_STATUS) {
            }
            break;
        case CBN_CLOSEUP:
            if (LOWORD(wParam)== IDC_STATUS) {
            }
            break;
        case EN_CHANGE:
            if (LOWORD(wParam) == IDC_SMS
                || LOWORD(wParam) == IDC_MYNAME
                || LOWORD(wParam) == IDC_MYPHONE
                ) sw->editChanged();
            break;

        }     
        break;



   default:
            return 0;//DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}


INT_PTR CALLBACK cSMSWindow::StatusComboProc(HWND hwnd , UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CTLCOLORLISTBOX: {
            RECT rc;
            GetWindowRect(GetParent(hwnd) , &rc);
            SetWindowPos((HWND)lParam , 0 , rc.left , rc.bottom , rc.right - rc.left , 100 , SWP_NOZORDER);
/*            CStdString s;
            GetClassName(GetParent(hwnd) , s.GetBuffer(100) , 100);
            s.ReleaseBuffer();
            IMLOG("CTLCOLOR - Parent %x  class %s" , GetParent(hwnd) , s.c_str());
*/
            }
        case WM_CTLCOLOREDIT : case WM_CTLCOLORSTATIC:
            SetBkMode((HDC)wParam,TRANSPARENT);
            SetBkColor((HDC)wParam,GetSysColor(COLOR_WINDOW));
            return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
    };
    return CallWindowProc(_stdComboProc , hwnd  , message ,wParam , lParam);
}


// ----------------------------------------------------



// ----------------------------------------------------
// ----------------------------------------------------
// ----------------------------------------------------
// ----------------------------------------------------
// ----------------------------------------------------
// ----------------------------------------------------

int __stdcall DllMain(void * hinstDLL, unsigned long fdwReason, void * lpvReserved)
{
        return true;
}

int Init() {

    return 1;
}

int DeInit() {
  return 1;
}

int IStart() {

//    Ctrl->IMessageDirect(Ctrl->ID() , &sIMessage_2params(IMSMS_GETTOKEN , (int)"Bleeeee" , (int)"idea.gif"));


    return 1;
}
int IEnd() {
    SMS.saveUsage();
    return 1;
}

#define CFGSETCOL(i,t,d,n) {sSETCOL sc;sc.id=(i);sc.type=(t);sc.def=(int)(d);sc.name=n;ICMessage(IMC_CFG_SETCOL,(int)&sc);}
#define CNTSETCOL(i,t,d,n) {sSETCOL sc;sc.id=(i);sc.type=(t);sc.def=(int)(d);sc.name=n;ICMessage(IMC_CNT_SETCOL,(int)&sc);}

int ISetCols() {
    CFGSETCOL(CFG_SMS_REUSEWINDOW , DT_CT_INT , 1 , 0);
    CFGSETCOL(CFG_SMS_USELASTGATE , DT_CT_INT , 1 , 0);
    CFGSETCOL(CFG_SMS_GATEUSAGE , DT_CT_PCHAR , "" , "SMS_GateUsage");
    CFGSETCOL(CFG_SMS_SIG , DT_CT_PCHAR | DT_CF_CXOR , "" , "SMS_Sig");
    CFGSETCOL(CFG_SMS_SIG_SAVE , DT_CT_INT , 0 , 0);
    CFGSETCOL(CFG_SMS_DELETEBODY , DT_CT_INT , 0 , "SMS_DeleteBody");
    CFGSETCOL(CFG_SMS_DELMSG_ONFAILURE , DT_CT_INT , 1 , "SMS_DeleteMessageOnFailure");
    CNTSETCOL(CNT_SMS_GATE , DT_CT_PCHAR , "" , 0);
    SMS.CFGprepare();
    return 1;
}

int IPrepare() {
    
    IconRegister(IML_16 , UIIcon(IT_MESSAGE , NET_NONE , MT_SMS , 0) , (HINSTANCE)Ctrl->hDll() , IDI_MT_SMS);
	IconRegister(IML_16 , UIIcon(IT_LOGO , NET_SMS , 0 , 0) , (HINSTANCE)Ctrl->hDll() , IDI_MT_SMS);

	UIActionInsert(ICMessage(IMI_GETPLUGINSGROUP) , IMIA_SENDSMS , -1 , 0 , "Wyœlij SMSa" , UIIcon(IT_MESSAGE , NET_NONE , MT_SMS , 0));
    UIActionInsert(IMIG_CNT , IMIA_SENDSMS , UIActionGetPos(IMIG_CNT , IMIA_CNT_SEP) , ACTR_INIT , "Wyœlij SMSa" , UIIcon(IT_MESSAGE , NET_NONE , MT_SMS , 0));

    UIActionInsert(IMIG_MSGTB , IMIA_SENDSMS , -1 , ACTR_INIT , "SMS" , UIIcon(IT_MESSAGE , NET_NONE , MT_SMS , 0));

//    UIActionSetStatus(IMIG_MSGBAR , IMIG_MSGSENDTB , 0 , ACTS_HIDDEN);

    UIGroupAdd(IMIG_CFG_PLUGS , IMIG_SMS , 0 , "SMS" , UIIcon(IT_MESSAGE , NET_NONE , MT_SMS , 0));

      UIActionAdd(IMIG_SMS , 0 , ACTT_GROUP , "Informacje");
      UIActionAdd(IMIG_SMS , 0 , ACTT_IMAGE | ACTSC_INLINE , "reg://IML16/"+CStdString(inttoch(UIIcon(IT_MESSAGE , NET_NONE , MT_SMS , 0)))+".ico" , 0 , 16 , 16);
      UIActionAdd(IMIG_SMS , 0 , ACTT_INFO , "Aby wtyczka mog³a wysy³aæ SMSy, skrypty obs³uguj¹ce bramki musz¹ siê "
                    "znajdowaæ w katalogu 'konnekt\\sms\\'.\r\n"
                    "Nie wys³ane wiadomoœci dostêpne s¹ w Historii w zak³adce 'Kolejka'. "
                    "Naciskaj¹c [Przeœlij] w historii, wszystkie wiadomoœci w kolejce zostan¹ wys³ane ponownie." , 0 , 0 , 70);
      UIActionAdd(IMIG_SMS , 0 , ACTT_GROUPEND);

      UIActionAdd(IMIG_SMS , 0 , ACTT_GROUP , "");
      UIActionAdd(IMIG_SMS , 0 , ACTT_CHECK , "U¿ywaj jednego okna wysy³ania" , CFG_SMS_REUSEWINDOW);
      UIActionAdd(IMIG_SMS , 0 , ACTT_CHECK , "Kasuj treœæ wiadomoœci w okienku" , CFG_SMS_DELETEBODY);
      UIActionAdd(IMIG_SMS , 0 , ACTT_CHECK , "Nie zostawiaj nie wys³anej wiadomoœci w kolejce" , CFG_SMS_DELMSG_ONFAILURE);
//      UIActionAdd(IMIG_SMS , 0 , ACTT_CHECK , "Zapamiêtuj ostatnio u¿yt¹ bramkê dla kontaktu" , CFG_SMS_USELASTGATE);
      UIActionAdd(IMIG_SMS , 0 , ACTT_GROUPEND);
      UIActionAdd(IMIG_SMS , 0 , ACTT_GROUP , "Podpis [zale¿ny od bramek]");
      UIActionAdd(IMIG_SMS , 0 , ACTT_CHECK , "Zapamiêtuj ostatni podpis" , CFG_SMS_SIG_SAVE);
      UIActionAdd(IMIG_SMS , 0 , ACTT_COMMENT | ACTSC_INLINE , "Podpis");
      UIActionAdd(IMIG_SMS , 0 , ACTT_EDIT | ACTSC_FULLWIDTH , "" , CFG_SMS_SIG);

      UIActionAdd(IMIG_SMS , 0 , ACTT_GROUPEND);

    UIGroupAdd(IMIG_SMS , IMIG_SMS_GATE , 0 , "Bramki" , 3);
    SMS.UIprepare();
    return 1;
}

int ActionCfgProc(sUIActionNotify_base * anBase) {
  sUIActionNotify_2params * an = static_cast<sUIActionNotify_2params*>(anBase);
  switch (anBase->act.id & ~IMIB_CFG) {
    
  }
  return 0;
}

ActionProc(sUIActionNotify_base * anBase) {
  sUIActionNotify_2params * an = static_cast<sUIActionNotify_2params*>(anBase);

  if ((anBase->act.id & IMIB_) == IMIB_CFG) return ActionCfgProc(anBase); 
  switch (anBase->act.id) {
      case IMIA_SENDSMS: 
          if (anBase->code == ACTN_ACTION) {
              if (anBase->act.cnt == -1) return 0;
              SMS.openWnd(anBase->act.cnt);
          } else if (anBase->code == ACTN_CREATE) {
              UIActionSetStatus(anBase->act , (anBase->act.cnt != -1 && anBase->act.cnt!=0) && *GETCNTC(anBase->act.cnt , CNT_CELLPHONE)?0:ACTS_HIDDEN , ACTS_HIDDEN);
          } else if (anBase->code == ACTN_DEFAULT) {
              if (anBase->act.cnt == -1 || anBase->act.cnt==0) return 0;
              return (*GETCNTC(anBase->act.cnt , CNT_CELLPHONE)!=0);
          }
          break;
    
  }
  return 0;
}



int __stdcall IMessageProc(sIMessage_base * msgBase) {
    sIMessage_2params * msg = static_cast<sIMessage_2params*>(msgBase);
    switch (msgBase->id) {
    case IM_PLUG_NET:        return NET_SMS; // Zwracamy wartoœæ NET, która MUSI byæ ró¿na od 0 i UNIKATOWA!
    case IM_PLUG_TYPE:       return IMT_UI | IMT_CONFIG | IMT_MESSAGE; // Zwracamy jakiego typu jest nasza wtyczka (które wiadomoœci bêdziemy obs³ugiwaæ)
    case IM_PLUG_VERSION:    return (int)"1.0.0.0"; // Wersja wtyczki tekstowo major.minor.release.build ...
    case IM_PLUG_SDKVERSION:    return KONNEKT_SDK_V; 
    case IM_PLUG_SIG:        return (int)"SMS"; // Sygnaturka wtyczki (krótka, kilkuliterowa nazwa)
    case IM_PLUG_CORE_V:     return (int)"W98"; // Wymagana wersja rdzenia
    case IM_PLUG_UI_V:       return 0; // Wymagana wersja UI
    case IM_PLUG_NAME:       return (int)"SMS"; // Pe³na nazwa wtyczki
    case IM_PLUG_NETNAME:    return 0; // Nazwa obs³ugiwanej sieci (o ile jak¹œ sieæ obs³uguje)
    case IM_PLUG_INIT:       Plug_Init(msg->p1,msg->p2);return Init();
    case IM_PLUG_DEINIT:     Plug_Deinit(msg->p1,msg->p2);return DeInit();
	case IM_ALLPLUGINSINITIALIZED:
	    SMS.prepare();
		return 0;


    case IM_SETCOLS:     return ISetCols();

    case IM_UI_PREPARE:      return IPrepare();
    case IM_START:           return IStart();
    case IM_END:             return IEnd();

    case IM_UIACTION:        return ActionProc((sUIActionNotify_base*)msg->p1);

    /* Tutaj obs³ugujemy wszystkie pozosta³e wiadomoœci */

	case IM_CFG_CHANGED:
		SMS.CFGset();
		return 0;
    case IM_MSG_RCV:{
        cMessage * m = (cMessage*)msg->p1;
        if (m->net == NET_SMS && m->type == MT_SMS) {
			string gate = GetExtParam(m->ext , Sms::extGate);            
            if (SMS.findGate(gate)) return IM_MSG_ok;
            else return IM_MSG_delete;
        }
        return 0;}
    case IM_MSG_SEND:{
        cMessage * m = (cMessage*)msg->p1;
		string gateID = GetExtParam(m->ext , Sms::extGate);            
        cSMSGate * gate = SMS.findGate(gateID);
        if (!gate) return IM_MSG_delete;
		if (GetExtParam(m->ext , Sms::extPart)=="") {
			// najpierw trzeba to podzieliæ...
			gate->send(m->toUid , m->body , m->ext , 0);
			return IM_MSG_delete;
		} else { // obslugujemy poprawny kawa³ek
	        gate->msgSend(messageDuplicate(m));
			return IM_MSG_processing;
        }
		}
	case Sms::IM::getGatewaysComboText: {
		if (!msg->p1) 
			return (int)"";
		const char * number = (const char*) msg->p1;
		cPreg Preg;
		Preg.setSubject(number);
		vector <cSMSGate *> gates;
		for (gate_it_t gate_it = SMS.gate.begin(); gate_it != SMS.gate.end(); gate_it++) {
			if ((*gate_it)->enabled && Preg.match((*gate_it)->acceptNumber))
				gates.push_back(*gate_it);
		}
		sort(gates.begin() , gates.end() , cSMSGate::compare); // Sortujemy wg. uzywalnosci
		CStdString val;
		for (gate_it_t it = gates.begin(); it != gates.end(); ++it) {
			val += (*it)->name + CFGVALUE + (*it)->id + "\n";
		}
		char * buff = (char*) Ctrl->GetTempBuffer(val.size() + 1);
		strcpy(buff, val);
		return (int) buff;
		}
	case Sms::IM::getGatewayLimit: {
		if (!msg->p1)
			return (int)"";
        cSMSGate * gate = SMS.findGate((char*) msg->p1);
		if (!gate) return 0;
		return gate->vars["maxChars"];
		}

    default:
        if (Ctrl) Ctrl->setError(IMERROR_NORESULT);
        return 0;

 }
 return 0;
}



