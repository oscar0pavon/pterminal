#include "input.h"
#include "macros.h"
#include "window.h"
#include "terminal.h"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>
#include <string.h>
#include "ansi_escapes.h"

/* Internal keyboard shortcuts. */
#define MODKEY Mod1Mask
#define TERMMOD (Mod1Mask|ShiftMask)

static Shortcut shortcuts[] = {
	/* mask                 keysym          function        argument */
	// { XK_ANY_MOD,           XK_Break,       sendbreak,      {.i =  0} },
	// { ControlMask,          XK_Print,       toggleprinter,  {.i =  0} },
	// { ShiftMask,            XK_Print,       printscreen,    {.i =  0} },
	// { XK_ANY_MOD,           XK_Print,       printsel,       {.i =  0} },
	// { TERMMOD,              XK_K,						zoom,           {.f = +1} },
	// { TERMMOD,              XK_J,						zoom,           {.f = -1} },
	// { TERMMOD,              XK_I,						zoomreset,      {.f =  0} },
	// //{ TERMMOD,              XK_C,           clipcopy,       {.i =  0} },
	// //{ ControlMask | ShiftMask,              XK_V,           clippaste,      {.i =  0} },
	// //{ TERMMOD,              XK_Y,           selpaste,       {.i =  0} },
	// //{ ShiftMask,            XK_Insert,      selpaste,       {.i =  0} },
	// { TERMMOD,              XK_Num_Lock,    numlock,        {.i =  0} },
	// { ShiftMask,            XK_Page_Up,     kscrollup,      {.f = -0.1} },
	// { ShiftMask,            XK_Page_Down,   kscrolldown,    {.f = -0.1} },
};

/*
 * State bits to ignore when matching key or button events.  By default,
 * numlock (Mod2Mask) and keyboard layout (XK_SWITCH_MOD) are ignored.
 */
static uint ignoremod = Mod2Mask|XK_SWITCH_MOD;


/*
 * Special keys (change & recompile st.info accordingly)
 *
 * Mask value:
 * * Use XK_ANY_MOD to match the key no matter modifiers state
 * * Use XK_NO_MOD to match the key alone (no modifiers)
 * appkey value:
 * * 0: no value
 * * > 0: keypad application mode enabled
 * *   = 2: term.numlock = 1
 * * < 0: keypad application mode disabled
 * appcursor value:
 * * 0: no value
 * * > 0: cursor application mode enabled
 * * < 0: cursor application mode disabled
 *
 * Be careful with the order of the definitions because st searches in
 * this table sequentially, so any XK_ANY_MOD must be in the last
 * position for a key.
 */

/*
 * This is the huge key array which defines all compatibility to the Linux
 * world. Please decide about changes wisely.
 */
#define APP_CURSOR +1
#define NO_APP_CURSOR -1
#define NO_MATTER_MODE 0
#define APP_KEY +1
#define NO_APP_KEY -1
#define NUM_LOCK +2

static Key special_keys[] = {
	/* keysym           mask            string      appkey appcursor */
	{ XKB_KEY_KP_Home,       ShiftMask,      "\033[2J",       0,   -1},
	{ XKB_KEY_KP_Home,       ShiftMask,      "\033[1;2H",     0,   +1},
	{ XKB_KEY_KP_Home,       XK_ANY_MOD,     "\033[H",        0,   -1},
	{ XKB_KEY_KP_Home,       XK_ANY_MOD,     "\033[1~",       0,   +1},
	{ XKB_KEY_KP_Up,         XK_ANY_MOD,     "\033Ox",       +1,    0},
	{ XKB_KEY_KP_Up,         XK_ANY_MOD,     "\033[A",        0,   -1},
	{ XKB_KEY_KP_Up,         XK_ANY_MOD,     "\033OA",        0,   +1},
	{ XKB_KEY_KP_Down,       XK_ANY_MOD,     "\033Or",       +1,    0},
	{ XKB_KEY_KP_Down,       XK_ANY_MOD,     "\033[B",        0,   -1},
	{ XKB_KEY_KP_Down,       XK_ANY_MOD,     "\033OB",        0,   +1},
	{ XKB_KEY_KP_Left,       XK_ANY_MOD,     "\033Ot",       +1,    0},
	{ XKB_KEY_KP_Left,       XK_ANY_MOD,     "\033[D",        0,   -1},
	{ XKB_KEY_KP_Left,       XK_ANY_MOD,     "\033OD",        0,   +1},
	{ XKB_KEY_KP_Right,      XK_ANY_MOD,     "\033Ov",       +1,    0},
	{ XKB_KEY_KP_Right,      XK_ANY_MOD,     "\033[C",        0,   -1},
	{ XKB_KEY_KP_Right,      XK_ANY_MOD,     "\033OC",        0,   +1},
	{ XKB_KEY_KP_Prior,      ShiftMask,      "\033[5;2~",     0,    0},
	{ XKB_KEY_KP_Prior,      XK_ANY_MOD,     "\033[5~",       0,    0},
	{ XKB_KEY_KP_Begin,      XK_ANY_MOD,     "\033[E",        0,    0},
	{ XKB_KEY_KP_End,        ControlMask,    "\033[J",       -1,    0},
	{ XKB_KEY_KP_End,        ControlMask,    "\033[1;5F",    +1,    0},
	{ XKB_KEY_KP_End,        ShiftMask,      "\033[K",       -1,    0},
	{ XKB_KEY_KP_End,        ShiftMask,      "\033[1;2F",    +1,    0},
	{ XKB_KEY_KP_End,        XK_ANY_MOD,     "\033[4~",       0,    0},
	{ XKB_KEY_KP_Next,       ShiftMask,      "\033[6;2~",     0,    0},
	{ XKB_KEY_KP_Next,       XK_ANY_MOD,     "\033[6~",       0,    0},
	{ XKB_KEY_KP_Insert,     ShiftMask,      "\033[2;2~",    +1,    0},
	{ XKB_KEY_KP_Insert,     ShiftMask,      "\033[4l",      -1,    0},
	{ XKB_KEY_KP_Insert,     ControlMask,    "\033[L",       -1,    0},
	{ XKB_KEY_KP_Insert,     ControlMask,    "\033[2;5~",    +1,    0},
	{ XKB_KEY_KP_Insert,     XK_ANY_MOD,     "\033[4h",      -1,    0},
	{ XKB_KEY_KP_Insert,     XK_ANY_MOD,     "\033[2~",      +1,    0},
	{ XKB_KEY_KP_Delete,     ControlMask,    "\033[M",       -1,    0},
	{ XKB_KEY_KP_Delete,     ControlMask,    "\033[3;5~",    +1,    0},
	{ XKB_KEY_KP_Delete,     ShiftMask,      "\033[2K",      -1,    0},
	{ XKB_KEY_KP_Delete,     ShiftMask,      "\033[3;2~",    +1,    0},
	{ XKB_KEY_KP_Delete,     XK_ANY_MOD,     "\033[P",       -1,    0},
	{ XKB_KEY_KP_Delete,     XK_ANY_MOD,     "\033[3~",      +1,    0},
	{ XKB_KEY_multiply,   XK_ANY_MOD,     "\033Oj",       +2,    0},
	// { XKB_KEY_add,        XK_ANY_MOD,     "\033Ok",       +2,    0},
	// { XKB_KEY_enter,      XK_ANY_MOD,     "\033OM",       +2,    0},
	// { XKB_KEY_enter,      XK_ANY_MOD,     "\r",           -1,    0},
	// { XKB_KEY_subtract,   XK_ANY_MOD,     "\033Om",       +2,    0},
	// { XKB_KEY_decimal,    XK_ANY_MOD,     "\033On",       +2,    0},
	// { XKB_KEY_divide,     XK_ANY_MOD,     "\033Oo",       +2,    0},
	{ XKB_KEY_KP_0,          XK_ANY_MOD,     "\033Op",       +2,    0},
	{ XKB_KEY_KP_1,          XK_ANY_MOD,     "\033Oq",       NUM_LOCK,    NO_MATTER_MODE},
	{ XKB_KEY_KP_2,          XK_ANY_MOD,     "\033Or",       +2,    0},
	{ XKB_KEY_KP_3,          XK_ANY_MOD,     "\033Os",       +2,    0},
	{ XKB_KEY_KP_4,          XK_ANY_MOD,     "\033Ot",       +2,    0},
	{ XKB_KEY_KP_5,          XK_ANY_MOD,     "\033Ou",       +2,    0},
	{ XKB_KEY_KP_6,          XK_ANY_MOD,     "\033Ov",       +2,    0},
	{ XKB_KEY_KP_7,          XK_ANY_MOD,     "\033Ow",       +2,    0},
	{ XKB_KEY_KP_8,          XK_ANY_MOD,     "\033Ox",       +2,    0},
	{ XKB_KEY_9,          XK_ANY_MOD,     "\033Oy",       +2,    0},
	{ XKB_KEY_Up,            ShiftMask,      "\033[1;2A",     0,    0},
	{ XKB_KEY_Up,            Mod1Mask,       "\033[1;3A",     0,    0},
	{ XKB_KEY_Up,         ShiftMask|Mod1Mask,"\033[1;4A",     0,    0},
	{ XKB_KEY_Up,            ControlMask,    "\033[1;5A",     0,    0},
	{ XKB_KEY_Up,      ShiftMask|ControlMask,"\033[1;6A",     0,    0},
	{ XKB_KEY_Up,       ControlMask|Mod1Mask,"\033[1;7A",     0,    0},
	{ XKB_KEY_Up,ShiftMask|ControlMask|Mod1Mask,"\033[1;8A",  0,    0},
	{ XKB_KEY_Up,            XK_ANY_MOD,     "\033[A",        0,   -1},
	{ XKB_KEY_Up,            XK_ANY_MOD,     "\033OA",        0,   +1},
	{ XKB_KEY_Down,          ShiftMask,      "\033[1;2B",     0,    0},
	{ XKB_KEY_Down,          Mod1Mask,       "\033[1;3B",     0,    0},
	{ XKB_KEY_Down,       ShiftMask|Mod1Mask,"\033[1;4B",     0,    0},
	{ XKB_KEY_Down,          ControlMask,    "\033[1;5B",     0,    0},
	{ XKB_KEY_Down,    ShiftMask|ControlMask,"\033[1;6B",     0,    0},
	{ XKB_KEY_Down,     ControlMask|Mod1Mask,"\033[1;7B",     0,    0},
	{ XKB_KEY_Down,ShiftMask|ControlMask|Mod1Mask,"\033[1;8B",0,    0},
	{ XKB_KEY_Down,          XK_ANY_MOD,     "\033[B",        0,   -1},
	{ XKB_KEY_Down,          XK_ANY_MOD,     "\033OB",        0,   +1},
	{ XKB_KEY_Left,          ShiftMask,      "\033[1;2D",     0,    0},
	{ XKB_KEY_Left,          Mod1Mask,       "\033[1;3D",     0,    0},
	{ XKB_KEY_Left,       ShiftMask|Mod1Mask,"\033[1;4D",     0,    0},
	{ XKB_KEY_Left,          ControlMask,    "\033[1;5D",     0,    0},
	{ XKB_KEY_Left,    ShiftMask|ControlMask,"\033[1;6D",     0,    0},
	{ XKB_KEY_Left,     ControlMask|Mod1Mask,"\033[1;7D",     0,    0},
	{ XKB_KEY_Left,ShiftMask|ControlMask|Mod1Mask,"\033[1;8D",0,    0},
	{ XKB_KEY_Left,          XK_ANY_MOD,     "\033[D",        0,   -1},
	{ XKB_KEY_Left,          XK_ANY_MOD,     "\033OD",        0,   +1},
	{ XKB_KEY_Right,         ShiftMask,      "\033[1;2C",     0,    0},
	{ XKB_KEY_Right,         Mod1Mask,       "\033[1;3C",     0,    0},
	{ XKB_KEY_Right,      ShiftMask|Mod1Mask,"\033[1;4C",     0,    0},
	{ XKB_KEY_Right,         ControlMask,    "\033[1;5C",     0,    0},
	{ XKB_KEY_Right,   ShiftMask|ControlMask,"\033[1;6C",     0,    0},
	{ XKB_KEY_Right,    ControlMask|Mod1Mask,"\033[1;7C",     0,    0},
	{ XKB_KEY_Right,ShiftMask|ControlMask|Mod1Mask,"\033[1;8C",0,   0},
	{ XKB_KEY_Right,         XK_ANY_MOD,     "\033[C",        0,   -1},
	{ XKB_KEY_Right,         XK_ANY_MOD,     "\033OC",        0,   +1},
	{ XKB_KEY_ISO_Left_Tab,  ShiftMask,      "\033[Z",        0,    0},
	{ XKB_KEY_Return,        Mod1Mask,       "\033\r",        0,    0},
	{ XKB_KEY_Return,        XK_ANY_MOD,     "\r",            0,    0},
	{ XKB_KEY_Insert,        ShiftMask,      "\033[4l",      -1,    0},
	{ XKB_KEY_Insert,        ShiftMask,      "\033[2;2~",    +1,    0},
	{ XKB_KEY_Insert,        ControlMask,    "\033[L",       -1,    0},
	{ XKB_KEY_Insert,        ControlMask,    "\033[2;5~",    +1,    0},
	{ XKB_KEY_Insert,        XK_ANY_MOD,     "\033[4h",      -1,    0},
	{ XKB_KEY_Insert,        XK_ANY_MOD,     "\033[2~",      +1,    0},
	{ XKB_KEY_Delete,        ControlMask,    "\033[M",       -1,    0},
	{ XKB_KEY_Delete,        ControlMask,    "\033[3;5~",    +1,    0},
	{ XKB_KEY_Delete,        ShiftMask,      "\033[2K",      -1,    0},
	{ XKB_KEY_Delete,        ShiftMask,      "\033[3;2~",    +1,    0},
	{ XKB_KEY_Delete,        XK_ANY_MOD,     "\033[P",       -1,    0},
	{ XKB_KEY_Delete,        XK_ANY_MOD,     "\033[3~",      +1,    0},
	{ XKB_KEY_BackSpace,     XK_NO_MOD,      "\177",          0,    0},
	{ XKB_KEY_BackSpace,     Mod1Mask,       "\033\177",      0,    0},
	{ XKB_KEY_Home,          ShiftMask,      "\033[2J",       0,   -1},
	{ XKB_KEY_Home,          ShiftMask,      "\033[1;2H",     0,   +1},
	{ XKB_KEY_Home,          XK_ANY_MOD,     "\033[H",        0,   -1},
	{ XKB_KEY_Home,          XK_ANY_MOD,     "\033[1~",       0,   +1},
	{ XKB_KEY_End,           ControlMask,    "\033[J",       -1,    0},
	{ XKB_KEY_End,           ControlMask,    "\033[1;5F",    +1,    0},
	{ XKB_KEY_End,           ShiftMask,      "\033[K",       -1,    0},
	{ XKB_KEY_End,           ShiftMask,      "\033[1;2F",    +1,    0},
	{ XKB_KEY_End,           XK_ANY_MOD,     "\033[4~",       0,    0},
	{ XKB_KEY_Prior,         ControlMask,    "\033[5;5~",     0,    0},
	{ XKB_KEY_Prior,         ShiftMask,      "\033[5;2~",     0,    0},
	{ XKB_KEY_Prior,         XK_ANY_MOD,     "\033[5~",       0,    0},
	{ XKB_KEY_Next,          ControlMask,    "\033[6;5~",     0,    0},
	{ XKB_KEY_Next,          ShiftMask,      "\033[6;2~",     0,    0},
	{ XKB_KEY_Next,          XK_ANY_MOD,     "\033[6~",       0,    0},
	{ XKB_KEY_F1,            XK_NO_MOD,      "\033OP" ,       0,    0},
	{ XKB_KEY_F1, /* F13 */  ShiftMask,      "\033[1;2P",     0,    0},
	{ XKB_KEY_F1, /* F25 */  ControlMask,    "\033[1;5P",     0,    0},
	{ XKB_KEY_F1, /* F37 */  Mod4Mask,       "\033[1;6P",     0,    0},
	{ XKB_KEY_F1, /* F49 */  Mod1Mask,       "\033[1;3P",     0,    0},
	{ XKB_KEY_F1, /* F61 */  Mod3Mask,       "\033[1;4P",     0,    0},
	{ XKB_KEY_F2,            XK_NO_MOD,      "\033OQ" ,       0,    0},
	{ XKB_KEY_F2, /* F14 */  ShiftMask,      "\033[1;2Q",     0,    0},
	{ XKB_KEY_F2, /* F26 */  ControlMask,    "\033[1;5Q",     0,    0},
	{ XKB_KEY_F2, /* F38 */  Mod4Mask,       "\033[1;6Q",     0,    0},
	{ XKB_KEY_F2, /* F50 */  Mod1Mask,       "\033[1;3Q",     0,    0},
	{ XKB_KEY_F2, /* F62 */  Mod3Mask,       "\033[1;4Q",     0,    0},
	{ XKB_KEY_F3,            XK_NO_MOD,      "\033OR" ,       0,    0},
	{ XKB_KEY_F3, /* F15 */  ShiftMask,      "\033[1;2R",     0,    0},
	{ XKB_KEY_F3, /* F27 */  ControlMask,    "\033[1;5R",     0,    0},
	{ XKB_KEY_F3, /* F39 */  Mod4Mask,       "\033[1;6R",     0,    0},
	{ XKB_KEY_F3, /* F51 */  Mod1Mask,       "\033[1;3R",     0,    0},
	{ XKB_KEY_F3, /* F63 */  Mod3Mask,       "\033[1;4R",     0,    0},
	{ XKB_KEY_F4,            XK_NO_MOD,      "\033OS" ,       0,    0},
	{ XKB_KEY_F4, /* F16 */  ShiftMask,      "\033[1;2S",     0,    0},
	{ XKB_KEY_F4, /* F28 */  ControlMask,    "\033[1;5S",     0,    0},
	{ XKB_KEY_F4, /* F40 */  Mod4Mask,       "\033[1;6S",     0,    0},
	{ XKB_KEY_F4, /* F52 */  Mod1Mask,       "\033[1;3S",     0,    0},
	{ XKB_KEY_F5,            XK_NO_MOD,      "\033[15~",      0,    0},
	{ XKB_KEY_F5, /* F17 */  ShiftMask,      "\033[15;2~",    0,    0},
	{ XKB_KEY_F5, /* F29 */  ControlMask,    "\033[15;5~",    0,    0},
	{ XKB_KEY_F5, /* F41 */  Mod4Mask,       "\033[15;6~",    0,    0},
	{ XKB_KEY_F5, /* F53 */  Mod1Mask,       "\033[15;3~",    0,    0},
	{ XKB_KEY_F6,            XK_NO_MOD,      "\033[17~",      0,    0},
	{ XKB_KEY_F6, /* F18 */  ShiftMask,      "\033[17;2~",    0,    0},
	{ XKB_KEY_F6, /* F30 */  ControlMask,    "\033[17;5~",    0,    0},
	{ XKB_KEY_F6, /* F42 */  Mod4Mask,       "\033[17;6~",    0,    0},
	{ XKB_KEY_F6, /* F54 */  Mod1Mask,       "\033[17;3~",    0,    0},
	{ XKB_KEY_F7,            XK_NO_MOD,      "\033[18~",      0,    0},
	{ XKB_KEY_F7, /* F19 */  ShiftMask,      "\033[18;2~",    0,    0},
	{ XKB_KEY_F7, /* F31 */  ControlMask,    "\033[18;5~",    0,    0},
	{ XKB_KEY_F7, /* F43 */  Mod4Mask,       "\033[18;6~",    0,    0},
	{ XKB_KEY_F7, /* F55 */  Mod1Mask,       "\033[18;3~",    0,    0},
	{ XKB_KEY_F8,            XK_NO_MOD,      "\033[19~",      0,    0},
	{ XKB_KEY_F8, /* F20 */  ShiftMask,      "\033[19;2~",    0,    0},
	{ XKB_KEY_F8, /* F32 */  ControlMask,    "\033[19;5~",    0,    0},
	{ XKB_KEY_F8, /* F44 */  Mod4Mask,       "\033[19;6~",    0,    0},
	{ XKB_KEY_F8, /* F56 */  Mod1Mask,       "\033[19;3~",    0,    0},
	{ XKB_KEY_F9,            XK_NO_MOD,      "\033[20~",      0,    0},
	{ XKB_KEY_F9, /* F21 */  ShiftMask,      "\033[20;2~",    0,    0},
	{ XKB_KEY_F9, /* F33 */  ControlMask,    "\033[20;5~",    0,    0},
	{ XKB_KEY_F9, /* F45 */  Mod4Mask,       "\033[20;6~",    0,    0},
	{ XKB_KEY_F9, /* F57 */  Mod1Mask,       "\033[20;3~",    0,    0},
	{ XKB_KEY_F10,           XK_NO_MOD,      "\033[21~",      0,    0},
	{ XKB_KEY_F10, /* F22 */ ShiftMask,      "\033[21;2~",    0,    0},
	{ XKB_KEY_F10, /* F34 */ ControlMask,    "\033[21;5~",    0,    0},
	{ XKB_KEY_F10, /* F46 */ Mod4Mask,       "\033[21;6~",    0,    0},
	{ XKB_KEY_F10, /* F58 */ Mod1Mask,       "\033[21;3~",    0,    0},
	{ XKB_KEY_F11,           XK_NO_MOD,      "\033[23~",      0,    0},
	{ XKB_KEY_F11, /* F23 */ ShiftMask,      "\033[23;2~",    0,    0},
	{ XKB_KEY_F11, /* F35 */ ControlMask,    "\033[23;5~",    0,    0},
	{ XKB_KEY_F11, /* F47 */ Mod4Mask,       "\033[23;6~",    0,    0},
	{ XKB_KEY_F11, /* F59 */ Mod1Mask,       "\033[23;3~",    0,    0},
	{ XKB_KEY_F12,           XK_NO_MOD,      "\033[24~",      0,    0},
	{ XKB_KEY_F12, /* F24 */ ShiftMask,      "\033[24;2~",    0,    0},
	{ XKB_KEY_F12, /* F36 */ ControlMask,    "\033[24;5~",    0,    0},
	{ XKB_KEY_F12, /* F48 */ Mod4Mask,       "\033[24;6~",    0,    0},
	{ XKB_KEY_F12, /* F60 */ Mod1Mask,       "\033[24;3~",    0,    0},
	{ XKB_KEY_F13,           XK_NO_MOD,      "\033[1;2P",     0,    0},
	{ XKB_KEY_F14,           XK_NO_MOD,      "\033[1;2Q",     0,    0},
	{ XKB_KEY_F15,           XK_NO_MOD,      "\033[1;2R",     0,    0},
	{ XKB_KEY_F16,           XK_NO_MOD,      "\033[1;2S",     0,    0},
	{ XKB_KEY_F17,           XK_NO_MOD,      "\033[15;2~",    0,    0},
	{ XKB_KEY_F18,           XK_NO_MOD,      "\033[17;2~",    0,    0},
	{ XKB_KEY_F19,           XK_NO_MOD,      "\033[18;2~",    0,    0},
	{ XKB_KEY_F20,           XK_NO_MOD,      "\033[19;2~",    0,    0},
	{ XKB_KEY_F21,           XK_NO_MOD,      "\033[20;2~",    0,    0},
	{ XKB_KEY_F22,           XK_NO_MOD,      "\033[21;2~",    0,    0},
	{ XKB_KEY_F23,           XK_NO_MOD,      "\033[23;2~",    0,    0},
	{ XKB_KEY_F24,           XK_NO_MOD,      "\033[24;2~",    0,    0},
	{ XKB_KEY_F25,           XK_NO_MOD,      "\033[1;5P",     0,    0},
	{ XKB_KEY_F26,           XK_NO_MOD,      "\033[1;5Q",     0,    0},
	{ XKB_KEY_F27,           XK_NO_MOD,      "\033[1;5R",     0,    0},
	{ XKB_KEY_F28,           XK_NO_MOD,      "\033[1;5S",     0,    0},
	{ XKB_KEY_F29,           XK_NO_MOD,      "\033[15;5~",    0,    0},
	{ XKB_KEY_F30,           XK_NO_MOD,      "\033[17;5~",    0,    0},
	{ XKB_KEY_F31,           XK_NO_MOD,      "\033[18;5~",    0,    0},
	{ XKB_KEY_F32,           XK_NO_MOD,      "\033[19;5~",    0,    0},
	{ XKB_KEY_F33,           XK_NO_MOD,      "\033[20;5~",    0,    0},
	{ XKB_KEY_F34,           XK_NO_MOD,      "\033[21;5~",    0,    0},
	{ XKB_KEY_F35,           XK_NO_MOD,      "\033[23;5~",    0,    0},
};
int match(uint mask, uint state) {
  return mask == XK_ANY_MOD || mask == (state & ~ignoremod);
}

bool print_special_key(xkb_keysym_t sym){

  char* esc_to_print = get_esc_from_special_keys(sym, 0);
  if(esc_to_print){
    uint32_t len = strlen(esc_to_print);
    write_to_tty(esc_to_print, len, 0);
    print_csi(esc_to_print);
    printf("printed special key\n");
    return true;
  }

  return false;
}

char *get_esc_from_special_keys(xkb_keysym_t key_sym, uint state) {
  Key *current_key;
  int i;

  for (current_key = special_keys; 
      current_key < special_keys + LEN(special_keys); 
      current_key++) {

    if (current_key->key_sym != key_sym)
      continue;

    if (!match(current_key->mask, state))
      continue;

    if (IS_WINDOSET(MODE_APPKEYPAD) ? current_key->appkey < 0 : current_key->appkey > 0)
      continue;
    if (IS_WINDOSET(MODE_NUMLOCK) && current_key->appkey == 2)
      continue;

    if (IS_WINDOSET(MODE_APPCURSOR) ? current_key->appcursor < 0 : current_key->appcursor > 0)
      continue;

    return current_key->esc_to_print;
  }

  return NULL;
}

void numlock(const Arg *dummy) { terminal_window.mode ^= MODE_NUMLOCK; }

