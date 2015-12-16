/*
** filesystem-binding.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 Jonas Kulla <Nyocurio@gmail.com>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "binding-util.h"

#include "sharedstate.h"
#include "filesystem.h"
#include "util.h"

//NOTE: These are needed because we put implementation details for Steam in here.
//      We might consider eventually moving them.
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <sstream>

#include "steam_api.h"
#include "ruby/encoding.h"
#include "ruby/intern.h"

static void
fileIntFreeInstance(void *inst)
{
	SDL_RWops *ops = static_cast<SDL_RWops*>(inst);

	SDL_RWclose(ops);
	SDL_FreeRW(ops);
}

DEF_TYPE_CUSTOMFREE(FileInt, fileIntFreeInstance);

static VALUE
fileIntForPath(const char *path, bool rubyExc)
{
	SDL_RWops *ops = SDL_AllocRW();

	try
	{
		shState->fileSystem().openReadRaw(*ops, path);
	}
	catch (const Exception &e)
	{
		SDL_FreeRW(ops);

		if (rubyExc)
			raiseRbExc(e);
		else
			throw e;
	}

	VALUE klass = rb_const_get(rb_cObject, rb_intern("FileInt"));

	VALUE obj = rb_obj_alloc(klass);

	setPrivateData(obj, ops);

	return obj;
}

RB_METHOD(fileIntRead)
{

	int length = -1;
	rb_get_args(argc, argv, "i", &length RB_ARG_END);

	SDL_RWops *ops = getPrivateData<SDL_RWops>(self);

	if (length == -1)
	{
		Sint64 cur = SDL_RWtell(ops);
		Sint64 end = SDL_RWseek(ops, 0, SEEK_END);
		length = end - cur;
		SDL_RWseek(ops, cur, SEEK_SET);
	}

	if (length == 0)
		return Qnil;

	VALUE data = rb_str_new(0, length);

	SDL_RWread(ops, RSTRING_PTR(data), 1, length);

	return data;
}

RB_METHOD(fileIntClose)
{
	RB_UNUSED_PARAM;

	SDL_RWops *ops = getPrivateData<SDL_RWops>(self);
	SDL_RWclose(ops);

	return Qnil;
}

RB_METHOD(fileIntGetByte)
{
	RB_UNUSED_PARAM;

	SDL_RWops *ops = getPrivateData<SDL_RWops>(self);

	unsigned char byte;
	size_t result = SDL_RWread(ops, &byte, 1, 1);

	return (result == 1) ? rb_fix_new(byte) : Qnil;
}

RB_METHOD(fileIntBinmode)
{
	RB_UNUSED_PARAM;

	return Qnil;
}

VALUE
kernelLoadDataInt(const char *filename, bool rubyExc)
{
	rb_gc_start();

	VALUE port = fileIntForPath(filename, rubyExc);

	VALUE marsh = rb_const_get(rb_cObject, rb_intern("Marshal"));

	// FIXME need to catch exceptions here with begin rescue
	VALUE result = rb_funcall2(marsh, rb_intern("load"), 1, &port);

	rb_funcall2(port, rb_intern("close"), 0, NULL);

	return result;
}

RB_METHOD(kernelLoadData)
{
	RB_UNUSED_PARAM;

	const char *filename;
	rb_get_args(argc, argv, "z", &filename RB_ARG_END);

	return kernelLoadDataInt(filename, true);
}

RB_METHOD(kernelSaveData)
{
	RB_UNUSED_PARAM;

	VALUE obj;
	VALUE filename;

	rb_get_args(argc, argv, "oS", &obj, &filename RB_ARG_END);

	VALUE file = rb_file_open_str(filename, "wb");

	VALUE marsh = rb_const_get(rb_cObject, rb_intern("Marshal"));

	VALUE v[] = { obj, file };
	rb_funcall2(marsh, rb_intern("dump"), ARRAY_SIZE(v), v);

	rb_io_close(file);

	return Qnil;
}

static VALUE stringForceUTF8(VALUE arg)
{
	if (RB_TYPE_P(arg, RUBY_T_STRING) && ENCODING_IS_ASCII8BIT(arg))
		rb_enc_associate_index(arg, rb_utf8_encindex());

	return arg;
}

static VALUE customProc(VALUE arg, VALUE proc)
{
	VALUE obj = stringForceUTF8(arg);
	obj = rb_funcall2(proc, rb_intern("call"), 1, &obj);

	return obj;
}

RB_METHOD(_marshalLoad)
{
	RB_UNUSED_PARAM;

	VALUE port, proc = Qnil;

	rb_get_args(argc, argv, "o|o", &port, &proc RB_ARG_END);

	VALUE utf8Proc;
	if (NIL_P(proc))
		utf8Proc = rb_proc_new(RUBY_METHOD_FUNC(stringForceUTF8), Qnil);
	else
		utf8Proc = rb_proc_new(RUBY_METHOD_FUNC(customProc), proc);

	VALUE marsh = rb_const_get(rb_cObject, rb_intern("Marshal"));

	VALUE v[] = { port, utf8Proc };
	return rb_funcall2(marsh, rb_intern("_mkxp_load_alias"), ARRAY_SIZE(v), v);
}

RB_METHOD(ppGetString)
{
	RB_UNUSED_PARAM;

        //These are all required (we don't follow the weird Windows specification)
	const char* section = "";
	const char* key = "";
	const char* defValue = "";
	const char* filePath = "";

	rb_get_args(argc, argv, "zzzz|", &section, &key, &defValue, &filePath RB_ARG_END);

        std::string res = "";
	GUARD_EXC( res = GetPPString((section?section:""), (key?key:""), (defValue?defValue:""), (filePath?filePath:"")); )

	return rb_str_new_cstr(res.c_str());
}

RB_METHOD(ppWriteString)
{
	RB_UNUSED_PARAM;

        //These are all required (we don't follow the weird Windows specification)
	const char* section = "";
	const char* key = "";
	const char* value = "";
	const char* filePath = "";

	rb_get_args(argc, argv, "zzzz|", &section, &key, &value, &filePath RB_ARG_END);

	GUARD_EXC( WritePPString((section?section:""), (key?key:""), (value?value:""), (filePath?filePath:"")); )

	return Qnil;
}

///////////////////////////////////////////////////////////////////////////////////////
// BEGIN STEAM
///////////////////////////////////////////////////////////////////////////////////////

//This class holds everything about Steam, and is used as a broker with the Steam API from within RGSS.
class MySteamSuperClass {
public:
	MySteamSuperClass() : waitingOnSteam(false), error(false) {
	}

	//Initialize.
	int init() {
		//Try to open our logfile.
		if (!log_file.is_open()) {
			log_file.open("steam_mkxp_log.txt");
		}

		//Try to initialize steam.
		if (SteamAPI_Init()) {
			get_logfile() << "Steam API initialized." << std::endl;
		} else {
			get_logfile() << "Steam API failed to initialize for some reason..." << std::endl;
			error = true;
			return 1;
		}

		//Try to over-ride the screenshot key.
		ISteamScreenshots* ss = SteamScreenshots();
		if (ss) { ss->HookScreenshots(true); }

		return 0;
	}

	int shutdown() {
		//Cancel callbacks.
		steamLeaderboardCallback.Cancel();
		steamLeaderboardCallback2.Cancel();

		//Now, shut down steam.
		SteamAPI_Shutdown();
		get_logfile() << "Steam API shut down." << std::endl;

		//Now, reset all our internal state.
		log_file.close();
		leaderboards.clear();
		leaderToSync.clear();
		leaderSyncingName = "";
		waitingOnSteam = false;
		error = false;

		return 0;
	}


	//Get our output stream (file).
	std::ostream& get_logfile() const {
		if (log_file.is_open()) {
			return log_file;
		}
		return std::cout; //Umm... better than crashing?
	}

	//Find and start our next request. Returns:
	//  0 == no error, still busy (or still requests to start).
	//  1 == ERROR, requests will no longer go through.
	//  2 == done. No need to call this function until an external input (leaderboard sync request) occurs.
	int doNextRequest() {
		//Errors override all.
		if (error) {
			return 1;
		}

		//Always get the latest state from Steam.
		SteamAPI_RunCallbacks();

		//Still waiting for the next request?
		if (waitingOnSteam) {
			return 0;
		}

		//Is there a new request to do?
		for (std::map<std::string, SteamLeaderboard_t>::const_iterator it = leaderboards.begin(); it != leaderboards.end(); it++) {
			if (it->second == 0) {
				//Fire off an async "find" request.
				get_logfile() << "Next leaderboard to find is: \"" << it->first << "\"" << std::endl;
				SteamAPICall_t apiHdl = SteamUserStats()->FindLeaderboard(it->first.c_str());
				if (apiHdl == 0) {
					get_logfile() << "ERROR calling SteamUserStats()->FindLeaderboard() on leaderboard \"" << it->first << "\"" << std::endl;
					return 1;
				}

				//Set a handler for it.
				steamLeaderboardCallback.Set(apiHdl, this, &MySteamSuperClass::OnFindLeaderboard);

				//Done for now.
				waitingOnSteam = true;
				return 0;
			}
		}

		//Do we have any outstanding requests to sync a leaderboard value?
		std::map<std::string, int32>::const_iterator toSync = leaderToSync.begin();
		if (toSync != leaderToSync.end()) {
			//Remove it; save it as the one we're looking for.
			leaderSyncingName = toSync->first;
			int32 leaderSyncingValue = toSync->second;
			leaderToSync.erase(toSync);

			//Make sure it has a handle.
			std::map<std::string, SteamLeaderboard_t>::const_iterator leadHdl = leaderboards.find(leaderSyncingName);
			if (leadHdl == leaderboards.end() || leadHdl->second==0) {
				get_logfile() << "ERROR, leaderboard should always be found first: \"" << leaderSyncingName << "\"" << std::endl;
				return 1;
			}

			//Fire off an async "sync" request.
			SteamAPICall_t apiHdl = SteamUserStats()->UploadLeaderboardScore(leadHdl->second, k_ELeaderboardUploadScoreMethodKeepBest, leaderSyncingValue, NULL, 0);
			if (apiHdl == 0) {
				get_logfile() << "ERROR calling SteamUserStats()->UploadLeaderboardScore() on leaderboard \"" << leadHdl->first << "\"" << std::endl;
				return 1;
			}

			//Set a handler for it.
			steamLeaderboardCallback2.Set(apiHdl, this, &MySteamSuperClass::OnSyncLeaderboard);

			//Done for now.
			waitingOnSteam = true;
			return 0;
		}

		//Done
		return 2;
	}


	//Just add a leaderboard to the list of ones to find.
	void findLeaderboard(std::string leaderboardName) {
		//Ignore empty leaderboard strings.
		if (leaderboardName.empty()) {
			return;
		}

		//Add it to the list of handles to find (if we don't know about it).
		addNewLeaderboardHandle(leaderboardName);
	}


	//File a new request to sync the leaderboard. You'll have to call doNextRequest() for this to actually be sent out.
	void syncLeaderboard(std::string leaderboardName, int32 leaderboardValue) {
		//Ignore empty leaderboard strings.
		if (leaderboardName.empty()) {
			return;
		}

		//Add it as a candidate.
		leaderToSync[leaderboardName] = leaderboardValue;

		//Add it to the list of handles to find (if we don't know about it).
		addNewLeaderboardHandle(leaderboardName);
	}


	//Achievements are far simpler. Returns 0 for ok, 1 for error.
	int syncAchievement(std::string achievementName) {
		//Can't do achievements if steam is not loaded.
		ISteamUser* user = SteamUser();
		ISteamUserStats* userStats = SteamUserStats();
		ISteamFriends* userFriends = SteamFriends();
		if (!(user && userStats)) {
			get_logfile() << "Can't get Steam achievements; user or user_stats is null." << std::endl;
			return 1;
		}
		get_logfile() << "Requesting stats for Steam player: " << std::string(userFriends ? userFriends->GetPersonaName() : "<unknown>") << std::endl;
		if (!userStats->RequestCurrentStats()) {
			get_logfile() << "Can't get Steam achievements; unknown error." << std::endl;
			return 1;
		}

		//Print the current achievements
		get_logfile() << "Steam server lists " << userStats->GetNumAchievements() << " total achievements." << std::endl;

		//The only thing we refuse to sync is an empty string. Go wild with crazy SQL-injection achievements!
		if (!achievementName.empty()) {
			bool status = false;
			if (!userStats->GetAchievement(achievementName.c_str(), &status)) {
				get_logfile() << "Can't get Steam achievement \"" << achievementName << "; unknown error." << std::endl;
				return 1;
			}

			//Only update achievements steam doesn't know we've earned yet.
			if (!status) {
				get_logfile() << "Registering achievement: " << achievementName << std::endl;
				userStats->SetAchievement(achievementName.c_str());
			}
		}

		//Update the server.
		userStats->StoreStats();

		return 0;
	}


	//Are there any known errors?
	// 0 = ok and waiting
	// 1 = error
	// 2 = ok and done
	int getError() const {
		if (error) {
			return 1;
		}
		if (waitingOnSteam) {
			return 0;
		}
		return 2;
	}



	//CALLBACK: Leaderboard found.
	void OnFindLeaderboard(LeaderboardFindResult_t* pResult, bool bIOFailure) {
		const char* leaderName = getLeaderboardName(pResult->m_hSteamLeaderboard, pResult->m_bLeaderboardFound>0, bIOFailure);
		if (leaderName == NULL) {
			error = true;
			return;
		}

		//Are we clobbering another leaderboard?
		if (leaderboards[leaderName] != 0) {
			get_logfile() << "ERROR: Leaderboard callback referenced an already-defined leaderboard \"" << leaderName << "\"." << std::endl;
			error = true;
			return;
		}

		//Ok, save it.
		leaderboards[leaderName] = pResult->m_hSteamLeaderboard;
		get_logfile() << "Leaderboard found \"" << leaderName << "\"" << std::endl;
		waitingOnSteam = false;
	}

	//CALLBACK: Leaderboard synced.
	void OnSyncLeaderboard(LeaderboardScoreUploaded_t* pResult, bool bIOFailure) {
		const char* leaderName = getLeaderboardName(pResult->m_hSteamLeaderboard, pResult->m_bSuccess, bIOFailure);
		if (leaderName == NULL) {
			error = true;
			return;
		}

		//Are we clobbering another leaderboard?
		if (leaderSyncingName != leaderName) {
			get_logfile() << "ERROR: Leaderboard callback referenced a not-requested-to-sync leaderboard \"" << leaderName << "\"." << std::endl;
			error = true;
			return;
		}

		//Done, stop waiting.
		leaderSyncingName = "";
		get_logfile() << "Leaderboard synced \"" << leaderName << "\"" << std::endl;
		waitingOnSteam = false;
	}


protected:
	//HELPER: Perform some checks and get the name of the given leaderboard from the various result values.
	const char* getLeaderboardName(SteamLeaderboard_t& leaderBoard, bool pSuccess, bool bIOFailure) const {
		//Was there a generic I/O error?
		if (bIOFailure) {
			get_logfile() << "ERROR: Leaderboard callback returned an I/O error." << std::endl;
			return NULL;
		}

		//Was the leaderboard found?
		if (!pSuccess) {
			get_logfile() << "ERROR: Leaderboard callback did not find the requested leaderboard." << std::endl;
			return NULL;
		}

		//So far so good. Extract its name.
		const char* leaderName = SteamUserStats()->GetLeaderboardName(leaderBoard);

		//Are we referencing a non-existent leaderboard?
		if (leaderboards.find(leaderName) == leaderboards.end()) {
			get_logfile() << "ERROR: Leaderboard callback referenced unknown leaderboard \"" << leaderName << "\"." << std::endl;
			return NULL;
		}

		//Good.
		return leaderName;
	}

	//HELPER: Add empty handles for new leaderboards, skipping those we already know the handle for
	void addNewLeaderboardHandles(const std::vector<std::string>& leaderboardNames) {
		for (std::vector<std::string>::const_iterator it = leaderboardNames.begin(); it != leaderboardNames.end(); it++) {
			addNewLeaderboardHandle(*it);
		}
	}
	void addNewLeaderboardHandle(const std::string& leaderboardName) {
		if (leaderboards.find(leaderboardName) == leaderboards.end()) {
			get_logfile() << "New leaderboard requested by name: \"" << leaderboardName << "\"" << std::endl;
			leaderboards[leaderboardName] = 0;
		}
	}


private:
	//Logfile for all actions. Created on startup.
	mutable std::ofstream log_file;

	//Map of known leaderboard handles. Can be added to/removed from as needed.
	std::map<std::string, SteamLeaderboard_t> leaderboards;

	//Map of leaderboard=>value items that we want to sync.
	std::map<std::string, int32> leaderToSync;

	//The name of the leaderboard we are currently waiting on a sync from. 
	//(Used for confirmation only).
	std::string leaderSyncingName;

	//Are we currently waiting for a response from Steam for anything?
	bool waitingOnSteam;

	//Are we currently in error? (If so, we will always be in error.)
	//Errors relate to leaderboards, or steam not having loaded at all. An error should not strictly cause achievements to fail, but will cause the leaderboard code to not even continue trying.
	//TODO: Need to see how robust this is in the face of Steam disconnects.
	bool error;

	//Callback for our two functions. We could (probably) map multiple, but let's keep it simple.
	CCallResult<MySteamSuperClass, LeaderboardFindResult_t> steamLeaderboardCallback;
	CCallResult<MySteamSuperClass, LeaderboardScoreUploaded_t> steamLeaderboardCallback2;
	

};



//Get our Steam singleton instance.
MySteamSuperClass& get_steam() {
	static MySteamSuperClass my_steam_super;
	return my_steam_super;
}


/////////////////////
// NOTE: Usage instructions are out of date. 
////////////////////

//To use this, first save the function pointer:
//  fnSteamSync =  Win32API.new("SteamConsole.dll", "sync_steam", "pi", "i")
//Then, build an array of strings containing all the achievements for which the switch is ON:
//  achieveStr = ["SomeAch","AnotherAch"]
//Then, call it:
//  fnSteamSync.call(achieveStr.join(":"), achieveStr.length)
//Output of the LAST call to this function is stored in steam_log.txt.
//You might want to wrap this in a rescue stanza to avoid crashing the program in case of weird errors.

RB_METHOD(steamInit)
{
	int res = 1;

	GUARD_EXC( res = get_steam().init(); )

	return rb_int_new(res);
}

RB_METHOD(steamShutdown)
{
	int res = 1;

	GUARD_EXC( res = get_steam().shutdown(); )

	return rb_int_new(res);
}

RB_METHOD(steamSpecialCode)
{
	int res = 1;

	GUARD_EXC( res = get_steam().getError(); )

	return rb_int_new(res);
}

RB_METHOD(steamTick)
{
	int res = 1;

	GUARD_EXC( res = get_steam().doNextRequest(); )

	return rb_int_new(res);
}

RB_METHOD(steamFindLeaderboard)
{
	int res = 0;

	const char* leaderboardName = "";

	rb_get_args(argc, argv, "z|", &leaderboardName RB_ARG_END);

	GUARD_EXC( /*res =*/ get_steam().findLeaderboard(leaderboardName); )

	return rb_int_new(res);
}

RB_METHOD(steamSyncLeaderboard)
{
	int res = 0;

	const char* leaderboardName = "";
	const char* leaderboardValue = "";

	rb_get_args(argc, argv, "zz|", &leaderboardName, &leaderboardValue RB_ARG_END);

	int32 leadVal32 = 0;
	std::stringstream ss(leaderboardValue);
	ss >> leadVal32;
	if (ss.fail()) {
		get_steam().get_logfile() << "ERROR, leaderboard couldn't parse value as int: \"" << leaderboardValue << " \"" << std::endl;
		res = 1;
	} else {
		GUARD_EXC( /*res =*/ get_steam().syncLeaderboard(leaderboardName, leadVal32); )
	}

	return rb_int_new(res);
}

RB_METHOD(steamSyncAchievement)
{
	int res = 0;

	const char* achievementName = "";

	rb_get_args(argc, argv, "z|", &achievementName RB_ARG_END);

	GUARD_EXC( /*res =*/ get_steam().syncAchievement(achievementName); )

	return rb_int_new(res);
}


/////////////////////////////////////////////////////////////////////////
// END STEAM
/////////////////////////////////////////////////////////////////////////


RB_METHOD(configOverSetVsync)
{
	RB_UNUSED_PARAM;

	bool setOn;
	rb_get_args(argc, argv, "b", &setOn RB_ARG_END); \

	GUARD_EXC( shState->overrideConfigVsync(setOn); )

	return Qnil;
}

RB_METHOD(configOverGetVsync)
{
	RB_UNUSED_PARAM;

	bool res = false;
	GUARD_EXC( res=shState->getConfigVsync(); )

	return rb_bool_new(res);
}

RB_METHOD(configOverSetSmooth)
{
	RB_UNUSED_PARAM;

	bool setOn;
	rb_get_args(argc, argv, "b", &setOn RB_ARG_END); \

	GUARD_EXC( shState->overrideConfigSmooth(setOn); )

	return Qnil;
}

RB_METHOD(configOverGetSmooth)
{
	RB_UNUSED_PARAM;

	bool res = false;
	GUARD_EXC( res=shState->getConfigSmooth(); )

	return rb_bool_new(res);
}




void
fileIntBindingInit()
{
	//Special module: PrivateProfile
	{
	VALUE module = rb_define_module("PrivateProfile");
	_rb_define_module_function(module, "get_string", ppGetString);
	_rb_define_module_function(module, "write_string", ppWriteString);
	}

	//Special module: SteamAchievements
	{
	VALUE module = rb_define_module("SteamAPI");

	//Must be called first.
	_rb_define_module_function(module, "steam_init", steamInit);

	//Unlikely you will need this, but it *does* reset the state of the system in case you want to try again.
	_rb_define_module_function(module, "steam_shutdown", steamShutdown);

	//Get a generic error code. (NOTE: for display purposes only; please use the return value of steam_tick for control).
	_rb_define_module_function(module, "steam_special_code", steamSpecialCode);

	//Call every time tick until it returns "2" (for "done") or "1" (for "error")
	_rb_define_module_function(module, "steam_tick", steamTick);

	//Optional: Call this to find a leaderboard you know you'll need later. 
	_rb_define_module_function(module, "steam_find_leaderboard", steamFindLeaderboard);

	//Call this to sync a new leaderboard. Note that you should then call steam_tick() until a 1 or 2 is returned.
	//(You can safely call several of these in a row.)
	_rb_define_module_function(module, "steam_sync_leaderboard", steamSyncLeaderboard);

	//Call this to sync a single achievement. Note that you do NOT need to call steam_tick().
	//Note that a return value of 1 does *not* disable everything (like leaderboards). 
	//Note that this should still work even if leaderboards are messed up for some reason.
	_rb_define_module_function(module, "steam_sync_achievement", steamSyncAchievement);
	}

	//Special module: ConfigOverride
	{
	VALUE module = rb_define_module("ConfigOverride");
	_rb_define_module_function(module, "set_vsync", configOverSetVsync);
	_rb_define_module_function(module, "get_vsync", configOverGetVsync);
	_rb_define_module_function(module, "set_smooth", configOverSetSmooth);
	_rb_define_module_function(module, "get_smooth", configOverGetSmooth);
	}


	VALUE klass = rb_define_class("FileInt", rb_cIO);
	rb_define_alloc_func(klass, classAllocate<&FileIntType>);

	_rb_define_method(klass, "read", fileIntRead);
	_rb_define_method(klass, "getbyte", fileIntGetByte);
	_rb_define_method(klass, "binmode", fileIntBinmode);
	_rb_define_method(klass, "close", fileIntClose);

	_rb_define_module_function(rb_mKernel, "load_data", kernelLoadData);
	_rb_define_module_function(rb_mKernel, "save_data", kernelSaveData);

	/* We overload the built-in 'Marshal::load()' function to silently
	 * insert our utf8proc that ensures all read strings will be
	 * UTF-8 encoded */
	VALUE marsh = rb_const_get(rb_cObject, rb_intern("Marshal"));
	rb_define_alias(rb_singleton_class(marsh), "_mkxp_load_alias", "load");
	_rb_define_module_function(marsh, "load", _marshalLoad);
}
