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
	//Hard-coded into keys-binding.cpp; don't change this without also changing that!
	const static int MaxButtons = 16;

	//How long each button has been held down.
	//Note that button ids go from [0...16] for buttons, [16...32] for axis, and [32...48] for hats
	uint8_t button_holds[MaxButtons] = {0};
	uint8_t axis_holds[MaxButtons] = {0};   //0,1,2,3 = [L,R,U,D]; the rest are unused (maybe L2/R2 use these?)
	uint8_t hat_holds[MaxButtons] = {0};   //This is actually MaxButtons/4, for [U,L,R,D]


public:
	bool isPluggedIn() {
		return EventThread::js != NULL;
	}

	void update() {
		//Increment button holds, cap to prevent overflow.
		for (size_t i=0; i<MaxButtons; i++) {
			if (EventThread::joyState.buttons[i]) {
				button_holds[i] += 1;
			} else {
				button_holds[i] = 0;
			}
			if (button_holds[i]>49) {
				button_holds[i] -= 10;
			}
		}

		//Increment axis holds.
		for (size_t i=0; i<4; i+=2) {
			int value = EventThread::joyState.axes[i/2];
			if (i==0) { //Left/Right
				if (value<-JAXIS_THRESHOLD) {
					axis_holds[i+0] += 1;
				} else {
					axis_holds[i+0] = 0;
				}
				if (value>JAXIS_THRESHOLD) {
					axis_holds[i+1] += 1;
				} else {
					axis_holds[i+1] = 0;
				}
			} else if (i==2) { //Up/Down
				if (value<-JAXIS_THRESHOLD) {
					axis_holds[i+0] += 1;
				} else {
					axis_holds[i+0] = 0;
				}
				if (value>JAXIS_THRESHOLD) {
					axis_holds[i+1] += 1;
				} else {
					axis_holds[i+1] = 0;
				}
			}

			for (size_t j=0; j<2; j++) {
				if (axis_holds[i+j]>49) {
					axis_holds[i+j] -= 10;
				}
			}
		}

		//Hat holds are a huge pain.
		for (size_t i=0; i<MaxButtons; i+=4) { //0,1,2,3 = [U,L,R,D]
			//Convert it to something reasonable.
			uint8_t code = EventThread::joyState.hats[i/4];
			if (code&SDL_HAT_UP) {
				hat_holds[i+0] += 1;
			} else {
				hat_holds[i+0] = 0;
			}
			if (code&SDL_HAT_LEFT) {
				hat_holds[i+1] += 1;
			} else {
				hat_holds[i+1] = 0;
			}
			if (code&SDL_HAT_RIGHT) {
				hat_holds[i+2] += 1;
			} else {
				hat_holds[i+2] = 0;
			}
			if (code&SDL_HAT_DOWN) {
				hat_holds[i+3] += 1;
			} else {
				hat_holds[i+3] = 0;
			}
			for (size_t j=0; j<4; j++) {
				if (hat_holds[i+j]>49) {
					hat_holds[i+j] -= 10;
				}
			}
		}
	}

	//State check functions
	bool isPress(int button) {
		//Count up
		uint8_t value = 0;
		if (button>=0 && button<16) {
			value = button_holds[button];
		} else if (button>=16 && button<32) {
			value = axis_holds[button-16];
		} else if (button>=32 && button<48) {
			value = hat_holds[button-32];
		}

		//Normal
		return value>0;
	}
	bool isTrigger(int button) {
		//Count up
		uint8_t value = 0;
		if (button>=0 && button<16) {
			value = button_holds[button];
		} else if (button>=16 && button<32) {
			value = axis_holds[button-16];
		} else if (button>=32 && button<48) {
			value = hat_holds[button-32];
		}

		//Normal
		return value==1;
	}
	bool isRepeat(int button) {
		//Count up
		uint8_t value = 0;
		if (button>=0 && button<16) {
			value = button_holds[button];
		} else if (button>=16 && button<32) {
			value = axis_holds[button-16];
		} else if (button>=32 && button<48) {
			value = hat_holds[button-32];
		}

		//Normal.
		return (value==1) || (value>30 && (value%5)==0);
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
