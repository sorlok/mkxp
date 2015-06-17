/*
 * keys-bindings.cpp
 *
 * This file was made as an extension to mkxp, and should be considered
 *   a derivative work.
 *
 * Copyright (C) 2015 Seth N. Hetu <seth.hetu@gmail.com>
 *
 * mkxp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * mkxp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "keys.h"
#include "sharedstate.h"
#include "binding-util.h"
#include "binding-types.h"
#include "exception.h"


RB_METHOD(keysGetKeyname)
{
	RB_UNUSED_PARAM;

	int key = 0;

	rb_get_args(argc, argv, "i|", &key RB_ARG_END);

	const char* res = "";
	GUARD_EXC( res = shState->keys().getKeyname(key); )

	//Over-ride: Unknown ("") and LSHIFT/RSHIFT, etc.
	if (!*res) { res = "Unknown Key"; }
	if (strcmp(res, "Left Ctrl")==0 || strcmp(res, "Right Ctrl")==0) { res = "Ctrl"; }
	if (strcmp(res, "Left Alt")==0 || strcmp(res, "Right Alt")==0) { res = "Alt"; }
	if (strcmp(res, "Left Shift")==0 || strcmp(res, "Right Shift")==0) { res = "Shift"; }

	return rb_str_new_cstr(res);
}

RB_METHOD(keysUpdate)
{
	RB_UNUSED_PARAM;

	shState->keys().update();

	return Qnil;
}

RB_METHOD(keysPress)
{
	RB_UNUSED_PARAM;

	int key = 0;

	rb_get_args(argc, argv, "i|", &key RB_ARG_END);

	bool res = false;
	GUARD_EXC( res = shState->keys().isPress(key); )

	return rb_bool_new(res);
}

RB_METHOD(keysTrigger)
{
	RB_UNUSED_PARAM;

	int key = 0;

	rb_get_args(argc, argv, "i|", &key RB_ARG_END);

	bool res = false;
	GUARD_EXC( res = shState->keys().isTrigger(key); )

	return rb_bool_new(res);
}

RB_METHOD(keysRepeat)
{
	RB_UNUSED_PARAM;

	int key = 0;

	rb_get_args(argc, argv, "i|", &key RB_ARG_END);

	bool res = false;
	GUARD_EXC( res = shState->keys().isRepeat(key); )

	return rb_bool_new(res);
}

RB_METHOD(keysRelease)
{
	RB_UNUSED_PARAM;

	int key = 0;

	rb_get_args(argc, argv, "i|", &key RB_ARG_END);

	bool res = false;
	GUARD_EXC( res = shState->keys().isRelease(key); )

	return rb_bool_new(res);
}


RB_METHOD(wolfpadUpdate)
{
	RB_UNUSED_PARAM;

	shState->wolfpad().update();

	return Qnil;
}

RB_METHOD(wolfpadPress)
{
	RB_UNUSED_PARAM;

	int key = 0;

	rb_get_args(argc, argv, "i|", &key RB_ARG_END);

	bool res = false;
	GUARD_EXC( res = shState->wolfpad().isPress(key); )

	return rb_bool_new(res);
}

RB_METHOD(wolfpadTrigger)
{
	RB_UNUSED_PARAM;

	int key = 0;

	rb_get_args(argc, argv, "i|", &key RB_ARG_END);

	bool res = false;
	GUARD_EXC( res = shState->wolfpad().isTrigger(key); )

	return rb_bool_new(res);
}

RB_METHOD(wolfpadRepeat)
{
	RB_UNUSED_PARAM;

	int key = 0;

	rb_get_args(argc, argv, "i|", &key RB_ARG_END);

	bool res = false;
	GUARD_EXC( res = shState->wolfpad().isRepeat(key); )

	return rb_bool_new(res);
}



void keysBindingInit()
{
	//Wolf Pad module
	{
	VALUE module = rb_define_module("WolfPad");

	_rb_define_module_function(module, "update", wolfpadUpdate);
	_rb_define_module_function(module, "press?", wolfpadPress);
	_rb_define_module_function(module, "trigger?", wolfpadTrigger);
	_rb_define_module_function(module, "repeat?", wolfpadRepeat);

	rb_const_set(module, rb_intern("UP"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_DPAD_UP)));
	rb_const_set(module, rb_intern("LEFT"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_DPAD_LEFT)));
	rb_const_set(module, rb_intern("DOWN"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_DPAD_DOWN)));
	rb_const_set(module, rb_intern("RIGHT"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)));
	rb_const_set(module, rb_intern("A"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_A)));
	rb_const_set(module, rb_intern("B"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_B)));
	rb_const_set(module, rb_intern("X"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_X)));
	rb_const_set(module, rb_intern("Y"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_Y)));
	rb_const_set(module, rb_intern("L1"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)));
	rb_const_set(module, rb_intern("R1"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)));
	rb_const_set(module, rb_intern("START"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_START)));
	rb_const_set(module, rb_intern("SELECT"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_BACK)));
	rb_const_set(module, rb_intern("L3"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_LEFTSTICK)));
	rb_const_set(module, rb_intern("R3"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_RIGHTSTICK)));

	//We cheat a bit to make these:
	rb_const_set(module, rb_intern("L2"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_MAX)+1));
	rb_const_set(module, rb_intern("R2"), rb_int_new(static_cast<int>(SDL_CONTROLLER_BUTTON_MAX)+2));
	}

	VALUE module = rb_define_module("Keys");

	//_rb_define_module_function(module, "moduleInit", keysModuleInit);
	_rb_define_module_function(module, "determine_keyname", keysGetKeyname);
	_rb_define_module_function(module, "update", keysUpdate);
	_rb_define_module_function(module, "press?", keysPress);
	_rb_define_module_function(module, "trigger?", keysTrigger);
	_rb_define_module_function(module, "repeat?", keysRepeat);
	_rb_define_module_function(module, "release?", keysRelease);

	//SDL Scancodes. 
	// TODO: Test these in parallel on Windows, OSX
	// TODO: Have someone with a different keyboard layout test these.
	rb_const_set(module, rb_intern("CANCEL"), rb_int_new(static_cast<int>(SDL_SCANCODE_CANCEL)));
	rb_const_set(module, rb_intern("BACKSPACE"), rb_int_new(static_cast<int>(SDL_SCANCODE_BACKSPACE)));
	rb_const_set(module, rb_intern("TAB"), rb_int_new(static_cast<int>(SDL_SCANCODE_TAB)));
	rb_const_set(module, rb_intern("CLEAR"), rb_int_new(static_cast<int>(SDL_SCANCODE_CLEAR)));
	rb_const_set(module, rb_intern("RETURN"), rb_int_new(static_cast<int>(SDL_SCANCODE_RETURN)));
	rb_const_set(module, rb_intern("MENU"), rb_int_new(static_cast<int>(SDL_SCANCODE_MENU)));
	rb_const_set(module, rb_intern("PAUSE"), rb_int_new(static_cast<int>(SDL_SCANCODE_PAUSE)));
	rb_const_set(module, rb_intern("ESCAPE"), rb_int_new(static_cast<int>(SDL_SCANCODE_ESCAPE)));
	rb_const_set(module, rb_intern("SPACE"), rb_int_new(static_cast<int>(SDL_SCANCODE_SPACE)));
	rb_const_set(module, rb_intern("PRIOR"), rb_int_new(static_cast<int>(SDL_SCANCODE_PAGEUP)));
	rb_const_set(module, rb_intern("NEXT"), rb_int_new(static_cast<int>(SDL_SCANCODE_PAGEDOWN)));
	rb_const_set(module, rb_intern("ENDS"), rb_int_new(static_cast<int>(SDL_SCANCODE_END)));
	rb_const_set(module, rb_intern("GRAVE"), rb_int_new(static_cast<int>(SDL_SCANCODE_GRAVE)));
	rb_const_set(module, rb_intern("HOME"), rb_int_new(static_cast<int>(SDL_SCANCODE_HOME)));
	rb_const_set(module, rb_intern("LEFT"), rb_int_new(static_cast<int>(SDL_SCANCODE_LEFT)));
	rb_const_set(module, rb_intern("UP"), rb_int_new(static_cast<int>(SDL_SCANCODE_UP)));
	rb_const_set(module, rb_intern("RIGHT"), rb_int_new(static_cast<int>(SDL_SCANCODE_RIGHT)));
	rb_const_set(module, rb_intern("DOWN"), rb_int_new(static_cast<int>(SDL_SCANCODE_DOWN)));
	rb_const_set(module, rb_intern("SELECT"), rb_int_new(static_cast<int>(SDL_SCANCODE_SELECT)));
	rb_const_set(module, rb_intern("BACKSLASH"), rb_int_new(static_cast<int>(SDL_SCANCODE_BACKSLASH)));
	rb_const_set(module, rb_intern("NONBACKSLASH"), rb_int_new(static_cast<int>(SDL_SCANCODE_NONUSBACKSLASH)));
	rb_const_set(module, rb_intern("LBRACKET"), rb_int_new(static_cast<int>(SDL_SCANCODE_LEFTBRACKET)));
	rb_const_set(module, rb_intern("RBRACKET"), rb_int_new(static_cast<int>(SDL_SCANCODE_RIGHTBRACKET)));
	rb_const_set(module, rb_intern("MINUS"), rb_int_new(static_cast<int>(SDL_SCANCODE_MINUS)));
	rb_const_set(module, rb_intern("EQUALS"), rb_int_new(static_cast<int>(SDL_SCANCODE_EQUALS)));
	rb_const_set(module, rb_intern("SEMICOLON"), rb_int_new(static_cast<int>(SDL_SCANCODE_SEMICOLON)));
	rb_const_set(module, rb_intern("APOSTROPHE"), rb_int_new(static_cast<int>(SDL_SCANCODE_APOSTROPHE)));
	rb_const_set(module, rb_intern("COMMA"), rb_int_new(static_cast<int>(SDL_SCANCODE_COMMA)));
	rb_const_set(module, rb_intern("PERIOD"), rb_int_new(static_cast<int>(SDL_SCANCODE_PERIOD)));
	rb_const_set(module, rb_intern("SLASH"), rb_int_new(static_cast<int>(SDL_SCANCODE_SLASH)));
	rb_const_set(module, rb_intern("WINAPP"), rb_int_new(static_cast<int>(SDL_SCANCODE_APPLICATION)));
	rb_const_set(module, rb_intern("EXECUTE"), rb_int_new(static_cast<int>(SDL_SCANCODE_EXECUTE)));
	rb_const_set(module, rb_intern("SNAPSHOT"), rb_int_new(static_cast<int>(SDL_SCANCODE_PRINTSCREEN)));
	rb_const_set(module, rb_intern("DELETE"), rb_int_new(static_cast<int>(SDL_SCANCODE_DELETE)));
	rb_const_set(module, rb_intern("HELP"), rb_int_new(static_cast<int>(SDL_SCANCODE_HELP)));

	//Number keys
	rb_const_set(module, rb_intern("N0"), rb_int_new(static_cast<int>(SDL_SCANCODE_0)));
	rb_const_set(module, rb_intern("N1"), rb_int_new(static_cast<int>(SDL_SCANCODE_1)));
	rb_const_set(module, rb_intern("N2"), rb_int_new(static_cast<int>(SDL_SCANCODE_2)));
	rb_const_set(module, rb_intern("N3"), rb_int_new(static_cast<int>(SDL_SCANCODE_3)));
	rb_const_set(module, rb_intern("N4"), rb_int_new(static_cast<int>(SDL_SCANCODE_4)));
	rb_const_set(module, rb_intern("N5"), rb_int_new(static_cast<int>(SDL_SCANCODE_5)));
	rb_const_set(module, rb_intern("N6"), rb_int_new(static_cast<int>(SDL_SCANCODE_6)));
	rb_const_set(module, rb_intern("N7"), rb_int_new(static_cast<int>(SDL_SCANCODE_7)));
	rb_const_set(module, rb_intern("N8"), rb_int_new(static_cast<int>(SDL_SCANCODE_8)));
	rb_const_set(module, rb_intern("N9"), rb_int_new(static_cast<int>(SDL_SCANCODE_9)));

	//Letter keys
	rb_const_set(module, rb_intern("A"), rb_int_new(static_cast<int>(SDL_SCANCODE_A)));
	rb_const_set(module, rb_intern("B"), rb_int_new(static_cast<int>(SDL_SCANCODE_B)));
	rb_const_set(module, rb_intern("C"), rb_int_new(static_cast<int>(SDL_SCANCODE_C)));
	rb_const_set(module, rb_intern("D"), rb_int_new(static_cast<int>(SDL_SCANCODE_D)));
	rb_const_set(module, rb_intern("E"), rb_int_new(static_cast<int>(SDL_SCANCODE_E)));
	rb_const_set(module, rb_intern("F"), rb_int_new(static_cast<int>(SDL_SCANCODE_F)));
	rb_const_set(module, rb_intern("G"), rb_int_new(static_cast<int>(SDL_SCANCODE_G)));
	rb_const_set(module, rb_intern("H"), rb_int_new(static_cast<int>(SDL_SCANCODE_H)));
	rb_const_set(module, rb_intern("I"), rb_int_new(static_cast<int>(SDL_SCANCODE_I)));
	rb_const_set(module, rb_intern("J"), rb_int_new(static_cast<int>(SDL_SCANCODE_J)));
	rb_const_set(module, rb_intern("K"), rb_int_new(static_cast<int>(SDL_SCANCODE_K)));
	rb_const_set(module, rb_intern("L"), rb_int_new(static_cast<int>(SDL_SCANCODE_L)));
	rb_const_set(module, rb_intern("M"), rb_int_new(static_cast<int>(SDL_SCANCODE_M)));
	rb_const_set(module, rb_intern("N"), rb_int_new(static_cast<int>(SDL_SCANCODE_N)));
	rb_const_set(module, rb_intern("O"), rb_int_new(static_cast<int>(SDL_SCANCODE_O)));
	rb_const_set(module, rb_intern("P"), rb_int_new(static_cast<int>(SDL_SCANCODE_P)));
	rb_const_set(module, rb_intern("Q"), rb_int_new(static_cast<int>(SDL_SCANCODE_Q)));
	rb_const_set(module, rb_intern("R"), rb_int_new(static_cast<int>(SDL_SCANCODE_R)));
	rb_const_set(module, rb_intern("S"), rb_int_new(static_cast<int>(SDL_SCANCODE_S)));
	rb_const_set(module, rb_intern("T"), rb_int_new(static_cast<int>(SDL_SCANCODE_T)));
	rb_const_set(module, rb_intern("U"), rb_int_new(static_cast<int>(SDL_SCANCODE_U)));
	rb_const_set(module, rb_intern("V"), rb_int_new(static_cast<int>(SDL_SCANCODE_V)));
	rb_const_set(module, rb_intern("W"), rb_int_new(static_cast<int>(SDL_SCANCODE_W)));
	rb_const_set(module, rb_intern("X"), rb_int_new(static_cast<int>(SDL_SCANCODE_X)));
	rb_const_set(module, rb_intern("Y"), rb_int_new(static_cast<int>(SDL_SCANCODE_Y)));
	rb_const_set(module, rb_intern("Z"), rb_int_new(static_cast<int>(SDL_SCANCODE_Z)));

	//Multiplexed keys
        rb_const_set(module, rb_intern("CTRL"), rb_int_new(static_cast<int>(SDL_SCANCODE_LCTRL)));    //L/R CTRL
        rb_const_set(module, rb_intern("ALT"), rb_int_new(static_cast<int>(SDL_SCANCODE_LALT)));      //L/R ALT
	rb_const_set(module, rb_intern("SHIFT"), rb_int_new(static_cast<int>(SDL_SCANCODE_LSHIFT)));  //L/R SHIFT
	rb_const_set(module, rb_intern("WIN"), rb_int_new(static_cast<int>(SDL_SCANCODE_LGUI)));  //L/R WINDOWS

	//Number pad keys
	rb_const_set(module, rb_intern("NUMPAD0"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_0)));
	rb_const_set(module, rb_intern("NUMPAD1"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_1)));
	rb_const_set(module, rb_intern("NUMPAD2"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_2)));
	rb_const_set(module, rb_intern("NUMPAD3"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_3)));
	rb_const_set(module, rb_intern("NUMPAD4"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_4)));
	rb_const_set(module, rb_intern("NUMPAD5"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_5)));
	rb_const_set(module, rb_intern("NUMPAD6"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_6)));
	rb_const_set(module, rb_intern("NUMPAD7"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_7)));
	rb_const_set(module, rb_intern("NUMPAD8"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_8)));
	rb_const_set(module, rb_intern("NUMPAD9"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_9)));
	rb_const_set(module, rb_intern("MULTIPLY"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_MULTIPLY)));
	rb_const_set(module, rb_intern("ADD"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_PLUS)));
	rb_const_set(module, rb_intern("SUBTRACT"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_MINUS)));
	rb_const_set(module, rb_intern("DECIMAL"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_PERIOD)));
	rb_const_set(module, rb_intern("DIVIDE"), rb_int_new(static_cast<int>(SDL_SCANCODE_KP_DIVIDE)));

	//Function keys
	rb_const_set(module, rb_intern("F1"), rb_int_new(static_cast<int>(SDL_SCANCODE_F1)));
	rb_const_set(module, rb_intern("F2"), rb_int_new(static_cast<int>(SDL_SCANCODE_F2)));
	rb_const_set(module, rb_intern("F3"), rb_int_new(static_cast<int>(SDL_SCANCODE_F3)));
	rb_const_set(module, rb_intern("F4"), rb_int_new(static_cast<int>(SDL_SCANCODE_F4)));
	rb_const_set(module, rb_intern("F5"), rb_int_new(static_cast<int>(SDL_SCANCODE_F5)));
	rb_const_set(module, rb_intern("F6"), rb_int_new(static_cast<int>(SDL_SCANCODE_F6)));
	rb_const_set(module, rb_intern("F7"), rb_int_new(static_cast<int>(SDL_SCANCODE_F7)));
	rb_const_set(module, rb_intern("F8"), rb_int_new(static_cast<int>(SDL_SCANCODE_F8)));
	rb_const_set(module, rb_intern("F9"), rb_int_new(static_cast<int>(SDL_SCANCODE_F9)));
	rb_const_set(module, rb_intern("F10"), rb_int_new(static_cast<int>(SDL_SCANCODE_F10)));
	rb_const_set(module, rb_intern("F11"), rb_int_new(static_cast<int>(SDL_SCANCODE_F11)));
	rb_const_set(module, rb_intern("F12"), rb_int_new(static_cast<int>(SDL_SCANCODE_F12)));
	rb_const_set(module, rb_intern("F13"), rb_int_new(static_cast<int>(SDL_SCANCODE_F13)));
	rb_const_set(module, rb_intern("F14"), rb_int_new(static_cast<int>(SDL_SCANCODE_F14)));
	rb_const_set(module, rb_intern("F15"), rb_int_new(static_cast<int>(SDL_SCANCODE_F15)));
	rb_const_set(module, rb_intern("F16"), rb_int_new(static_cast<int>(SDL_SCANCODE_F16)));
	rb_const_set(module, rb_intern("F17"), rb_int_new(static_cast<int>(SDL_SCANCODE_F17)));
	rb_const_set(module, rb_intern("F18"), rb_int_new(static_cast<int>(SDL_SCANCODE_F18)));
	rb_const_set(module, rb_intern("F19"), rb_int_new(static_cast<int>(SDL_SCANCODE_F19)));
	rb_const_set(module, rb_intern("F20"), rb_int_new(static_cast<int>(SDL_SCANCODE_F20)));
	rb_const_set(module, rb_intern("F21"), rb_int_new(static_cast<int>(SDL_SCANCODE_F21)));
	rb_const_set(module, rb_intern("F22"), rb_int_new(static_cast<int>(SDL_SCANCODE_F22)));
	rb_const_set(module, rb_intern("F23"), rb_int_new(static_cast<int>(SDL_SCANCODE_F23)));
	rb_const_set(module, rb_intern("F24"), rb_int_new(static_cast<int>(SDL_SCANCODE_F24)));

	//Repurpose SDL_SCANCODE_UNKNOWN as "any"
	rb_const_set(module, rb_intern("ANYKEY"), rb_int_new(static_cast<int>(SDL_SCANCODE_UNKNOWN)));
}

