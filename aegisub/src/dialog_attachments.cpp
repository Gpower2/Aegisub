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

/// @file dialog_attachments.cpp
/// @brief Manage files attached to the subtitle file
/// @ingroup tools_ui
///

#include "config.h"

#include "dialog_attachments.h"

#include <wx/button.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>

#include "ass_attachment.h"
#include "ass_file.h"
#include "compat.h"
#include "help_button.h"
#include "libresrc/libresrc.h"
#include "main.h"
#include "utils.h"

#include <libaegisub/of_type_adaptor.h>

enum {
	BUTTON_ATTACH_FONT = 1300,
	BUTTON_ATTACH_GRAPHICS,
	BUTTON_EXTRACT,
	BUTTON_DELETE,
	ATTACHMENT_LIST
};

DialogAttachments::DialogAttachments(wxWindow *parent, AssFile *ass)
: wxDialog(parent,-1,_("Attachment List"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE)
, ass(ass)
{
	SetIcon(GETICON(attach_button_16));

	listView = new wxListView(this,ATTACHMENT_LIST,wxDefaultPosition,wxSize(500,200));
	UpdateList();

	// Buttons
	extractButton = new wxButton(this,BUTTON_EXTRACT,_("E&xtract"));
	deleteButton = new wxButton(this,BUTTON_DELETE,_("&Delete"));
	extractButton->Enable(false);
	deleteButton->Enable(false);

	// Buttons sizer
	wxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	buttonSizer->Add(new wxButton(this,BUTTON_ATTACH_FONT,_("Attach &Font")),1,0,0);
	buttonSizer->Add(new wxButton(this,BUTTON_ATTACH_GRAPHICS,_("Attach &Graphics")),1,0,0);
	buttonSizer->Add(extractButton,1,0,0);
	buttonSizer->Add(deleteButton,1,0,0);
	buttonSizer->Add(new HelpButton(this,"Attachment Manager"),1,wxLEFT,5);
	buttonSizer->Add(new wxButton(this,wxID_CANCEL,_("&Close")),1,0,0);

	// Main sizer
	wxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
	mainSizer->Add(listView,1,wxTOP | wxLEFT | wxRIGHT | wxEXPAND,5);
	mainSizer->Add(buttonSizer,0,wxALL | wxEXPAND,5);
	SetSizerAndFit(mainSizer);
	CenterOnParent();
}

void DialogAttachments::UpdateList() {
	listView->ClearAll();

	// Insert list columns
	listView->InsertColumn(0, _("Attachment name"), wxLIST_FORMAT_LEFT, 280);
	listView->InsertColumn(1, _("Size"), wxLIST_FORMAT_LEFT, 100);
	listView->InsertColumn(2, _("Group"), wxLIST_FORMAT_LEFT, 100);

	// Fill list
	for (auto attach : ass->Line | agi::of_type<AssAttachment>()) {
		int row = listView->GetItemCount();
		listView->InsertItem(row,attach->GetFileName(true));
		listView->SetItem(row,1,PrettySize(attach->GetSize()));
		listView->SetItem(row,2,attach->GroupHeader());
		listView->SetItemPtrData(row,wxPtrToUInt(attach));
	}
}

BEGIN_EVENT_TABLE(DialogAttachments,wxDialog)
	EVT_BUTTON(BUTTON_ATTACH_FONT,DialogAttachments::OnAttachFont)
	EVT_BUTTON(BUTTON_ATTACH_GRAPHICS,DialogAttachments::OnAttachGraphics)
	EVT_BUTTON(BUTTON_EXTRACT,DialogAttachments::OnExtract)
	EVT_BUTTON(BUTTON_DELETE,DialogAttachments::OnDelete)
	EVT_LIST_ITEM_SELECTED(ATTACHMENT_LIST,DialogAttachments::OnListClick)
	EVT_LIST_ITEM_DESELECTED(ATTACHMENT_LIST,DialogAttachments::OnListClick)
	EVT_LIST_ITEM_FOCUSED(ATTACHMENT_LIST,DialogAttachments::OnListClick)
END_EVENT_TABLE()

void DialogAttachments::AttachFile(wxFileDialog &diag, AssEntryGroup group, wxString const& commit_msg) {
	if (diag.ShowModal() == wxID_CANCEL) return;

	wxArrayString filenames;
	diag.GetFilenames(filenames);

	wxArrayString paths;
	diag.GetPaths(paths);

	// Create attachments
	for (size_t i = 0; i < filenames.size(); ++i) {
		AssAttachment *newAttach = new AssAttachment(filenames[i], group);
		try {
			newAttach->Import(paths[i]);
		}
		catch (...) {
			delete newAttach;
			return;
		}
		ass->InsertLine(newAttach);
	}

	ass->Commit(commit_msg, AssFile::COMMIT_ATTACHMENT);

	UpdateList();
}

void DialogAttachments::OnAttachFont(wxCommandEvent &) {
	wxFileDialog diag(this,
		_("Choose file to be attached"),
		to_wx(OPT_GET("Path/Fonts Collector Destination")->GetString()), "", "Font Files (*.ttf)|*.ttf",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

	AttachFile(diag, ENTRY_FONT, _("attach font file"));
}

void DialogAttachments::OnAttachGraphics(wxCommandEvent &) {
	wxFileDialog diag(this,
		_("Choose file to be attached"),
		"", "",
		"Graphic Files (*.bmp,*.gif,*.jpg,*.ico,*.wmf)|*.bmp;*.gif;*.jpg;*.ico;*.wmf",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

	AttachFile(diag, ENTRY_GRAPHIC, _("attach graphics file"));
}

void DialogAttachments::OnExtract(wxCommandEvent &) {
	int i = listView->GetFirstSelected();
	if (i == -1) return;

	wxString path;
	bool fullPath = false;

	// Multiple or single?
	if (listView->GetNextSelected(i) != -1)
		path = wxDirSelector(_("Select the path to save the files to:"),to_wx(OPT_GET("Path/Fonts Collector Destination")->GetString())) + "/";
	else {
		// Default path
		wxString defPath = ((AssAttachment*)wxUIntToPtr(listView->GetItemData(i)))->GetFileName();
		path = wxFileSelector(
			_("Select the path to save the file to:"),
			to_wx(OPT_GET("Path/Fonts Collector Destination")->GetString()),
			defPath,
			".ttf",
			"Font Files (*.ttf)|*.ttf",
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT,
			this);
		fullPath = true;
	}
	if (!path) return;

	// Loop through items in list
	while (i != -1) {
		AssAttachment *attach = (AssAttachment*)wxUIntToPtr(listView->GetItemData(i));
		attach->Extract(fullPath ? path : path + attach->GetFileName());
		i = listView->GetNextSelected(i);
	}
}

void DialogAttachments::OnDelete(wxCommandEvent &) {
	int i = listView->GetFirstSelected();
	if (i == -1) return;

	while (i != -1) {
		delete (AssEntry*)wxUIntToPtr(listView->GetItemData(i));
		i = listView->GetNextSelected(i);
	}

	ass->Commit(_("remove attachment"), AssFile::COMMIT_ATTACHMENT);

	UpdateList();
	extractButton->Enable(false);
	deleteButton->Enable(false);
}

void DialogAttachments::OnListClick(wxListEvent &) {
	bool hasSel = listView->GetFirstSelected() != -1;
	extractButton->Enable(hasSel);
	deleteButton->Enable(hasSel);
}
