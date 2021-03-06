// Copyright (c) 2005, Rodrigo Braz Monteiro
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

/// @file ass_override.h
/// @see ass_override.cpp
/// @ingroup subs_storage
///

#include <boost/noncopyable.hpp>
#include <memory>
#include <vector>

class AssDialogueBlockOverride;
class wxString;

/// Type of parameter; probably only used by the resample tool
enum AssParameterClass {
	PARCLASS_NORMAL,
	PARCLASS_ABSOLUTE_SIZE,
	PARCLASS_ABSOLUTE_POS_X,
	PARCLASS_ABSOLUTE_POS_Y,
	PARCLASS_RELATIVE_SIZE_X,
	PARCLASS_RELATIVE_SIZE_Y,
	PARCLASS_RELATIVE_TIME_START,
	PARCLASS_RELATIVE_TIME_END,
	PARCLASS_KARAOKE,
	PARCLASS_DRAWING
};

enum VariableDataType {
	VARDATA_INT,
	VARDATA_FLOAT,
	VARDATA_TEXT,
	VARDATA_BOOL,
	VARDATA_BLOCK
};

/// A single parameter to an override tag
class AssOverrideParameter : boost::noncopyable {
	wxString value;
	mutable std::unique_ptr<AssDialogueBlockOverride> block;
	VariableDataType type;

public:
	AssOverrideParameter(VariableDataType type, AssParameterClass classification);
	AssOverrideParameter(AssOverrideParameter&&);
	AssOverrideParameter& operator=(AssOverrideParameter&&);
	~AssOverrideParameter();

	/// Type of parameter
	AssParameterClass classification;

	/// Is this parameter actually present?
	bool omitted;

	VariableDataType GetType() const { return type; }
	template<class T> void Set(T param);
	template<class T> T Get() const;
	template<class T> T Get(T def) const {
		return !omitted ? Get<T>() : def;
	}
};

class AssOverrideTag : boost::noncopyable {
	bool valid;

public:
	AssOverrideTag();
	AssOverrideTag(AssOverrideTag&&);
	AssOverrideTag(wxString const& text);
	AssOverrideTag& operator=(AssOverrideTag&&);

	wxString Name;
	std::vector<AssOverrideParameter> Params;

	bool IsValid() const { return valid; }
	void Clear();
	void SetText(const wxString &text);
	operator wxString() const;
};
