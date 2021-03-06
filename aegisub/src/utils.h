// Copyright (c) 2005-2009, Rodrigo Braz Monteiro, Niels Martin Hansen
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

/// @file utils.h
/// @see utils.cpp
/// @ingroup utility
///


#pragma once

#include <cstdint>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include <wx/app.h>
#include <wx/icon.h>
#include <wx/thread.h>

class wxMouseEvent;
class wxWindow;

/// @brief Make a path relative to reference
wxString MakeRelativePath(wxString path,wxString reference);
/// @brief Extract original path from relative
wxString DecodeRelativePath(wxString path,wxString reference);
wxString AegiFloatToString(double value);
wxString AegiIntegerToString(int value);
wxString PrettySize(int bytes);

/// @brief Get the smallest power of two that is greater or equal to x
///
/// Algorithm from http://bob.allegronetwork.com/prog/tricks.html
int SmallestPowerOf2(int x);

/// Check if wchar 'c' is a whitespace character
bool IsWhitespace(wchar_t c);

/// Check if every character in str is whitespace
bool StringEmptyOrWhitespace(const wxString &str);

/// @brief String to integer
///
/// wxString::ToLong() is slow and not as flexible
int AegiStringToInt(const wxString &str,int start=0,int end=-1);
int AegiStringToFix(const wxString &str,size_t decimalPlaces,int start=0,int end=-1);

/// Get the length in characters of the longest line in the given text
size_t MaxLineLength(wxString const& text);

/// @brief Launch a new copy of Aegisub.
///
/// Contrary to what the name suggests, this does not close the currently
/// running process.
void RestartAegisub();

/// Add the OS X 10.7+ full-screen button to a window
void AddFullScreenButton(wxWindow *window);

void SetFloatOnParent(wxWindow *window);

void SetPlaceholderText(wxWindow *window, wxString const& placeholder);

/// Forward a mouse wheel event to the window under the mouse if needed
/// @param source The initial target of the wheel event
/// @param evt The event
/// @return Should the calling code process the event?
bool ForwardMouseWheelEvent(wxWindow *source, wxMouseEvent &evt);

/// Clean up the given cache directory, limiting the size to max_size
/// @param directory Directory to clean
/// @param file_type Wildcard pattern for files to clean up
/// @param max_size Maximum size of directory in MB
/// @param max_files Maximum number of files
void CleanCache(wxString const& directory, wxString const& file_type, int64_t max_size, int64_t max_files = -1);

/// @brief Templated abs() function
template <typename T> T tabs(T x) { return x < 0 ? -x : x; }

/// Get the middle value of a, b, and c (i.e. clamp b to [a,c])
/// @precondition a <= c
template<typename T> inline T mid(T a, T b, T c) { return std::max(a, std::min(b, c)); }

/// Get the text contents of the clipboard, or empty string on failure
wxString GetClipboard();
/// Try to set the clipboard to the given string
void SetClipboard(wxString const& new_value);
void SetClipboard(wxBitmap const& new_value);

#ifndef FORCEINLINE
#ifdef __VISUALC__
#define FORCEINLINE __forceinline
#else
#define FORCEINLINE inline
// __attribute__((always_inline)) gives me errors on g++ ~amz
#endif
#endif

#define countof(array) (sizeof(array) / sizeof(array[0]))

/// Polymorphic delete functor
struct delete_ptr {
	template<class T> void operator()(T* ptr) const {
		delete ptr;
	}
};

/// Delete all of the items in a container of pointers and clear the container
template<class T>
void delete_clear(T& container) {
	if (!container.empty()) {
		std::for_each(container.begin(), container.end(), delete_ptr());
		container.clear();
	}
}

/// Helper class for background_delete_clear
template<class Container>
class BackgroundDeleter : public wxThread {
	Container cont;
	wxThread::ExitCode Entry() {
		cont.clear_and_dispose(delete_ptr());
		return (wxThread::ExitCode)0;
	}
public:
	BackgroundDeleter(Container &source)
	: wxThread(wxTHREAD_DETACHED)
	{
		using std::swap;
		swap(cont, source);

		Create();
		SetPriority(WXTHREAD_MIN_PRIORITY);
		Run();
	}
};

/// Clear a container of pointers and delete the pointed to members on a
/// background thread
template<class T>
void background_delete_clear(T& container) {
	if (!container.empty())
		new BackgroundDeleter<T>(container);
}


template<class Out>
struct cast {
	template<class In>
	Out operator()(In *in) const {
		return dynamic_cast<Out>(in);
	}

	template<class In>
	Out operator()(In &in) const {
		return dynamic_cast<Out>(&in);
	}
};

wxDECLARE_EVENT(EVT_CALL_THUNK, wxThreadEvent);

template<typename Function>
void InvokeOnMainThreadAsync(Function const& f) {
	wxThreadEvent *evt = new wxThreadEvent(EVT_CALL_THUNK);
	evt->SetPayload<std::function<void()>>(f);
	wxTheApp->QueueEvent(evt);
}

template<typename Function>
void InvokeOnMainThread(Function const& f) {
	wxSemaphore sema(0, 1);
	InvokeOnMainThreadAsync([&] { f(); sema.Post(); });
	sema.Wait();
}
