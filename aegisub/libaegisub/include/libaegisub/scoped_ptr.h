// Copyright (c) 2010, Thomas Goyne <plorkyeran@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

/// @file scoped_ptr.h
/// @brief
/// @ingroup libaegisub

#pragma once

namespace agi {

/// @class scoped_ptr
/// @brief auto_ptr without the transfer of ownership semantics
template<class T>
class scoped_ptr {
	T* ptr;
	scoped_ptr(scoped_ptr const&);
	scoped_ptr& operator=(scoped_ptr const&);

	typedef T *scoped_ptr<T>::*unspecified_bool_type;
public:
	typedef T element_type;

	T& operator*() const {return *ptr; }
	T* operator->() const { return ptr; }
	T* get() const { return ptr; }
	operator unspecified_bool_type() const { return ptr ? &scoped_ptr<T>::ptr : 0; }

	void reset(T *p = 0) {
		delete ptr;
		ptr = p;
	}

	explicit scoped_ptr(T *ptr = 0) : ptr(ptr){ }
	~scoped_ptr() { delete ptr; }
};

/// A generic scoped holder for non-pointer handles
template<class T, class Del = void(*)(T)>
class scoped_holder {
	T value;
	Del destructor;

	scoped_holder(scoped_holder const&);
	scoped_holder& operator=(scoped_holder const&);
public:
	operator T() const { return value; }
	T operator->() const { return value; }

	scoped_holder& operator=(T new_value) {
		if (value)
			destructor(value);
		value = new_value;
		return *this;
	}

	scoped_holder(T value, Del destructor)
	: value(value)
	, destructor(destructor)
	{
	}

	~scoped_holder() { if (value) destructor(value); }
};

}
