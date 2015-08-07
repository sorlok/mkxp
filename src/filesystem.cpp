/*
** filesystem.cpp
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

#include "filesystem.h"

#include "rgssad.h"
#include "font.h"
#include "util.h"
#include "exception.h"
#include "sharedstate.h"
#include "boost-hash.h"
#include "debugwriter.h"

#include <physfs.h>

#include <SDL_sound.h>

#include <stdio.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <stack>

//Steam stuff
#include "steam_api.h"

#ifdef __APPLE__
#include <iconv.h>
#endif

//Generic reader/writer for profile strings.
//"value" is either "defValue" (if reading) or the value to write.
//returns NULL on write; a new string on read (assuming no error).
const char* pp_read_write(bool write, std::string section, std::string key, std::string value, const char* filePath)
{
	//Make the file if it doesn't exist.
	{
	std::ifstream f(filePath);
	if (!f.good()) {
		std::ofstream out(filePath);
		if (!out.good()) {
			return value.c_str();
		}
	}
	}

	//Open the file for reading.
	std::ifstream f(filePath);
	if (!f.good()) { return value.c_str(); }

	//Stores the current section.
	std::string currSection = "";
	bool foundSection = false;
	bool foundProp = false;

	//Read each line, buffering it to the output stream if appropriate.
	std::stringstream buff;
	std::string line;
	while (std::getline(f, line)) {
		//Trim (L+R)
		line.erase(line.find_last_not_of(" \t\r\n") + 1);
		line.erase(0, line.find_first_not_of(" \t\r\n"));

		//React to it (required in both cases).
		if (!line.empty()) {
			if (line[0]=='[' && line[line.size()-1]==']') {
				//Are we leaving the section without finding our property?
				if (write && currSection==section && !foundProp) {
					buff <<key <<"=" <<value <<"\n";
				}

				//Track it.
				currSection = line.substr(1, line.size()-2);
				if (currSection==section) {
					foundSection = true;
				}
			} else {
				size_t eq = line.find('=');
				if (eq != std::string::npos) {
					//It's a property. 
					std::string lhs = line.substr(0,eq);
					std::string rhs = line.substr(eq+1, std::string::npos);
					if (currSection==section && lhs==key) {
						if (write) {
							//Writing: re-write if we've found our string.
							line = lhs + "=" + value;
							foundProp = true;
						} else {
							//Reading: we can return early if this is what we're looking for.
							return rhs.c_str(); //TODO: I think Ruby takes ownership...
						}
					}
				}
			}
		}

		//Either way, append the line (in write mode).
		if (write) {
			buff <<line <<"\n";
		}
	}
	f.close();

	if (write) {
		//Write the updated file.
		std::ofstream out(filePath);
		if (out.good()) {
			out <<buff.str();

			//Did we not encounter the section at all?
			if (!foundSection) {
				out <<"[" <<section <<"]\n";
				out <<key <<"=" <<value <<"\n";
			}

			//Did we find the section, but not the property?
			if (currSection==section && !foundProp) {
				out <<key <<"=" <<value <<"\n";
			}
		}
		return value.c_str();
	} else {
		//Return the default value.
		return value.c_str();
	}
}

//These vaguely mimic the Windows ini functions. They are designed to be as simple as possible, and don't handle anything that reacts to a null pointer.
const char* GetPPString(const char* section, const char* key, const char* defValue, const char* filePath)
{
	//Null path?
	if (!filePath) { return NULL; }

	//Now, use our generic reader/writer function, converting strings as you go.
	return pp_read_write(false, (section?section:""), (key?key:""), (defValue?defValue:""), filePath);
}
void WritePPString(const char* section, const char* key, const char* value, const char* filePath)
{
	//Null path?
	if (!filePath) { return; }

	//Now, use our generic reader/writer function, converting strings as you go.
	pp_read_write(true, (section?section:""), (key?key:""), (value?value:""), filePath);
}

//Mapping from LastDream ID to achievement name.
//(Don't want to use libc++11 on OS-X just yet, so no initializer list.)
namespace {
//Use the get_* functions to access this; they initialize it properly.
std::vector<std::string> achievement_names;

size_t get_achievements_size() {
  if (achievement_names.empty()) {
    achievement_names.push_back("DrillAcquired");
    achievement_names.push_back("Drilled5Items");
    achievement_names.push_back("Drilled10Items");
    achievement_names.push_back("Tracker");
    achievement_names.push_back("Hunter");
    achievement_names.push_back("MasterHunter");
    achievement_names.push_back("TerranSavior");
    achievement_names.push_back("HaltHelios");
    achievement_names.push_back("CrushHelios");
    achievement_names.push_back("DefeatDarkLord");
    achievement_names.push_back("StrongEnough");
    achievement_names.push_back("ReleaseKraken");
    achievement_names.push_back("CrushKraken");
    achievement_names.push_back("MagiGuardian");
    achievement_names.push_back("MurderedMagi");
    achievement_names.push_back("CCDrifter");
    achievement_names.push_back("CCWanderer");
    achievement_names.push_back("CCExplorer");
    achievement_names.push_back("CCCartographer");
    achievement_names.push_back("Completionist");
    achievement_names.push_back("AboveBeyond");
    achievement_names.push_back("TerranChampion");
    achievement_names.push_back("LegendaryWarrior");
    achievement_names.push_back("MonsterCrippler");
    achievement_names.push_back("MonsterMaimer");
    achievement_names.push_back("MonsterDispatcher");
    achievement_names.push_back("MonsterPunisher");
    achievement_names.push_back("MonsterDestroyer");
    achievement_names.push_back("MonsterEradictor");
    achievement_names.push_back("MonsterAnnihilator");
    achievement_names.push_back("Submariner");
    achievement_names.push_back("BlueMoaTamer");
    achievement_names.push_back("AbyssalAdventurer");
    achievement_names.push_back("AbyssalPathfinder");
    achievement_names.push_back("AbyssalTrailblazer");
    achievement_names.push_back("AbyssalExplorer");
    achievement_names.push_back("MasterSpelunker");
    achievement_names.push_back("ECDrifter");
    achievement_names.push_back("ECWanderer");
    achievement_names.push_back("ECExplorer");
    achievement_names.push_back("ECCartographer");
    achievement_names.push_back("LeadFeet");
    achievement_names.push_back("NotAfraidBattle");
    achievement_names.push_back("NoCoward");
    achievement_names.push_back("FightLikeMan");
    achievement_names.push_back("NeverRunBattle");
    achievement_names.push_back("ThievesHideout");
    achievement_names.push_back("SkeletonKey");
    achievement_names.push_back("BanditKiller");
    achievement_names.push_back("LeaveNoDoorLocked");
    achievement_names.push_back("Gambler");
    achievement_names.push_back("HotStreak");
    achievement_names.push_back("GamblingAddict");
    achievement_names.push_back("CasinoVIP");
    achievement_names.push_back("HighRoller");
    achievement_names.push_back("MysticalExplorer");
    achievement_names.push_back("RevealingMystery");
    achievement_names.push_back("MysteriousChampion");
    achievement_names.push_back("NCDrifter");
    achievement_names.push_back("NCWanderer");
    achievement_names.push_back("NCExplorer");
    achievement_names.push_back("NCCartographer");
    achievement_names.push_back("Overleveled");
    achievement_names.push_back("Overpowered");
    achievement_names.push_back("Grinder");
    achievement_names.push_back("GodlikeStrength");
    achievement_names.push_back("TendencyGettingLost");
    achievement_names.push_back("HereThere");
    achievement_names.push_back("RamblingMan");
    achievement_names.push_back("WanderingSoul");
    achievement_names.push_back("WellTraveled");
    achievement_names.push_back("WorldTraveler");
    achievement_names.push_back("YouReallyGetAround");
    achievement_names.push_back("AroundWorld");
    achievement_names.push_back("LeaveNoStoneUnturned");
    achievement_names.push_back("ThereBackAgain");
    achievement_names.push_back("Miner");
    achievement_names.push_back("Dredger");
    achievement_names.push_back("Sapper");
    achievement_names.push_back("Prospector");
    achievement_names.push_back("Mole");
    achievement_names.push_back("Bombadier");
    achievement_names.push_back("Excavator");
    achievement_names.push_back("VillageSky");
    achievement_names.push_back("UnderSea");
    achievement_names.push_back("BefriendDwarves");
    achievement_names.push_back("NWCDrifter");
    achievement_names.push_back("NWCWanderer");
    achievement_names.push_back("NWCExplorer");
    achievement_names.push_back("NWCCartographer");
    achievement_names.push_back("WornSoles");
    achievement_names.push_back("WearyTraveler");
    achievement_names.push_back("FewShortcuts");
    achievement_names.push_back("SwiftFoot");
    achievement_names.push_back("WingedShoes");
    achievement_names.push_back("AmateurPuzzler");
    achievement_names.push_back("PuzzleDabbler");
    achievement_names.push_back("PuzzleApprentice");
    achievement_names.push_back("CasualPuzzler");
    achievement_names.push_back("PuzzleSolver");
    achievement_names.push_back("AboveAveragePuzzler");
    achievement_names.push_back("SuperiorPuzzler");
    achievement_names.push_back("PuzzleLover");
    achievement_names.push_back("Brainiac");
    achievement_names.push_back("PuzzleWizard");
    achievement_names.push_back("RapidRescue");
    achievement_names.push_back("DespairNoMore");
    achievement_names.push_back("HelpingHand");
    achievement_names.push_back("FriendAsgard");
    achievement_names.push_back("AfraidDark");
    achievement_names.push_back("HighJumper");
    achievement_names.push_back("BellyBeast");
    achievement_names.push_back("NoHelpNecessary");
    achievement_names.push_back("MagiMaster");
    achievement_names.push_back("DoItYourself");
    achievement_names.push_back("GodlikeGear");
    achievement_names.push_back("UltimateChallenge");
    achievement_names.push_back("Knight");
    achievement_names.push_back("Monk");
    achievement_names.push_back("Thief");
    achievement_names.push_back("HunterCharacter");
    achievement_names.push_back("GrayMage");
    achievement_names.push_back("WhiteMage");
    achievement_names.push_back("BlackMage");
    achievement_names.push_back("Engineer");
    achievement_names.push_back("MoaForestExplorer");
    achievement_names.push_back("AvidSubmariner");
    achievement_names.push_back("AvidFisherman");
    achievement_names.push_back("SCDrifter");
    achievement_names.push_back("SCWanderer");
    achievement_names.push_back("SCExplorer");
    achievement_names.push_back("SCCartographer");
    achievement_names.push_back("SporadicBoarder");
    achievement_names.push_back("RollingStone");
    achievement_names.push_back("Nomad");
    achievement_names.push_back("RoughingIt");
    achievement_names.push_back("NoMoreGoldfish");
    achievement_names.push_back("WeekendFisherman");
    achievement_names.push_back("NeverHungry");
    achievement_names.push_back("GiantsSea");
    achievement_names.push_back("MasterFisherman");
    achievement_names.push_back("BeginnerLuck");
    achievement_names.push_back("PickingUpSpeed");
    achievement_names.push_back("MoaMaster");
    achievement_names.push_back("MineCarter");
    achievement_names.push_back("MineCartMadness");
    achievement_names.push_back("MineCartMaster");
    achievement_names.push_back("CIDrifter");
    achievement_names.push_back("CIWanderer");
    achievement_names.push_back("CIExplorer");
    achievement_names.push_back("CICartographer");
    achievement_names.push_back("OnlyDesignatedLocations");
    achievement_names.push_back("NoRestWeary");
    achievement_names.push_back("Sonar");
    achievement_names.push_back("SonarNovice");
    achievement_names.push_back("SonarOperator");
    achievement_names.push_back("SonarTinkerer");
    achievement_names.push_back("SonarExplorer");
    achievement_names.push_back("SonarSpecialist");
    achievement_names.push_back("SonarVeteran");
    achievement_names.push_back("SonarProfessional");
    achievement_names.push_back("SonarWizard");
    achievement_names.push_back("SonarExpert");
    achievement_names.push_back("SonarMaster");
    achievement_names.push_back("Brawler");
    achievement_names.push_back("Fighter");
    achievement_names.push_back("Dueler");
    achievement_names.push_back("Gladiator");
    achievement_names.push_back("GodArena");
    achievement_names.push_back("DefeatDaedalus");
    achievement_names.push_back("DoubleDefeat");
    achievement_names.push_back("MazeMaster");
    achievement_names.push_back("OIDrifter");
    achievement_names.push_back("OIWanderer");
    achievement_names.push_back("OIExplorer");
    achievement_names.push_back("OICartrographer");
    achievement_names.push_back("InfrequentSaver");
    achievement_names.push_back("MiserlySaver");
    achievement_names.push_back("ScarceSaver");
    achievement_names.push_back("WorthTheRisk");
    achievement_names.push_back("FeetFire");
    achievement_names.push_back("FireExtinguisher");
    achievement_names.push_back("GuardMauler");
    achievement_names.push_back("MonsterChaser");
    achievement_names.push_back("MonsterTracker");
    achievement_names.push_back("MonsterStalker");
    achievement_names.push_back("MonsterHunter");
    achievement_names.push_back("MonsterButcher");
    achievement_names.push_back("MonsterExecutioner");
    achievement_names.push_back("MonsterBane");
    achievement_names.push_back("MonsterAssassin");
    achievement_names.push_back("BlueWhale");
    achievement_names.push_back("GiantSquid");
    achievement_names.push_back("GiantSea");
    achievement_names.push_back("SaltwaterFish");
    achievement_names.push_back("IcewaterFish");
    achievement_names.push_back("FreshwaterFish");
    achievement_names.push_back("HighFish");
    achievement_names.push_back("AllSwimmingSpecies");
    achievement_names.push_back("SWCDrifter");
    achievement_names.push_back("SWCWanderer");
    achievement_names.push_back("SWCExplorer");
    achievement_names.push_back("SWCCartographer");
    achievement_names.push_back("MobMonsters");
    achievement_names.push_back("EndlessMonsterHordes");
    achievement_names.push_back("PumpBellows");
    achievement_names.push_back("SynthesizeThis");
    achievement_names.push_back("SynthesizingFool");
    achievement_names.push_back("SynthesisSnob");
    achievement_names.push_back("BrokkerApprentice");
    achievement_names.push_back("FullyOutfitted");
    achievement_names.push_back("CantGetEnough");
    achievement_names.push_back("MasterBlacksmith");
    achievement_names.push_back("JustFewRecipes");
    achievement_names.push_back("BuildingCookbook");
    achievement_names.push_back("RecipeGatherer");
    achievement_names.push_back("RecipeCollector");
    achievement_names.push_back("PileRecipes");
    achievement_names.push_back("RecipeHoarder");
    achievement_names.push_back("NearlyComplete");
    achievement_names.push_back("RecipeMaster");
    achievement_names.push_back("JailBreak");
    achievement_names.push_back("RosettaStone");
    achievement_names.push_back("TerranDrifter");
    achievement_names.push_back("TerranWanderer");
    achievement_names.push_back("TerranExplorer");
    achievement_names.push_back("TerranCartographer");
    achievement_names.push_back("TerranTreasureMaster");
    achievement_names.push_back("FECDrifter");
    achievement_names.push_back("FECWanderer");
    achievement_names.push_back("FECExplorer");
    achievement_names.push_back("FECCartographer");
    achievement_names.push_back("PushingPace");
    achievement_names.push_back("Sprinter");
    achievement_names.push_back("Speedster");
    achievement_names.push_back("SpeedDemon");
  }
  return achievement_names.size();
}

std::string get_achievement_name(unsigned int id) {
	if (id<get_achievements_size()) { //Initializes the vector.
		return achievement_names.at(id);
	}
	return "";
}

} //End un-named namespace.




void SteamSyncAchievements(const char* achieveStr)
{
	//Sanity check
	std::string ourAchieves(achieveStr);
	if (ourAchieves.size() != get_achievements_size()) {
		std::cout <<"Steam achievements string size mismatch (client).\n";
		return;
	}

	//Can't do achievements if steam is not loaded.
	ISteamUser* user = SteamUser();
	ISteamUserStats* userStats = SteamUserStats();
	ISteamFriends* userFriends = SteamFriends();
	if (!(user && userStats)) {
		std::cout <<"Can't get Steam achievements; user or user_stats is null.\n";
		return;
	}
	std::cout <<"Requesting stats for Steam player: " <<std::string(userFriends?userFriends->GetPersonaName():"<unknown>") <<"\n";
	if (!userStats->RequestCurrentStats()) {
		std::cout <<"Can't get Steam achievements; unknown error.\n";
		return;
	}

	//Print the current achievements; compare to what we have:
	unsigned int nm = userStats->GetNumAchievements();
	if (nm != get_achievements_size()) {
		std::cout <<"Steam achievements string size mismatch (server).\n";
		return;
	}

	//Check every one.
	for (unsigned int i=0; i<nm; i++) {
		std::string name = get_achievement_name(i);
		bool status = false;
		if (!userStats->GetAchievement(name.c_str(), &status)) {
			std::cout <<"Can't get Steam achievement \"" <<name <<"\" (" <<i <<"); unknown error.\n";
			continue;
		}

		//Only update ones that we claim to have but Steam knows nothing about.
		bool us = (ourAchieves[i]=='1');
		if (us && !status) {
			std::cout <<"Registering achievement: " <<name <<"\n";
			userStats->SetAchievement(name.c_str());
		}
	}

	//Now that we're done, store them all to the server.
	userStats->StoreStats();
}


struct SDLRWIoContext
{
	SDL_RWops *ops;
	std::string filename;

	SDLRWIoContext(const char *filename)
	    : ops(SDL_RWFromFile(filename, "r")),
	      filename(filename)
	{
		if (!ops)
			throw Exception(Exception::SDLError,
			                "Failed to open file: %s", SDL_GetError());
	}

	~SDLRWIoContext()
	{
		SDL_RWclose(ops);
	}
};

static PHYSFS_Io *createSDLRWIo(const char *filename);

static SDL_RWops *getSDLRWops(PHYSFS_Io *io)
{
	return static_cast<SDLRWIoContext*>(io->opaque)->ops;
}

static PHYSFS_sint64 SDLRWIoRead(struct PHYSFS_Io *io, void *buf, PHYSFS_uint64 len)
{
	return SDL_RWread(getSDLRWops(io), buf, 1, len);
}

static int SDLRWIoSeek(struct PHYSFS_Io *io, PHYSFS_uint64 offset)
{
	return (SDL_RWseek(getSDLRWops(io), offset, RW_SEEK_SET) != -1);
}

static PHYSFS_sint64 SDLRWIoTell(struct PHYSFS_Io *io)
{
	return SDL_RWseek(getSDLRWops(io), 0, RW_SEEK_CUR);
}

static PHYSFS_sint64 SDLRWIoLength(struct PHYSFS_Io *io)
{
	return SDL_RWsize(getSDLRWops(io));
}

static struct PHYSFS_Io *SDLRWIoDuplicate(struct PHYSFS_Io *io)
{
	SDLRWIoContext *ctx = static_cast<SDLRWIoContext*>(io->opaque);
	int64_t offset = io->tell(io);
	PHYSFS_Io *dup = createSDLRWIo(ctx->filename.c_str());

	if (dup)
		SDLRWIoSeek(dup, offset);

	return dup;
}

static void SDLRWIoDestroy(struct PHYSFS_Io *io)
{
	delete static_cast<SDLRWIoContext*>(io->opaque);
	delete io;
}

static PHYSFS_Io SDLRWIoTemplate =
{
	0, 0, /* version, opaque */
	SDLRWIoRead,
	0, /* write */
	SDLRWIoSeek,
	SDLRWIoTell,
	SDLRWIoLength,
	SDLRWIoDuplicate,
	0, /* flush */
	SDLRWIoDestroy
};

static PHYSFS_Io *createSDLRWIo(const char *filename)
{
	SDLRWIoContext *ctx;

	try
	{
		ctx = new SDLRWIoContext(filename);
	}
	catch (const Exception &e)
	{
		Debug() << "Failed mounting" << filename;
		return 0;
	}

	PHYSFS_Io *io = new PHYSFS_Io;
	*io = SDLRWIoTemplate;
	io->opaque = ctx;

	return io;
}

static inline PHYSFS_File *sdlPHYS(SDL_RWops *ops)
{
	return static_cast<PHYSFS_File*>(ops->hidden.unknown.data1);
}

static Sint64 SDL_RWopsSize(SDL_RWops *ops)
{
	PHYSFS_File *f = sdlPHYS(ops);

	if (!f)
		return -1;

	return PHYSFS_fileLength(f);
}

static Sint64 SDL_RWopsSeek(SDL_RWops *ops, int64_t offset, int whence)
{
	PHYSFS_File *f = sdlPHYS(ops);

	if (!f)
		return -1;

	int64_t base;

	switch (whence)
	{
	default:
	case RW_SEEK_SET :
		base = 0;
		break;
	case RW_SEEK_CUR :
		base = PHYSFS_tell(f);
		break;
	case RW_SEEK_END :
		base = PHYSFS_fileLength(f);
		break;
	}

	int result = PHYSFS_seek(f, base + offset);

	return (result != 0) ? PHYSFS_tell(f) : -1;
}

static size_t SDL_RWopsRead(SDL_RWops *ops, void *buffer, size_t size, size_t maxnum)
{
	PHYSFS_File *f = sdlPHYS(ops);

	if (!f)
		return 0;

	PHYSFS_sint64 result = PHYSFS_readBytes(f, buffer, size*maxnum);

	return (result != -1) ? (result / size) : 0;
}

static size_t SDL_RWopsWrite(SDL_RWops *ops, const void *buffer, size_t size, size_t num)
{
	PHYSFS_File *f = sdlPHYS(ops);

	if (!f)
		return 0;

	PHYSFS_sint64 result = PHYSFS_writeBytes(f, buffer, size*num);

	return (result != -1) ? (result / size) : 0;
}

static int SDL_RWopsClose(SDL_RWops *ops)
{
	PHYSFS_File *f = sdlPHYS(ops);

	if (!f)
		return -1;

	int result = PHYSFS_close(f);
	ops->hidden.unknown.data1 = 0;

	return (result != 0) ? 0 : -1;
}

static int SDL_RWopsCloseFree(SDL_RWops *ops)
{
	int result = SDL_RWopsClose(ops);

	SDL_FreeRW(ops);

	return result;
}

/* Copies the first srcN characters from src into dst,
 * or the full string if srcN == -1. Never writes more
 * than dstMax, and guarantees dst to be null terminated.
 * Returns copied bytes (minus terminating null) */
static size_t
strcpySafe(char *dst, const char *src,
           size_t dstMax, int srcN)
{
	if (srcN < 0)
		srcN = strlen(src);

	size_t cpyMax = std::min<size_t>(dstMax-1, srcN);

	memcpy(dst, src, cpyMax);
	dst[cpyMax] = '\0';

	return cpyMax;
}

/* Attempt to locate an extension string in a filename.
 * Either a pointer into the input string pointing at the
 * extension, or null is returned */
static const char *
findExt(const char *filename)
{
	size_t len;

	for (len = strlen(filename); len > 0; --len)
	{
		if (filename[len] == '/')
			return 0;

		if (filename[len] == '.')
			return &filename[len+1];
	}

	return 0;
}

static void
initReadOps(PHYSFS_File *handle,
            SDL_RWops &ops,
            bool freeOnClose)
{
	ops.size  = SDL_RWopsSize;
	ops.seek  = SDL_RWopsSeek;
	ops.read  = SDL_RWopsRead;
	ops.write = SDL_RWopsWrite;

	if (freeOnClose)
		ops.close = SDL_RWopsCloseFree;
	else
		ops.close = SDL_RWopsClose;

	ops.type = SDL_RWOPS_PHYSFS;
	ops.hidden.unknown.data1 = handle;
}

static void strTolower(std::string &str)
{
	for (size_t i = 0; i < str.size(); ++i)
		str[i] = tolower(str[i]);
}

const Uint32 SDL_RWOPS_PHYSFS = SDL_RWOPS_UNKNOWN+10;

struct FileSystemPrivate
{
	/* Maps: lower case full filepath,
	 * To:   mixed case full filepath */
	BoostHash<std::string, std::string> pathCache;
	/* Maps: lower case directory path,
	 * To:   list of lower case filenames */
	BoostHash<std::string, std::vector<std::string> > fileLists;

	/* This is for compatibility with games that take Windows'
	 * case insensitivity for granted */
	bool havePathCache;
};

FileSystem::FileSystem(const char *argv0,
                       bool allowSymlinks)
{
	p = new FileSystemPrivate;
	p->havePathCache = false;

	PHYSFS_init(argv0);

	PHYSFS_registerArchiver(&RGSS1_Archiver);
	PHYSFS_registerArchiver(&RGSS2_Archiver);
	PHYSFS_registerArchiver(&RGSS3_Archiver);

	if (allowSymlinks)
		PHYSFS_permitSymbolicLinks(1);
}

FileSystem::~FileSystem()
{
	delete p;

	if (PHYSFS_deinit() == 0)
		Debug() << "PhyFS failed to deinit.";
}

void FileSystem::addPath(const char *path)
{
	/* Try the normal mount first */
	if (!PHYSFS_mount(path, 0, 1))
	{
		/* If it didn't work, try mounting via a wrapped
		 * SDL_RWops */
		PHYSFS_Io *io = createSDLRWIo(path);

		if (io)
			PHYSFS_mountIo(io, path, 0, 1);
	}
}

struct CacheEnumData
{
	FileSystemPrivate *p;
	std::stack<std::vector<std::string>*> fileLists;

#ifdef __APPLE__
	iconv_t nfd2nfc;
	char buf[512];
#endif

	CacheEnumData(FileSystemPrivate *p)
	    : p(p)
	{
#ifdef __APPLE__
		nfd2nfc = iconv_open("utf-8", "utf-8-mac");
#endif
	}

	~CacheEnumData()
	{
#ifdef __APPLE__
		iconv_close(nfd2nfc);
#endif
	}

	/* Converts in-place */
	void toNFC(char *inout)
	{
#ifdef __APPLE__
		size_t srcSize = strlen(inout);
		size_t bufSize = sizeof(buf);
		char *bufPtr = buf;
		char *inoutPtr = inout;

		/* Reserve room for null terminator */
		--bufSize;

		iconv(nfd2nfc,
			  &inoutPtr, &srcSize,
			  &bufPtr, &bufSize);
		/* Null-terminate */
		*bufPtr = 0;
		strcpy(inout, buf);
#else
		(void) inout;
#endif
	}
};

static void cacheEnumCB(void *d, const char *origdir,
                        const char *fname)
{
	CacheEnumData &data = *static_cast<CacheEnumData*>(d);
	char fullPath[512];

	if (!*origdir)
		snprintf(fullPath, sizeof(fullPath), "%s", fname);
	else
		snprintf(fullPath, sizeof(fullPath), "%s/%s", origdir, fname);

	/* Deal with OSX' weird UTF-8 standards */
	data.toNFC(fullPath);

	std::string mixedCase(fullPath);
	std::string lowerCase = mixedCase;
	strTolower(lowerCase);

	PHYSFS_Stat stat;
	PHYSFS_stat(fullPath, &stat);

	if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY)
	{
		/* Create a new list for this directory */
		std::vector<std::string> &list = data.p->fileLists[lowerCase];

		/* Iterate over its contents */
		data.fileLists.push(&list);
		PHYSFS_enumerateFilesCallback(fullPath, cacheEnumCB, d);
		data.fileLists.pop();
	}
	else
	{
		/* Get the file list for the directory we're currently
		 * traversing and append this filename to it */
		std::vector<std::string> &list = *data.fileLists.top();
		std::string lowerFilename(fname);
		strTolower(lowerFilename);
		list.push_back(lowerFilename);

		/* Add the lower -> mixed mapping of the file's full path */
		data.p->pathCache.insert(lowerCase, mixedCase);
	}
}

void FileSystem::createPathCache()
{
	CacheEnumData data(p);
	data.fileLists.push(&p->fileLists[""]);
	PHYSFS_enumerateFilesCallback("", cacheEnumCB, &data);

	p->havePathCache = true;
}

struct FontSetsCBData
{
	FileSystemPrivate *p;
	SharedFontState *sfs;
};

static void fontSetEnumCB(void *data, const char *,
                          const char *fname)
{
	FontSetsCBData *d = static_cast<FontSetsCBData*>(data);

	/* Only consider filenames with font extensions */
	const char *ext = findExt(fname);

	if (!ext)
		return;

	char lowExt[8];
	size_t i;

	for (i = 0; i < sizeof(lowExt)-1 && ext[i]; ++i)
		lowExt[i] = tolower(ext[i]);
	lowExt[i] = '\0';

	if (strcmp(lowExt, "ttf") && strcmp(lowExt, "otf"))
		return;

	char filename[512];
	snprintf(filename, sizeof(filename), "Fonts/%s", fname);

	PHYSFS_File *handle = PHYSFS_openRead(filename);

	if (!handle)
		return;

	SDL_RWops ops;
	initReadOps(handle, ops, false);

	d->sfs->initFontSetCB(ops, filename);

	SDL_RWclose(&ops);
}

void FileSystem::initFontSets(SharedFontState &sfs)
{
	FontSetsCBData d = { p, &sfs };

	PHYSFS_enumerateFilesCallback("Fonts", fontSetEnumCB, &d);
}

struct OpenReadEnumData
{
	FileSystem::OpenHandler &handler;
	SDL_RWops ops;

	/* The filename (without directory) we're looking for */
	const char *filename;
	size_t filenameN;

	/* Optional hash to translate full filepaths
	 * (used with path cache) */
	BoostHash<std::string, std::string> *pathTrans;

	/* Number of files we've attempted to read and parse */
	size_t matchCount;
	bool stopSearching;

	/* In case of a PhysFS error, save it here so it
	 * doesn't get changed before we get back into our code */
	const char *physfsError;

	OpenReadEnumData(FileSystem::OpenHandler &handler,
	                 const char *filename, size_t filenameN,
	                 BoostHash<std::string, std::string> *pathTrans)
	    : handler(handler), filename(filename), filenameN(filenameN),
	      pathTrans(pathTrans), matchCount(0), stopSearching(false),
	      physfsError(0)
	{}
};

static void openReadEnumCB(void *d, const char *dirpath,
                           const char *filename)
{
	OpenReadEnumData &data = *static_cast<OpenReadEnumData*>(d);
	char buffer[512];
	const char *fullPath;

	if (data.stopSearching)
		return;

	/* If there's not even a partial match, continue searching */
	if (strncmp(filename, data.filename, data.filenameN) != 0)
		return;

	if (!*dirpath)
	{
		fullPath = filename;
	}
	else
	{
		snprintf(buffer, sizeof(buffer), "%s/%s", dirpath, filename);
		fullPath = buffer;
	}

	char last = filename[data.filenameN];

	/* If fname matches up to a following '.' (meaning the rest is part
	 * of the extension), or up to a following '\0' (full match), we've
	 * found our file */
	if (last != '.' && last != '\0')
		return;

	/* If the path cache is active, translate from lower case
	 * to mixed case path */
	if (data.pathTrans)
		fullPath = (*data.pathTrans)[fullPath].c_str();

	PHYSFS_File *phys = PHYSFS_openRead(fullPath);

	if (!phys)
	{
		/* Failing to open this file here means there must
		 * be a deeper rooted problem somewhere within PhysFS.
		 * Just abort alltogether. */
		data.stopSearching = true;
		data.physfsError = PHYSFS_getLastError();

		return;
	}

	initReadOps(phys, data.ops, false);

	const char *ext = findExt(filename);

	if (data.handler.tryRead(data.ops, ext))
		data.stopSearching = true;

	++data.matchCount;
}

void FileSystem::openRead(OpenHandler &handler, const char *filename)
{
	char buffer[512];
	size_t len = strcpySafe(buffer, filename, sizeof(buffer), -1);
	char *delim;

	if (p->havePathCache)
		for (size_t i = 0; i < len; ++i)
			buffer[i] = tolower(buffer[i]);

	/* Find the deliminator separating directory and file name */
	for (delim = buffer + len; delim > buffer; --delim)
		if (*delim == '/')
			break;

	const bool root = (delim == buffer);

	const char *file = buffer;
	const char *dir = "";

	if (!root)
	{
		/* Cut the buffer in half so we can use it
		 * for both filename and directory path */
		*delim = '\0';
		file = delim+1;
		dir = buffer;
	}

	OpenReadEnumData data(handler, file, len + buffer - delim - !root,
	                      p->havePathCache ? &p->pathCache : 0);

	if (p->havePathCache)
	{
		/* Get the list of files contained in this directory
		 * and manually iterate over them */
		const std::vector<std::string> &fileList = p->fileLists[dir];

		for (size_t i = 0; i < fileList.size(); ++i)
			openReadEnumCB(&data, dir, fileList[i].c_str());
	}
	else
	{
		PHYSFS_enumerateFilesCallback(dir, openReadEnumCB, &data);
	}

	if (data.physfsError)
		throw Exception(Exception::PHYSFSError, "PhysFS: %s", data.physfsError);

	if (data.matchCount == 0)
		throw Exception(Exception::NoFileError, "%s", filename);
}

void FileSystem::openReadRaw(SDL_RWops &ops,
                             const char *filename,
                             bool freeOnClose)
{
	PHYSFS_File *handle = PHYSFS_openRead(filename);
	assert(handle);

	initReadOps(handle, ops, freeOnClose);
}

bool FileSystem::exists(const char *filename)
{
	return PHYSFS_exists(filename);
}
