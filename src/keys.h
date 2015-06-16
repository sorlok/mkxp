/*
 * TODO
 */

#ifndef KEYS_H
#define KEYS_H

#include <SDL_scancode.h>
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

	bool validate(int button) {
		//Basically, "will it crash anything"? If they want to check bogus keycodes, then by all means!
		return button>=0 && button<SDL_CONTROLLER_BUTTON_MAX; 
	}

	int key_holds(int button) {
		return holds[button];
	}

public:
	void update() {
		//Increment holds, cap.
		for (size_t i=0; i<SDL_CONTROLLER_BUTTON_MAX; i++) {
			if (EventThread::padStates[i]) {
				holds[i] += 1;
			} else {
				holds[i] = 0;
			}

			//Prevent overflow.
			if (holds[i]>49) {
				holds[i] -= 10;
			}
		}
		
	}

	//State check functions
	bool isPress(int button) {
		return validate(button) && key_holds(button)>0;
	}
	bool isTrigger(int button) {
		return validate(button) && key_holds(button)==1;
	}
	bool isRepeat(int button) {
		if (validate(button)) {
			int result = key_holds(button);
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
