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
    //! Empty when the current tool is not a brush.
    virtual QString toolName() const                 = 0;
    virtual QStringList presets() const              = 0;
    //! Empty for <custom>.
    virtual QString currentPreset() const            = 0;
    virtual void choosePreset(const QString &name)   = 0;
    virtual void addPreset(const QString &name)      = 0;
    virtual void removePreset(const QString &name)   = 0;
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
