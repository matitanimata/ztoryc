#pragma once

#ifndef BRUSHSTYLEMEMORY_H
#define BRUSHSTYLEMEMORY_H

// Ztoryc: every style of a palette remembers the brush it was last drawn with
// — size, hardness, smoothing, everything a preset holds — the way it
// remembers its colour (asked by Franco, 2026-10-23). The settings live in a
// file next to the level, «<level>.brushes», and not in the palette itself:
// Tahoma, OpenToonz and Ztoryc up to 0.18 refuse a whole palette with a tag
// they do not know, and the level would lose its colours.

#include <QString>
#include <QStringList>

#include <memory>
#include <string>

class TPersist;
class TTool;

//=============================================================================
// BrushPresetClient
//-----------------------------------------------------------------------------
// What the three brush tools offer to the style memory and to the Style
// Editor (through BrushPresetBridge).

class BrushPresetClient {
public:
  virtual ~BrushPresetClient() {}

  //! "vector", "smartraster" or "raster": one memory per brush kind.
  virtual std::string brushKind() const = 0;
  virtual QString brushName() const     = 0;

  virtual QStringList presetNames()                     = 0;
  virtual QString currentPresetName()                   = 0;  // empty: custom
  virtual void selectPreset(const QString &name)        = 0;  // empty: custom
  virtual void addPreset(QString name)                  = 0;
  virtual void removeNamedPreset(const QString &name)   = 0;

  //! Applies the settings a style remembers.
  virtual void applyStyleBrush(const TPersist *data) = 0;
};

//=============================================================================
// BrushStyleMemory
//-----------------------------------------------------------------------------

namespace BrushStyleMemory {

//! Stores the brush settings for the current style of the current palette.
void remember(const std::string &kind, std::shared_ptr<TPersist> brush);

//! When the current style differs from the last one this brush saw, applies
//! what that style remembers.
void applyTo(BrushPresetClient *client);

//! The tool, its presets or its current preset changed: tells the Style
//! Editor.
void notifyChanged();

}  // namespace BrushStyleMemory

#endif  // BRUSHSTYLEMEMORY_H
