/*
 * TODO
 */

#ifndef KEYS_H
#define KEYS_H

#include <SDL_scancode.h>
#include "util.h"

#include "eventthread.h"

class Scene;
class Bitmap;
class Disposable;
struct RGSSThreadData;
struct GraphicsPrivate;
struct AtomicFlag;

class Keys
{
private:
	//Copy of EventThread::keyStates the last time we checked it.
	uint8_t lastKeyStates[SDL_NUM_SCANCODES];

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
	void moduleInit() {
		memcpy(lastKeyStates, EventThread::keyStates, sizeof(lastKeyStates)*sizeof(uint8_t)); //TODO: Zeroing may be better...
		for (size_t i=0; i<SDL_NUM_SCANCODES; i++) { //We could technically memset this...
			states[i] = KeyState();
		}
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
		return validate(key) && states[key].press;
	}
	bool isTrigger(int key) {
		if (key==ANY_KEY) {
			for (size_t i=0; i<SDL_NUM_SCANCODES; i++) {
				if (states[i].trigger) { return true; }
			}
		}
		return validate(key) && states[key].trigger;
	}
	bool isRepeat(int key) {
		if (key==ANY_KEY) {
			for (size_t i=0; i<SDL_NUM_SCANCODES; i++) {
				if (states[i].repeat) { return true; }
			}
		}
		return validate(key) && states[key].repeat;
	}
	bool isRelease(int key) {
		if (key==ANY_KEY) {
			for (size_t i=0; i<SDL_NUM_SCANCODES; i++) {
				if (states[i].release) { return true; }
			}
		}
		return validate(key) && states[key].release;
	}	


};

#endif // KEYS_H
