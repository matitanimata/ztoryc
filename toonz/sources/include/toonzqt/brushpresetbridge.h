#pragma once

#ifndef BRUSHPRESETBRIDGE_H
#define BRUSHPRESETBRIDGE_H

#include "tcommon.h"

#include <QObject>
#include <QStringList>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// BrushPresetBridge
//-----------------------------------------------------------------------------
// Ztoryc: lets the Style Editor list, choose, add and remove the presets of
// the current brush tool. The brush tools live in tnztools, which depends on
// toonzqt and not the other way round, so tnztools installs a Provider here
// and the Style Editor only talks to the bridge.

class DVAPI BrushPresetBridge final : public QObject {
  Q_OBJECT

public:
  class Provider {
  public:
    virtual ~Provider() {}
    //! The current tool, when it is a brush; empty otherwise.
    virtual QString toolName() const = 0;
    //! "vector", "smartraster" or "raster"; empty when not a brush.
    virtual QString toolKind() const = 0;
    //! Saves the current brush settings as a preset of the current brush.
    virtual void addPreset(const QString &name) = 0;

    //! The presets of a brush kind, whatever the current tool.
    virtual QStringList presets(const QString &kind) const = 0;
    //! The preset that brush is using; empty for <custom>.
    virtual QString currentPreset(const QString &kind) const = 0;
    //! The style the preset was made with («.tpl» holding one style), or an
    //! empty string for presets saved before Ztoryc 0.19.
    virtual QString presetStylePath(const QString &kind,
                                    const QString &name) const = 0;
    //! Gives the preset's settings to the selected style of the palette.
    virtual void applyPreset(const QString &kind, const QString &name) = 0;
    virtual void removePreset(const QString &kind, const QString &name) = 0;
  };

  static BrushPresetBridge *instance();

  void setProvider(Provider *provider) { m_provider = provider; }
  Provider *provider() const { return m_provider; }

  void notifyChanged() { emit changed(); }

signals:
  //! The brush tool, its presets or the current preset changed.
  void changed();

private:
  BrushPresetBridge() : m_provider(nullptr) {}

  Provider *m_provider;
};

#endif  // BRUSHPRESETBRIDGE_H
