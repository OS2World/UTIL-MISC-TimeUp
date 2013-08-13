
/* VMEM.C - Copyright (c) 1992 M.B.Mallory
// Compiler: IBM C Set/2 1.0 

// VMEM displays the amount of virtual memory available to the user
// in the smallest window possible... a title bar. VMEM can run along
// the edge of the screen (or under it's own icon) with no loss of 
// screen real estate.*/

#define INCL_WIN
#define INCL_GPIPRIMITIVES
#define INCL_DOSMISC

#include <os2.h>
#include <string.h>
#include "vmem.h"

#define CMD (COMMANDMSG(&msg)->cmd)


/* the following  3 lines were added by Ronald Van Iwaarden, 05/03/93 so
   that this program would compile under EMX 0.8f */
typedef LHANDLE HINI;
typedef HINI *PHINI;
#define HINI_USERPROFILE   (HINI) -1L



/* data structure and profile info to be written to OS2.INI */
typedef struct
   {
   POINTL pt;
   SHORT savescrpos;
   SHORT format;
   } INIINFO;

#define MEM_APP      "VMEM"
#define MEM_KEY      "OptionsData"

/* define items to remove from the System menu*/
#define MIR_COUNT    5
#define MIR_DATA     SC_RESTORE, SC_SIZE, SC_HIDE, SC_MINIMIZE, SC_MAXIMIZE

/* define additions to the System menu*/
#define MI_COUNT     4
#define IDM_ABOUT    88
#define IDM_OPTIONS  89
#define MI_STR       0, 0, "~About  VMEM", "VMEM  ~Options"
#define MI_DATA      MIT_END, MIS_SEPARATOR, 0, 0, 0, 0,\
                     MIT_END, MIS_SEPARATOR, 0, 0, 0, 0,\
                     MIT_END, MIS_TEXT, 0, IDM_ABOUT, 0, 0,\
                     MIT_END, MIS_TEXT, 0, IDM_OPTIONS, 0, 0

/* initial title bar text*/
#define TITLEBAR_TEXT "Virtual Memory"

/* no timers error message*/
#define TIMER_TITLE  "Error initializing  VMEM"
#define TIMER_TEXT   "VMEM cannot be initialized because"\
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
LONG GetMemAvail (VOID);
BOOL GetIniInfo (VOID);
/* Commented out by Ronald Van Iwaarden, 05/03/93
   CHAR * _System ULongToString (LONG number, LONG width, LONG dec_places);*/
void _setuparg (void);



CHAR * ULongToString (LONG number, LONG width, LONG dec_places)

/* this was added by Ronald Van Iwaarden on 05/03/93 since it is not an
   included call in EMX 0.8f.  I do not make any claims as to the quality
   of this piece of code and neither do I claim that it models the code
   in the icc compiler very well.
*/

{
  static char str[12];
  LONG pwrof10 = 1;
  int temp;

  while(pwrof10<number) pwrof10 = pwrof10 * 10;
  strcpy(str,"");
  while (pwrof10 > 1){
    if (dec_places == (int) log(pwrof10)/LOG_10) strcat(str,".");
    if ((strlen(str) > 0) && (3 == (int) log(pwrof10)/LOG_10)) strcat(str,",");
    if ((strlen(str) > 0) && (6 == (int) log(pwrof10)/LOG_10)) strcat(str,",");
    if ((strlen(str) > 0) && (9 == (int) log(pwrof10)/LOG_10)) strcat(str,",");
    pwrof10 = pwrof10/10;
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
  };
  return str;
};




/* globals*/
HWND hwndFrame, hwndTitleBar;
INIINFO ini;
BOOL bRestoreFlag = FALSE;
LONG lPrevMemAvail;

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
            VMEM_ICON };

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
            WinDispatchMsg (hab, &qmsg);

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


LONG GetMemAvail (VOID)
    {
    LONG X;
    DosQuerySysInfo (19, 19, &X, sizeof X); /* get available virtual memory*/
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
        ini.format  = ARB_DISPLAYMB;
        }
    return rval;
    }


VOID SetFramePosition (PRECTL prect)
    {
    CHAR pPtr[30];
    HPS hps ;
    POINTL apt[TXTBOX_COUNT];
    SHORT xMax, yMax;

    /* produce initial string*/
    strcpy (pPtr, ULongToString (GetMemAvail () / ONE_KB, 0, 0));
    strcat (pPtr, "   K available    ");
    
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
    CHAR pTempText[40];
    HINI hini = HINI_USERPROFILE;
    POINTL pt;
    LONG lTemp, lMemAvail;
    MRESULT rc;

    switch (msg)
        {
        case WM_TIMER:
            lMemAvail = GetMemAvail ();
            if (lMemAvail != lPrevMemAvail)
                {
                if (lMemAvail > 0)
                    {
                    if (ini.format == ARB_DISPLAYMB)
                        {
                        lTemp = lMemAvail / TENTH_MB;
                        if (lMemAvail % TENTH_MB > TENTH_MB / 2)
                            lTemp++;
                        strcpy (pTempText, ULongToString (lTemp, 0, 1));
                        strcat (pTempText, " MB available");
                        }
                    else
                        {
                        lTemp = lMemAvail / ONE_KB;
                        strcpy (pTempText, ULongToString (lTemp, 0, 0));
                        strcat (pTempText, " K available");
                        }
                    }
                else
                    strcpy (pTempText, "Memory Depleted.") ;

                WinSetWindowText (hwndFrame, pTempText);
                lPrevMemAvail = lMemAvail;
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

        case WM_BUTTON2DBLCLK:
            /* activate options dialog box with double-click of mouse*/
            /*  button 2 on the title bar*/
            WinDlgBox (HWND_DESKTOP, hwnd, 
                    (PFNWP) OptionsProc, 0, IDD_OPTIONS, 0);
            return 0;

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
                PrfWriteProfileData (hini, MEM_APP, MEM_KEY, &ini, (ULONG)sizeof ini);
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

            case ARB_DISPLAYMB:
            case ARB_DISPLAYKB:
                ini.format = SHORT1FROMMP (mp1);
                lPrevMemAvail = 0;  /* force data update*/
                WinSendMsg (hwndFrame, WM_TIMER, 0, 0);
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


