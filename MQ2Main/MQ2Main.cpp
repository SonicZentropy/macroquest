/*****************************************************************************
MQ2Main.dll: MacroQuest's extension DLL for EverQuest
    Copyright (C) 2002-2003 Plazmic, 2003-2005 Lax

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
******************************************************************************/

// Exclude rarely-used stuff from Windows headers
#if !defined(CINTERFACE)
#error /DCINTERFACE
#endif

#define DBG_SPEW
#define DEBUG_TRY

#include "MQ2Main.h"

#ifdef EQLIB_EXPORTS
#pragma message("EQLIB_EXPORTS")
#else
#pragma message("EQLIB_IMPORTS")
#endif

DWORD WINAPI MQ2Start(LPVOID lpParameter);
#ifndef ISXEQ

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    CHAR szFilename[MAX_STRING]={0};
    PCHAR szProcessName;
    ghModule = (HMODULE)hModule;
    ghInstance = (HINSTANCE)hModule;

    GetModuleFileName(ghModule,szFilename,MAX_STRING);
    szProcessName = strrchr(szFilename,'\\');
    szProcessName[0] = '\0';
    strcat(szFilename,"\\eqgame.ini");

    GetModuleFileName(NULL,szFilename,MAX_STRING);

    szProcessName = strrchr(szFilename,'.');
    szProcessName[0] = '\0';
    szProcessName = strrchr(szFilename,'\\')+1;
	if (!stricmp(szProcessName,__ClientName)) {
        if (ul_reason_for_call == DLL_PROCESS_ATTACH) {

            GetModuleFileName(ghModule,szFilename,MAX_STRING);
            szProcessName = strrchr(szFilename,'\\');
            szProcessName[0] = '\0';
            g_Loaded = TRUE;
            CreateThread(NULL,0,&MQ2Start,strdup(szFilename),0,&ThreadID);
        } else if (ul_reason_for_call == DLL_PROCESS_DETACH){
            gbUnload=TRUE;
            //Fuck waiting for us to cleanly exit... we're being unloaded!
            //while (g_Loaded) Sleep(20);
            return TRUE;
        }
    }
    return TRUE;
}

#endif

// ***************************************************************************
// Function:    ParseINIFile
// Description: Parse INI file for memory locations
// ***************************************************************************
BOOL ParseINIFile(PCHAR lpINIPath)
{
    CHAR Filename[MAX_STRING] = {0};
    CHAR MQChatSettings[MAX_STRING] = {0};
    CHAR CustomSettings[MAX_STRING] = {0};
    CHAR ClientINI[MAX_STRING] = {0};
    CHAR szBuffer[MAX_STRING] = {0};
    CHAR ClientName[MAX_STRING] = {0};
    CHAR FilterList[MAX_STRING*10] = {0};
   GetEQPath(gszEQPath);


    sprintf(Filename,"%s\\MacroQuest.ini",lpINIPath);
    sprintf(ClientINI,"%s\\eqgame.ini",lpINIPath);
    strcpy(gszINIFilename,Filename);

    DebugSpewAlways("Expected Client version: %s %s",__ExpectedVersionDate,__ExpectedVersionTime);
    DebugSpewAlways("    Real Client version: %s %s",__ActualVersionDate,__ActualVersionTime);

	// note: __ClientOverride is always #defined as 1 or 0
#if (!__ClientOverride)
	if (strncmp(__ExpectedVersionDate,(const char *)__ActualVersionDate,strlen(__ExpectedVersionDate)) ||
		strncmp(__ExpectedVersionTime,(const char *)__ActualVersionTime,strlen(__ExpectedVersionTime)))
	{
        MessageBox(NULL,"Incorrect client version","MacroQuest",MB_OK);
        return FALSE;
	}
#endif

	gFilterSkillsAll = 0!=GetPrivateProfileInt("MacroQuest","FilterSkills",0,Filename);
    gFilterSkillsIncrease = 2==GetPrivateProfileInt("MacroQuest","FilterSkills",0,Filename);
    gFilterDebug  = 1==GetPrivateProfileInt("MacroQuest","FilterDebug",0,Filename);
    gFilterMQ2DataErrors  = 1==GetPrivateProfileInt("MacroQuest","FilterMQ2Data",0,Filename);
    gFilterTarget = 1==GetPrivateProfileInt("MacroQuest","FilterTarget",0,Filename);
    gFilterMoney  = 1==GetPrivateProfileInt("MacroQuest","FilterMoney",0,Filename);
    gFilterFood   = 1==GetPrivateProfileInt("MacroQuest","FilterFood",0,Filename);
    gFilterMacro  = GetPrivateProfileInt("MacroQuest","FilterMacro",0,Filename);
    gFilterEncumber=1==GetPrivateProfileInt("MacroQuest","FilterEncumber",0,Filename);
    gFilterCustom = 1==GetPrivateProfileInt("MacroQuest","FilterCustom",1,Filename);
    gSpewToFile   = 1==GetPrivateProfileInt("MacroQuest","DebugSpewToFile",0,Filename);
    gMQPauseOnChat= 1==GetPrivateProfileInt("MacroQuest","MQPauseOnChat",0,Filename);
    gKeepKeys     = 1==GetPrivateProfileInt("MacroQuest","KeepKeys",0,Filename);
	bLaxColor=1==GetPrivateProfileInt("MacroQuest","LaxColor",0,Filename);
	bAllErrorsDumpStack = 1==GetPrivateProfileInt("MacroQuest","AllErrorsDumpStack",1,Filename);
	bAllErrorsFatal = 1==GetPrivateProfileInt("MacroQuest","AllErrorsFatal",0,Filename);

	GetPrivateProfileString("MacroQuest","HUDMode","UnderUI",CustomSettings,MAX_STRING,Filename);
	if (!stricmp(CustomSettings,"normal"))
	{
		gbAlwaysDrawMQHUD=false;
		gbHUDUnderUI=false;
	}
	else
	if (!stricmp(CustomSettings,"underui"))
	{
		gbHUDUnderUI=true;
		gbAlwaysDrawMQHUD=false;
	}
	else
	if (!stricmp(CustomSettings,"always"))
	{
		gbHUDUnderUI=true;
		gbAlwaysDrawMQHUD=true;
	}



	GetPrivateProfileString("Captions","NPC",gszSpawnNPCName,gszSpawnNPCName,MAX_STRING,Filename);
	GetPrivateProfileString("Captions","Player1",gszSpawnPlayerName[1],gszSpawnPlayerName[1],MAX_STRING,Filename);
	GetPrivateProfileString("Captions","Player2",gszSpawnPlayerName[2],gszSpawnPlayerName[2],MAX_STRING,Filename);
	GetPrivateProfileString("Captions","Player3",gszSpawnPlayerName[3],gszSpawnPlayerName[3],MAX_STRING,Filename);
	GetPrivateProfileString("Captions","Player4",gszSpawnPlayerName[4],gszSpawnPlayerName[4],MAX_STRING,Filename);
	GetPrivateProfileString("Captions","Corpse",gszSpawnCorpseName,gszSpawnCorpseName,MAX_STRING,Filename);
	GetPrivateProfileString("Captions","Pet",gszSpawnPetName,gszSpawnPetName,MAX_STRING,Filename);
	gMaxSpawnCaptions=GetPrivateProfileInt("Captions","Update",gMaxSpawnCaptions,Filename);
	ConvertCR(gszSpawnNPCName);
	ConvertCR(gszSpawnPlayerName[1]);
	ConvertCR(gszSpawnPlayerName[2]);
	ConvertCR(gszSpawnPlayerName[3]);
	ConvertCR(gszSpawnPlayerName[4]);
	ConvertCR(gszSpawnCorpseName);
	ConvertCR(gszSpawnPetName);

	gFilterSWho.Lastname= GetPrivateProfileInt("SWho Filter","Lastname",1,Filename);
	gFilterSWho.Class	= GetPrivateProfileInt("SWho Filter","Class",1,Filename);
	gFilterSWho.Race	= GetPrivateProfileInt("SWho Filter","Race",1,Filename);
	gFilterSWho.Level	= GetPrivateProfileInt("SWho Filter","Level",1,Filename);
	gFilterSWho.GM		= GetPrivateProfileInt("SWho Filter","GM",1,Filename);
	gFilterSWho.Guild	= GetPrivateProfileInt("SWho Filter","Guild",1,Filename);
    gFilterSWho.Sneak   = GetPrivateProfileInt("SWho Filter","Sneak",1,Filename); 
	gFilterSWho.LD		= GetPrivateProfileInt("SWho Filter","LD",1,Filename);
	gFilterSWho.LFG		= GetPrivateProfileInt("SWho Filter","LFG",1,Filename);
	gFilterSWho.NPCTag	= GetPrivateProfileInt("SWho Filter","NPCTag",1,Filename);
	gFilterSWho.Trader	= GetPrivateProfileInt("SWho Filter","Trader",1,Filename);
	gFilterSWho.AFK		= GetPrivateProfileInt("SWho Filter","AFK",1,Filename);
	gFilterSWho.Anon	= GetPrivateProfileInt("SWho Filter","Anon",1,Filename);
	gFilterSWho.Distance= GetPrivateProfileInt("SWho Filter","Distance",1,Filename);
	gFilterSWho.Light	= GetPrivateProfileInt("SWho Filter","Light",0,Filename);
	gFilterSWho.Body	= GetPrivateProfileInt("SWho Filter","Body",0,Filename);
	gFilterSWho.SpawnID = GetPrivateProfileInt("SWho Filter","SpawnID",0,Filename);
	gFilterSWho.Holding = GetPrivateProfileInt("SWho Filter","Holding",0,Filename);
	gFilterSWho.ConColor= GetPrivateProfileInt("SWho Filter","ConColor",0,Filename);
	gFilterSWho.Invisible= GetPrivateProfileInt("SWho Filter","Invisible",0,Filename);

    GetPrivateProfileString("MacroQuest","MacroPath",".",szBuffer,MAX_STRING,Filename);
    if (szBuffer[0]=='.') {
        sprintf(gszMacroPath,"%s%s",lpINIPath,szBuffer+1);
    } else {
        strcat(gszMacroPath,szBuffer);
    }


    GetPrivateProfileString("MacroQuest","LogPath",".",szBuffer,MAX_STRING,Filename);
    if (szBuffer[0]=='.') {
        sprintf(gszLogPath,"%s%s",lpINIPath,szBuffer+1);
    } else {
        strcat(gszLogPath,szBuffer);
    }

    DefaultFilters();
    GetPrivateProfileString("Filter Names",NULL,"",FilterList,MAX_STRING*10,Filename);
    PCHAR pFilterList = FilterList;
    while (pFilterList[0]!=0) {
        GetPrivateProfileString("Filter Names",pFilterList,"",szBuffer,MAX_STRING,Filename);
        if (szBuffer[0]!=0 && strcmp(szBuffer,"NOBODY")) {
            AddFilter(szBuffer,-1,&gFilterCustom);
        }
        pFilterList+=strlen(pFilterList)+1;
    }

	sprintf(Filename,"%s\\ItemDB.txt",lpINIPath);
    FILE *fDB = fopen(Filename,"rt");
    strcpy(gszItemDB,Filename);
    if (fDB) {
        fgets(szBuffer,MAX_STRING,fDB);
        while ((!feof(fDB)) && (strstr(szBuffer,"\t"))) {
            PITEMDB Item = (PITEMDB)malloc(sizeof(ITEMDB));
            Item->pNext = gItemDB;
            Item->ID = atoi(szBuffer);
            strcpy(Item->szName,strstr(szBuffer,"\t")+1);
            Item->szName[strstr(Item->szName,"\n")-Item->szName]=0;
            gItemDB = Item;
            fgets(szBuffer,MAX_STRING,fDB);
        }
        fclose(fDB);
    }

    return TRUE;
}


bool __cdecl MQ2Initialize()
{
    if (!ParseINIFile(gszINIPath)) {
        DebugSpewAlways("ParseINIFile returned false - thread aborted.");
        g_Loaded = FALSE;
        return false;
    }
    srand((unsigned int)time(0));
    ZeroMemory(gDiKeyName,sizeof(gDiKeyName));
    unsigned long i;
    for (i = 0 ; gDiKeyID[i].Id ; i++) {
        gDiKeyName[gDiKeyID[i].Id]=gDiKeyID[i].szName;
    }

    ZeroMemory(szEQMappableCommands,sizeof(szEQMappableCommands));
    for (i = 0 ; i < nEQMappableCommands && EQMappableCommandList[i] ; i++) {
        szEQMappableCommands[i]=EQMappableCommandList[i];
    }
    gnNormalEQMappableCommands=i; 
    szEQMappableCommands[0xf4]="UNKNOWN0xF4";
    szEQMappableCommands[0xf5]="UNKNOWN0xF5";
    szEQMappableCommands[0xf6]="UNKNOWN0xF6";
    szEQMappableCommands[0xf7]="UNKNOWN0xF7";
    szEQMappableCommands[0xf8]="CHAT_SEMICOLON";
    szEQMappableCommands[0xf9]="CHAT_SLASH";
    szEQMappableCommands[0xfa]="UNKNOWN0xFA";
    szEQMappableCommands[0xfb]="UNKNOWN0xFB";
    szEQMappableCommands[0xfc]="INSTANT_CAMP";
    szEQMappableCommands[0xfd]="UNKNOWN0xFD";
    szEQMappableCommands[0xfe]="UNKNOWN0xFE";
    szEQMappableCommands[0xff]="CHAT_EMPTY";
    szEQMappableCommands[0x100]="TOGGLE_WINDOWMODE";
    szEQMappableCommands[0x101]="UNKNOWN0x101";
    szEQMappableCommands[0x102]="UNKNOWN0x102";
    szEQMappableCommands[0x103]="CHANGEFACE";// maybe? something that requires models.
    szEQMappableCommands[0x104]="UNKNOWN0x104";
    szEQMappableCommands[0x105]="UNKNOWN0x105";
    szEQMappableCommands[0x106]="UNKNOWN0x106";
    szEQMappableCommands[0x107]="UNKNOWN0x107";
    szEQMappableCommands[0x108]="UNKNOWN0x108";
    szEQMappableCommands[0x109]="UNKNOWN0x109";

	for (nColorAdjective=0 ; szColorAdjective[nColorAdjective] ; nColorAdjective++){}
	for (nColorAdjectiveYou=0 ; szColorAdjectiveYou[nColorAdjectiveYou] ; nColorAdjectiveYou++) {}
	for (nColorExpletive=0 ; szColorExpletive[nColorExpletive] ; nColorExpletive++) {}
	for (nColorSyntaxError=0 ; szColorSyntaxError[nColorSyntaxError] ; nColorSyntaxError++) {}
	for (nColorMacroError=0 ; szColorMacroError[nColorMacroError] ; nColorMacroError++) {}
	for (nColorMQ2DataError=0 ; szColorMQ2DataError[nColorMQ2DataError] ; nColorMQ2DataError++) {}
	for (nColorFatalError=0 ; szColorFatalError[nColorFatalError] ; nColorFatalError++) {}

	InitializeMQ2Benchmarks();
#ifndef ISXEQ
	InitializeParser();
#endif
	InitializeMQ2Detours();
	InitializeDisplayHook();
	InitializeChatHook();
	InitializeMQ2Spawns();
#ifndef ISXEQ
	InitializeMQ2Pulse();
#endif
	InitializeMQ2Commands();
	InitializeMQ2Windows();
	Sleep(100);
	InitializeMQ2KeyBinds();
#ifndef ISXEQ
	InitializeMQ2Plugins();
#endif
#ifndef ExactLocation
	bLaxColor=TRUE;
	gSpewToFile=1;
	gFilterMQ=1;
#endif

	return true;
}

void __cdecl MQ2Shutdown()
{
	DebugTry(ShutdownMQ2KeyBinds());
	DebugTry(ShutdownMQ2Spawns());
	DebugTry(ShutdownDisplayHook());
#ifndef ISXEQ
	DebugTry(ShutdownMQ2DInput());
#endif
	DebugTry(ShutdownChatHook());
#ifndef ISXEQ
	DebugTry(ShutdownMQ2Pulse());
	DebugTry(ShutdownMQ2Plugins());
#endif
	DebugTry(ShutdownMQ2Windows());
#ifndef ISXEQ
	DebugTry(ShutdownParser());
#endif
	DebugTry(ShutdownMQ2Commands());
	DebugTry(ShutdownMQ2Detours());
	DebugTry(ShutdownMQ2Benchmarks());

}

#ifndef ISXEQ
// ***************************************************************************
// Function:    MQ2Start
// Description: Where we start execution during the insertion
// ***************************************************************************
DWORD WINAPI MQ2Start(LPVOID lpParameter)
{
    PCHAR lpINIPath = (PCHAR)lpParameter;
    strcpy(gszINIPath, lpINIPath);
    free(lpINIPath);
    CHAR szBuffer[MAX_STRING] = {0};

	if (!MQ2Initialize())
		return 1;

	while (gGameState != GAMESTATE_CHARSELECT && gGameState != GAMESTATE_INGAME) 
		Sleep(500);
	InitializeMQ2DInput();


    WriteChatColor(LoadedString,USERCOLOR_DEFAULT);
    DebugSpewAlways(LoadedString);

    while (!gbUnload) {
        Sleep(500);
    }

    WriteChatColor(UnloadedString,USERCOLOR_DEFAULT);
    DebugSpewAlways(UnloadedString);
	UnloadMQ2Plugins();
	MQ2Shutdown();


	DebugSpew("Shutdown completed");
    g_Loaded = FALSE;
    return 0;
}


#ifdef DEBUG_ALLOC
DWORD CountMallocs=0;
DWORD CountFrees=0;
#endif

#ifdef malloc
#undef malloc
#endif

void *MQ2Malloc(size_t size)
{
#ifdef DEBUG_ALLOC
	CountMallocs++;
#endif
	return malloc(size);
}

#ifdef free
#undef free
#endif

void MQ2Free(void *memblock)
{
#ifdef DEBUG_ALLOC
	CountFrees++;
#endif
	free(memblock);
}
#endif

class CMQNewsWnd : public CCustomWnd
{
public:
	CMQNewsWnd(char *Template):CCustomWnd(Template)
	{
		SetWndNotification(CMQNewsWnd);
		InputBox=(CTextEntryWnd*)GetChildItem("CWChatInput");
		InputBox->WindowStyle|=0x800C0;
		BitOff(WindowStyle,CWS_CLOSE);
		InputBox->UnknownCW|=0xFFFFFFFF;
		InputBox->Enabled=0;
		InputBox->SetMaxChars(512);
		OutputBox=(CStmlWnd*)GetChildItem("CWChatOutput");
	}

	~CMQNewsWnd()
	{
	}

	int WndNotification(CXWnd *pWnd, unsigned int Message, void *unknown)
	{	
		if (pWnd==0)
		{
			if (Message==XWM_CLOSE)
			{
				Show=1;
				return 1;
			}
		}
		return CSidlScreenWnd::WndNotification(pWnd,Message,unknown);
	};

	CTextEntryWnd *InputBox;
	CStmlWnd *OutputBox;
};


CMQNewsWnd *pNewsWindow=0;
VOID InsertMQ2News();
VOID CreateMQ2NewsWindow()
{
	CHAR Filename[MAX_STRING]={0};
	sprintf(Filename,"%s\\changes.txt",gszINIPath);
	if (!pNewsWindow && _FileExists(Filename))
	{
		pNewsWindow = new CMQNewsWnd("ChatWindow");
		pNewsWindow->Location.top=250;
		pNewsWindow->Location.bottom=450;
		pNewsWindow->Location.left=250;
		pNewsWindow->Location.right=950;
		SetCXStr(&pNewsWindow->WindowText,"MacroQuest2 Recent Changes");
	}
	InsertMQ2News();
}

VOID AddNewsLine(PCHAR Line, DWORD Color)
{
	Color=pChatManager->GetRGBAFromIndex(Color);

	CHAR szProcessed[MAX_STRING];
	MQToSTML(Line,szProcessed,MAX_STRING,Color);
	strcat(szProcessed,"<br>");
	CXStr NewText(szProcessed);
	ConvertItemTags(NewText,0);
	CXSize Whatever;
	pNewsWindow->OutputBox->AppendSTML(&Whatever,NewText);
//	((CXWnd*)pNewsWindow->OutputBox)->SetVScrollPos(0);

}

VOID DeleteMQ2NewsWindow()
{
	if (pNewsWindow)
	{
		delete pNewsWindow;
		pNewsWindow=0;
	}
}

VOID InsertMQ2News()
{
	if (!pNewsWindow)
		return;
	CHAR Filename[MAX_STRING]={0};
	sprintf(Filename,"%s\\changes.txt",gszINIPath);
	FILE *file=fopen(Filename,"rb");
	if (!file)
	{
		DeleteMQ2NewsWindow();	
		return;
	}
	AddNewsLine("If you need help, refer to README.CHM, the MacroQuest2 manual.",CONCOLOR_RED);
	AddNewsLine("Recent changes...",CONCOLOR_RED);
	CHAR szLine[MAX_STRING]={0};
	DWORD nLines=0;
	while(fgets(szLine,MAX_STRING,file))
	{
		strtok(szLine,"\r\n");
		if (atoi(szLine) && strstr(szLine," by "))
			AddNewsLine(szLine,CONCOLOR_GREEN);
		else
			AddNewsLine(szLine,CONCOLOR_YELLOW);
		nLines++;
		if (nLines>200)
		{
			AddNewsLine("...read changes.txt for more.",CONCOLOR_RED);
			break;
		}
	}
	fclose(file);
}


#ifndef ISXEQ

HHOOK g_hHook;

LRESULT CALLBACK hookCBTProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    return ::CallNextHookEx( g_hHook, nCode, wParam, lParam );
}

VOID InjectEnable()
{
    // Install the global hook, injecting this DLL into every other process
    g_hHook = SetWindowsHookEx( WH_CBT, hookCBTProc, ghInstance, 0 );
}

VOID InjectDisable()
{
    UnhookWindowsHookEx( g_hHook );
    g_hHook = NULL;
}
#endif

/* OTHER FUNCTIONS IMPORTED FROM EQ */
#ifdef __CastRay
FUNCTION_AT_ADDRESS(int CastRay(PSPAWNINFO,float y,float x,float z),__CastRay);
#endif
#ifdef __FastTime
FUNCTION_AT_ADDRESS(unsigned long  GetFastTime(void),__FastTime);
#endif

