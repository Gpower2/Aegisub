// Copyright (c) 2009, Thomas Goyne
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

/// @file video_out_gl.h
/// @brief OpenGL based video renderer
/// @ingroup video
///

#include <libaegisub/exception.h>

#include <vector>

#include "compat.h"

class AegiVideoFrame;

/// @class VideoOutGL
/// @brief OpenGL based video renderer
class VideoOutGL {
private:
	struct TextureInfo;

	/// The maximum texture size supported by the user's graphics card
	int maxTextureSize;
	/// Whether rectangular textures are supported by the user's graphics card
	bool supportsRectangularTextures;
	/// The internalformat to use
	int internalFormat;

	/// The frame height which the texture grid has been set up for
	int frameWidth;
	/// The frame width which the texture grid has been set up for
	int frameHeight;
	/// The frame format which the texture grid has been set up for
	GLenum frameFormat;
	/// Whether the grid is set up for flipped video
	bool frameFlipped;
	/// List of OpenGL texture ids used in the grid
	std::vector<GLuint> textureIdList;
	/// List of precalculated texture display information
	std::vector<TextureInfo> textureList;
	/// OpenGL display list which draws the frames
	GLuint dl;
	/// The total texture count
	int textureCount;
	/// The number of rows of textures
	int textureRows;
	/// The number of columns of textures
	int textureCols;

	void DetectOpenGLCapabilities();
	void InitTextures(int width, int height, GLenum format, int bpp, bool flipped);

	VideoOutGL(const VideoOutGL &);
	VideoOutGL& operator=(const VideoOutGL&);
public:
	/// @brief Set the frame to be displayed when Render() is called
	/// @param frame The frame to be displayed
	void UploadFrameData(const AegiVideoFrame& frame);

	/// @brief Render a frame
	/// @param x Bottom left x coordinate
	/// @param y Bottom left y coordinate
	/// @param width Width in pixels of viewport
	/// @param height Height in pixels of viewport
	void Render(int x, int y, int width, int height);

	/// @brief Constructor
	VideoOutGL();
	/// @brief Destructor
	~VideoOutGL();
};

/// @class VideoOutException
/// @extends Aegisub::Exception
/// @brief Base class for all exceptions thrown by VideoOutGL
DEFINE_BASE_EXCEPTION_NOINNER(VideoOutException, agi::Exception)

/// @class VideoOutRenderException
/// @extends VideoOutException
/// @brief An OpenGL error occured while uploading or displaying a frame
class VideoOutRenderException : public VideoOutException {
public:
	VideoOutRenderException(const char *func, int err)
		: VideoOutException(from_wx(wxString::Format("%s failed with error code %d", func, err)))
	{ }
	const char * GetName() const { return "videoout/opengl/render"; }
	Exception * Copy() const { return new VideoOutRenderException(*this); }
};
/// @class VideoOutOpenGLException
/// @extends VideoOutException
/// @brief An OpenGL error occured while setting up the video display
class VideoOutInitException : public VideoOutException {
public:
	VideoOutInitException(const char *func, int err)
		: VideoOutException(from_wx(wxString::Format("%s failed with error code %d", func, err)))
	{ }
	VideoOutInitException(const char *err) : VideoOutException(err) { }
	const char * GetName() const { return "videoout/opengl/init"; }
	Exception * Copy() const { return new VideoOutInitException(*this); }
};
