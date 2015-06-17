/*
 * keys.h
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

#ifndef KEYS_H
#define KEYS_H

#include <iostream>
#include <SDL_keyboard.h>
#include <SDL_gamecontroller.h>
#include "util.h"

#include "eventthread.h"

class Scene;
class Bitmap;
class Disposable;
struct RGSSThreadData;
struct GraphicsPrivate;
struct AtomicFlag;


//TODO: It would be better to treat key repeats using similar code for GamePads and the Keyboard.
class WolfPad
{
private:
	//How long each button has been held down.
	uint8_t holds[SDL_CONTROLLER_BUTTON_MAX] = {0};
	uint8_t holdsTrig[2] = {0}; //Trigger holds (L,R)

	bool validate(int button) {
		//Basically, "will it crash anything"? If they want to check bogus keycodes, then by all means!
		return button>=0 && button<SDL_CONTROLLER_BUTTON_MAX; 
	}

public:
	void update() {
		//Increment holds, cap to prevent overflow.
		for (size_t i=0; i<SDL_CONTROLLER_BUTTON_MAX; i++) {
			if (EventThread::padStates[i]) {
				holds[i] += 1;
			} else {
				holds[i] = 0;
			}
			if (holds[i]>49) {
				holds[i] -= 10;
			}
		}

		//Increment trigger holds, cap to prevent overflow.
		if (EventThread::padAxes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] > 0) {  // >50%
			holdsTrig[0] += 1;
		}
		if (EventThread::padAxes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] > 0) { // >50%
			holdsTrig[1] += 1;
		}
		if (holdsTrig[0]>49) { holds[0] -= 10; }
		if (holdsTrig[1]>49) { holds[1] -= 10; }
	}

	//State check functions
	bool isPress(int button) {
		//Cheaty, cheaty.
		if (button == static_cast<int>(SDL_CONTROLLER_BUTTON_MAX)+1) { return holdsTrig[0]>0; } //L2
		if (button == static_cast<int>(SDL_CONTROLLER_BUTTON_MAX)+2) { return holdsTrig[1]>0; } //R2

		//Normal
		return validate(button) && holds[button]>0;
	}
	bool isTrigger(int button) {
		//Cheaty, cheaty.
		if (button == static_cast<int>(SDL_CONTROLLER_BUTTON_MAX)+1) { return holdsTrig[0]==1; } //L2
		if (button == static_cast<int>(SDL_CONTROLLER_BUTTON_MAX)+2) { return holdsTrig[1]==1; } //R2

		//Normal
		return validate(button) && holds[button]==1;
	}
	bool isRepeat(int button) {
		//Cheaty, cheaty.
		if (button == static_cast<int>(SDL_CONTROLLER_BUTTON_MAX)+1) { //L2
			int result = holdsTrig[0];
			return (result==1) || (result>30 && (result%5)==0);
		}
		if (button == static_cast<int>(SDL_CONTROLLER_BUTTON_MAX)+2) { //R2
			int result = holdsTrig[1];
			return (result==1) || (result>30 && (result%5)==0);
		}

		//Normal.
		if (validate(button)) {
			int result = holds[button];
			return (result==1) || (result>30 && (result%5)==0);
		}
		return false;
	}
};


class Keys
{
private:
	//Copy of EventThread::keyStates the last time we checked it.
	uint8_t lastKeyStates[SDL_NUM_SCANCODES] = {0};

	//State arrays
	struct KeyState {
		bool press;           //Key currently pressed
		bool trigger;         //Key initially pressed
		bool repeat;          //Key being held
		bool release;         //Key being released
		//bool toggle;          //Key currently toggled TODO
		int8_t repeatCount;  //Counter for repetition.
		KeyState() : press(false), trigger(false), repeat(false), release(false), /*toggle(false),*/ repeatCount(0) {}
	};

	KeyState states[SDL_NUM_SCANCODES];

	const static int ANY_KEY = static_cast<int>(SDL_SCANCODE_UNKNOWN);

	bool validate(int key) {
		//Basically, "will it crash anything"? If they want to check bogus scancodes, then by all means!
		return key>=0 && key<SDL_NUM_SCANCODES; 
	}

public:
	/*void moduleInit() {
		memcpy(lastKeyStates, EventThread::keyStates, sizeof(lastKeyStates)*sizeof(uint8_t)); //TODO: Zeroing may be better...
		for (size_t i=0; i<SDL_NUM_SCANCODES; i++) { //We could technically memset this...
			states[i] = KeyState();
		}
	}*/

	//Key lookup
	const char* getKeyname(int key) {
		if (!validate(key)) { return ""; }
		return SDL_GetKeyName(SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(key)));
	}

	void update() {
		//Clear
		for (size_t i=0; i<SDL_NUM_SCANCODES; i++) {
			states[i].trigger = false;
			states[i].repeat = false;
			states[i].release = false;
		}

		//Cycle through the current keyState
		for (size_t i=0; i<SDL_NUM_SCANCODES; i++) {
			//Skip if we're not tracking this key.
			//if (!validate(i)) { continue; } //Seems like Input handles this.

			//Set press
			KeyState& ks = states[i];
			ks.press = EventThread::keyStates[i];
			
			//Has the key state changed?
			if (EventThread::keyStates[i] != lastKeyStates[i]) {
				//Repeat every 15 frames.
				if (ks.repeatCount<=0 && ks.press) {
					ks.repeat = true;
					ks.repeatCount = 15;
				}
				//Release or trigger, depending.
				if (ks.press) {
					ks.trigger = true;
				} else {
					ks.release = true;
				}
			} else {
				//Decrement the repeat counter, or cycle it (3 frames)
				if (ks.repeatCount>0 && ks.press) {
					ks.repeatCount -= 1;
				} else if (ks.repeatCount<=0 && ks.press) {
					ks.repeat = true;
					ks.repeatCount = 3;
				} else if (ks.repeatCount!=0 && !ks.press) {
					ks.repeatCount = 0;
				}
			}
		}

		//Update the last-known key state.
		memcpy(lastKeyStates, EventThread::keyStates, sizeof(lastKeyStates)*sizeof(uint8_t));
	}

	//State check functions.
	bool isPress(int key) {
		if (key==ANY_KEY) {
			for (size_t i=0; i<SDL_NUM_SCANCODES; i++) {
				if (states[i].press) { return true; }
			}
		}
		if (!validate(key)) { return false; }
		if (key==SDL_SCANCODE_LCTRL && states[SDL_SCANCODE_RCTRL].press) { return true; }
		if (key==SDL_SCANCODE_LALT && states[SDL_SCANCODE_RALT].press) { return true; }
		if (key==SDL_SCANCODE_LSHIFT && states[SDL_SCANCODE_RSHIFT].press) { return true; }
		if (key==SDL_SCANCODE_LGUI && states[SDL_SCANCODE_RGUI].press) { return true; }
		return  states[key].press;
	}
	bool isTrigger(int key) {
		if (key==ANY_KEY) {
			for (size_t i=0; i<SDL_NUM_SCANCODES; i++) {
				if (states[i].trigger) { return true; }
			}
		}
		if (!validate(key)) { return false; }
		if (key==SDL_SCANCODE_LCTRL && states[SDL_SCANCODE_RCTRL].trigger) { return true; }
		if (key==SDL_SCANCODE_LALT && states[SDL_SCANCODE_RALT].trigger) { return true; }
		if (key==SDL_SCANCODE_LSHIFT && states[SDL_SCANCODE_RSHIFT].trigger) { return true; }
		if (key==SDL_SCANCODE_LGUI && states[SDL_SCANCODE_RGUI].trigger) { return true; }
		return  states[key].trigger;
	}
	bool isRepeat(int key) {
		if (key==ANY_KEY) {
			for (size_t i=0; i<SDL_NUM_SCANCODES; i++) {
				if (states[i].repeat) { return true; }
			}
		}
		if (!validate(key)) { return false; }
		if (key==SDL_SCANCODE_LCTRL && states[SDL_SCANCODE_RCTRL].repeat) { return true; }
		if (key==SDL_SCANCODE_LALT && states[SDL_SCANCODE_RALT].repeat) { return true; }
		if (key==SDL_SCANCODE_LSHIFT && states[SDL_SCANCODE_RSHIFT].repeat) { return true; }
		if (key==SDL_SCANCODE_LGUI && states[SDL_SCANCODE_RGUI].repeat) { return true; }
		return  states[key].repeat;
	}
	bool isRelease(int key) {
		if (key==ANY_KEY) {
			for (size_t i=0; i<SDL_NUM_SCANCODES; i++) {
				if (states[i].release) { return true; }
			}
		}
		if (!validate(key)) { return false; }
		if (key==SDL_SCANCODE_LCTRL && states[SDL_SCANCODE_RCTRL].release) { return true; }
		if (key==SDL_SCANCODE_LALT && states[SDL_SCANCODE_RALT].release) { return true; }
		if (key==SDL_SCANCODE_LSHIFT && states[SDL_SCANCODE_RSHIFT].release) { return true; }
		if (key==SDL_SCANCODE_LGUI && states[SDL_SCANCODE_RGUI].release) { return true; }
		return  states[key].release;
	}	


};

#endif // KEYS_H
