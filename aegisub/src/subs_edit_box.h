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

/// @file subs_edit_box.h
/// @see subs_edit_box.cpp
/// @ingroup main_ui
///

#include <array>
#include <deque>
#include <boost/container/map.hpp>
#include <boost/flyweight/flyweight_fwd.hpp>
#include <vector>

#include <wx/panel.h>
#include <wx/timer.h>

#include <libaegisub/scoped_ptr.h>
#include <libaegisub/signal.h>

#include "selection_controller.h"

namespace agi { namespace vfr { class Framerate; } }
namespace agi { struct Context; }
class AssDialogue;
class AssTime;
class SubsTextEditCtrl;
class TextSelectionController;
class TimeEdit;
class wxButton;
class wxCheckBox;
class wxComboBox;
class wxRadioButton;
class wxSizer;
class wxSpinCtrl;
class wxStyledTextCtrl;
class wxStyledTextEvent;
class wxTextCtrl;

template<class Base> class Placeholder;

/// @brief Main subtitle edit box
///
/// Controls the text edit and all surrounding controls
class SubsEditBox : public wxPanel {
	enum TimeField {
		TIME_START = 0,
		TIME_END,
		TIME_DURATION
	};

	std::deque<agi::signal::Connection> connections;

	/// Currently active dialogue line
	AssDialogue *line;
	/// Last seen grid selection
	SubtitleSelection sel;

	/// Are the buttons currently split into two lines?
	bool button_bar_split;
	/// Are the controls currently enabled?
	bool controls_enabled;

	agi::Context *c;

	agi::signal::Connection file_changed_slot;

	// Box controls
	wxCheckBox *comment_box;
	wxComboBox *style_box;
	Placeholder<wxComboBox> *actor_box;
	TimeEdit *start_time;
	TimeEdit *end_time;
	TimeEdit *duration;
	wxSpinCtrl *layer;
	std::array<wxTextCtrl *, 3> margin;
	Placeholder<wxComboBox> *effect_box;
	wxRadioButton *by_time;
	wxRadioButton *by_frame;
	wxTextCtrl *char_count;

	wxSizer *top_sizer;
	wxSizer *middle_right_sizer;
	wxSizer *middle_left_sizer;

	void SetControlsState(bool state);
	/// @brief Update times of selected lines
	/// @param field Field which changed
	void CommitTimes(TimeField field);
	/// @brief Commits the current edit box contents
	/// @param desc Undo description to use
	void CommitText(wxString const& desc);

	/// Last commit ID for undo coalescing
	int commit_id;

	/// Last used commit message to avoid coalescing different types of changes
	wxString last_commit_type;

	/// Last field to get a time commit, as they all have the same commit message
	int last_time_commit_type;

	/// Timer to stop coalescing changes after a break with no edits
	wxTimer undo_timer;

	/// The start and end times of the selected lines without changes made to
	/// avoid negative durations, so that they can be restored if future changes
	/// eliminate the negative durations
	boost::container::map<AssDialogue *, std::pair<AssTime, AssTime>> initial_times;

	// Constructor helpers
	wxTextCtrl *MakeMarginCtrl(wxString const& tooltip, int margin, wxString const& commit_msg);
	TimeEdit *MakeTimeCtrl(wxString const& tooltip, TimeField field);
	void MakeButton(const char *cmd_name);
	wxComboBox *MakeComboBox(wxString const& initial_text, int style, void (SubsEditBox::*handler)(wxCommandEvent&), wxString const& tooltip);
	wxRadioButton *MakeRadio(wxString const& text, bool start, wxString const& tooltip);

	void OnChange(wxStyledTextEvent &event);
	void OnKeyDown(wxKeyEvent &event);

	void OnActiveLineChanged(AssDialogue *new_line);
	void OnSelectedSetChanged(const SubtitleSelection &, const SubtitleSelection &);

	void OnFrameTimeRadio(wxCommandEvent &event);
	void OnStyleChange(wxCommandEvent &event);
	void OnActorChange(wxCommandEvent &event);
	void OnLayerEnter(wxCommandEvent &event);
	void OnCommentChange(wxCommandEvent &);
	void OnEffectChange(wxCommandEvent &);
	void OnSize(wxSizeEvent &event);

	void SetPlaceholderCtrl(wxControl *ctrl, wxString const& value);

	/// @brief Set a field in each selected line to a specified value
	/// @param set   Callable which updates a passed line
	/// @param desc  Undo description to use
	/// @param type  Commit type to use
	/// @param amend Coalesce sequences of commits of the same type
	template<class setter>
	void SetSelectedRows(setter set, wxString const& desc, int type, bool amend = false);

	/// @brief Set a field in each selected line to a specified value
	/// @param field Field to set
	/// @param value Value to set the field to
	/// @param desc  Undo description to use
	/// @param type  Commit type to use
	/// @param amend Coalesce sequences of commits of the same type
	template<class T>
	void SetSelectedRows(T AssDialogue::*field, T value, wxString const& desc, int type, bool amend = false);

	/// @brief Reload the current line from the file
	/// @param type AssFile::CommitType
	void OnCommit(int type);

	/// Regenerate a dropdown list with the unique values of a dialogue field
	void PopulateList(wxComboBox *combo, boost::flyweight<wxString> AssDialogue::*field);

	/// @brief Enable or disable frame timing mode
	void UpdateFrameTiming(agi::vfr::Framerate const& fps);

	/// Update the character count box for the given text
	void UpdateCharacterCount(wxString const& text);

	/// Call a command the restore focus to the edit box
	void CallCommand(const char *cmd_name);

	SubsTextEditCtrl *edit_ctrl;
	agi::scoped_ptr<TextSelectionController> textSelectionController;

public:
	/// @brief Constructor
	/// @param parent Parent window
	SubsEditBox(wxWindow *parent, agi::Context *context);
	~SubsEditBox();
};
