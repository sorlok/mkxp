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
const std::vector<std::string> achievement_names = {
  "DrillAcquired",
  "Drilled5Items",
  "Drilled10Items",
  "Tracker",
  "Hunter",
  "MasterHunter",
  "TerranSavior",
  "HaltHelios",
  "CrushHelios",
  "DefeatDarkLord",
  "StrongEnough",
  "ReleaseKraken",
  "CrushKraken",
  "MagiGuardian",
  "MurderedMagi",
  "CCDrifter",
  "CCWanderer",
  "CCExplorer",
  "CCCartographer",
  "Completionist",
  "AboveBeyond",
  "TerranChampion",
  "LegendaryWarrior",
  "MonsterCrippler",
  "MonsterMaimer",
  "MonsterDispatcher",
  "MonsterPunisher",
  "MonsterDestroyer",
  "MonsterEradictor",
  "MonsterAnnihilator",
  "Submariner",
  "BlueMoaTamer",
  "AbyssalAdventurer",
  "AbyssalPathfinder",
  "AbyssalTrailblazer",
  "AbyssalExplorer",
  "MasterSpelunker",
  "ECDrifter",
  "ECWanderer",
  "ECExplorer",
  "ECCartographer",
  "LeadFeet",
  "NotAfraidBattle",
  "NoCoward",
  "FightLikeMan",
  "NeverRunBattle",
  "ThievesHideout",
  "SkeletonKey",
  "BanditKiller",
  "LeaveNoDoorLocked",
  "Gambler",
  "HotStreak",
  "GamblingAddict",
  "CasinoVIP",
  "HighRoller",
  "MysticalExplorer",
  "RevealingMystery",
  "MysteriousChampion",
  "NCDrifter",
  "NCWanderer",
  "NCExplorer",
  "NCCartographer",
  "Overleveled",
  "Overpowered",
  "Grinder",
  "GodlikeStrength",
  "TendencyGettingLost",
  "HereThere",
  "RamblingMan",
  "WanderingSoul",
  "WellTraveled",
  "WorldTraveler",
  "YouReallyGetAround",
  "AroundWorld",
  "LeaveNoStoneUnturned",
  "ThereBackAgain",
  "Miner",
  "Dredger",
  "Sapper",
  "Prospector",
  "Mole",
  "Bombadier",
  "Excavator",
  "VillageSky",
  "UnderSea",
  "BefriendDwarves",
  "NWCDrifter",
  "NWCWanderer",
  "NWCExplorer",
  "NWCCartographer",
  "WornSoles",
  "WearyTraveler",
  "FewShortcuts",
  "SwiftFoot",
  "WingedShoes",
  "AmateurPuzzler",
  "PuzzleDabbler",
  "PuzzleApprentice",
  "CasualPuzzler",
  "PuzzleSolver",
  "AboveAveragePuzzler",
  "SuperiorPuzzler",
  "PuzzleLover",
  "Brainiac",
  "PuzzleWizard",
  "RapidRescue",
  "DespairNoMore",
  "HelpingHand",
  "FriendAsgard",
  "AfraidDark",
  "HighJumper",
  "BellyBeast",
  "NoHelpNecessary",
  "MagiMaster",
  "DoItYourself",
  "GodlikeGear",
  "UltimateChallenge",
  "Knight",
  "Monk",
  "Thief",
  "HunterCharacter",
  "GrayMage",
  "WhiteMage",
  "BlackMage",
  "Engineer",
  "MoaForestExplorer",
  "AvidSubmariner",
  "AvidFisherman",
  "SCDrifter",
  "SCWanderer",
  "SCExplorer",
  "SCCartographer",
  "SporadicBoarder",
  "RollingStone",
  "Nomad",
  "RoughingIt",
  "NoMoreGoldfish",
  "WeekendFisherman",
  "NeverHungry",
  "GiantsSea",
  "MasterFisherman",
  "BeginnerLuck",
  "PickingUpSpeed",
  "MoaMaster",
  "MineCarter",
  "MineCartMadness",
  "MineCartMaster",
  "CIDrifter",
  "CIWanderer",
  "CIExplorer",
  "CICartographer",
  "OnlyDesignatedLocations",
  "NoRestWeary",
  "Sonar",
  "SonarNovice",
  "SonarOperator",
  "SonarTinkerer",
  "SonarExplorer",
  "SonarSpecialist",
  "SonarVeteran",
  "SonarProfessional",
  "SonarWizard",
  "SonarExpert",
  "SonarMaster",
  "Brawler",
  "Fighter",
  "Dueler",
  "Gladiator",
  "GodArena",
  "DefeatDaedalus",
  "DoubleDefeat",
  "MazeMaster",
  "OIDrifter",
  "OIWanderer",
  "OIExplorer",
  "OICartrographer",
  "InfrequentSaver",
  "MiserlySaver",
  "ScarceSaver",
  "WorthTheRisk",
  "FeetFire",
  "FireExtinguisher",
  "GuardMauler",
  "MonsterChaser",
  "MonsterTracker",
  "MonsterStalker",
  "MonsterHunter",
  "MonsterButcher",
  "MonsterExecutioner",
  "MonsterBane",
  "MonsterAssassin",
  "BlueWhale",
  "GiantSquid",
  "GiantSea",
  "SaltwaterFish",
  "IcewaterFish",
  "FreshwaterFish",
  "HighFish",
  "AllSwimmingSpecies",
  "SWCDrifter",
  "SWCWanderer",
  "SWCExplorer",
  "SWCCartographer",
  "MobMonsters",
  "EndlessMonsterHordes",
  "PumpBellows",
  "SynthesizeThis",
  "SynthesizingFool",
  "SynthesisSnob",
  "BrokkerApprentice",
  "FullyOutfitted",
  "CantGetEnough",
  "MasterBlacksmith",
  "JustFewRecipes",
  "BuildingCookbook",
  "RecipeGatherer",
  "RecipeCollector",
  "PileRecipes",
  "RecipeHoarder",
  "NearlyComplete",
  "RecipeMaster",
  "JailBreak",
  "RosettaStone",
  "TerranDrifter",
  "TerranWanderer",
  "TerranExplorer",
  "TerranCartographer",
  "TerranTreasureMaster",
  "FECDrifter",
  "FECWanderer",
  "FECExplorer",
  "FECCartographer",
  "PushingPace",
  "Sprinter",
  "Speedster",
  "SpeedDemon",
};



void SteamSyncAchievements(const char* achieveStr)
{
	//Sanity check
	std::string ourAchieves(achieveStr);
	if (ourAchieves.size() != achievement_names.size()) {
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
	if (nm != achievement_names.size()) {
		std::cout <<"Steam achievements string size mismatch (server).\n";
		return;
	}

	//Check every one.
	for (unsigned int i=0; i<nm; i++) {
		std::string name = achievement_names[i];
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
