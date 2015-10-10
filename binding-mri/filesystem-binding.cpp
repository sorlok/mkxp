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


RB_METHOD(steamAchieveInit)
{
	RB_UNUSED_PARAM;

	//This contains every steam achievement name, separated by a ":"
	const char* achieveStr = "";
	int numAchievements = 0;

	rb_get_args(argc, argv, "zi|", &achieveStr, &numAchievements RB_ARG_END);

	GUARD_EXC( SteamInitAchievementNames(achieveStr, numAchievements); )

	return Qnil;
}


RB_METHOD(steamAchieveSync)
{
	RB_UNUSED_PARAM;

        //This contains a 1/0 for every achievement, in order. (1==got it)
	const char* achieveStr = "";

	rb_get_args(argc, argv, "z|", &achieveStr RB_ARG_END);

	GUARD_EXC( SteamSyncAchievements(achieveStr); )

	return Qnil;
}

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

	return rb_int_new(res);
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

	return rb_int_new(res);
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
	VALUE module = rb_define_module("SteamAchievements");
	_rb_define_module_function(module, "init_names", steamAchieveInit);
	_rb_define_module_function(module, "sync_all", steamAchieveSync);
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
