/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of Spearmint Source Code.

Spearmint Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Spearmint Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Spearmint Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, Spearmint Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
// console.c

#include "client.h"


int g_console_field_width = 78;


#define		CON_TEXTSIZE	32768
typedef struct {
	qboolean	initialized;

	short	text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line

	int 	linewidth;		// characters across screen
	int		totallines;		// total lines in console scrollback

	float	xadjust;		// for wide aspect screens

	float	displayFrac;	// aproaches finalFrac at scr_conspeed
	float	finalFrac;		// 0.0 to 1.0 lines of console to display

	int		vislines;		// in scanlines

	vec4_t	color;
} console_t;

extern	console_t	con;

console_t	con;

cvar_t		*con_conspeed;

#define	DEFAULT_CONSOLE_WIDTH	78


/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void) {
	// Can't toggle the console when it's the only thing available
	if ( clc.state == CA_DISCONNECTED && Key_GetCatcher( ) == KEYCATCH_CONSOLE ) {
		return;
	}

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_CONSOLE );
}

/*
===================
Con_ToggleMenu_f
===================
*/
void Con_ToggleMenu_f( void ) {
	CL_KeyEvent( K_ESCAPE, qtrue, Sys_Milliseconds() );
	CL_KeyEvent( K_ESCAPE, qfalse, Sys_Milliseconds() );
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void) {
	chat_playerNum = -1;
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;

	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void) {
	chat_playerNum = -1;
	chat_team = qtrue;
	Field_Clear( &chatField );
	chatField.widthInChars = 25;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode3_f
================
*/
void Con_MessageMode3_f (void) {
	chat_playerNum = VM_Call( cgvm, CG_CROSSHAIR_PLAYER, 0 );
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode4_f
================
*/
void Con_MessageMode4_f (void) {
	chat_playerNum = VM_Call( cgvm, CG_LAST_ATTACKER, 0 );
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void) {
	int		i;

	for ( i = 0 ; i < CON_TEXTSIZE ; i++ ) {
		con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}

	Con_Bottom();		// go to end
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f (void)
{
	int		l, x, i;
	short	*line;
	fileHandle_t	f;
	char	buffer[1024];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: condump <filename>\n");
		return;
	}

	Com_Printf ("Dumped console text to %s.\n", Cmd_Argv(1) );

	f = FS_FOpenFileWrite( Cmd_Argv( 1 ) );
	if (!f)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1 ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x=0 ; x<con.linewidth ; x++)
			if ((line[x] & 0xff) != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	// write the remaining lines
	buffer[con.linewidth] = 0;
	for ( ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for(i=0; i<con.linewidth; i++)
			buffer[i] = line[i] & 0xff;
		for (x=con.linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		strcat( buffer, "\n" );
		FS_Write(buffer, strlen(buffer), f);
	}

	FS_FCloseFile( f );
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void ) {
	Cmd_TokenizeString( NULL );
	CL_GameConsoleText( );
}

						

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	short	tbuf[CON_TEXTSIZE];

#ifdef IOQ3ZTM // FONT_REWRITE
	if (cls.glconfig.vidWidth > 0)
		width = (cls.glconfig.vidWidth / Com_FontCharWidth(&cls.fontSmall, '.', 0)) - 2;
	else
#endif
	width = (SCREEN_WIDTH / SMALLCHAR_WIDTH) - 2;

	if (width == con.linewidth)
		return;

#ifndef IOQ3ZTM // UNUSED
	if (width < 1)			// video hasn't been initialized yet
	{
		width = DEFAULT_CONSOLE_WIDTH;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		for(i=0; i<CON_TEXTSIZE; i++)

			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}
	else
#endif
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;
	
		if (con.linewidth < numchars)
			numchars = con.linewidth;

		Com_Memcpy (tbuf, con.text, CON_TEXTSIZE * sizeof(short));
		for(i=0; i<CON_TEXTSIZE; i++)

			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';


		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}

/*
==================
Cmd_CompleteTxtName
==================
*/
void Cmd_CompleteTxtName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "", "txt", qfalse, qtrue );
	}
}


/*
================
Con_Init
================
*/
void Con_Init (void) {
	int		i;

	con_conspeed = Cvar_Get ("scr_conspeed", "3", 0);

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;
	for ( i = 0 ; i < COMMAND_HISTORY ; i++ ) {
		Field_Clear( &historyEditLines[i] );
		historyEditLines[i].widthInChars = g_console_field_width;
	}
	CL_LoadConsoleHistory( );

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglemenu", Con_ToggleMenu_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("messagemode3", Con_MessageMode3_f);
	Cmd_AddCommand ("messagemode4", Con_MessageMode4_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	Cmd_SetCommandCompletionFunc( "condump", Cmd_CompleteTxtName );
}

/*
================
Con_Shutdown
================
*/
void Con_Shutdown(void)
{
	Cmd_RemoveCommand("toggleconsole");
	Cmd_RemoveCommand("togglemenu");
	Cmd_RemoveCommand("messagemode");
	Cmd_RemoveCommand("messagemode2");
	Cmd_RemoveCommand("messagemode3");
	Cmd_RemoveCommand("messagemode4");
	Cmd_RemoveCommand("clear");
	Cmd_RemoveCommand("condump");
}

/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	int		i;

	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	for(i=0; i<con.linewidth; i++)
		con.text[(con.current%con.totallines)*con.linewidth+i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
}

/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void CL_ConsolePrint( char *txt ) {
	int		y, l;
	unsigned char	c;
	unsigned short	color;
	qboolean skipnotify = qfalse;		// NERVE - SMF
#ifdef IOQ3ZTM // FONT_REWRITE
	float	lineWidth;
	float	wordWidth;
	int		i;
#endif

	// TTimo - prefix for text that shows up in console but not in notify
	// backported from RTCW
	if ( !Q_strncmp( txt, "[skipnotify]", 12 ) ) {
		skipnotify = qtrue;
		txt += 12;
	}
	
	// for some demos we don't want to ever show anything on the console
	if ( cl_noprint && cl_noprint->integer ) {
		return;
	}
	
	if (!con.initialized) {
		con.color[0] = 
		con.color[1] = 
		con.color[2] =
		con.color[3] = 1.0f;
		con.linewidth = -1;
		Con_CheckResize ();
		con.initialized = qtrue;
	}

	if( !skipnotify && !( Key_GetCatcher( ) & KEYCATCH_CONSOLE ) ) {
		Cmd_SaveCmdContext( );

		// feed the text to cgame
		Cmd_TokenizeString( txt );
		CL_GameConsoleText( );

		Cmd_RestoreCmdContext( );
	}

	color = ColorIndex(COLOR_WHITE);

	while ( (c = *((unsigned char *) txt)) != 0 ) {
		if ( Q_IsColorString( txt ) ) {
			color = ColorIndex( *(txt+1) );
			txt += 2;
			continue;
		}

		// count word length
		for (l=0 ; l< con.linewidth ; l++) {
			if ( txt[l] <= ' ') {
				break;
			}

		}

		// word wrap
#ifdef IOQ3ZTM // FONT_REWRITE
		// ZTM: FIXME?: (Not sure this can really be fixed) For this to work correctly at startup we need,
		//   some cvars and the render started... Cvars can be easily fixed, but to load the font we need the render.
		if (cls.glconfig.vidWidth && cl_conXOffset) {
			int screenWidth = cls.glconfig.vidWidth;

			lineWidth = con.xadjust + cl_conXOffset->integer + Com_FontCharWidth(&cls.fontSmall, ']', 0);

			for (i = 0; i < con.x; i++) {
				lineWidth += Com_FontCharWidth(&cls.fontSmall, con.text[(con.current%con.totallines)*con.linewidth+i]&255, 0);
			}

			wordWidth = Com_FontStringWidthExt( &cls.fontSmall, txt, 0, l+1, qtrue );

			if (l != con.linewidth && (con.x + l >= con.linewidth)) {
				Con_Linefeed();
			} else if (lineWidth + wordWidth >= screenWidth) {
				Con_Linefeed();
			}
		}
		else
#endif
		if (l != con.linewidth && (con.x + l >= con.linewidth) ) {
			Con_Linefeed();

		}

		txt++;

		switch (c)
		{
		case '\n':
			Con_Linefeed ();
			break;
		case '\r':
			con.x = 0;
			break;
		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = (color << 8) | c;
			con.x++;
			if(con.x >= con.linewidth)
				Con_Linefeed();
			break;
		}
	}
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

Draw the editline after a ] prompt
================
*/
void Con_DrawInput (void) {
	int		y;
	int		x;

	if ( clc.state != CA_DISCONNECTED && !(Key_GetCatcher( ) & KEYCATCH_CONSOLE ) ) {
		return;
	}

#ifdef IOQ3ZTM // FONT_REWRITE
	y = con.vislines - ( Com_FontCharHeight(&cls.fontSmall, 0) * 2 );
	x = con.xadjust + cl_conXOffset->integer + Com_FontCharWidth(&cls.fontSmall, ']', 0);
#else
	y = con.vislines - ( SMALLCHAR_HEIGHT * 2 );
	x = con.xadjust + 2 * SMALLCHAR_WIDTH;
#endif

	re.SetColor( con.color );

#ifdef IOQ3ZTM // FONT_REWRITE
	SCR_DrawFontChar(&cls.fontSmall, con.xadjust + cl_conXOffset->integer, y, ']', qfalse);

	Field_Draw( &g_consoleField, x, y,
		SCREEN_WIDTH - x - Com_FontCharWidth(&cls.fontSmall, ' ', 0), qtrue, qtrue );
#else
	SCR_DrawSmallChar( con.xadjust + 1 * SMALLCHAR_WIDTH, y, ']' );

	Field_Draw( &g_consoleField, x, y,
		SCREEN_WIDTH - x - SMALLCHAR_WIDTH, qtrue, qtrue );
#endif
}

#ifdef IOQ3ZTM
/*
==========
SCR_DrawPicFullWidth
==========
*/
void SCR_DrawPicFullWidth(int h480, qhandle_t hShader)
{
	float yfrac = (float)h480 / (float)SCREEN_HEIGHT;
	float x = 0, y = 0, w = cls.glconfig.vidWidth, h = cls.glconfig.vidHeight;
	const float picX = SCREEN_WIDTH;
	const float picY = SCREEN_HEIGHT;
	float yscale = h / picY; // scale shader to fit vertically
	float xscale = yscale; // w / picX;
	float s1, t1, s2, t2;
	float sDelta, tDelta;

	yscale /= yfrac;

	// Get aspect correct coords
	s1 = x/(picX * xscale);
	t1 = y/(picY * yscale);
	s2 = (x+w)/(picX * xscale);
	t2 = (y+h)/(picY * yscale);

	// Center X
	sDelta = (1.0f - s2) / 2.0f;
	s1 += sDelta;
	s2 += sDelta;

	// Scroll so bottom of image is always at the bottom of the console
	tDelta = (1.0f - t2);
	t1 += tDelta;
	t2 += tDelta;

	re.DrawStretchPic( x, y, w, h * yfrac, s1, t1, s2, t2, hShader );
}
#endif

/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole( float frac ) {
	int				i, x, y;
	int				rows;
	short			*text;
	int				row;
	int				lines;
//	qhandle_t		conShader;
	int				currentColor;

	lines = cls.glconfig.vidHeight * frac;
	if (lines <= 0)
		return;

	if (lines > cls.glconfig.vidHeight )
		lines = cls.glconfig.vidHeight;

	// on wide screens, we will center the text
	con.xadjust = 0;
	SCR_AdjustFrom640( &con.xadjust, NULL, NULL, NULL );

	// draw the background
	y = frac * SCREEN_HEIGHT;
	if ( y < 1 ) {
		y = 0;
	}
	else {
#ifdef IOQ3ZTM
		SCR_DrawPicFullWidth(y, cls.consoleShader );
#else
		SCR_DrawPic( 0, 0, SCREEN_WIDTH, y, cls.consoleShader );
#endif
	}

	SCR_FillRect( 0, y, SCREEN_WIDTH, 2, g_color_table[ColorIndex(COLOR_GREEN)] );


	// draw the version number

	re.SetColor( g_color_table[ColorIndex(COLOR_GREEN)] );

	i = strlen( Q3_VERSION );

#ifdef IOQ3ZTM // FONT_REWRITE
	float totalwidth = Com_FontStringWidthExt( &cls.fontSmall, Q3_VERSION, 0, 0, qfalse ) + cl_conXOffset->integer;
	float currentWidthLocation = 0;
 	for (x=0 ; x<i ; x++) {
         SCR_DrawFontChar( &cls.fontSmall, cls.glconfig.vidWidth - totalwidth + currentWidthLocation,
				lines - Com_FontCharHeight(&cls.fontSmall, 0), Q3_VERSION[x], qfalse );
        currentWidthLocation += Com_FontCharWidth( &cls.fontSmall, Q3_VERSION[x], 0 );
 	}
#else
	for (x=0 ; x<i ; x++) {
		SCR_DrawSmallChar( cls.glconfig.vidWidth - ( i - x + 1 ) * SMALLCHAR_WIDTH,
			lines - SMALLCHAR_HEIGHT, Q3_VERSION[x] );
	}
#endif


	// draw the text
	con.vislines = lines;
#ifdef IOQ3ZTM // FONT_REWRITE
	rows = lines/Com_FontCharHeight(&cls.fontSmall, 0);		// rows of text to draw

	y = lines - (Com_FontCharHeight(&cls.fontSmall, 0)*3);
#else
	rows = (lines-SMALLCHAR_WIDTH)/SMALLCHAR_WIDTH;		// rows of text to draw

	y = lines - (SMALLCHAR_HEIGHT*3);
#endif

	// draw from the bottom up
	if (con.display != con.current)
	{
	// draw arrows to show the buffer is backscrolled
		re.SetColor( g_color_table[ColorIndex(COLOR_RED)] );

#ifdef IOQ3ZTM // FONT_REWRITE
		int characterWidth = Com_FontCharWidth(&cls.fontSmall, '^', 0);
		for (x=con.xadjust+characterWidth ; x<cls.glconfig.vidWidth ; x+=4*characterWidth)
			SCR_DrawFontChar( &cls.fontSmall, x, y, '^', qfalse );
		y -= Com_FontCharHeight(&cls.fontSmall, 0);
#else
		for (x=0 ; x<con.linewidth ; x+=4)
			SCR_DrawSmallChar( con.xadjust + (x+1)*SMALLCHAR_WIDTH, y, '^' );
		y -= SMALLCHAR_HEIGHT;
#endif
		rows--;
	}
	
	row = con.display;

	if ( con.x == 0 ) {
		row--;
	}

	currentColor = 7;
	re.SetColor( g_color_table[currentColor] );

#ifdef IOQ3ZTM // FONT_REWRITE
	for (i=0 ; i<rows ; i++, y -= Com_FontCharHeight(&cls.fontSmall, 0), row--)
#else
	for (i=0 ; i<rows ; i++, y -= SMALLCHAR_HEIGHT, row--)
#endif
	{
		if (row < 0)
			break;
		if (con.current - row >= con.totallines) {
			// past scrollback wrap point
			continue;	
		}

		text = con.text + (row % con.totallines)*con.linewidth;

#ifdef IOQ3ZTM // USE_FREETYPE
		float currentWidthLocation = cl_conXOffset->integer;
#endif
		for (x=0 ; x<con.linewidth ; x++) {
#ifndef IOQ3ZTM // USE_FREETYPE
			if ( ( text[x] & 0xff ) == ' ' ) {
				continue;
			}
#endif

			if ( ( (text[x]>>8)&7 ) != currentColor ) {
				currentColor = (text[x]>>8)&7;
				re.SetColor( g_color_table[currentColor] );
			}
#ifdef IOQ3ZTM // FONT_REWRITE
			SCR_DrawFontChar(&cls.fontSmall, con.xadjust + currentWidthLocation, y, text[x] & 0xff, qfalse);
			currentWidthLocation += Com_FontCharWidth(&cls.fontSmall, text[x], 0);
#else
			SCR_DrawSmallChar(  con.xadjust + (x+1)*SMALLCHAR_WIDTH, y, text[x] & 0xff );
#endif
		}
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();

	re.SetColor( NULL );
}



/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole( void ) {
	// check for console width changes from a vid mode change
	Con_CheckResize ();

	// if disconnected, render console full screen
	if ( clc.state == CA_DISCONNECTED ) {
		if ( !( Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME)) ) {
			Con_DrawSolidConsole( 1.0 );
			return;
		}
	}

	if ( con.displayFrac ) {
		Con_DrawSolidConsole( con.displayFrac );
	}

	if( Key_GetCatcher( ) & ( KEYCATCH_UI | KEYCATCH_CGAME ) )
		return;

	// draw the chat line
	// ZTM: FIXME: Move to cgame or ui
	if( Key_GetCatcher( ) & KEYCATCH_MESSAGE )
	{
		int skip;

		if( chat_team ) {
			SCR_DrawBigString( 8, 232, "Team Say:", 1.0f, qfalse );
			skip = 11;
		} else {
			SCR_DrawBigString( 8, 232, "Say:", 1.0f, qfalse );
			skip = 5;
		}

		Field_BigDraw( &chatField, skip * BIGCHAR_WIDTH, 232,
				SCREEN_WIDTH - ( skip + 1 ) * BIGCHAR_WIDTH, qtrue, qfalse );
	}
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole (void) {
	// decide on the destination height of the console
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
		con.finalFrac = 0.5;		// half screen
	else
		con.finalFrac = 0;				// none visible
	
	// scroll towards the destination height
	if (con.finalFrac < con.displayFrac)
	{
		con.displayFrac -= con_conspeed->value*cls.realFrametime*0.001;
		if (con.finalFrac > con.displayFrac)
			con.displayFrac = con.finalFrac;

	}
	else if (con.finalFrac > con.displayFrac)
	{
		con.displayFrac += con_conspeed->value*cls.realFrametime*0.001;
		if (con.finalFrac < con.displayFrac)
			con.displayFrac = con.finalFrac;
	}

}


void Con_PageUp( void ) {
	con.display -= 2;
	if ( con.current - con.display >= con.totallines ) {
		con.display = con.current - con.totallines + 1;
	}
}

void Con_PageDown( void ) {
	con.display += 2;
	if (con.display > con.current) {
		con.display = con.current;
	}
}

void Con_Top( void ) {
	con.display = con.totallines;
	if ( con.current - con.display >= con.totallines ) {
		con.display = con.current - con.totallines + 1;
	}
}

void Con_Bottom( void ) {
	con.display = con.current;
}


void Con_Close( void ) {
	if ( !com_cl_running->integer ) {
		return;
	}
	Field_Clear( &g_consoleField );
	Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CONSOLE );
	con.finalFrac = 0;				// none visible
	con.displayFrac = 0;
}
