/*
  LICENSE
  -------
Copyright 2005 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer. 

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution. 

  * Neither the name of Nullsoft nor the names of its contributors may be used to 
    endorse or promote products derived from this software without specific prior written permission. 
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR 
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#include <windows.h>
#include <stdlib.h>
#include <vfw.h>
#include <commctrl.h>
#include "resource.h"
#include "r_defs.h"
#include "../Agave/Language/api_language.h"
#include <stdio.h>


#ifndef LASER

#define MOD_NAME "Render / Timescope"
#define C_THISCLASS C_TimescopeClass

class C_THISCLASS : public C_RBASE {
	protected:
	public:
		C_THISCLASS();
		virtual ~C_THISCLASS();
		virtual int render(char visdata[2][2][576], int isBeat, int *framebuffer, int *fbout, int w, int h);
		virtual char *get_desc() { static char desc[128]; return (!desc[0]?WASABI_API_LNGSTRING_BUF(IDS_RENDER_TIMESCOPE,desc,128):desc); }
		virtual HWND conf(HINSTANCE hInstance, HWND hwndParent);
		virtual void load_config(unsigned char *data, int len);
		virtual int  save_config(unsigned char *data);
		int which_ch;
    int enabled;
    int color;
		int blend, blendavg;
		int nbands;
		int x;
		int oldh;
	};


static C_THISCLASS *g_ConfigThis; // global configuration dialog pointer 
static HINSTANCE g_hDllInstance; // global DLL instance pointer (not needed in this example, but could be useful)


C_THISCLASS::~C_THISCLASS() 
{
}

// configuration read/write

C_THISCLASS::C_THISCLASS() // set up default configuration
{
	x = 0;
	color = 0xFFFFFF;
  blend = 2;
  blendavg = 0;
  enabled=1;
	nbands = 576;
	oldh=-1;
  which_ch=2;
}

#define GET_INT() (data[pos]|(data[pos+1]<<8)|(data[pos+2]<<16)|(data[pos+3]<<24))
void C_THISCLASS::load_config(unsigned char *data, int len) // read configuration of max length "len" from data.
{
	int pos=0;
	if (len-pos >= 4) { enabled=GET_INT(); pos+=4; }
	if (len-pos >= 4) { color=GET_INT(); pos+=4; }
	if (len-pos >= 4) { blend=GET_INT(); pos+=4; }
	if (len-pos >= 4) { blendavg=GET_INT(); pos+=4; }
	if (len-pos >= 4) { which_ch=GET_INT(); pos+=4; }
	if (len-pos >= 4) { nbands=GET_INT(); pos+=4; }
	oldh=-1;
}

#define PUT_INT(y) data[pos]=(y)&255; data[pos+1]=(y>>8)&255; data[pos+2]=(y>>16)&255; data[pos+3]=(y>>24)&255
int  C_THISCLASS::save_config(unsigned char *data) // write configuration to data, return length. config data should not exceed 64k.
{
  int pos=0;
  PUT_INT(enabled); pos+=4;
  PUT_INT(color); pos+=4;
  PUT_INT(blend); pos+=4;
  PUT_INT(blendavg); pos+=4;
	PUT_INT(which_ch); pos+=4;
	PUT_INT(nbands); pos+=4;
  return pos;
}

// render function
// render should return 0 if it only used framebuffer, or 1 if the new output data is in fbout. this is
// used when you want to do something that you'd otherwise need to make a copy of the framebuffer.
// w and h are the width and height of the screen, in pixels.
// isBeat is 1 if a beat has been detected.
// visdata is in the format of [spectrum:0,wave:1][channel][band].

int C_THISCLASS::render(char visdata[2][2][576], int isBeat, int *framebuffer, int *fbout, int w, int h)
{
	int i,j;
	int c;
  char center_channel[576];
  unsigned char *fa_data;

  if (!enabled) return 0;
  if (isBeat&0x80000000) return 0;

  if (which_ch >=2)
  {
    for (j = 0; j < 576; j ++) center_channel[j]=visdata[1][0][j]/2+visdata[1][1][j]/2;
    fa_data=(unsigned char *)center_channel;
  }
  else fa_data=(unsigned char *)&visdata[1][which_ch][0];

  x++;
	x %= w;
  framebuffer+=x;
  int r,g,b;
  r=color&0xff;
  g=(color>>8)&0xff;
  b=(color>>16)&0xff;
	for (i=0;i<h;i++)
		{
		c = visdata[0][0][(i*nbands)/h] & 0xFF;
		c = (r*c)/256 + (((g*c)/256)<<8) + (((b*c)/256)<<16);
    if (blend == 2)
      BLEND_LINE(framebuffer,c);
    else if (blend == 1)
      framebuffer[0]=BLEND(framebuffer[0],c);
    else if (blendavg)
      framebuffer[0]=BLEND_AVG(framebuffer[0],c);
    else
      framebuffer[0]=c;
    framebuffer+=w;
		}

	return 0;
}


// configuration dialog stuff


static BOOL CALLBACK g_DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,LPARAM lParam)
{
switch (uMsg)
	{
	case WM_INITDIALOG:
        if (g_ConfigThis->enabled) CheckDlgButton(hwndDlg,IDC_CHECK1,BST_CHECKED);
        if (g_ConfigThis->blend==2) CheckDlgButton(hwndDlg,IDC_DEFAULTBLEND,BST_CHECKED);
        else if (g_ConfigThis->blend) CheckDlgButton(hwndDlg,IDC_ADDITIVE,BST_CHECKED);
        if (g_ConfigThis->blendavg) CheckDlgButton(hwndDlg,IDC_5050,BST_CHECKED);
        if (!g_ConfigThis->blend && !g_ConfigThis->blendavg)
					CheckDlgButton(hwndDlg,IDC_REPLACE,BST_CHECKED);
				SendDlgItemMessage(hwndDlg, IDC_BANDS, TBM_SETTICFREQ, 32, 0);
				SendDlgItemMessage(hwndDlg, IDC_BANDS, TBM_SETRANGE, TRUE, MAKELONG(16, 576));
				SendDlgItemMessage(hwndDlg, IDC_BANDS, TBM_SETPOS, TRUE, g_ConfigThis->nbands);
				{
				char txt[64];				
				wsprintf(txt, WASABI_API_LNGSTRING(IDS_DRAW_X_BANDS), g_ConfigThis->nbands);
				SetDlgItemText(hwndDlg, IDC_BANDTXT, txt);
				}
      if (g_ConfigThis->which_ch==0)
        CheckDlgButton(hwndDlg,IDC_LEFT,BST_CHECKED);
      else if (g_ConfigThis->which_ch==1)
        CheckDlgButton(hwndDlg,IDC_RIGHT,BST_CHECKED);
      else
        CheckDlgButton(hwndDlg,IDC_CENTER,BST_CHECKED);
		return 1;
	case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT *di=(DRAWITEMSTRUCT *)lParam;
			if (di->CtlID == IDC_DEFCOL) // paint nifty color button
			{
        int w=di->rcItem.right-di->rcItem.left;
        int _color=g_ConfigThis->color;
          _color = ((_color>>16)&0xff)|(_color&0xff00)|((_color<<16)&0xff0000);

	        HPEN hPen,hOldPen;
	        HBRUSH hBrush,hOldBrush;
	        LOGBRUSH lb={ (COLORREF)BS_SOLID,(COLORREF)_color,(COLORREF)0};
	        hPen = (HPEN)CreatePen(PS_SOLID,0,_color);
	        hBrush = CreateBrushIndirect(&lb);
	        hOldPen=(HPEN)SelectObject(di->hDC,hPen);
	        hOldBrush=(HBRUSH)SelectObject(di->hDC,hBrush);
	        Rectangle(di->hDC,di->rcItem.left,di->rcItem.top,di->rcItem.right,di->rcItem.bottom);
	        SelectObject(di->hDC,hOldPen);
	        SelectObject(di->hDC,hOldBrush);
	        DeleteObject(hBrush);
	        DeleteObject(hPen);
      }
	  }
		return 0;
	case WM_COMMAND:
      if ((LOWORD(wParam) == IDC_CHECK1) ||
	     (LOWORD(wParam) == IDC_ADDITIVE) ||
	     (LOWORD(wParam) == IDC_REPLACE) ||
	     (LOWORD(wParam) == IDC_5050) || 
       (LOWORD(wParam) == IDC_DEFAULTBLEND))
      {
        g_ConfigThis->enabled=IsDlgButtonChecked(hwndDlg,IDC_CHECK1)?1:0;
		    g_ConfigThis->blend=IsDlgButtonChecked(hwndDlg,IDC_ADDITIVE)?1:0;
        if (!g_ConfigThis->blend)
          g_ConfigThis->blend=IsDlgButtonChecked(hwndDlg,IDC_DEFAULTBLEND)?2:0;       
		    g_ConfigThis->blendavg=IsDlgButtonChecked(hwndDlg,IDC_5050)?1:0;
      }
      if (LOWORD(wParam) == IDC_DEFCOL) // handle clicks to nifty color button
      {
      int *a=&(g_ConfigThis->color);
      static COLORREF custcolors[16];
      CHOOSECOLOR cs;
      cs.lStructSize = sizeof(cs);
      cs.hwndOwner = hwndDlg;
      cs.hInstance = 0;
      cs.rgbResult=((*a>>16)&0xff)|(*a&0xff00)|((*a<<16)&0xff0000);
      cs.lpCustColors = custcolors;
      cs.Flags = CC_RGBINIT|CC_FULLOPEN;
      if (ChooseColor(&cs))
	        {
		        *a = ((cs.rgbResult>>16)&0xff)|(cs.rgbResult&0xff00)|((cs.rgbResult<<16)&0xff0000);
	        	g_ConfigThis->color = *a;
					}
      InvalidateRect(GetDlgItem(hwndDlg,IDC_DEFCOL),NULL,TRUE);            
      }
    if (LOWORD(wParam) == IDC_LEFT || LOWORD(wParam) == IDC_RIGHT || LOWORD(wParam)==IDC_CENTER)
      {
        if (IsDlgButtonChecked(hwndDlg,IDC_LEFT)) g_ConfigThis->which_ch=0;
        else if (IsDlgButtonChecked(hwndDlg,IDC_RIGHT)) g_ConfigThis->which_ch=1;
        else g_ConfigThis->which_ch=2;
      }
			break;
	case WM_NOTIFY:
		if (LOWORD(wParam) == IDC_BANDS)
			{
			g_ConfigThis->nbands = SendDlgItemMessage(hwndDlg, IDC_BANDS, TBM_GETPOS, 0, 0);
				{
				char txt[64];				
				wsprintf(txt, WASABI_API_LNGSTRING(IDS_DRAW_X_BANDS), g_ConfigThis->nbands);
				SetDlgItemText(hwndDlg, IDC_BANDTXT, txt);
				}
			}
		return 0;
	}
return 0;
}


HWND C_THISCLASS::conf(HINSTANCE hInstance, HWND hwndParent) // return NULL if no config dialog possible
{
	g_ConfigThis = this;
	return WASABI_API_CREATEDIALOG(IDD_CFG_TIMESCOPE,hwndParent,g_DlgProc);
}



// export stuff

C_RBASE *R_Timescope(char *desc) // creates a new effect object if desc is NULL, otherwise fills in desc with description
{
	if (desc) { strcpy(desc,MOD_NAME); return NULL; }
	return (C_RBASE *) new C_THISCLASS();
}

#else
C_RBASE *R_Timescope(char *desc) // creates a new effect object if desc is NULL, otherwise fills in desc with description
{return NULL; }
#endif