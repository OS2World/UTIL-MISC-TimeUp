/* TimeUP.C, Ronald Van Iwaarden 1993
// Compiler: EMX/GCC 0.8f or better.

// Modified from
// VMEM.C - Copyright (c) 1992 M.B.Mallory

// TimeUP displays the amount time has elapsed since OS/2 2.0 was last
// booted in the smallest window possible... a title bar. TimeUP can run
// along the edge of the screen (or under it's own icon) with no loss of 
// screen real estate.

*/

#define INCL_WIN
#define INCL_GPIPRIMITIVES
#define INCL_DOSMISC
#define INCL_WINWINDOWMGR

#include <os2.h>
#include <string.h>
#include "timeup.h"

#define CMD (COMMANDMSG(&msg)->cmd)

/* data structure and profile info to be written to OS2.INI */

typedef LHANDLE HINI;
typedef HINI *PHINI;

#define HINI_USERPROFILE   (HINI) -1L

typedef struct
   {
   POINTL pt;
   SHORT savescrpos;
   SHORT format;
   ULONG record;
   } INIINFO;


#define MEM_APP      "TIMEUP"
#define MEM_KEY      "OptionsData"

/* define items to remove from the System menu*/
#define MIR_COUNT    5
#define MIR_DATA     SC_RESTORE, SC_SIZE, SC_HIDE, SC_MINIMIZE, SC_MAXIMIZE

/* define additions to the System menu*/
#define MI_COUNT     4
#define IDM_ABOUT    88
#define IDM_OPTIONS  89
#define MI_STR       0, 0, "~About  TIMEUP", "TIMEUP  ~Options"
#define MI_DATA      MIT_END, MIS_SEPARATOR, 0, 0, 0, 0,\
                     MIT_END, MIS_SEPARATOR, 0, 0, 0, 0,\
                     MIT_END, MIS_TEXT, 0, IDM_ABOUT, 0, 0,\
                     MIT_END, MIS_TEXT, 0, IDM_OPTIONS, 0, 0

/* initial title bar text*/
#define TITLEBAR_TEXT "Time Since Boot"

/* no timers error message*/
#define TIMER_TITLE  "Error initializing  TIMEUP"
#define TIMER_TEXT   "TIMEUP cannot be initialized because"\
                     " too many timers are already running."\
                     " Close some applications to free system resources."

/* prototype for revectored (subclassed) system frame procedure*/
MRESULT (* EXPENTRY DefaultFrameProc) (HWND, ULONG, MPARAM, MPARAM);

/* prototypes for new frame & dialog procedures*/
MRESULT EXPENTRY NewFrameProc (HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY AboutProc (HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY OptionsProc (HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY HelpProc (HWND, ULONG, MPARAM, MPARAM);

/* misc. prototypes*/
VOID ReviseSystemMenu (VOID);
VOID SetFramePosition (PRECTL);
ULONG GetTimeUp (VOID);
BOOL GetIniInfo (VOID);
void _setuparg (void);


CHAR * ULongToString (LONG number)
{
  static char str[5];
  LONG pwrof10 = 1;
  int temp;

  pwrof10 = (number > 9) ? 10 : 1;
  strcpy(str,"");
  while (pwrof10 != 0){
    temp=number/pwrof10;
    switch(temp){
    case 0: strcat(str,"0"); break;
    case 1: strcat(str,"1"); break;
    case 2: strcat(str,"2"); break;
    case 3: strcat(str,"3"); break;
    case 4: strcat(str,"4"); break;
    case 5: strcat(str,"5"); break;
    case 6: strcat(str,"6"); break;
    case 7: strcat(str,"7"); break;
    case 8: strcat(str,"8"); break;
    case 9: strcat(str,"9"); break;
    }
    number = number - pwrof10*(temp);
    pwrof10 = pwrof10/10;
    };
  if (strlen(str) == 0) strcat(str,"0");
  return str;
};



char * NumToDate(ULONG Number)
{
  ULONG Day, Hour, Minute;
  static char str[40];
  Day    = Number/NumDay;
  Hour   = (Number/NumHour) % 24;
  /*Minute = ((Number/NumMilSeconds) % 1440)%60;*/
  Minute = Number/NumMilSeconds - Day*1440 - Hour*60;
  strcpy (str, ULongToString (Day));
  if (Day != 1) strcat(str, " Days  ");
  else          strcat(str, " Day  ");
  strcat (str, ULongToString (Hour));
  strcat (str, ":");
  if (Minute < 10) strcat (str, "0");
  strcat (str, ULongToString (Minute));
  return str;
}



/* globals*/
HWND hwndFrame, hwndTitleBar;
INIINFO ini;
BOOL bRestoreFlag = FALSE;
BOOL bDisplayTimeUP = FALSE;

/* declare dummy function to eliminate command line parsing */
void _setuparg (void)  
    {}

int main (void)
{
  HAB hab;
  HMQ hmq;
  QMSG qmsg;
  RECTL rect;
  
  FRAMECDATA framecdata = {
    sizeof framecdata,
    FCF_TITLEBAR | FCF_SYSMENU | FCF_TASKLIST | FCF_ICON | FCF_BORDER,
    0,
    TIMEUP_ICON };
  
  hab = WinInitialize (0);            /* create anchor block*/
  hmq = WinCreateMsgQueue (hab, 0);   /* create message queue*/
  
  /* initialize handle & create frame window*/
  hwndFrame = WinCreateWindow (
			       HWND_DESKTOP,           /* window parent */
			       WC_FRAME,               /* frame window class */
			       TITLEBAR_TEXT,          /* window text */
			       0,                      /* frame creation style (invisible) */
			       0,0,                    /* x, y position */
			       0,0,                    /* width, height */
			       0,                      /* owner window */
			       HWND_TOP,               /* top of z-order */
			       ID_FRAME,               /* frame window ID */
			       &framecdata,            /* control data */
			       0);                     /* presentation parameters */
  
  /* initialize handle to title bar*/
  hwndTitleBar = WinWindowFromID (hwndFrame, FID_TITLEBAR);
  
  /* hook the vector to this instance of the system frame procedure*/
  DefaultFrameProc = WinSubclassWindow (hwndFrame, (PFNWP) NewFrameProc);
  
  ReviseSystemMenu ();   /* delete disabled stuff & add new items*/
  
  SetFramePosition (&rect);    /* initial window frame position*/
  
  WinSetWindowPos (hwndFrame, HWND_TOP, rect.xLeft, rect.yBottom,
		   rect.xRight, rect.yTop + 1, SWP_SIZE | SWP_MOVE | SWP_SHOW);
  
  if (WinStartTimer (hab, hwndFrame, ID_TIMER, 1000))
    /* enter message loop*/
    {
      while (WinGetMsg (hab, &qmsg, 0, 0, 0))
	{
	  WinDispatchMsg (hab, &qmsg);
	}      

      WinStopTimer (hab, hwndFrame, ID_TIMER);
    }
  
  else    /* display error message*/
    WinMessageBox (HWND_DESKTOP, hwndFrame, TIMER_TEXT, TIMER_TITLE, 0,
		   MB_OK | MB_WARNING | MB_MOVEABLE );
  
  /* destroy window frame and exit*/
  WinDestroyWindow (hwndFrame);
  WinDestroyMsgQueue (hmq);
  WinTerminate (hab);
  return 0;
}


VOID ReviseSystemMenu (VOID)
{
  HWND hwndSysMenu;
  MENUITEM miSysMenu;
  SHORT sSysMenu, i;
  static PCHAR pItemIns[MI_COUNT]  = { MI_STR };
  static SHORT sItemDel[MIR_COUNT] = { MIR_DATA };
    static MENUITEM mi[MI_COUNT]     = { MI_DATA };
  
  /* get handle of the system menu*/
  hwndSysMenu = WinWindowFromID (hwndFrame, FID_SYSMENU);
  
  /* get ID of the system menu bitmap*/
  sSysMenu = SHORT1FROMMR (
            WinSendMsg (hwndSysMenu, MM_ITEMIDFROMPOSITION, 0, 0));
  
  /* get handle of system submenu (in miSysMenu structure)*/
    WinSendMsg (hwndSysMenu, MM_QUERYITEM,
		MPFROM2SHORT (sSysMenu, FALSE), MPFROMP (&miSysMenu));
  
  /* remove disabled and unneeded menu items*/
  for (i = 0; i < MIR_COUNT; i++)
    WinSendMsg (miSysMenu.hwndSubMenu, MM_REMOVEITEM,
                MPFROM2SHORT (sItemDel[i], FALSE), 0);
  
  /* insert new menu items*/
  for (i = 0; i < MI_COUNT; i++)
    WinSendMsg (miSysMenu.hwndSubMenu, MM_INSERTITEM,
                MPFROMP (mi + i), MPFROMP (pItemIns[i]));
    }


LONG GetTimeUP (VOID)
{
  LONG X;
  DosQuerySysInfo (14, 14, &X, sizeof X); /* get Time elapsed since boot*/
  return X;
}



BOOL GetIniInfo (VOID)  /* get data from OS2.INI, if it exists*/
{
  ULONG length = sizeof ini;
  HINI hini = HINI_USERPROFILE;
  BOOL rval;
  rval = PrfQueryProfileData (hini, MEM_APP, MEM_KEY, &ini, &length);
  if (!rval)
    {
      ini.savescrpos  = TRUE;     /* set defaults*/
      ini.format  = ARB_TIMEUP;
    }
  if (ini.format == ARB_TIMEUP) bDisplayTimeUP = TRUE;
  return rval;
}


VOID SetFramePosition (PRECTL prect)
{
  CHAR pPtr[30];
  HPS hps ;
  POINTL apt[TXTBOX_COUNT];
  SHORT xMax, yMax;
  
  /* produce initial string*/
  strcpy (pPtr, "  Time Since Boot  R ");
  
  /* get length of text string*/
  hps = WinGetPS (hwndFrame);
  GpiQueryTextBox (hps, strlen (pPtr), pPtr, TXTBOX_COUNT, apt);
  WinReleasePS (hps);
  
  /* put text length + room for system menu into RECTL */
    prect->xLeft = prect->yBottom = prect->yTop = 0;
  prect->xRight = (apt[2].x - apt[0].x) + (apt[0].y - apt[1].y);
  
  /* expand text string to frame coordinates*/
  WinCalcFrameRect (hwndFrame, prect, FALSE);
  
    /* get current absolute screen size*/
  xMax = WinQuerySysValue (HWND_DESKTOP, SV_CXSCREEN);
  yMax = WinQuerySysValue (HWND_DESKTOP, SV_CYSCREEN);
  
  if (GetIniInfo ())
        {
	  /* set window position to OS2.INI data*/
	  prect->xLeft = ini.pt.x;
	  prect->yBottom = ini.pt.y;
	  
	  /* logic to ensure window displays on current screen size*/
	  if ((prect->xLeft + prect->xRight / 2 - 3) > xMax)
            prect->xLeft = xMax - prect->xRight / 2 + 3;
	  if (prect->xLeft + 3 < 0)
            prect->xLeft = -3;
	  if ((prect->yBottom + prect->yTop - 3) > yMax)
            prect->yBottom = yMax - prect->yTop + 3;
	  if (prect->yBottom + 3 < 0)
            prect->yBottom = -3;
        }
  else
    {
      /* set window position to center of screen (default)*/
      prect->xLeft = (xMax - prect->xRight) / 2;
      prect->yBottom = (yMax - prect->yTop) / 2;
    }
  
  /* set INIINFO structure to current values*/
  ini.pt.x = prect->xLeft;
  ini.pt.y = prect->yBottom;
}


MRESULT EXPENTRY 
  NewFrameProc (HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  ULONG MSecondsUP;
  static  CHAR oldtext[40];
  CHAR pTempText[40];
  HINI hini = HINI_USERPROFILE;
  POINTL pt;
  MRESULT rc;
  LONG display;

  switch (msg)
    {
    case WM_TIMER:
      MSecondsUP = GetTimeUP();
      display = (bDisplayTimeUP) ? MSecondsUP : ini.record;
      strcpy(pTempText, NumToDate(display));
      if (!bDisplayTimeUP) strcat(pTempText, " R");
      if (strcmp(oldtext, pTempText)) /* determine whether or not to update */
	{                             /* the display */
	  strcpy(oldtext, pTempText);
	  WinSetWindowText (hwndFrame, pTempText);

	  /* check if update of record is needed */
	  if (MSecondsUP >= ini.record)
	    {
	      ini.record = MSecondsUP;
	      PrfWriteProfileData(hini,MEM_APP,MEM_KEY,&ini,(ULONG)sizeof ini);
	}
	}
      return 0 ;
      
    case WM_PAINT:
    case WM_ACTIVATE:
      /* set title bar hilite to always ON*/
      rc = DefaultFrameProc (hwnd, msg, mp1, mp2);
      WinSendMsg (hwndTitleBar, TBM_SETHILITE, (MPARAM) TRUE, 0);
      return rc;
      
    case WM_COMMAND:
      switch (CMD)
	{
	case IDM_ABOUT:     /* open About dialog box*/
	  WinDlgBox (HWND_DESKTOP, hwnd, 
		     (PFNWP) AboutProc, 0, IDD_ABOUT, 0);    
	  return 0;
	  
	case IDM_OPTIONS:   /* open Options dialog box*/
	  WinDlgBox (HWND_DESKTOP, hwnd, 
		     (PFNWP) OptionsProc, 0, IDD_OPTIONS, 0);
	  return 0;
	}
      break;
    case WM_BUTTON1DBLCLK:
      /* activate options dialog box with double-click of mouse*/
      /*  button 1 on the title bar  Actually does not work right now.*/
      WinDlgBox (HWND_DESKTOP, hwnd, 
		 (PFNWP) OptionsProc, 0, IDD_OPTIONS, 0);
      return 0;
      
    case WM_BUTTON2DBLCLK:
      /*  switch from Time UP to maximum Time UP with double-click of mouse*/
      /*  button 2 on the title bar*/
      bDisplayTimeUP ^= TRUE;  /* Toggle Display */
      WinSendMsg (hwndFrame, WM_TIMER, 0,  0);
      if (ini.format == ARB_TIMEUP) ini.format = ARB_RECORD;
      else ini.format = ARB_TIMEUP;
      return 0;
    case WM_QUIT:
    case WM_DESTROY:
      if (bRestoreFlag)
	/* delete data from OS2.INI*/
	PrfWriteProfileData (hini, MEM_APP, 0, 0, 0);
      else
	{
	  if (ini.savescrpos)
	    {
	      /* get current screen position*/
	      pt.x = pt.y = 0;
	      WinMapWindowPoints (hwndFrame, HWND_DESKTOP, &pt, 1);
	      ini.pt.x = pt.x;
	      ini.pt.y = pt.y;
	}
      /* write current settings to OS2.INI*/
	  PrfWriteProfileData(hini, MEM_APP, MEM_KEY, &ini, (ULONG)sizeof ini);
	}
    }
  return DefaultFrameProc (hwnd, msg, mp1, mp2);
}
  

MRESULT EXPENTRY 
  AboutProc (HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  if (msg == WM_COMMAND)
    if (CMD == DID_OK || CMD == DID_CANCEL)
      {
	WinDismissDlg (hwnd, TRUE);
	return 0;
      }
  return WinDefDlgProc (hwnd, msg, mp1, mp2);
}


MRESULT EXPENTRY 
  HelpProc (HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    if (msg == WM_COMMAND)
      if (CMD == DID_OK || CMD == DID_CANCEL)
            {
	      WinDismissDlg (hwnd, TRUE);
	      return 0;
            }
    return WinDefDlgProc (hwnd, msg, mp1, mp2);
  }


MRESULT EXPENTRY 
  OptionsProc (HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  SHORT sTemp;
  if (msg == WM_CONTROL)
    {
      switch (SHORT1FROMMP (mp1)) /* get settings as they are changed*/
	{
	case ACB_SAVESCRPOS:
	  if (!bRestoreFlag)
	    {
	      ini.savescrpos ^= TRUE; /* toggle flag*/
	      WinSendDlgItemMsg (hwnd, ACB_SAVESCRPOS, BM_SETCHECK,
				 (MPARAM) ini.savescrpos, 0);
	    }
	  return 0;
	  
	case ACB_RESTORE:
	  bRestoreFlag ^= TRUE; /* toggle flag*/
	  if (bRestoreFlag)
	    WinSendDlgItemMsg (hwnd, ACB_SAVESCRPOS, BM_SETCHECK,
			       (MPARAM) CB_3RDSTATE, 0);
	  else
	    WinSendDlgItemMsg (hwnd, ACB_SAVESCRPOS, BM_SETCHECK,
			       (MPARAM) ini.savescrpos, 0);
	  return 0;
	  
	case ARB_RECORD:
	  bDisplayTimeUP = FALSE;  /* Toggle Display */
	  WinSendMsg (hwndFrame, WM_TIMER, 0,  0);
	  ini.format = ARB_RECORD;
	  return 0;
	case ARB_TIMEUP:
	  bDisplayTimeUP = TRUE;  /* Toggle Display */
	  WinSendMsg (hwndFrame, WM_TIMER, 0,  0);
	  ini.format = ARB_TIMEUP;
	  return 0;
	}
    }
  
  if (msg == WM_COMMAND)
    {
      switch (CMD)
	{
	case DID_OK:        /* close dialog box */
	case DID_CANCEL:
	  WinDismissDlg (hwnd, TRUE);
	  return 0;
	  
	case DID_ABOUT:
	  WinDlgBox (HWND_DESKTOP, hwnd, 
		     (PFNWP) AboutProc, 0, IDD_ABOUT, 0);    
	  return 0;
	  
	case DID_HELP:
	  WinDlgBox (HWND_DESKTOP, hwnd, 
		     (PFNWP) HelpProc, 0, IDD_HELP, 0);
	  return 0;
	}
    }
  
  if (msg == WM_INITDLG)
    {
      sTemp = bRestoreFlag ? CB_3RDSTATE : ini.savescrpos;
      
      /* send current settings to Options dialog */
      WinCheckButton (hwnd, ini.format, BM_SETCHECK);
      WinSendDlgItemMsg (hwnd, ACB_SAVESCRPOS, BM_SETCHECK,
			 MPFROM2SHORT (sTemp, 0), 0);
      WinSendDlgItemMsg (hwnd, ACB_RESTORE, BM_SETCHECK,
			 (MPARAM) bRestoreFlag, 0);
      return 0;
    }
  
  return WinDefDlgProc (hwnd, msg, mp1, mp2);
}


