#include "brushstylememory.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/brushpresetbridge.h"

// TnzLib includes
#include "toonz/tapplication.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzscene.h"
#include "toonz/fullcolorpalette.h"
#include "toonz/toonzfolders.h"

// TnzCore includes
#include "tpersist.h"
#include "tstream.h"
#include "tsystem.h"
#include "tfilepath_io.h"
#include "tpalette.h"
#include "tcolorstyles.h"

#include <QDateTime>
#include <QTimer>

#include <map>
#include <set>

namespace {

typedef std::pair<int, std::string> EntryKey;  // style id, brush kind

struct Twin {
  QDateTime m_modified;
  // A file that could not be read is never overwritten: what it held would
  // be lost with it.
  bool m_unreadable = false;
  std::map<EntryKey, std::shared_ptr<TPersist>> m_entries;
};

std::map<std::wstring, Twin> s_twins;

// What each brush kind saw last: the twin file and the style.
std::map<std::string, std::pair<std::wstring, int>> s_lastSeen;
// Twin files changed and not yet written, and the timer that writes them.
std::set<std::wstring> s_dirty;
QTimer *s_saveTimer = nullptr;

//-----------------------------------------------------------------------------

TTool::Application *app() { return TTool::getApplication(); }

// The file next to the palette the brushes draw with. Only level palettes and
// the scene's full-colour palette have a file of their own to sit beside;
// anything else (a studio palette being edited) gives an empty path.
TFilePath twinPath(int &styleId) {
  styleId = -1;
  if (!app()) return TFilePath();
  TPaletteHandle *ph = app()->getPaletteController()->getCurrentLevelPalette();
  TPalette *palette  = ph ? ph->getPalette() : nullptr;
  ToonzScene *scene  = app()->getCurrentScene()->getScene();
  if (!palette || !scene) return TFilePath();
  styleId = ph->getStyleIndex();

  TFilePath fp;
  if (FullColorPalette::instance()->isFullColorPalette(palette))
    fp = scene->decodeFilePath(FullColorPalette::instance()->getPath());
  else if (TXshSimpleLevel *sl = app()->getCurrentLevel()->getSimpleLevel()) {
    if (sl->getPalette() == palette) fp = scene->decodeFilePath(sl->getPath());
  }
  // A level not saved yet has only a provisional path (the project's sandbox
  // for an untitled scene): a twin written there stays behind when the level
  // is saved, and the next level of that name inherits it (review of
  // 2026-10-23). Its styles remember once the level is on disk.
  if (fp.isEmpty() || !TSystem::doesExistFileOrLevel(fp)) return TFilePath();
  return fp.withType("brushes");  // as the .zmouth: TXshSimpleLevel twins
}

//-----------------------------------------------------------------------------

QDateTime modifiedTime(const TFilePath &fp) {
  TFileStatus status(fp);
  return status.doesExist() ? status.getLastModificationTime() : QDateTime();
}

// Read again whenever the file changed on disk: a level copied or replaced
// meanwhile brings its own twin.
Twin &twin(const TFilePath &fp) {
  Twin &t                  = s_twins[fp.getWideString()];
  const QDateTime modified = modifiedTime(fp);
  if (t.m_modified == modified) return t;

  t.m_entries.clear();
  t.m_modified   = modified;
  t.m_unreadable = false;
  if (modified.isNull()) return t;

  try {
    TIStream is(fp);
    if (!is) return t;
    std::string tagName;
    while (is.matchTag(tagName)) {
      if (tagName != "styles") {
        is.skipCurrentTag();
        continue;
      }
      while (is.matchTag(tagName)) {
        if (tagName != "style") {
          is.skipCurrentTag();
          continue;
        }
        int id           = -1;
        std::string kind = is.getTagAttribute("kind");
        is.getTagParam("id", id);
        TPersist *data = nullptr;
        is >> data;
        if (data && id >= 0 && !kind.empty())
          t.m_entries[EntryKey(id, kind)].reset(data);
        else
          delete data;
        is.matchEndTag();
      }
      is.matchEndTag();
    }
  } catch (...) {
    t.m_unreadable = true;
  }
  return t;
}

//-----------------------------------------------------------------------------

void save(const TFilePath &fp, Twin &t) {
  if (t.m_unreadable) return;
  try {
    if (t.m_entries.empty()) {
      if (TFileStatus(fp).doesExist()) TSystem::deleteFile(fp);
    } else {
      TSystem::touchParentDir(fp);
      TOStream os(fp);
      os.openChild("version");
      os << 1 << 0;
      os.closeChild();
      os.openChild("styles");
      for (auto &entry : t.m_entries) {
        std::map<std::string, std::string> attr;
        attr["id"]   = std::to_string(entry.first.first);
        attr["kind"] = entry.first.second;
        os.openChild("style", attr);
        // The pointer form writes the class tag that `is >> TPersist *&`
        // needs to rebuild the object; `os << *p` writes only its data.
        os << entry.second.get();
        os.closeChild();
      }
      os.closeChild();
    }
  } catch (...) {
  }
  t.m_modified = modifiedTime(fp);
}

void saveDirty() {
  for (const std::wstring &path : s_dirty) {
    const TFilePath fp(path);
    save(fp, twin(fp));
  }
  s_dirty.clear();
}

//=============================================================================

BrushPresetClient *currentClient() {
  if (!app()) return nullptr;
  return dynamic_cast<BrushPresetClient *>(
      app()->getCurrentTool()->getTool());
}

// The brush of a kind, current or not: T_Brush has one instance per target.
BrushPresetClient *clientFor(const QString &kind) {
  TTool::ToolTargetType target =
      kind == "vector"        ? TTool::VectorImage
      : kind == "smartraster" ? TTool::ToonzImage
      : kind == "raster"      ? TTool::RasterImage
                              : TTool::NoTarget;
  if (target == TTool::NoTarget) return nullptr;
  return dynamic_cast<BrushPresetClient *>(TTool::getTool(T_Brush, target));
}

// File names cannot hold every character a preset name can.
QString cardFileName(const QString &name) {
  QString safe = name;
  for (QChar &ch : safe)
    if (QString("/\\:*?\"<>|").contains(ch)) ch = '_';
  return safe + ".tpl";
}

TFilePath cardPath(const std::string &kind, const QString &name) {
  return ToonzFolder::getMyModuleDir() + TFilePath("brush presets") +
         TFilePath(kind) + TFilePath(cardFileName(name).toStdWString());
}

class Provider final : public BrushPresetBridge::Provider {
public:
  QString toolName() const override {
    BrushPresetClient *client = currentClient();
    return client ? client->brushName() : QString();
  }
  QString toolKind() const override {
    BrushPresetClient *client = currentClient();
    return client ? QString::fromStdString(client->brushKind()) : QString();
  }
  void addPreset(const QString &name) override {
    if (BrushPresetClient *client = currentClient()) client->addPreset(name);
  }
  QStringList presets(const QString &kind) const override {
    BrushPresetClient *client = clientFor(kind);
    return client ? client->presetNames() : QStringList();
  }
  QString currentPreset(const QString &kind) const override {
    BrushPresetClient *client = clientFor(kind);
    return client ? client->currentPresetName() : QString();
  }
  QString presetStylePath(const QString &kind,
                          const QString &name) const override {
    const TFilePath fp = cardPath(kind.toStdString(), name);
    return TFileStatus(fp).doesExist() ? fp.getQString() : QString();
  }
  void applyPreset(const QString &kind, const QString &name) override {
    BrushPresetClient *client = clientFor(kind);
    if (!client) return;
    std::shared_ptr<TPersist> data = client->presetData(name);
    if (!data) return;
    const std::string k = kind.toStdString();
    BrushStyleMemory::remember(k, data);
    if (client == currentClient())
      client->applyStyleBrush(data.get());
    else
      // The brush picks the settings up when it becomes current again.
      s_lastSeen.erase(k);
    BrushStyleMemory::notifyChanged();
  }
  void removePreset(const QString &kind, const QString &name) override {
    if (BrushPresetClient *client = clientFor(kind))
      client->removeNamedPreset(name);
  }
};

// The Style Editor lists the custom brushes before any brush is used.
struct ProviderInstaller {
  ProviderInstaller() {
    static Provider provider;
    BrushPresetBridge::instance()->setProvider(&provider);
  }
} s_providerInstaller;

// Listens to style and tool switches once, the first time a brush needs it.
void ensureWatcher() {
  static QObject *watcher = nullptr;
  if (watcher || !app()) return;
  watcher = new QObject;

  s_saveTimer = new QTimer(watcher);
  s_saveTimer->setSingleShot(true);
  s_saveTimer->setInterval(500);
  QObject::connect(s_saveTimer, &QTimer::timeout, watcher, saveDirty);


  auto onStyle = []() {
    if (BrushPresetClient *client = currentClient())
      BrushStyleMemory::applyTo(client);
  };
  TPaletteHandle *ph = app()->getPaletteController()->getCurrentLevelPalette();
  QObject::connect(ph, &TPaletteHandle::colorStyleSwitched, watcher, onStyle);
  QObject::connect(ph, &TPaletteHandle::paletteSwitched, watcher, onStyle);
  QObject::connect(app()->getCurrentTool(), &ToolHandle::toolSwitched,
                   watcher, []() { BrushStyleMemory::notifyChanged(); });
}

}  // namespace

//=============================================================================

void BrushStyleMemory::remember(const std::string &kind,
                                std::shared_ptr<TPersist> brush) {
  int styleId;
  const TFilePath fp = twinPath(styleId);
  if (fp.isEmpty() || styleId < 0 || !brush) return;
  twin(fp).m_entries[EntryKey(styleId, kind)] = brush;
  s_lastSeen[kind] = std::make_pair(fp.getWideString(), styleId);

  // A slider sends a change at every step: write once it settles.
  s_dirty.insert(fp.getWideString());
  ensureWatcher();
  s_saveTimer->start();
}

//-----------------------------------------------------------------------------

void BrushStyleMemory::applyTo(BrushPresetClient *client) {
  if (!client) return;
  ensureWatcher();

  const std::string kind = client->brushKind();
  int styleId;
  const TFilePath fp = twinPath(styleId);
  const std::pair<std::wstring, int> seen(fp.getWideString(), styleId);
  if (s_lastSeen.count(kind) && s_lastSeen[kind] == seen) return;
  s_lastSeen[kind] = seen;
  if (fp.isEmpty() || styleId < 0) return;

  // A style never drawn with keeps the brush as it is.
  Twin &t = twin(fp);
  auto it = t.m_entries.find(EntryKey(styleId, kind));
  if (it == t.m_entries.end()) return;
  client->applyStyleBrush(it->second.get());
  notifyChanged();
}

//-----------------------------------------------------------------------------

void BrushStyleMemory::notifyChanged() {
  ensureWatcher();
  BrushPresetBridge::instance()->notifyChanged();
}

//-----------------------------------------------------------------------------

void BrushStyleMemory::saveCard(const std::string &kind, const QString &name) {
  TColorStyle *style = app() ? app()->getCurrentLevelStyle() : nullptr;
  if (!style || name.isEmpty()) return;
  try {
    const TFilePath fp = cardPath(kind, name);
    TSystem::touchParentDir(fp);
    TPaletteP palette = new TPalette;
    palette->addStyle(style->clone());
    TOStream os(fp);
    os << *palette;
  } catch (...) {
  }
}

//-----------------------------------------------------------------------------

void BrushStyleMemory::removeCard(const std::string &kind,
                                  const QString &name) {
  const TFilePath fp = cardPath(kind, name);
  try {
    if (TFileStatus(fp).doesExist()) TSystem::deleteFile(fp);
  } catch (...) {
  }
}
