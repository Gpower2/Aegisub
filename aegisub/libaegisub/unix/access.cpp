// Copyright (c) 2010, Amar Takhar <verm@aegisub.org>
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

/// @file access.cpp
/// @brief Unix access methods.
/// @ingroup libaegisub unix

#include "config.h"

#include "libaegisub/access.h"

#include <sys/stat.h>
#include <errno.h>

#include <iostream>
#include <fstream>

#include <unistd.h>

#include "libaegisub/util.h"

namespace agi {
	namespace acs {


void CheckFileRead(const std::string &file) {
	Check(file, acs::FileRead);
}


void CheckFileWrite(const std::string &file) {
	Check(file, acs::FileWrite);
}


void CheckDirRead(const std::string &dir) {
	Check(dir, acs::DirRead);
}


void CheckDirWrite(const std::string &dir) {
	Check(dir, acs::DirWrite);
}


void Check(const std::string &file, acs::Type type) {
	struct stat file_stat;
	int file_status;

	file_status = stat(file.c_str(), &file_stat);

	if (file_status != 0) {
		switch (errno) {
			case ENOENT:
				throw FileNotFoundError("File or path not found.");
			break;

			case EACCES:
				throw Read("Access Denied to file, path or path component.");
			break;

			case EIO:
				throw Fatal("Fatal I/O error occurred.");
			break;
		}
	}

	switch (type) {
		case FileRead:
		case FileWrite:
			if ((file_stat.st_mode & S_IFREG) == 0)
				throw NotAFile("Not a file.");
		break;

		case DirRead:
		case DirWrite:
			if ((file_stat.st_mode & S_IFDIR) == 0)
				throw NotADirectory("Not a directory.");
		break;
	}

	file_status = access(file.c_str(), R_OK);
	if (file_status != 0)
		throw Read("File or directory is not readable.");

	if (type == DirWrite || type == FileWrite) {
		file_status = access(file.c_str(), W_OK);
		if (file_status != 0)
			throw Write("File or directory is not writable.");
	}
}

	} // namespace acs
} // namespace agi
