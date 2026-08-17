#pragma once

#ifndef TEXTUREUTILS_H
#define TEXTUREUTILS_H

#include "ext/ttexturesstorage.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//! \file textureutils.h
/*!
  This file contains functions related to the texturization of Toonz scene
  contents,
  namely image levels and whole xsheets.
*/

//==================================================================

//    Forward declarations

class TXsheet;
class TXshSimpleLevel;
class TFrameId;

//==================================================================

//**********************************************************************************************
//    Texture Utility Functions  declaration
//**********************************************************************************************

namespace texture_utils {

//! Returns the OpenGL data of a loaded texture corresponding to sl's content
//! at fid with specified subsampling.
DrawableTextureDataP getTextureData(const TXshSimpleLevel *sl,
                                    const TFrameId &fid, int subsampling,
                                    bool isMask);

//! Invalidates any currently stored texture associated with sl at the specified
//! fid.
void invalidateTexture(const TXshSimpleLevel *sl, const TFrameId &fid);

//! Invalidates any currently stored texture associated with sl.
void invalidateTextures(const TXshSimpleLevel *sl);

//------------------------------------------------------------------------------------

//! Returns the OpenGL data of a loaded texture corresponding to xsh's content
//! at the specified frame.
DrawableTextureDataP getTextureData(const TXsheet *xsh, int frame);

//! Invalidates any currently stored texture associated with xsh at the
//! specified frame.
void invalidateTexture(const TXsheet *xsh, int frame);

//! Invalidates any currently stored texture associated with sl.
//!
//! ⚠️ ESPORTATA perche' la chiama l'ESEGUIBILE (ztorymouthapply.cpp), non solo
//! toonzlib. Senza `DVAPI` il link fallisce con LNK2001 SOLO su Windows —
//! gcc/clang esportano tutto per default e su macOS e Linux non si vede
//! niente. Successo il 2026-08-18: il difetto era entrato il giorno prima e
//! nessuna build Windows era girata nel frattempo.
DVAPI void invalidateTextures(const TXsheet *xsh);

}  // namespace texture_utils

#endif  // TEXTUREUTILS_H
