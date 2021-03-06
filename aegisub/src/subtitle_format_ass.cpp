// Copyright (c) 2006, Rodrigo Braz Monteiro
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

/// @file subtitle_format_ass.cpp
/// @brief Reading/writing of SSA-lineage subtitles
/// @ingroup subtitle_io
///

#include "config.h"

#include "subtitle_format_ass.h"

#include "ass_file.h"
#include "ass_parser.h"
#include "compat.h"
#include "text_file_reader.h"
#include "text_file_writer.h"
#include "version.h"

DEFINE_SIMPLE_EXCEPTION(AssParseError, SubtitleFormatParseError, "subtitle_io/parse/ass")

AssSubtitleFormat::AssSubtitleFormat()
: SubtitleFormat("Advanced Substation Alpha")
{
}

wxArrayString AssSubtitleFormat::GetReadWildcards() const {
	wxArrayString formats;
	formats.Add("ass");
	formats.Add("ssa");
	return formats;
}

wxArrayString AssSubtitleFormat::GetWriteWildcards() const {
	wxArrayString formats;
	formats.Add("ass");
	formats.Add("ssa");
	return formats;
}

void AssSubtitleFormat::ReadFile(AssFile *target, wxString const& filename, wxString const& encoding) const {
	target->Clear();

	TextFileReader file(filename, encoding);
	int version = filename.Right(4).Lower() != ".ssa";
	AssParser parser(target, version);

	while (file.HasMoreLines()) {
		wxString line = file.ReadLineFromFile();
		try {
			parser.AddLine(line);
		}
		catch (const char *err) {
			target->Clear();
			throw AssParseError("Error processing line: " + from_wx(line) + ": " + err, 0);
		}
	}
}

#ifdef _WIN32
#define LINEBREAK "\r\n"
#else
#define LINEBREAK "\n"
#endif

static inline wxString format(AssEntryGroup group, bool ssa) {
	if (group == ENTRY_DIALOGUE) {
		if (ssa)
			return "Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text" LINEBREAK;
		else
			return "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text" LINEBREAK;
	}

	if (group == ENTRY_STYLE) {
		if (ssa)
			return "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding" LINEBREAK;
		else
			return "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding" LINEBREAK;
	}

	return wxS("");
}

void AssSubtitleFormat::WriteFile(const AssFile *src, wxString const& filename, wxString const& encoding) const {
	TextFileWriter file(filename, encoding);

	file.WriteLineToFile(wxString("; Script generated by Aegisub ") + GetAegisubLongVersionString());
	file.WriteLineToFile("; http://www.aegisub.org/");

	bool ssa = filename.Right(4).Lower() == ".ssa";
	AssEntryGroup group = ENTRY_GROUP_MAX;

	for (auto const& line : src->Line) {
		if (line.Group() != group) {
			// Add a blank line between each group
			if (group != ENTRY_GROUP_MAX)
				file.WriteLineToFile("");

			file.WriteLineToFile(line.GroupHeader(ssa));
			file.WriteLineToFile(format(line.Group(), ssa), false);

			group = line.Group();
		}

		file.WriteLineToFile(ssa ? line.GetSSAText() : line.GetEntryData());
	}
}
