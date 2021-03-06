// Copyright (c) 2006, 2007, Niels Martin Hansen
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file auto4_lua.cpp
/// @brief Lua 5.1-based scripting engine
/// @ingroup scripting
///

#include "config.h"

#ifdef WITH_AUTO4_LUA

#include "auto4_lua.h"

#include <cassert>
#include <cstdint>

#include <algorithm>

#include <wx/clipbrd.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>
#include <wx/window.h>

#include <libaegisub/access.h>
#include <libaegisub/log.h>
#include <libaegisub/scoped_ptr.h>

#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_style.h"
#include "auto4_lua_factory.h"
#include "auto4_lua_scriptreader.h"
#include "compat.h"
#include "include/aegisub/context.h"
#include "main.h"
#include "selection_controller.h"
#include "standard_paths.h"
#include "video_context.h"
#include "utils.h"

// This must be below the headers above.
#ifdef __WINDOWS__
#include "../../contrib/lua51/src/lualib.h"
#include "../../contrib/lua51/src/lauxlib.h"
#else
#include <lua.hpp>
#endif

namespace {
	inline void push_value(lua_State *L, lua_CFunction fn)
	{
		lua_pushcfunction(L, fn);
	}

	inline void push_value(lua_State *L, int n)
	{
		lua_pushinteger(L, n);
	}

	inline void push_value(lua_State *L, void *p)
	{
		lua_pushlightuserdata(L, p);
	}

	template<class T>
	inline void set_field(lua_State *L, const char *name, T value)
	{
		push_value(L, value);
		lua_setfield(L, -2, name);
	}

	inline wxString get_wxstring(lua_State *L, int idx)
	{
		return wxString(lua_tostring(L, idx), wxConvUTF8);
	}

	inline wxString check_wxstring(lua_State *L, int idx)
	{
		return wxString(luaL_checkstring(L, idx), wxConvUTF8);
	}

	wxString get_global_string(lua_State *L, const char *name)
	{
		lua_getglobal(L, name);
		wxString ret;
		if (lua_isstring(L, -1))
			ret = get_wxstring(L, -1);
		lua_pop(L, 1);
		return ret;
	}

	void set_context(lua_State *L, const agi::Context *c)
	{
		// Explicit cast is needed to discard the const
		lua_pushlightuserdata(L, (void *)c);
		lua_setfield(L, LUA_REGISTRYINDEX, "project_context");
	}

	const agi::Context *get_context(lua_State *L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "project_context");
		if (!lua_islightuserdata(L, -1)) {
			lua_pop(L, 1);
			return 0;
		}
		const agi::Context * c = static_cast<const agi::Context *>(lua_touserdata(L, -1));
		lua_pop(L, 1);
		return c;
	}

	int get_file_name(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		if (c && c->ass->filename.size())
			lua_pushstring(L, wxFileName(c->ass->filename).GetFullName().utf8_str());
		else
			lua_pushnil(L);
		return 1;
	}

	int get_translation(lua_State *L)
	{
		wxString str(check_wxstring(L, 1));
		lua_pushstring(L, _(str).utf8_str());
		return 1;
	}

	inline wxRegEx *get_regex(lua_State *L)
	{
		return static_cast<wxRegEx*>(luaL_checkudata(L, 1, "aegisub.regex"));
	}

	int regex_matches(lua_State *L)
	{
		lua_pushboolean(L, get_regex(L)->Matches(check_wxstring(L, 2)));
		return 1;
	}

	int regex_match_count(lua_State *L)
	{
		wxRegEx *re = get_regex(L);
		if (re->Matches(check_wxstring(L, 2)))
			lua_pushinteger(L, re->GetMatchCount());
		else
			lua_pushinteger(L, 0);
		return 1;
	}

	size_t utf8_len(wxString const& w)
	{
#if wxUSE_UNICODE_UTF8
		return w.utf8_length();
#else
		return w.utf8_str().length();
#endif
	}

	int regex_get_match(lua_State *L)
	{
		wxString str(check_wxstring(L, 2));
		size_t start, len;
		get_regex(L)->GetMatch(&start, &len, luaL_checkinteger(L, 3));
		lua_pushinteger(L, utf8_len(str.Left(start)) + 1);
		lua_pushinteger(L, utf8_len(str.Left(start + len)));
		return 2;
	}

	int regex_replace(lua_State *L)
	{
		wxString str(check_wxstring(L, 3));
		int reps = get_regex(L)->Replace(&str, check_wxstring(L, 2), luaL_checkinteger(L, 4));
		lua_pushstring(L, str.utf8_str());
		lua_pushinteger(L, reps);
		return 2;
	}

	int regex_compile(lua_State *L)
	{
		wxString pattern(check_wxstring(L, 1));
		int flags = luaL_checkinteger(L, 2);
		wxRegEx *re = static_cast<wxRegEx*>(lua_newuserdata(L, sizeof(wxRegEx)));
		new(re) wxRegEx(pattern, wxRE_ADVANCED | flags);

		luaL_getmetatable(L, "aegisub.regex");
		lua_setmetatable(L, -2);

		// return nil and handle the error in lua as it's a bit easier to
		// report the actual call site from there
		if (!re->IsValid()) {
			lua_pop(L, 1);
			lua_pushnil(L);
		}

		return 1;
	}

	int regex_gc(lua_State *L) {
		get_regex(L)->~wxRegEx();
		return 0;
	}

	int regex_process_flags(lua_State *L)
	{
		int ret = 0;
		int nargs = lua_gettop(L);
		for (int i = 1; i <= nargs; ++i) {
			if (!lua_islightuserdata(L, i)) {
				lua_pushstring(L, "Flags must follow all non-flag arguments");
				return 1;
			}
			ret |= (int)(intptr_t)lua_touserdata(L, i);
		}

		lua_pushinteger(L, ret);
		return 1;
	}

	int regex_init_flags(lua_State *L)
	{
		lua_newtable(L);

		set_field(L, "ICASE", (void*)wxRE_ICASE);
		set_field(L, "NOSUB", (void*)wxRE_NOSUB);
		set_field(L, "NEWLINE", (void*)wxRE_NEWLINE);

		return 1;
	}

	int regex_init(lua_State *L)
	{
		if (luaL_newmetatable(L, "aegisub.regex")) {
			set_field(L, "__gc", regex_gc);
			lua_pop(L, 1);
		}

		lua_newtable(L);
		set_field(L, "matches", regex_matches);
		set_field(L, "match_count", regex_match_count);
		set_field(L, "get_match", regex_get_match);
		set_field(L, "replace", regex_replace);
		set_field(L, "compile", regex_compile);
		set_field(L, "process_flags", regex_process_flags);
		set_field(L, "init_flags", regex_init_flags);
		return 1;
	}

	int clipboard_get(lua_State *L)
	{
		wxString data = GetClipboard();
		if (!data)
			lua_pushnil(L);
		else
			lua_pushstring(L, data.utf8_str());
		return 1;
	}

	int clipboard_set(lua_State *L)
	{
		wxString str(check_wxstring(L, 1));

		bool succeeded = false;

#if wxUSE_OLE
		// OLE needs to be initialized on each thread that wants to write to
		// the clipboard, which wx does not handle automatically
		wxClipboard cb;
		wxClipboard *theCB = &cb;
#else
		wxClipboard *theCB = wxTheClipboard;
#endif
		if (theCB->Open()) {
			succeeded = theCB->SetData(new wxTextDataObject(str));
			theCB->Close();
			theCB->Flush();
		}

		lua_pushboolean(L, succeeded);
		return 1;
	}

	int clipboard_init(lua_State *L)
	{
		lua_newtable(L);
		set_field(L, "get", clipboard_get);
		set_field(L, "set", clipboard_set);
		return 1;
	}
}

	// LuaStackcheck
#ifdef _DEBUG
	struct LuaStackcheck {
		lua_State *L;
		int startstack;
		void check_stack(int additional)
		{
			int top = lua_gettop(L);
			if (top - additional != startstack) {
				LOG_D("automation/lua") << "lua stack size mismatch.";
				dump();
				assert(top - additional == startstack);
			}
		}
		void dump()
		{
			int top = lua_gettop(L);
			LOG_D("automation/lua/stackdump") << "--- dumping lua stack...";
			for (int i = top; i > 0; i--) {
				lua_pushvalue(L, i);
				wxString type(lua_typename(L, lua_type(L, -1)), wxConvUTF8);
				if (lua_isstring(L, i)) {
					LOG_D("automation/lua/stackdump") << type << ": " << lua_tostring(L, -1);
				} else {
					LOG_D("automation/lua/stackdump") << type;
				}
				lua_pop(L, 1);
			}
			LOG_D("automation/lua") << "--- end dump";
		}
		LuaStackcheck(lua_State *L) : L(L) { startstack = lua_gettop(L); }
		~LuaStackcheck() { check_stack(0); }
	};
#else
	struct LuaStackcheck {
		void check_stack(int) { }
		void dump() { }
		LuaStackcheck(lua_State*) { }
	};
#endif

namespace Automation4 {
	// LuaScript
	LuaScript::LuaScript(wxString const& filename)
	: Script(filename)
	, L(0)
	{
		Create();
	}

	LuaScript::~LuaScript()
	{
		Destroy();
	}

	void LuaScript::Create()
	{
		Destroy();

		try {
			// create lua environment
			L = lua_open();
			LuaStackcheck _stackcheck(L);

			// register standard libs
			lua_pushcfunction(L, luaopen_base); lua_call(L, 0, 0);
			lua_pushcfunction(L, luaopen_package); lua_call(L, 0, 0);
			lua_pushcfunction(L, luaopen_string); lua_call(L, 0, 0);
			lua_pushcfunction(L, luaopen_table); lua_call(L, 0, 0);
			lua_pushcfunction(L, luaopen_math); lua_call(L, 0, 0);
			lua_pushcfunction(L, luaopen_io); lua_call(L, 0, 0);
			lua_pushcfunction(L, luaopen_os); lua_call(L, 0, 0);
			_stackcheck.check_stack(0);

			// dofile and loadfile are replaced with include
			lua_pushnil(L);
			lua_setglobal(L, "dofile");
			lua_pushnil(L);
			lua_setglobal(L, "loadfile");
			lua_pushcfunction(L, LuaInclude);
			lua_setglobal(L, "include");

			// add include_path to the module load path
			lua_getglobal(L, "package");
			lua_pushstring(L, "path");
			lua_pushstring(L, "path");
			lua_gettable(L, -3);

			for (wxString const& path : include_path) {
				wxCharBuffer p = path.utf8_str();
				lua_pushfstring(L, ";%s/?.lua;%s/?/init.lua", p.data(), p.data());
				lua_concat(L, 2);
			}

			lua_settable(L, -3);

			// Replace the default lua module loader with our unicode compatible one
			lua_getfield(L, -1, "loaders");
			lua_pushcfunction(L, LuaModuleLoader);
			lua_rawseti(L, -2, 2);
			lua_pop(L, 2);
			_stackcheck.check_stack(0);

			// prepare stuff in the registry

			// store the script's filename
			lua_pushstring(L, wxFileName(GetFilename()).GetName().utf8_str().data());
			lua_setfield(L, LUA_REGISTRYINDEX, "filename");
			_stackcheck.check_stack(0);

			// reference to the script object
			lua_pushlightuserdata(L, this);
			lua_setfield(L, LUA_REGISTRYINDEX, "aegisub");
			_stackcheck.check_stack(0);

			// make "aegisub" table
			lua_pushstring(L, "aegisub");
			lua_newtable(L);

			set_field(L, "register_macro", LuaCommand::LuaRegister);
			set_field(L, "register_filter", LuaExportFilter::LuaRegister);
			set_field(L, "text_extents", LuaTextExtents);
			set_field(L, "frame_from_ms", LuaFrameFromMs);
			set_field(L, "ms_from_frame", LuaMsFromFrame);
			set_field(L, "video_size", LuaVideoSize);
			set_field(L, "keyframes", LuaGetKeyframes);
			set_field(L, "decode_path", LuaDecodePath);
			set_field(L, "cancel", LuaCancel);
			set_field(L, "lua_automation_version", 4);
			set_field(L, "__init_regex", regex_init);
			set_field(L, "__init_clipboard", clipboard_init);
			set_field(L, "file_name", get_file_name);
			set_field(L, "gettext", get_translation);

			// store aegisub table to globals
			lua_settable(L, LUA_GLOBALSINDEX);
			_stackcheck.check_stack(0);

			// load user script
			LuaScriptReader script_reader(GetFilename());
			if (lua_load(L, script_reader.reader_func, &script_reader, GetPrettyFilename().utf8_str())) {
				wxString err = wxString::Format("Error loading Lua script \"%s\":\n\n%s", GetPrettyFilename(), get_wxstring(L, -1));
				lua_pop(L, 1);
				throw ScriptLoadError(from_wx(err));
			}
			_stackcheck.check_stack(1);

			// and execute it
			// this is where features are registered
			// don't thread this, as there's no point in it and it seems to break on wx 2.8.3, for some reason
			if (lua_pcall(L, 0, 0, 0)) {
				// error occurred, assumed to be on top of Lua stack
				wxString err = wxString::Format("Error initialising Lua script \"%s\":\n\n%s", GetPrettyFilename(), get_wxstring(L, -1));
				lua_pop(L, 1);
				throw ScriptLoadError(from_wx(err));
			}
			_stackcheck.check_stack(0);

			lua_getglobal(L, "version");
			if (lua_isnumber(L, -1) && lua_tointeger(L, -1) == 3) {
				lua_pop(L, 1); // just to avoid tripping the stackcheck in debug
				throw ScriptLoadError("Attempted to load an Automation 3 script as an Automation 4 Lua script. Automation 3 is no longer supported.");
			}

			name = get_global_string(L, "script_name");
			description = get_global_string(L, "script_description");
			author = get_global_string(L, "script_author");
			version = get_global_string(L, "script_version");

			if (name.empty())
				name = GetPrettyFilename();

			lua_pop(L, 1);
			// if we got this far, the script should be ready
			_stackcheck.check_stack(0);

		}
		catch (agi::Exception const& e) {
			Destroy();
			name = GetPrettyFilename();
			description = e.GetChainedMessage();
		}
	}

	void LuaScript::Destroy()
	{
		// Assume the script object is clean if there's no Lua state
		if (!L) return;

		// loops backwards because commands remove themselves from macros when
		// they're unregistered
		for (int i = macros.size() - 1; i >= 0; --i)
			cmd::unreg(macros[i]->name());

		delete_clear(filters);

		lua_close(L);
		L = 0;
	}

	void LuaScript::Reload()
	{
		Create();
	}

	void LuaScript::RegisterCommand(LuaCommand *command)
	{
		for (auto macro : macros) {
			if (macro->name() == command->name()) {
				luaL_error(L,
					"A macro named '%s' is already defined in script '%s'",
					command->StrDisplay(0).utf8_str().data(), name.utf8_str().data());
			}
		}
		macros.push_back(command);
	}

	void LuaScript::UnregisterCommand(LuaCommand *command)
	{
		macros.erase(remove(macros.begin(), macros.end(), command), macros.end());
	}

	void LuaScript::RegisterFilter(LuaExportFilter *filter)
	{
		filters.push_back(filter);
	}

	LuaScript* LuaScript::GetScriptObject(lua_State *L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "aegisub");
		void *ptr = lua_touserdata(L, -1);
		lua_pop(L, 1);
		return (LuaScript*)ptr;
	}

	int LuaScript::LuaTextExtents(lua_State *L)
	{
		luaL_argcheck(L, lua_istable(L, 1), 1, "");
		luaL_argcheck(L, lua_isstring(L, 2), 2, "");

		lua_pushvalue(L, 1);
		agi::scoped_ptr<AssEntry> et(LuaAssFile::LuaToAssEntry(L));
		AssStyle *st = dynamic_cast<AssStyle*>(et.get());
		lua_pop(L, 1);
		if (!st)
			return luaL_error(L, "Not a style entry");

		double width, height, descent, extlead;
		if (!CalculateTextExtents(st, check_wxstring(L, 2), width, height, descent, extlead))
			return luaL_error(L, "Some internal error occurred calculating text_extents");

		lua_pushnumber(L, width);
		lua_pushnumber(L, height);
		lua_pushnumber(L, descent);
		lua_pushnumber(L, extlead);
		return 4;
	}

	/// @brief Module loader which uses our include rather than Lua's, for unicode file support
	/// @param L The Lua state
	/// @return Always 1 per loader_Lua?
	int LuaScript::LuaModuleLoader(lua_State *L)
	{
		int pretop = lua_gettop(L);
		wxString module(get_wxstring(L, -1));
		module.Replace(".", LUA_DIRSEP);

		// Get the lua package include path (which the user may have modified)
		lua_getglobal(L, "package");
		lua_pushstring(L, "path");
		lua_gettable(L, -2);
		wxString package_paths(get_wxstring(L, -1));
		lua_pop(L, 2);

		wxStringTokenizer toker(package_paths, ";", wxTOKEN_STRTOK);
		while (toker.HasMoreTokens()) {
			wxString filename = toker.GetNextToken();
			filename.Replace("?", module);
			try {
				LuaScriptReader script_reader(filename);
				if (lua_load(L, script_reader.reader_func, &script_reader, filename.utf8_str())) {
					return luaL_error(L, "Error loading Lua module \"%s\":\n\n%s", filename.utf8_str().data(), lua_tostring(L, -1));
				}
				break;
			}
			catch (agi::FileNotFoundError const&) {
				// Not an error so swallow and continue on
			}
			catch (agi::acs::NotAFile const&) {
				// Not an error so swallow and continue on
			}
			catch (agi::Exception const& e) {
				return luaL_error(L, "Error loading Lua module \"%s\":\n\n%s", filename.utf8_str().data(), e.GetChainedMessage().c_str());
			}
		}

		return lua_gettop(L) - pretop;
	}

	int LuaScript::LuaInclude(lua_State *L)
	{
		LuaScript *s = GetScriptObject(L);

		wxString fnames(check_wxstring(L, 1));

		wxFileName fname(fnames);
		if (fname.GetDirCount() == 0) {
			// filename only
			fname = s->include_path.FindAbsoluteValidPath(fnames);
		} else if (fname.IsRelative()) {
			// relative path
			wxFileName sfname(s->GetFilename());
			fname.MakeAbsolute(sfname.GetPath(true));
		} else {
			// absolute path, do nothing
		}
		if (!fname.IsOk() || !fname.FileExists())
			return luaL_error(L, "Lua include not found: %s", fnames.utf8_str().data());

		LuaScriptReader script_reader(fname.GetFullPath());
		if (lua_load(L, script_reader.reader_func, &script_reader, fname.GetFullName().utf8_str()))
			return luaL_error(L, "Error loading Lua include \"%s\":\n\n%s", fname.GetFullPath().utf8_str().data(), lua_tostring(L, -1));

		int pretop = lua_gettop(L) - 1; // don't count the function value itself
		lua_call(L, 0, LUA_MULTRET);
		return lua_gettop(L) - pretop;
	}

	int LuaScript::LuaFrameFromMs(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		int ms = lua_tointeger(L, -1);
		lua_pop(L, 1);
		if (c && c->videoController->TimecodesLoaded())
			lua_pushnumber(L, c->videoController->FrameAtTime(ms, agi::vfr::START));
		else
			lua_pushnil(L);

		return 1;
	}

	int LuaScript::LuaMsFromFrame(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		int frame = lua_tointeger(L, -1);
		lua_pop(L, 1);
		if (c && c->videoController->TimecodesLoaded())
			lua_pushnumber(L, c->videoController->TimeAtFrame(frame, agi::vfr::START));
		else
			lua_pushnil(L);
		return 1;
	}

	int LuaScript::LuaVideoSize(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		if (c && c->videoController->IsLoaded()) {
			lua_pushnumber(L, c->videoController->GetWidth());
			lua_pushnumber(L, c->videoController->GetHeight());
			lua_pushnumber(L, c->videoController->GetAspectRatioValue());
			lua_pushnumber(L, c->videoController->GetAspectRatioType());
			return 4;
		}
		else {
			lua_pushnil(L);
			return 1;
		}
	}

	int LuaScript::LuaGetKeyframes(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		if (!c) {
			lua_pushnil(L);
			return 1;
		}

		std::vector<int> const& kf = c->videoController->GetKeyFrames();

		lua_newtable(L);
		for (size_t i = 0; i < kf.size(); ++i) {
			lua_pushinteger(L, kf[i]);
			lua_rawseti(L, -2, i);
		}

		return 1;
	}

	int LuaScript::LuaDecodePath(lua_State *L)
	{
		wxString path = check_wxstring(L, 1);
		lua_pop(L, 1);
		lua_pushstring(L, StandardPaths::DecodePath(path).utf8_str());
		return 1;
	}

	int LuaScript::LuaCancel(lua_State *L)
	{
		lua_pushnil(L);
		return lua_error(L);
	}

	void LuaThreadedCall(lua_State *L, int nargs, int nresults, wxString const& title, wxWindow *parent, bool can_open_config)
	{
		bool failed = false;
		BackgroundScriptRunner bsr(parent, title);
		bsr.Run([&](ProgressSink *ps) {
			LuaProgressSink lps(L, ps, can_open_config);

			if (lua_pcall(L, nargs, nresults, 0)) {
				if (!lua_isnil(L, -1)) {
					// if the call failed, log the error here
					ps->Log("\n\nLua reported a runtime error:\n");
					ps->Log(lua_tostring(L, -1));
				}
				lua_pop(L, 1);
				failed = true;
			}

			lua_gc(L, LUA_GCCOLLECT, 0);
		});
		if (failed)
			throw agi::UserCancelException("Script threw an error");
	}

	// LuaFeature
	LuaFeature::LuaFeature(lua_State *L)
	: myid(0)
	, L(L)
	{
	}

	void LuaFeature::RegisterFeature()
	{
		myid = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	void LuaFeature::UnregisterFeature()
	{
		luaL_unref(L, LUA_REGISTRYINDEX, myid);
	}

	void LuaFeature::GetFeatureFunction(const char *function) const
	{
		// get this feature's function pointers
		lua_rawgeti(L, LUA_REGISTRYINDEX, myid);
		// get pointer for validation function
		lua_pushstring(L, function);
		lua_rawget(L, -2);
		// remove the function table
		lua_remove(L, -2);
		assert(lua_isfunction(L, -1));
	}

	// LuaFeatureMacro
	int LuaCommand::LuaRegister(lua_State *L)
	{
		cmd::reg(new LuaCommand(L));
		return 0;
	}

	LuaCommand::LuaCommand(lua_State *L)
	: LuaFeature(L)
	, display(check_wxstring(L, 1))
	, help(get_wxstring(L, 2))
	, cmd_type(cmd::COMMAND_NORMAL)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "filename");
		cmd_name = from_wx(wxString::Format("automation/lua/%s/%s", get_wxstring(L, -1), check_wxstring(L, 1)));

		if (!lua_isfunction(L, 3))
			luaL_error(L, "The macro processing function must be a function");

		if (lua_isfunction(L, 4))
			cmd_type |= cmd::COMMAND_VALIDATE;

		if (lua_isfunction(L, 5))
			cmd_type |= cmd::COMMAND_TOGGLE;

		// new table for containing the functions for this feature
		lua_newtable(L);

		// store processing function
		lua_pushstring(L, "run");
		lua_pushvalue(L, 3);
		lua_rawset(L, -3);

		// store validation function
		lua_pushstring(L, "validate");
		lua_pushvalue(L, 4);
		lua_rawset(L, -3);

		// store active function
		lua_pushstring(L, "isactive");
		lua_pushvalue(L, 5);
		lua_rawset(L, -3);

		// store the table in the registry
		RegisterFeature();

		LuaScript::GetScriptObject(L)->RegisterCommand(this);
	}

	LuaCommand::~LuaCommand()
	{
		UnregisterFeature();
		LuaScript::GetScriptObject(L)->UnregisterCommand(this);
	}

	static int transform_selection(lua_State *L, const agi::Context *c)
	{
		SubtitleSelection const& sel = c->selectionController->GetSelectedSet();
		AssDialogue *active_line = c->selectionController->GetActiveLine();

		lua_newtable(L);
		int active_idx = -1;

		int row = 0;
		int idx = 1;
		for (auto& line : c->ass->Line) {
			++row;
			AssDialogue *diag = dynamic_cast<AssDialogue*>(&line);
			if (!diag) continue;

			if (diag == active_line) active_idx = row;
			if (sel.count(diag)) {
				lua_pushinteger(L, row);
				lua_rawseti(L, -2, idx++);
			}
		}

		return active_idx;
	}

	bool LuaCommand::Validate(const agi::Context *c)
	{
		if (!(cmd_type & cmd::COMMAND_VALIDATE)) return true;

		set_context(L, c);

		GetFeatureFunction("validate");
		LuaAssFile *subsobj = new LuaAssFile(L, c->ass);
		lua_pushinteger(L, transform_selection(L, c));

		int err = lua_pcall(L, 3, 2, 0);

		subsobj->ProcessingComplete();

		if (err) {
			wxLogWarning("Runtime error in Lua macro validation function:\n%s", get_wxstring(L, -1));
			lua_pop(L, 1);
			return false;
		}

		bool result = !!lua_toboolean(L, -2);

		wxString new_help_string(get_wxstring(L, -1));
		if (new_help_string.size()) {
			help = new_help_string;
			cmd_type |= cmd::COMMAND_DYNAMIC_HELP;
		}

		lua_pop(L, 2);

		return result;
	}

	void LuaCommand::operator()(agi::Context *c)
	{
		LuaStackcheck stackcheck(L);
		set_context(L, c);
		stackcheck.check_stack(0);

		GetFeatureFunction("run");
		LuaAssFile *subsobj = new LuaAssFile(L, c->ass, true, true);
		lua_pushinteger(L, transform_selection(L, c));

		try {
			LuaThreadedCall(L, 3, 2, StrDisplay(c), c->parent, true);

			subsobj->ProcessingComplete(StrDisplay(c));

			AssDialogue *active_line = 0;
			int active_idx = 0;

			// Check for a new active row
			if (lua_isnumber(L, -1)) {
				active_idx = lua_tointeger(L, -1);
				if (active_idx < 1 || active_idx > (int)c->ass->Line.size()) {
					wxLogError("Active row %d is out of bounds (must be 1-%u)", active_idx, c->ass->Line.size());
					active_idx = 0;
				}
			}

			stackcheck.check_stack(2);
			lua_pop(L, 1);

			// top of stack will be selected lines array, if any was returned
			if (lua_istable(L, -1)) {
				std::set<AssDialogue*> sel;
				entryIter it = c->ass->Line.begin();
				int last_idx = 1;
				lua_pushnil(L);
				while (lua_next(L, -2)) {
					if (lua_isnumber(L, -1)) {
						int cur = lua_tointeger(L, -1);
						if (cur < 1 || cur > (int)c->ass->Line.size()) {
							wxLogError("Selected row %d is out of bounds (must be 1-%u)", cur, c->ass->Line.size());
							break;
						}

						advance(it, cur - last_idx);

						AssDialogue *diag = dynamic_cast<AssDialogue*>(&*it);
						if (!diag) {
							wxLogError("Selected row %d is not a dialogue line", cur);
							break;
						}

						sel.insert(diag);
						last_idx = cur;
						if (!active_line || active_idx == cur)
							active_line = diag;
					}
					lua_pop(L, 1);
				}

				AssDialogue *new_active = c->selectionController->GetActiveLine();
				if (active_line && (active_idx > 0 || !sel.count(new_active)))
					new_active = active_line;
				c->selectionController->SetSelectionAndActive(sel, new_active);
			}

			stackcheck.check_stack(1);
			lua_pop(L, 1);
		}
		catch (agi::UserCancelException const&) {
			subsobj->Cancel();
		}
		stackcheck.check_stack(0);
	}

	bool LuaCommand::IsActive(const agi::Context *c)
	{
		if (!(cmd_type & cmd::COMMAND_TOGGLE)) return false;

		LuaStackcheck stackcheck(L);

		set_context(L, c);
		stackcheck.check_stack(0);

		GetFeatureFunction("isactive");
		LuaAssFile *subsobj = new LuaAssFile(L, c->ass);
		lua_pushinteger(L, transform_selection(L, c));

		int err = lua_pcall(L, 3, 1, 0);
		subsobj->ProcessingComplete();

		bool result = false;
		if (err)
			wxLogWarning("Runtime error in Lua macro IsActive function:\n%s", get_wxstring(L, -1));
		else
			result = !!lua_toboolean(L, -1);

		// clean up stack (result or error message)
		stackcheck.check_stack(1);
		lua_pop(L, 1);

		return result;
	}

	// LuaFeatureFilter
	LuaExportFilter::LuaExportFilter(lua_State *L)
	: ExportFilter(check_wxstring(L, 1), get_wxstring(L, 2), lua_tointeger(L, 3))
	, LuaFeature(L)
	{
		if (!lua_isfunction(L, 4))
			luaL_error(L, "The filter processing function must be a function");

		// new table for containing the functions for this feature
		lua_newtable(L);

		// store processing function
		lua_pushstring(L, "run");
		lua_pushvalue(L, 4);
		lua_rawset(L, -3);

		// store config function
		lua_pushstring(L, "config");
		lua_pushvalue(L, 5);
		has_config = lua_isfunction(L, -1);
		lua_rawset(L, -3);

		// store the table in the registry
		RegisterFeature();

		LuaScript::GetScriptObject(L)->RegisterFilter(this);
	}

	int LuaExportFilter::LuaRegister(lua_State *L)
	{
		(void)new LuaExportFilter(L);

		return 0;
	}

	void LuaExportFilter::ProcessSubs(AssFile *subs, wxWindow *export_dialog)
	{
		LuaStackcheck stackcheck(L);

		GetFeatureFunction("run");
		stackcheck.check_stack(1);

		// The entire point of an export filter is to modify the file, but
		// setting undo points makes no sense
		LuaAssFile *subsobj = new LuaAssFile(L, subs, true);
		assert(lua_isuserdata(L, -1));
		stackcheck.check_stack(2);

		// config
		if (has_config && config_dialog) {
			int results_produced = config_dialog->LuaReadBack(L);
			assert(results_produced == 1);
			(void) results_produced;	// avoid warning on release builds
			// TODO, write back stored options here
		} else {
			// no config so put an empty table instead
			lua_newtable(L);
		}
		assert(lua_istable(L, -1));
		stackcheck.check_stack(3);

		try {
			LuaThreadedCall(L, 2, 0, GetName(), export_dialog, false);
			stackcheck.check_stack(0);
			subsobj->ProcessingComplete();
		}
		catch (agi::UserCancelException const&) {
			subsobj->Cancel();
			throw;
		}
	}

	ScriptDialog* LuaExportFilter::GenerateConfigDialog(wxWindow *parent, agi::Context *c)
	{
		if (!has_config)
			return 0;

		set_context(L, c);

		GetFeatureFunction("config");

		// prepare function call
		LuaAssFile *subsobj = new LuaAssFile(L, c->ass);
		// stored options
		lua_newtable(L); // TODO, nothing for now

		// do call
		int err = lua_pcall(L, 2, 1, 0);
		subsobj->ProcessingComplete();

		if (err) {
			wxLogWarning("Runtime error in Lua config dialog function:\n%s", get_wxstring(L, -1));
			lua_pop(L, 1); // remove error message
		} else {
			// Create config dialogue from table on top of stack
			config_dialog = new LuaDialog(L, false);
		}

		return config_dialog;
	}

	LuaScriptFactory::LuaScriptFactory()
	: ScriptFactory("Lua", "*.lua")
	{
		Register(this);
	}

	Script* LuaScriptFactory::Produce(const wxString &filename) const
	{
		// Just check if file extension is .lua
		// Reject anything else
		if (filename.Right(4).Lower() == ".lua") {
			return new LuaScript(filename);
		} else {
			return 0;
		}
	}
}

#endif // WITH_AUTO4_LUA
