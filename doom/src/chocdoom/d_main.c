//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//	plus functions to determine game mode (shareware, registered),
//	parse command line parameters, configure game parameters (turbo),
//	and call the startup functions.
//


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deh_main.h"
#include "doomdef.h"
#include "doomstat.h"

#include "dstrings.h"
#include "doomfeatures.h"
#include "sounds.h"

#include "d_iwad.h"

#include "z_zone.h"
#include "w_main.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_video.h"

#include "f_finale.h"
#include "f_wipe.h"

#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_menu.h"
#include "p_saveg.h"

#include "i_endoom.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"

#include "g_game.h"

#include <debug.h>
#include <misc_utils.h>
#include <dev_io.h>
#include <bsp_sys.h>
#include <bsp_cmd.h>
#include <audio_main.h>

#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "net_client.h"
#include "net_dedicated.h"
#include "net_query.h"

#include "p_setup.h"
#include "r_local.h"
#include "statdump.h"

#include "d_main.h"
#include "w_merge.h"

//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//
void D_DoomLoop(void);

// Location where savegames are stored

char            *savegamedir;

// location of IWAD and WAD files

char            *iwadfile;


boolean         devparm;        // started game with -devparm
boolean         nomonsters;     // checkparm of -nomonsters
boolean         respawnparm;    // checkparm of -respawn
boolean         fastparm;       // checkparm of -fast

extern  boolean inhelpscreens;

skill_t         startskill;
int             startepisode;
int             startmap;
boolean         autostart;
int             startloadgame;

boolean         advancedemo;

// Store demo, do not accept any inputs
boolean         storedemo;

// If true, the main game loop has started.
boolean         main_loop_started = false;


int             show_endoom = 1;


void D_ConnectNetGame(void);
void D_CheckNetGame(void);


//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents(void)
{
    event_t *ev;

    while ((ev = D_PopEvent()) != NULL)
    {
	if (M_Responder (ev))
	    continue;               // menu ate the event
	G_Responder (ev);
    }
}




//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t     wipegamestate = GS_LEVEL;
extern  boolean setsizeneeded;
extern  int             showMessages;
void R_ExecuteSetViewSize (void);

void D_Display (void)
{
    static  boolean		viewactivestate = false;
    static  boolean		menuactivestate = false;
    static  boolean		inhelpscreensstate = false;
    static  boolean		fullscreen = false;
    static  gamestate_t		oldgamestate = (gamestate_t)-1;
    static  int			borderdrawcount;
    int				tics;
    int				y;
    boolean			done;
    boolean			wipe;
    boolean			redrawsbar;
    profiler_enter();

    redrawsbar = false;
    // change the view size if needed
    if (setsizeneeded)
    {
		R_ExecuteSetViewSize ();
		oldgamestate = (gamestate_t)-1;                      // force background redraw
		borderdrawcount = 3;
    }

    // save the current screen if about to wipe
    if (gamestate != wipegamestate)
		{
		wipe = true;
    }
    else
    	wipe = false;

    if (gamestate == GS_LEVEL && gametic)
    	HU_Erase();
    
    // do buffered drawing
    switch (gamestate)
    {
      case GS_LEVEL:
		if (!gametic)
			break;
		if (automapactive)
			AM_Drawer ();
		if (wipe || (viewheight != 200 && fullscreen) )
			redrawsbar = true;
		if (inhelpscreensstate && !inhelpscreens)
			redrawsbar = true;              // just put away the help screen
		ST_Drawer (viewheight == 200, redrawsbar );
		fullscreen = viewheight == 200;
		break;

      case GS_INTERMISSION:
		WI_Drawer ();
		break;

      case GS_FINALE:
		F_Drawer ();
		break;

      case GS_DEMOSCREEN:
		D_PageDrawer ();
		break;
      default:
        break;
    }
    
    // draw the view directly
    if (gamestate == GS_LEVEL && !automapactive && gametic)
    	R_RenderPlayerView (&players[displayplayer]);

    if (gamestate == GS_LEVEL && gametic)
    	HU_Drawer ();
    
    // clean up border stuff
    if (gamestate != oldgamestate && gamestate != GS_LEVEL)
    	I_SetPalette ((byte *)W_CacheLumpName (DEH_String("PLAYPAL"),PU_CACHE), 0);

    // see if the border needs to be initially drawn
    if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL)
    {
		viewactivestate = false;        // view was not active
		R_FillBackScreen ();    // draw the pattern into the back screen
    }

    // see if the border needs to be updated to the screen
    if (gamestate == GS_LEVEL && !automapactive && scaledviewwidth != 320)
    {
		if (menuactive || menuactivestate || !viewactivestate)
			borderdrawcount = 3;
		if (borderdrawcount)
		{
			R_DrawViewBorder ();    // erase old menu stuff
			borderdrawcount--;
		}
    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;
    
    // draw pause pic
    if (paused)
    {
		if (automapactive)
			y = 4;
		else
			y = viewwindowy+4;
		V_DrawPatchDirect(viewwindowx + (scaledviewwidth - 68) / 2, y,
							  (patch_t *)W_CacheLumpName (DEH_String("M_PAUSE"), PU_CACHE));
    }
    if (wipe) {
        wipe_StartScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);
    }

    // menus go directly to the screen
    M_Drawer ();          // menu is drawn even on top of everything
    NetUpdate ();         // send out any new accumulation
    // draw buffered stuff to screen
    I_UpdateNoBlit ();

    // normal update
    if (!wipe)
    {
        I_FinishUpdate ();              // page flip or blit buffer
        profiler_exit();
        return;
    }
    
    // wipe update
    wipe_EndScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);

    do
    {
	done = wipe_ScreenWipe(wipe_Melt
			       , 0, 0, SCREENWIDTH, SCREENHEIGHT, tics);
	I_UpdateNoBlit ();
	M_Drawer ();                            // menu is drawn even on top of wipes
	I_FinishUpdate ();                      // page flip or blit buffer
    } while (!done);
    profiler_exit();
}

//
// Add configuration file variable bindings.
//

void D_BindVariables(void)
{
    int i;

    M_ApplyPlatformDefaults();

    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();

    M_BindBaseControls();
    M_BindWeaponControls();
    M_BindMapControls();
    M_BindMenuControls();
    M_BindChatControls(MAXPLAYERS);

    key_multi_msgplayer[0] = HUSTR_KEYGREEN;
    key_multi_msgplayer[1] = HUSTR_KEYINDIGO;
    key_multi_msgplayer[2] = HUSTR_KEYBROWN;
    key_multi_msgplayer[3] = HUSTR_KEYRED;

#ifdef FEATURE_MULTIPLAYER
    NET_BindVariables();
#endif
#if ORIGCODE
    M_BindVariable("mouse_sensitivity",      &mouseSensitivity);
#endif
    M_BindVariable("sfx_volume",             &sfxVolume);
    M_BindVariable("music_volume",           &musicVolume);
    M_BindVariable("show_messages",          &showMessages);
    M_BindVariable("screenblocks",           &screenblocks);
    M_BindVariable("detaillevel",            &detailLevel);
    M_BindVariable("snd_channels",           &snd_channels);
    M_BindVariable("vanilla_savegame_limit", &vanilla_savegame_limit);
    M_BindVariable("vanilla_demo_limit",     &vanilla_demo_limit);
    M_BindVariable("show_endoom",            &show_endoom);

    // Multiplayer chat macros

    for (i=0; i<10; ++i)
    {
        char buf[12];

        M_snprintf(buf, sizeof(buf), "chatmacro%i", i);
        M_BindVariable(buf, &chat_macros[i]);
    }
}

//
// D_GrabMouseCallback
//
// Called to determine whether to grab the mouse pointer
//

boolean D_GrabMouseCallback(void)
{
    // Drone players don't need mouse focus

    if (drone)
        return false;

    // when menu is active or game is paused, release the mouse 
 
    if (menuactive || paused)
        return false;

    // only grab mouse when playing levels (but not demos)

    return (gamestate == GS_LEVEL) && !demoplayback && !advancedemo;
}

//
//  D_DoomLoop
//
extern void DD_FpsUpdate (void);
extern void DD_FrameBegin (void);
extern void DD_FrameEnd (void);

void D_DoomLoop (void)
{
    if (demorecording)
            G_BeginRecording();

    TryRunTics();

    I_InitGraphics();

    R_ExecuteSetViewSize();

    D_StartGameLoop();

    while (1)
    {
	bsp_tickle();
        DD_FrameBegin();
        I_StartFrame ();

        TryRunTics(); // will run at least one tic

        S_UpdateSounds(players[consoleplayer].mo); // move positional sounds

        // Update display, next frame, with current state.
        if (screenvisible) {
            D_Display();
	}
	DD_ProcGameAct();
        DD_FrameEnd();
        DD_FpsUpdate();
    }
}



//
//  DEMO LOOP
//
int             demosequence;
int             pagetic;
char            *pagename;


//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker(void)
{
    if (--pagetic < 0)
	D_AdvanceDemo ();
}



//
// D_PageDrawer
//
void D_PageDrawer (void)
{
    V_DrawPatch (0, 0, (patch_t *)W_CacheLumpName(pagename, PU_CACHE));
}


//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo(void)
{
    advancedemo = true;
}


//
// This cycles through the demo sequences.
//
void D_DoAdvanceDemo(void)
{
    players[consoleplayer].playerstate = PST_LIVE;      // not reborn
    advancedemo = false;
    usergame = false;                                   // no save / end game here
    paused = false;
    gameaction = ga_nothing;

    //if (gamemode == retail)
    //    demosequence = (demosequence + 1) % 7;
    //else
    //    demosequence = (demosequence + 1) % 6;
    demosequence = 0;

    switch (demosequence)
    {
      case 0:
	if ( gamemode == commercial )
	    pagetic = TICRATE * 11;
	else
	    pagetic = 170;
	gamestate = GS_DEMOSCREEN;
	pagename = DEH_String("TITLEPIC");
	if ( gamemode == commercial )
	  S_StartMusic(mus_dm2int);
	else
	  S_StartMusic (mus_intro);
	break;
      case 1:
	G_DeferedPlayDemo(DEH_String("demo1"));
	break;
      case 2:
	pagetic = 200;
	gamestate = GS_DEMOSCREEN;
	pagename = DEH_String("CREDIT");
	break;
      case 3:
	G_DeferedPlayDemo(DEH_String("demo2"));
	break;
      case 4:
	gamestate = GS_DEMOSCREEN;
	if ( gamemode == commercial)
	{
	    pagetic = TICRATE * 11;
	    pagename = DEH_String("TITLEPIC");
	    S_StartMusic(mus_dm2ttl);
	}
	else
	{
	    pagetic = 200;

	    if ( gamemode == retail )
	      pagename = DEH_String("CREDIT");
	    else
	      pagename = DEH_String("HELP2");
	}
	break;
      case 5:
	G_DeferedPlayDemo(DEH_String("demo3"));
	break;
        // THE DEFINITIVE DOOM Special Edition demo
      case 6:
	G_DeferedPlayDemo(DEH_String("demo4"));
	break;
    }

    // The Doom 3: BFG Edition version of doom2.wad does not have a
    // TITLETPIC lump. Use INTERPIC instead as a workaround.
    if (bfgedition && !strcasecmp(pagename, "TITLEPIC")
        && W_CheckNumForName("titlepic") < 0)
    {
        pagename = DEH_String("INTERPIC");
    }
}



//
// D_StartTitle
//
void D_StartTitle (void)
{
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo ();
}

// Strings for dehacked replacements of the startup banner
//
// These are from the original source: some of them are perhaps
// not used in any dehacked patches

static char *banners[] =
{
    // doom2.wad
    "                         "
    "DOOM 2: Hell on Earth v%i.%i"
    "                           ",
    // doom1.wad
    "                            "
    "DOOM Shareware Startup v%i.%i"
    "                           ",
    // doom.wad
    "                            "
    "DOOM Registered Startup v%i.%i"
    "                           ",
    // Registered DOOM uses this
    "                          "
    "DOOM System Startup v%i.%i"
    "                          ",
    // doom.wad (Ultimate DOOM)
    "                         "
    "The Ultimate DOOM Startup v%i.%i"
    "                        ",
    // tnt.wad
    "                     "
    "DOOM 2: TNT - Evilution v%i.%i"
    "                           ",
    // plutonia.wad
    "                   "
    "DOOM 2: Plutonia Experiment v%i.%i"
    "                           ",
};

//
// Get game name: if the startup banner has been replaced, use that.
// Otherwise, use the name given
// 

static char *GetGameName(char *gamename)
{
    size_t i;
    char *deh_sub;
    
    for (i=0; i<arrlen(banners); ++i)
    {
        // Has the banner been replaced?

        deh_sub = DEH_String(banners[i]);
        
        if (deh_sub != banners[i])
        {
            size_t gamename_size;
            int version;

            // Has been replaced.
            // We need to expand via d_printf to include the Doom version number
            // We also need to cut off spaces to get the basic name

            gamename_size = strlen(deh_sub) + 10;
            gamename = (char *)Z_Malloc(gamename_size, PU_STATIC, 0);
            version = G_VanillaVersionCode();
            M_snprintf(gamename, gamename_size, deh_sub,
                       version / 100, version % 100);

            while (gamename[0] != '\0' && isspace((int)gamename[0]))
            {
                memmove(gamename, gamename + 1, gamename_size - 1);
            }

            while (gamename[0] != '\0' && isspace((int)gamename[strlen(gamename)-1]))
            {
                gamename[strlen(gamename) - 1] = '\0';
            }

            return gamename;
        }
    }

    return gamename;
}

static void SetMissionForPackName(char *pack_name)
{
    int i;
    static const struct
    {
        char *name;
        int mission;
    } packs[] = {
        { "doom2",    doom2 },
        { "tnt",      pack_tnt },
        { "plutonia", pack_plut },
    };

    for (i = 0; i < arrlen(packs); ++i)
    {
        if (!strcasecmp(pack_name, packs[i].name))
        {
            gamemission = (GameMission_t)packs[i].mission;
            return;
        }
    }

    I_Error("Unknown mission pack name: %s", pack_name);
}


//
// Find out what version of Doom is playing.
//

void D_IdentifyVersion(void)
{
    // gamemission is set up by the D_FindIWAD function.  But if
    // we specify '-iwad', we have to identify using
    // IdentifyIWADByName.  However, if the iwad does not match
    // any known IWAD name, we may have a dilemma.  Try to
    // identify by its contents.

    if (gamemission == none)
    {
        unsigned int i;

        for (i=0; i<numlumps; ++i)
        {
            if (!strncasecmp(lumpinfo[i].name, "MAP01", 8))
            {
                gamemission = doom2;
                break;
            }
            else if (!strncasecmp(lumpinfo[i].name, "E1M1", 8))
            {
                gamemission = doom;
                break;
            }
        }

        if (gamemission == none)
        {
            // Still no idea.  I don't think this is going to work.

            I_Error("Unknown or invalid IWAD file.");
        }
    }

    // Make sure gamemode is set up correctly

    if (gamemission == doom)
    {
        // Doom 1.  But which version?

        if (W_CheckNumForName("E4M1") > 0)
        {
            // Ultimate Doom

            gamemode = retail;
        }
        else if (W_CheckNumForName("E3M1") > 0)
        {
            gamemode = registered;
        }
        else
        {
            gamemode = shareware;
        }
    }
    else
    {
        // Doom 2 of some kind.

        gamemode = commercial;
    }

    //if (!bfgedition && nerve)
    //    gamemission = pack_nerve;
}

// Set the gamedescription string

void D_SetGameDescription(void)
{
    boolean is_freedoom = W_CheckNumForName("FREEDOOM") >= 0,
            is_freedm = W_CheckNumForName("FREEDM") >= 0;

    gamedescription = "Unknown";

    if (gamemission == doom)
    {
        // Doom 1.  But which version?

        if (is_freedoom)
        {
            gamedescription = GetGameName("Freedoom: Phase 1");
        }
        else if (gamemode == retail)
        {
            // Ultimate Doom

            gamedescription = GetGameName("The Ultimate DOOM");
        }
        else if (gamemode == registered)
        {
            gamedescription = GetGameName("DOOM Registered");
        }
        else if (gamemode == shareware)
        {
            gamedescription = GetGameName("DOOM Shareware");
        }
    }
    else
    {
        // Doom 2 of some kind.  But which mission?

        if (is_freedoom)
        {
            if (is_freedm)
            {
                gamedescription = GetGameName("FreeDM");
            }
            else
            {
                gamedescription = GetGameName("Freedoom: Phase 2");
            }
        }
        else if (gamemission == doom2)
        {
            gamedescription = GetGameName("DOOM 2: Hell on Earth");
        }
        else if (gamemission == pack_plut)
        {
            gamedescription = GetGameName("DOOM 2: Plutonia Experiment"); 
        }
        else if (gamemission == pack_tnt)
        {
            gamedescription = GetGameName("DOOM 2: TNT - Evilution");
        }
    }
}

// print title for every printed line
char            title[128];

static boolean D_AddFile(char *filename)
{
    wad_file_t *handle;

    handle = W_AddFile(filename, NULL);

    return handle != NULL;
}

static void D_MergeFileHdlr(void *_filename)
{
    char *filename = (char *)_filename;
    modifiedgame = true;

    if (!W_MergeFile(filename, false)) {
        dprintf("Failed to merge pwad : [%s]\n", filename);
    } else {
        dprintf("Merged pwad : [%s]\n", filename);
    }
}

static void D_AddFileHdlr(void *_filename)
{
    char *filename = (char *)_filename;
    modifiedgame = true;

    if (!W_AddFile(filename, NULL)) {
        dprintf("Failed to add pwad : [%s]\n", filename);
    } else {
        dprintf("Added pwad : [%s]\n", filename);
    }
}

void D_AddPwads (void)
{
    DD_PwadAddEach(D_AddFileHdlr);
}

void D_MergePwads (void)
{
    DD_PwadAddEach(D_MergeFileHdlr);
}

// Copyright message banners
// Some dehacked mods replace these.  These are only displayed if they are 
// replaced by dehacked.

static char *copyright_banners[] =
{
    "===========================================================================\n"
    "ATTENTION:  This version of DOOM has been modified.  If you would like to\n"
    "get a copy of the original game, call 1-800-IDGAMES or see the readme file.\n"
    "        You will not receive technical support for modified games.\n"
    "                      press enter to continue\n"
    "===========================================================================\n",

    "===========================================================================\n"
    "                 Commercial product - do not distribute!\n"
    "         Please report software piracy to the SPA: 1-800-388-PIR8\n"
    "===========================================================================\n",

    "===========================================================================\n"
    "                                Shareware!\n"
    "===========================================================================\n"
};

// Prints a message only if it has been modified by dehacked.

void PrintDehackedBanners(void)
{
    size_t i;

    for (i=0; i<arrlen(copyright_banners); ++i)
    {
        char *deh_s;

        deh_s = DEH_String(copyright_banners[i]);

        if (deh_s != copyright_banners[i])
        {

            // Make sure the modified banner always ends in a newline character.
            // If it doesn't, add a newline.  This fixes av.wad.

        }
    }
}

static struct 
{
    char *description;
    char *cmdline;
    GameVersion_t version;
} gameversions[] = {
    {"Doom 1.666",           "1.666",      exe_doom_1_666},
    {"Doom 1.7/1.7a",        "1.7",        exe_doom_1_7},
    {"Doom 1.8",             "1.8",        exe_doom_1_8},
    {"Doom 1.9",             "1.9",        exe_doom_1_9},
    {"Hacx",                 "hacx",       exe_hacx},
    {"Ultimate Doom",        "ultimate",   exe_ultimate},
    {"Final Doom",           "final",      exe_final},
    {"Final Doom (alt)",     "final2",     exe_final2},
    {"Chex Quest",           "chex",       exe_chex},
    { NULL,                  NULL,         (GameVersion_t)0},
};

char *uppercase(char *str)
{
    char *newstr;
    char *p;
#if defined(STM32_SDK)
    p = newstr = d_strdup(str);
    while (*p++ = d_toupper(*p));
#else
    p = newstr = strdup(str);
    while (*p++ = toupper(*p));
#endif
    return newstr;
}

// Initialize the game version

static void InitGameVersion(void)
{
    int p;
    int i;

    //! 
    // @arg <version>
    // @category compat
    //
    // Emulate a specific version of Doom.  Valid values are "1.9",
    // "ultimate", "final", "final2", "hacx" and "chex".
    //

    p = M_CheckParmWithArgs("-gameversion", 1);

    if (p)
    {
        for (i=0; gameversions[i].description != NULL; ++i)
        {
            if (!strcmp(myargv[p+1], gameversions[i].cmdline))
            {
                gameversion = gameversions[i].version;
                break;
            }
        }
        
        if (gameversions[i].description == NULL) 
        {
            for (i=0; gameversions[i].description != NULL; ++i)
            {
            }
            
            I_Error("Unknown game version '%s'", myargv[p+1]);
        }
    }
    else
    {
        // Determine automatically

        if (gamemode == shareware || gamemode == registered)
        {
            gameversion = exe_doom_1_9;

            // TODO: Detect IWADs earlier than Doom v1.9.
        }
        else if (gamemode == retail)
        {
            gameversion = exe_ultimate;
        }
        else if (gamemode == commercial)
        {
            if (gamemission == doom2)
            {
                gameversion = exe_doom_1_9;
            }
            else
            {
                // Final Doom: tnt or plutonia
                // Defaults to emulating the first Final Doom executable,
                // which has the crash in the demo loop; however, having
                // this as the default should mean that it plays back
                // most demos correctly.

                gameversion = exe_final;
            }
        }
    }

    // The original exe does not support retail - 4th episode not supported

    if (gameversion < exe_ultimate && gamemode == retail)
    {
        gamemode = registered;
    }

    // EXEs prior to the Final Doom exes do not support Final Doom.

    if (gameversion < exe_final && gamemode == commercial)
    {
        gamemission = doom2;
    }
}

//
// D_DoomMain
//
void D_DoomMain (void)
{
    int         p;
    char        file[256];
    char        demolumpname[9];
    int         temp;

    // print banner
    //I_PrintBanner(DD_DoomBanner);

    DEH_printf("Z_Init: Init zone memory allocation daemon. \n");
    Z_Init ();
    {
        const char *vol = "64";
        p = M_CheckParmWithArgs("-vol", 1);
        if (p > 0)
        {
            vol = myargv[p + 1];
        }
        snprintf(file, sizeof(file), "samplerate=22050, volume=%s", vol);
        audio_conf(file);
    }


    M_FindResponseFile();

    iwadfile = D_FindIWAD();

    // None found?

    if (iwadfile == NULL)
    {
        I_Error("Game mode indeterminate. No IWAD file was found. Try\n"
                "specifying one with the '-iwad' command line parameter.\n");
    }

    modifiedgame = false;

    nomonsters = M_CheckParm("-nomonsters");
    respawnparm = M_CheckParm("-respawn");
    fastparm = M_CheckParm("-fast");
    devparm = M_CheckParm ("-devparm");
    if (M_CheckParm("-altdeath"))
        deathmatch = 2;
    else if (M_CheckParm("-deathmatch"))
        deathmatch = 1;

    M_SetConfigDir(NULL);

    // turbo option
    if ((p = M_CheckParm("-turbo")))
    {
        int             scale = 200;
        extern int      forwardmove[2];
        extern int      sidemove[2];

        if (p < myargc - 1)
            scale = atoi(myargv[p + 1]);
        if (scale < 10)
            scale = 10;
        if (scale > 400)
            scale = 400;
        forwardmove[0] = forwardmove[0] * scale / 100;
        forwardmove[1] = forwardmove[1] * scale / 100;
        sidemove[0] = sidemove[0]*scale/100;
        sidemove[1] = sidemove[1]*scale/100;
    }

    // init subsystems
    V_Init();

    // Load configuration files before initialising other subsystems.
    M_LoadDefaults();

    D_AddFile(iwadfile);

    if (!W_MergeFile("doomretro.wad", false))
        I_Error("Can't find doomretro.wad.");

    p = M_CheckParmWithArgs("-file", 1);

    if (p > 0)
    {
        for (p = p + 1; p < myargc && myargv[p][0] != '-'; ++p)
        {
            char *filename;

            filename = uppercase(D_TryFindWADByName(myargv[p]));

            modifiedgame = true;

            if (W_MergeFile(filename, false))
                if (strstr(filename, "NERVE.WAD")) {
                    //nerve = true;
            }
        }
    }

    M_NEWG = W_CheckMultipleLumps("M_NEWG");
    M_EPISOD = W_CheckMultipleLumps("M_EPISOD");
    M_SKILL = W_CheckMultipleLumps("M_SKILL");
    M_SKULL1 = W_CheckMultipleLumps("M_SKULL1");
    M_LGTTL = W_CheckMultipleLumps("M_LGTTL");
    M_SGTTL = W_CheckMultipleLumps("M_SGTTL");
    M_SVOL = W_CheckMultipleLumps("M_SVOL");
    M_OPTTTL = W_CheckMultipleLumps("M_OPTTTL");
    M_MSGON = W_CheckMultipleLumps("M_MSGON");
    M_MSGOFF = W_CheckMultipleLumps("M_MSGOFF");
    M_NMARE = W_CheckMultipleLumps("M_NMARE");
    M_MSENS = W_CheckMultipleLumps("M_MSENS");
    STBAR    = W_CheckMultipleLumps("STBAR");
    STCFN034 = W_CheckMultipleLumps("STCFN034");
    STCFN039 = W_CheckMultipleLumps("STCFN039");
    WISCRT2  = W_CheckMultipleLumps("WISCRT2");
    STYSNUM0 = W_CheckMultipleLumps("STYSNUM0");
    MAPINFO  = (W_CheckNumForName("MAPINFO") >= 0);
    TITLEPIC = (W_CheckNumForName("TITLEPIC") >= 0);

    bfgedition = (W_CheckNumForName("DMENUPIC") >= 0 && W_CheckNumForName("M_ACPT") >= 0);

    p = M_CheckParmWithArgs ("-playdemo", 1);

    if (!p)
    {
        p = M_CheckParmWithArgs("-timedemo", 1);
    }

    if (p)
    {
        if (!strcasecmp(myargv[p+1] + strlen(myargv[p+1]) - 4, ".lmp"))
        {
            strcpy(file, myargv[p + 1]);
        }
        else
        {
            sprintf (file,"%s.lmp", myargv[p+1]);
        }

        if (D_AddFile(file))
        {
            strncpy(demolumpname, lumpinfo[numlumps - 1].name, 8);
            demolumpname[8] = '\0';
        }
    }

    // Generate the WAD hash table.  Speed things up a bit.

    W_GenerateHashTable();

    D_IdentifyVersion();
    InitGameVersion();
    D_SetGameDescription();
    D_SetSaveGameDir();

    // Check for -file in shareware
    if (modifiedgame)
    {
        // These are the lumps that will be checked in IWAD,
        // if any one is not present, execution will be aborted.
        char name[23][9] =
        {
            "e2m1", "e2m2", "e2m3", "e2m4", "e2m5", "e2m6", "e2m7", "e2m8", "e2m9",
            "e3m1", "e3m3", "e3m3", "e3m4", "e3m5", "e3m6", "e3m7", "e3m8", "e3m9",
            "dphoof", "bfgga0", "heada1", "cybra1", "spida1d1"
        };
        int i;

        if (gamemode == shareware)
            I_Error("You cannot use -file with the shareware version. Register!");

        // Check for fake IWAD with right name,
        // but w/o all the lumps of the registered version.
        if (gamemode == registered)
            for (i = 0; i < 23; i++)
                if (W_CheckNumForName(name[i]) < 0)
                    I_Error("This is not the registered version.");
    }

    // get skill / episode / map from parms
    startskill = sk_medium;
    startepisode = 1;
    startmap = 1;
    autostart = false;

    p = M_CheckParmWithArgs("-skill", 1);
    if (p)
    {
        temp = myargv[p + 1][0] - '1';
        if (temp >= sk_baby && temp <= sk_nightmare)
        {
            startskill = (skill_t)temp;
            autostart = true;
        }
    }

    p = M_CheckParmWithArgs("-episode", 1);
    if (p)
    {
        temp = myargv[p + 1][0] - '0';
        if ((gamemode == shareware && temp == 1)
            || (temp >= 1
                && ((gamemode == registered && temp <= 3)
                    || (gamemode == retail && temp <= 4))))
        {
            startepisode = temp;
            startmap = 1;
            autostart = true;
        }
    }

    timelimit = 0;

    p = M_CheckParmWithArgs("-timer", 1);
    if (p)
    {
        timelimit = atoi(myargv[p+1]);
    }

    p = M_CheckParm ("-avg");
    if (p)
    {
        timelimit = 20;
    }

    p = M_CheckParmWithArgs("-warp", 1);
    if (p)
    {
        if (gamemode == commercial)
            startmap = atoi(myargv[p + 1]);
        else
        {
            startepisode = myargv[p + 1][0] - '0';

            if (p + 2 < myargc)
            {
                startmap = myargv[p + 2][0] - '0';
            }
            else
            {
                startmap = 1;
            }
        }
        autostart = true;
    }


    p = M_CheckParmWithArgs("-loadgame", 1);
    if (p)
    {
        startloadgame = atoi(myargv[p+1]);
    }
    else
    {
        startloadgame = -1;
    }

    if (sfxVolume < 0)
        sfxVolume = 0;
    else if (sfxVolume > 15)
        sfxVolume = 15;
    if (musicVolume < 0)
        musicVolume = 0;
    else if (musicVolume > 15)
        musicVolume = 15;
    if (mouseSensitivity < 0)
        mouseSensitivity = 0;
    else if (mouseSensitivity > 9*2)
        mouseSensitivity = 9*2;
    gamepadSensitivity = 1.25f + mouseSensitivity / 18.0f;
    if (screenblocks < 3)
        screenblocks = 3;
    else if (screenblocks > 11)
        screenblocks = 11;
    //if (widescreen)
    //    screenblocks = 10;

    DEH_printf("\nP_Init: Init Playloop state.\n");
    P_Init ();

    DD_LoadAltPkgGame();

    DEH_printf("S_Init: Setting up sound.\n");
    S_Init (sfxVolume * 8, musicVolume * 8);

    DEH_printf("D_CheckNetGame: Checking network game status.\n");
    D_CheckNetGame ();

    DEH_printf("HU_Init: Setting up heads up display.\n");
    HU_Init ();

    DEH_printf("ST_Init: Init status bar.\n");
    ST_Init ();
    DEH_printf("Memory left: [0x%08x] bytes\n", heap_avail());

    // If Doom II without a MAP01 lump, this is a store demo.
    // Moved this here so that MAP01 isn't constantly looked up
    // in the main loop.

    if (gamemode == commercial && W_CheckNumForName("map01") < 0)
        storedemo = true;

    if (M_CheckParmWithArgs("-statdump", 1))
    {
        I_AtExit(StatDump, true);
        DEH_printf("External statistics registered.\n");
    }

    //!
    // @arg <x>
    // @category demo
    // @vanilla
    //
    // Record a demo named x.lmp.
    //

    p = M_CheckParmWithArgs("-record", 1);
    if (p)
    {
        G_RecordDemo(myargv[p + 1]);
        autostart = true;
    }

    p = M_CheckParmWithArgs("-playdemo", 1);
    if (p)
    {
        singledemo = true;                      // quit after one demo
        G_DeferredPlayDemo(demolumpname);
        D_DoomLoop();                           // never returns
    }

    p = M_CheckParmWithArgs("-timedemo", 1);
    if (p)
    {
        G_TimeDemo(demolumpname);
        D_DoomLoop();                           // never returns
    }

    if (startloadgame >= 0)
    {
        strcpy(file, P_SaveGameFile(startloadgame));
        G_LoadGame(file);
    }

    if (gameaction != ga_loadgame)
    {
        if (autostart || netgame)
            G_InitNew(startskill, startepisode, startmap);
        else
            D_StartTitle();                     // start up intro loop
    }

    D_DoomLoop();                               // never returns
}
