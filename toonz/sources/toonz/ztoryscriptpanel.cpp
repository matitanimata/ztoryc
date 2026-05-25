#include "ztoryscriptpanel.h"

#include "ztorymodel.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "tsystem.h"
#include "tfilepath.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QTextCodec>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QXmlStreamReader>
#include <QScrollBar>
#include <QTextCursor>
#include <zlib.h>
#include <QTextCharFormat>
#include <QColor>
#include <QFont>
#include <QSizePolicy>
#include <QPalette>

//=============================================================================
// ZtoryScriptView
//=============================================================================

ZtoryScriptView::ZtoryScriptView(QWidget *parent)
    : QWidget(parent) {

  // --- Toolbar superiore ---
  m_importButton = new QToolButton(this);
  m_importButton->setText(tr("Import…"));
  m_importButton->setToolTip(tr("Import screenplay (.fdx, .fountain, .docx, .odt, .txt)"));
  m_importButton->setFixedHeight(24);

  m_searchField = new QLineEdit(this);
  m_searchField->setPlaceholderText(tr("Search…"));
  m_searchField->setFixedHeight(24);
  m_searchField->setClearButtonEnabled(true);

  m_searchPrevBtn = new QToolButton(this);
  m_searchPrevBtn->setText("▲");
  m_searchPrevBtn->setFixedSize(24, 24);
  m_searchPrevBtn->setToolTip(tr("Previous match"));
  m_searchPrevBtn->setEnabled(false);

  m_searchNextBtn = new QToolButton(this);
  m_searchNextBtn->setText("▼");
  m_searchNextBtn->setFixedSize(24, 24);
  m_searchNextBtn->setToolTip(tr("Next match"));
  m_searchNextBtn->setEnabled(false);

  m_matchLabel = new QLabel(this);
  m_matchLabel->setFixedWidth(60);
  m_matchLabel->setAlignment(Qt::AlignCenter);
  m_matchLabel->setStyleSheet("color: #888; font-size: 11px;");

  QHBoxLayout *toolbar = new QHBoxLayout;
  toolbar->setContentsMargins(4, 4, 4, 4);
  toolbar->setSpacing(4);
  toolbar->addWidget(m_importButton);
  toolbar->addSpacing(8);
  toolbar->addWidget(m_searchField);
  toolbar->addWidget(m_searchPrevBtn);
  toolbar->addWidget(m_searchNextBtn);
  toolbar->addWidget(m_matchLabel);

  // --- Area testo ---
  m_textEdit = new QTextEdit(this);
  m_textEdit->setReadOnly(true);
  m_textEdit->setLineWrapMode(QTextEdit::WidgetWidth);
  m_textEdit->setMinimumSize(80, 60);
  m_textEdit->setFont(QFont("Courier", 11));
  m_textEdit->setStyleSheet(
      "QTextEdit { background: #1e1e1e; color: #d4d4d4; "
      "border: none; padding: 8px; }");
  m_textEdit->setPlaceholderText(
      tr("Import a screenplay (.fdx, .fountain, .docx, .odt or .txt)."));

  // --- Layout principale ---
  QVBoxLayout *main = new QVBoxLayout(this);
  main->setContentsMargins(0, 0, 0, 0);
  main->setSpacing(0);
  main->addLayout(toolbar);
  main->addWidget(m_textEdit);
  setLayout(main);

  setAcceptDrops(true);

  // --- Connessioni ---
  connect(m_importButton, &QToolButton::clicked,
          this, &ZtoryScriptView::onImportClicked);
  connect(m_searchField, &QLineEdit::textChanged,
          this, &ZtoryScriptView::onSearchTextChanged);
  connect(m_searchNextBtn, &QToolButton::clicked,
          this, &ZtoryScriptView::onSearchNext);
  connect(m_searchPrevBtn, &QToolButton::clicked,
          this, &ZtoryScriptView::onSearchPrev);

  // Keep the screenplay in sync with the current scene.  StoryboardPanel::
  // loadZtoryc() calls ZtoryModel::setScriptFile() for every scene it opens
  // (with an empty path when the scene has no screenplay), which emits
  // scriptFileChanged() — reloadFromModel() then shows the new scene's
  // screenplay or clears the panel.
  connect(ZtoryModel::instance(), &ZtoryModel::scriptFileChanged,
          this, &ZtoryScriptView::reloadFromModel);
}

//-----------------------------------------------------------------------------

void ZtoryScriptView::importScreenplay(const QString &srcPath) {
  if (srcPath.isEmpty()) return;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) {
    // No scene yet — just display it without persisting.
    m_currentFilePath = srcPath;
    loadFile(srcPath);
    return;
  }
  // Destination: the project's extras/script folder.
  QString destDir =
      scene->decodeFilePath(TFilePath("+extras")).getQString() + "/script";
  QString fileName = QFileInfo(srcPath).fileName();
  QString destPath = destDir + "/" + fileName;

  bool persisted = false;
  if (QFileInfo(srcPath).absoluteFilePath() !=
      QFileInfo(destPath).absoluteFilePath()) {
    QDir().mkpath(destDir);
    QFile::remove(destPath);  // overwrite any previous import of the same name
    persisted = QFile::copy(srcPath, destPath);
  } else {
    persisted = true;  // already inside extras/script
  }

  if (!persisted) {
    // Copy failed (permissions, bad path…) — show the original this session.
    m_currentFilePath = srcPath;
    loadFile(srcPath);
    return;
  }

  // Display first, then persist.  Setting m_currentFilePath before
  // setScriptFile() means the scriptFileChanged() → reloadFromModel() that
  // fires synchronously sees the file is already shown and skips a redundant
  // reload.  setScriptFile() records the project-relative path
  // ("+extras/script/<file>"); StoryboardPanel writes it into the .ztoryc.
  m_currentFilePath = destPath;
  loadFile(destPath);
  TFilePath coded = scene->codeFilePath(TFilePath(destPath.toStdWString()));
  ZtoryModel::instance()->setScriptFile(coded.getQString());
}

//-----------------------------------------------------------------------------

void ZtoryScriptView::reloadFromModel() {
  QString rel = ZtoryModel::instance()->scriptFile();
  if (rel.isEmpty()) {
    // Current scene has no screenplay — clear any leftover from a prior scene.
    if (!m_currentFilePath.isEmpty()) {
      clear();
      m_currentFilePath.clear();
    }
    return;
  }
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TFilePath abs = scene->decodeFilePath(TFilePath(rel.toStdWString()));
  QString absStr = abs.getQString();
  if (absStr == m_currentFilePath) return;  // already showing this screenplay
  if (TFileStatus(abs).doesExist()) {
    m_currentFilePath = absStr;
    loadFile(absStr);
  } else {
    clear();
    m_currentFilePath.clear();
  }
}

//-----------------------------------------------------------------------------

void ZtoryScriptView::dragEnterEvent(QDragEnterEvent *e) {
  if (!e->mimeData()->hasUrls()) { e->ignore(); return; }
  for (const QUrl &url : e->mimeData()->urls()) {
    QString path = url.toLocalFile().toLower();
    if (path.endsWith(".fdx")      || path.endsWith(".fountain") ||
        path.endsWith(".docx")    || path.endsWith(".odt")      ||
        path.endsWith(".doc")     || path.endsWith(".txt")) {
      e->acceptProposedAction();
      return;
    }
  }
  e->ignore();
}

void ZtoryScriptView::dropEvent(QDropEvent *e) {
  for (const QUrl &url : e->mimeData()->urls()) {
    QString path = url.toLocalFile();
    if (path.endsWith(".fdx",      Qt::CaseInsensitive) ||
        path.endsWith(".fountain", Qt::CaseInsensitive) ||
        path.endsWith(".docx",     Qt::CaseInsensitive) ||
        path.endsWith(".odt",      Qt::CaseInsensitive) ||
        path.endsWith(".doc",      Qt::CaseInsensitive) ||
        path.endsWith(".txt",      Qt::CaseInsensitive)) {
      importScreenplay(path);
      e->acceptProposedAction();
      return;
    }
  }
}

void ZtoryScriptView::onImportClicked() {
  // nullptr parent: prevents dialog from appearing behind the main window
  // on macOS when the panel is docked or embedded in a complex widget hierarchy.
  QString filePath = QFileDialog::getOpenFileName(
      nullptr,
      tr("Import Screenplay"),
      QString(),
      tr("Screenplay files (*.fdx *.fountain *.docx *.odt *.doc *.txt);;"
         "Final Draft (*.fdx);;Fountain (*.fountain);;"
         "Word 2007+ (*.docx);;OpenDocument (*.odt);;"
         "Word 97-2003 (*.doc);;Text (*.txt)"));

  if (filePath.isEmpty()) return;
  importScreenplay(filePath);
}

//-----------------------------------------------------------------------------

void ZtoryScriptView::loadFile(const QString &filePath) {
  QString content;

  if (filePath.endsWith(".fdx", Qt::CaseInsensitive))
    content = parseFdx(filePath);
  else if (filePath.endsWith(".fountain", Qt::CaseInsensitive))
    content = parseFountain(filePath);
  else if (filePath.endsWith(".docx", Qt::CaseInsensitive))
    content = parseDocx(filePath);
  else if (filePath.endsWith(".odt", Qt::CaseInsensitive))
    content = parseOdt(filePath);
  else if (filePath.endsWith(".doc", Qt::CaseInsensitive))
    content = tr("⚠  The legacy .doc format is not supported.\n\n"
                 "Please re-save the file as .docx from Word or LibreOffice,\n"
                 "then import the .docx version.");
  else
    content = parseTxt(filePath);

  if (content.isEmpty()) {
    m_textEdit->setPlaceholderText(tr("Could not read file or file is empty."));
    return;
  }

  m_textEdit->setPlainText(content);

  // Reset search
  m_searchField->clear();
  m_matchPositions.clear();
  m_currentMatch = -1;
  m_matchLabel->clear();
  m_searchPrevBtn->setEnabled(false);
  m_searchNextBtn->setEnabled(false);
}

//-----------------------------------------------------------------------------

void ZtoryScriptView::clear() {
  m_textEdit->clear();
  m_searchField->clear();
  m_matchPositions.clear();
  m_currentMatch = -1;
  m_matchLabel->clear();
}

//-----------------------------------------------------------------------------
// FDX parser — Final Draft XML
// Legge solo i nodi <Text> dentro <Paragraph>, preservando il tipo
// (Scene Heading, Action, Character, Dialogue, Transition…)
//-----------------------------------------------------------------------------

QString ZtoryScriptView::parseFdx(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();

  QString result;
  QXmlStreamReader xml(&file);

  QString currentType;
  QString currentText;

  while (!xml.atEnd() && !xml.hasError()) {
    xml.readNext();

    if (xml.isStartElement()) {
      if (xml.name() == QLatin1String("Paragraph")) {
        currentType = xml.attributes().value("Type").toString();
        currentText.clear();
      } else if (xml.name() == QLatin1String("Text")) {
        currentText += xml.readElementText();
      }
    } else if (xml.isEndElement()) {
      if (xml.name() == QLatin1String("Paragraph") && !currentText.trimmed().isEmpty()) {
        // Formattazione visiva per tipo
        if (currentType == "Scene Heading") {
          result += "\n" + currentText.trimmed().toUpper() + "\n";
        } else if (currentType == "Action") {
          result += "\n" + currentText.trimmed() + "\n";
        } else if (currentType == "Character") {
          result += "\n          " + currentText.trimmed().toUpper() + "\n";
        } else if (currentType == "Dialogue") {
          result += "     " + currentText.trimmed() + "\n";
        } else if (currentType == "Parenthetical") {
          result += "     (" + currentText.trimmed() + ")\n";
        } else if (currentType == "Transition") {
          result += "\n                              "
                    + currentText.trimmed().toUpper() + "\n";
        } else {
          result += currentText.trimmed() + "\n";
        }
        currentText.clear();
      }
    }
  }

  file.close();
  return result;
}

//-----------------------------------------------------------------------------
// Fountain parser — open screenplay markup format
//
// Spec: https://fountain.io/syntax
// Supports: title page (skipped), scene headings, action, character,
//           parenthetical, dialogue, transition, lyrics, boneyard, notes.
//-----------------------------------------------------------------------------

namespace {

// True if every letter in s is uppercase (ignores digits/punctuation).
static bool allCaps(const QString &s) {
  for (QChar c : s)
    if (c.isLetter() && c.isLower()) return false;
  return !s.isEmpty();
}

// Strip an inline extension like (V.O.) or (O.S.) from a character name.
static QString stripExtension(const QString &s, QString *ext = nullptr) {
  int open = s.lastIndexOf('(');
  int close = s.lastIndexOf(')');
  if (open != -1 && close == s.length() - 1 && open < close) {
    if (ext) *ext = s.mid(open);
    return s.left(open).trimmed();
  }
  if (ext) *ext = QString();
  return s;
}

static bool isFountainSceneHeading(const QString &s) {
  if (s.startsWith('.') && s.length() > 1 && s[1] != '.') return true;
  QString u = s.toUpper();
  return u.startsWith("INT.") || u.startsWith("EXT.") ||
         u.startsWith("INT ") || u.startsWith("EXT ") ||
         u.startsWith("INT/EXT") || u.startsWith("EXT/INT") ||
         u.startsWith("I/E ") || u.startsWith("EST.") ||
         u.startsWith("EST ");
}

}  // namespace

QString ZtoryScriptView::parseFountain(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
  QTextStream in(&file);
  in.setCodec("UTF-8");
  QStringList raw = in.readAll().split('\n');
  file.close();

  // Normalise line endings.
  for (auto &l : raw) l.remove('\r');

  int n = raw.size();

  // Skip title page: an initial block of "Key: value" lines terminated by a
  // blank line.  Heuristic: if the first non-blank line contains a colon we
  // treat the whole block up to the first blank line as title page.
  int i = 0;
  while (i < n && raw[i].trimmed().isEmpty()) ++i;
  if (i < n && raw[i].contains(':')) {
    while (i < n && !raw[i].trimmed().isEmpty()) ++i;  // skip title block
    while (i < n && raw[i].trimmed().isEmpty()) ++i;   // skip trailing blanks
  }

  QString result;
  bool inDialogue = false;
  bool inBoneyard = false;

  for (; i < n; ++i) {
    QString line = raw[i];
    QString s    = line.trimmed();

    // --- boneyard (block comment) ---
    if (!inBoneyard && s.startsWith("/*")) { inBoneyard = true; }
    if (inBoneyard) {
      if (s.contains("*/")) inBoneyard = false;
      continue;
    }

    // --- blank line ---
    if (s.isEmpty()) { inDialogue = false; continue; }

    // --- inline note [[…]] ---
    if (s.startsWith("[[") && s.endsWith("]]")) continue;

    // --- page break ---
    if (s == "===") { result += "\n"; continue; }

    // --- centered text >text< ---
    if (s.startsWith('>') && s.endsWith('<')) {
      result += "\n" + s.mid(1, s.length() - 2).trimmed() + "\n";
      inDialogue = false;
      continue;
    }

    // --- forced transition >text (not centered) ---
    if (s.startsWith('>') && !s.endsWith('<')) {
      result += "\n" + QString(30, ' ') + s.mid(1).trimmed().toUpper() + "\n";
      inDialogue = false;
      continue;
    }

    // --- transition: all-caps line ending with "TO:" ---
    {
      bool prevBlank = (i == 0 || raw[i - 1].trimmed().isEmpty());
      bool nextBlank = (i + 1 >= n || raw[i + 1].trimmed().isEmpty());
      if (prevBlank && nextBlank && s.endsWith("TO:") && allCaps(s)) {
        result += "\n" + QString(30, ' ') + s + "\n";
        inDialogue = false;
        continue;
      }
    }

    // --- scene heading ---
    if (isFountainSceneHeading(s)) {
      QString heading = s.startsWith('.') ? s.mid(1).trimmed() : s;
      result += "\n" + heading.toUpper() + "\n";
      inDialogue = false;
      continue;
    }

    // --- parenthetical inside dialogue ---
    if (inDialogue && s.startsWith('(') && s.endsWith(')')) {
      result += "     " + s + "\n";
      continue;
    }

    // --- character name ---
    // Conditions: preceded by blank line, followed by non-blank, all-caps base.
    {
      bool prevBlank = (i == 0 || raw[i - 1].trimmed().isEmpty());
      bool nextNonBlank = (i + 1 < n && !raw[i + 1].trimmed().isEmpty());
      bool forced = s.startsWith('@');
      QString base = forced ? s.mid(1).trimmed() : s;
      QString ext;
      QString nameOnly = stripExtension(base, &ext);

      if ((forced || (prevBlank && allCaps(nameOnly) && nameOnly.length() <= 40))
          && nextNonBlank) {
        result += "\n" + QString(10, ' ') + nameOnly.toUpper();
        if (!ext.isEmpty()) result += " " + ext;
        result += "\n";
        inDialogue = true;
        continue;
      }
    }

    // --- dialogue ---
    if (inDialogue) {
      result += "     " + s + "\n";
      continue;
    }

    // --- lyrics ---
    if (s.startsWith('~')) {
      result += "\n♪ " + s.mid(1).trimmed() + " ♪\n";
      continue;
    }

    // --- action (forced with ! or plain) ---
    result += "\n" + (s.startsWith('!') ? s.mid(1) : s) + "\n";
  }

  return result;
}

//-----------------------------------------------------------------------------
// TXT parser — testo plain con euristica screenplay
//
// Se il file contiene intestazioni di scena (SC\d+. o INT./EXT.) applica
// la stessa formattazione visiva dell'FDX.  Altrimenti restituisce il testo
// grezzo, che è il comportamento atteso per file di testo generici.
//-----------------------------------------------------------------------------

namespace {

static bool isTxtSceneHeading(const QString &s) {
  if (s.isEmpty()) return false;
  QString u = s.toUpper();
  // Italian style: SC1. or SC 1.
  if (u.startsWith("SC")) {
    int j = 2;
    while (j < u.length() && u[j] == ' ') ++j;
    if (j < u.length() && u[j].isDigit()) return true;
  }
  return u.startsWith("INT.") || u.startsWith("EXT.") ||
         u.startsWith("INT ") || u.startsWith("EXT ") ||
         u.startsWith("EST.") || u.startsWith("EST ");
}

static bool isTxtTransition(const QString &s) {
  QString u = s.toUpper();
  return u.startsWith("STACCO") && s.contains(':');
}

static bool isTxtCharacter(const QString &s) {
  if (s.isEmpty() || s.length() > 50) return false;
  // Remove inline parenthetical for the caps check.
  QString base = s;
  int p = base.lastIndexOf('(');
  if (p != -1 && base.endsWith(')')) base = base.left(p).trimmed();
  if (base.isEmpty() || !base[0].isUpper()) return false;
  for (QChar c : base)
    if (c.isLetter() && c.isLower()) return false;
  return true;
}

}  // namespace

QString ZtoryScriptView::parseTxt(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
  QTextStream in(&file);
  in.setCodec("UTF-8");
  QStringList raw = in.readAll().split('\n');
  file.close();

  for (auto &l : raw) l.remove('\r');

  // Detect whether this file looks like a screenplay.
  int sceneCount = 0;
  for (const auto &l : raw)
    if (isTxtSceneHeading(l.trimmed())) ++sceneCount;

  if (sceneCount == 0) {
    // Plain text — return as-is.
    QString result;
    for (const auto &l : raw) result += l + '\n';
    return result;
  }

  // Screenplay heuristic formatting.
  QString result;
  bool inDialogue = false;
  int n = raw.size();

  for (int i = 0; i < n; ++i) {
    QString s = raw[i].trimmed();

    if (s.isEmpty()) { inDialogue = false; continue; }

    if (isTxtTransition(s)) {
      result += "\n" + QString(30, ' ') +
                s.left(s.indexOf(':')).trimmed().toUpper() + "\n";
      inDialogue = false;
      continue;
    }

    if (isTxtSceneHeading(s)) {
      result += "\n" + s.toUpper() + "\n";
      inDialogue = false;
      continue;
    }

    // Parenthetical in dialogue context.
    if (inDialogue && s.startsWith('(') && s.endsWith(')')) {
      result += "     " + s + "\n";
      continue;
    }

    // Character: all-caps, followed by at least one non-blank line.
    if (!inDialogue && isTxtCharacter(s)) {
      bool hasNext = false;
      for (int j = i + 1; j < n; ++j)
        if (!raw[j].trimmed().isEmpty()) { hasNext = true; break; }
      if (hasNext) {
        QString base = s;
        QString ext;
        int p = base.lastIndexOf('(');
        if (p != -1 && base.endsWith(')')) {
          ext = base.mid(p).trimmed();
          base = base.left(p).trimmed();
        }
        result += "\n" + QString(10, ' ') + base;
        if (!ext.isEmpty()) result += " " + ext;
        result += "\n";
        inDialogue = true;
        continue;
      }
    }

    if (inDialogue) {
      result += "     " + s + "\n";
      continue;
    }

    result += "\n" + s + "\n";
  }

  return result;
}

//=============================================================================
// Minimal cross-platform ZIP extractor (zlib inflate, no extra dependencies)
// Used by parseDocx() and parseOdt().
//=============================================================================

namespace {  // ZipUtil — internal linkage

static inline quint16 le16(const quint8 *p) {
  return quint16(p[0]) | (quint16(p[1]) << 8);
}
static inline quint32 le32(const quint8 *p) {
  return quint32(p[0]) | (quint32(p[1]) << 8) |
         (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

struct Entry {
  QString  name;
  quint16  method       = 0;
  quint32  compSize     = 0;
  quint32  uncompSize   = 0;
  quint32  localOffset  = 0;
};

// Locate the End of Central Directory record (search backwards from end).
static qint64 findEOCD(const quint8 *buf, qint64 size) {
  qint64 from = qMax((qint64)0, size - 65557);
  for (qint64 i = size - 22; i >= from; --i)
    if (buf[i]==0x50 && buf[i+1]==0x4B &&
        buf[i+2]==0x05 && buf[i+3]==0x06) return i;
  return -1;
}

// Parse the Central Directory and return a list of entries.
static QList<Entry> centralDir(const quint8 *buf, qint64 size) {
  QList<Entry> list;
  qint64 eocd = findEOCD(buf, size);
  if (eocd < 0 || eocd + 22 > size) return list;

  quint32 cdOffset = le32(buf + eocd + 16);
  quint16 cdCount  = le16(buf + eocd + 8);

  qint64 pos = cdOffset;
  for (int i = 0; i < cdCount && pos + 46 <= size; ++i) {
    if (buf[pos]!=0x50 || buf[pos+1]!=0x4B ||
        buf[pos+2]!=0x01 || buf[pos+3]!=0x02) break;

    Entry e;
    e.method           = le16(buf + pos + 10);
    e.compSize         = le32(buf + pos + 20);
    e.uncompSize       = le32(buf + pos + 24);
    quint16 nameLen    = le16(buf + pos + 28);
    quint16 extraLen   = le16(buf + pos + 30);
    quint16 commentLen = le16(buf + pos + 32);
    e.localOffset      = le32(buf + pos + 42);

    if (pos + 46 + nameLen <= size)
      e.name = QString::fromUtf8(
          reinterpret_cast<const char *>(buf + pos + 46), nameLen);

    list.append(e);
    pos += 46 + nameLen + extraLen + commentLen;
  }
  return list;
}

// Extract a named entry. Returns empty QByteArray on failure.
static QByteArray extract(const QString &zipPath, const QString &entryName) {
  QFile f(zipPath);
  if (!f.open(QIODevice::ReadOnly)) return {};
  QByteArray raw = f.readAll();
  f.close();

  const quint8 *buf  = reinterpret_cast<const quint8 *>(raw.constData());
  qint64        size = raw.size();

  for (const Entry &e : centralDir(buf, size)) {
    if (e.name != entryName) continue;

    // Skip local file header to reach compressed data.
    qint64 lhPos = e.localOffset;
    if (lhPos + 30 > size) return {};
    quint16 lnameLen  = le16(buf + lhPos + 26);
    quint16 lextraLen = le16(buf + lhPos + 28);
    qint64  dataStart = lhPos + 30 + lnameLen + lextraLen;
    if (dataStart + (qint64)e.compSize > size) return {};

    if (e.method == 0)  // stored
      return raw.mid(dataStart, e.compSize);

    if (e.method == 8) {  // deflate
      QByteArray out(e.uncompSize, '\0');
      z_stream zs{};
      zs.next_in   = const_cast<quint8 *>(buf + dataStart);
      zs.avail_in  = e.compSize;
      zs.next_out  = reinterpret_cast<Bytef *>(out.data());
      zs.avail_out = e.uncompSize;
      if (inflateInit2(&zs, -MAX_WBITS) == Z_OK) {
        inflate(&zs, Z_FINISH);
        inflateEnd(&zs);
      }
      return out;
    }
    break;
  }
  return {};
}

}  // namespace (ZipUtil)

//-----------------------------------------------------------------------------
// Screenplay element types (shared by DOCX and ODT parsers)
//-----------------------------------------------------------------------------

namespace ScreenplayStyle {

enum Type { Unknown, SceneHeading, Action, Character,
            Parenthetical, Dialogue, Transition, General };

// Map style names used by common screenplay templates to our element types.
static Type fromStyleName(const QString &raw) {
  QString s = raw.toLower().remove(' ').remove('-').remove('_');

  if (s == "sceneheading" || s == "slug"      || s == "slugline" ||
      s == "shot"         || s == "intext"    || s == "scene")
    return SceneHeading;

  if (s == "action"       || s == "description" || s == "direction" ||
      s == "stagedir"     || s == "stageDirection")
    return Action;

  if (s == "character"    || s == "charname"  || s == "speakingcharacter")
    return Character;

  if (s == "dialogue"     || s == "dialog")
    return Dialogue;

  if (s == "parenthetical"|| s == "extension")
    return Parenthetical;

  if (s == "transition"   || s == "stacco")
    return Transition;

  if (s == "general"      || s == "normal"    || s == "titlepage" ||
      s == "title"        || s == "scripttitle")
    return General;

  return Unknown;
}

// Format a (type, text) pair into the display string (same style as FDX parser).
static QString format(Type t, const QString &text) {
  QString s = text.trimmed();
  if (s.isEmpty()) return QString();
  switch (t) {
    case SceneHeading:   return "\n" + s.toUpper() + "\n";
    case Action:         return "\n" + s + "\n";
    case Character:      return "\n" + QString(10, ' ') + s.toUpper() + "\n";
    case Dialogue:       return "     " + s + "\n";
    case Parenthetical:
      if (!s.startsWith('(')) s = "(" + s + ")";
      return "     " + s + "\n";
    case Transition:
      return "\n" + QString(30, ' ') + s.toUpper() + "\n";
    default:             return s + "\n";
  }
}

}  // namespace ScreenplayStyle

//-----------------------------------------------------------------------------
// DOCX parser — extracts word/document.xml from the OOXML ZIP and parses it.
//
// If paragraph styles match known screenplay names they are used directly.
// Otherwise the plain text is passed through the same heuristic as parseTxt().
//-----------------------------------------------------------------------------

QString ZtoryScriptView::parseDocx(const QString &filePath) {
  QByteArray xmlData = extract(filePath, "word/document.xml");
  if (xmlData.isEmpty()) return tr("Could not read document.xml inside the .docx file.");

  // --- parse XML ---
  struct Para { ScreenplayStyle::Type type; QString text; };
  QList<Para> paras;
  bool hasKnownStyles = false;

  QXmlStreamReader xml(xmlData);
  ScreenplayStyle::Type curType = ScreenplayStyle::Unknown;
  QString curText;
  bool inPara = false;

  while (!xml.atEnd()) {
    xml.readNext();
    if (!xml.isStartElement() && !xml.isEndElement()) continue;

    QStringView name = xml.name();

    if (xml.isStartElement()) {
      if (name == QLatin1String("p")) {              // <w:p> — new paragraph
        inPara  = true;
        curType = ScreenplayStyle::Unknown;
        curText.clear();
      } else if (name == QLatin1String("pStyle")) {  // <w:pStyle w:val="…">
        QString val = xml.attributes().value("w:val").toString();
        ScreenplayStyle::Type t = ScreenplayStyle::fromStyleName(val);
        if (t != ScreenplayStyle::Unknown) { curType = t; hasKnownStyles = true; }
      } else if (name == QLatin1String("t") && inPara) {  // <w:t>
        curText += xml.readElementText();
      } else if (name == QLatin1String("br") && inPara) { // <w:br> soft return
        curText += ' ';
      }
    } else {  // isEndElement
      if (name == QLatin1String("p") && inPara) {
        inPara = false;
        // Always append — empty paragraphs become blank line separators
        // in the fallback flat text, which the heuristic relies on.
        paras.append({curType, curText});
      }
    }
  }

  if (xml.hasError()) return tr("XML parse error in .docx file.");

  // If no styles were recognised fall back to the TXT heuristic.
  if (!hasKnownStyles) {
    // Join all paragraphs with newlines; empty ones become blank separators.
    QString flat;
    for (const Para &p : paras) flat += p.text + "\n";
    QString tmp = QDir::tempPath() + "/ztory_docx_import.txt";
    QFile tf(tmp);
    if (tf.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream ts(&tf);
      ts.setCodec("UTF-8");
      ts << flat;
    }
    return parseTxt(tmp);
  }

  QString result;
  for (const Para &p : paras)
    if (!p.text.trimmed().isEmpty())
      result += ScreenplayStyle::format(p.type, p.text);
  return result;
}

//-----------------------------------------------------------------------------
// ODT parser — extracts content.xml from the ODF ZIP and parses it.
//
// Same style mapping and fallback logic as parseDocx().
//-----------------------------------------------------------------------------

QString ZtoryScriptView::parseOdt(const QString &filePath) {
  QByteArray xmlData = extract(filePath, "content.xml");
  if (xmlData.isEmpty()) return tr("Could not read content.xml inside the .odt file.");

  struct Para { ScreenplayStyle::Type type; QString text; };
  QList<Para> paras;
  bool hasKnownStyles = false;

  // ODF text namespace URI
  static const QString kTextNs =
      QStringLiteral("urn:oasis:names:tc:opendocument:xmlns:text:1.0");

  QXmlStreamReader xml(xmlData);
  ScreenplayStyle::Type curType = ScreenplayStyle::Unknown;
  QString curText;
  bool inPara = false;

  while (!xml.atEnd()) {
    xml.readNext();

    if (xml.isStartElement()) {
      QString ns   = xml.namespaceUri().toString();
      QString name = xml.name().toString();

      if (ns == kTextNs && name == QLatin1String("p")) {
        // <text:p text:style-name="…"> — start of paragraph
        inPara  = true;
        curText.clear();
        // style-name is a namespace-qualified attribute
        QString styleName =
            xml.attributes().value(kTextNs, QStringLiteral("style-name"))
                .toString();
        if (styleName.isEmpty())
          styleName = xml.attributes()
                          .value(QStringLiteral("text:style-name"))
                          .toString();
        ScreenplayStyle::Type t = ScreenplayStyle::fromStyleName(styleName);
        curType = t;
        if (t != ScreenplayStyle::Unknown) hasKnownStyles = true;

      } else if (inPara && ns == kTextNs && name == QLatin1String("span")) {
        // <text:span> — inline run; readElementText collects all nested text
        curText += xml.readElementText(QXmlStreamReader::IncludeChildElements);

      } else if (inPara && ns == kTextNs && name == QLatin1String("s")) {
        // <text:s text:c="n"/> — repeated spaces
        int cnt = xml.attributes()
                      .value(kTextNs, QStringLiteral("c"))
                      .toInt();
        if (cnt < 1) cnt = 1;
        curText += QString(cnt, ' ');

      } else if (inPara && ns == kTextNs &&
                 name == QLatin1String("line-break")) {
        curText += ' ';
      }

    } else if (xml.isCharacters() && inPara) {
      // Direct text content inside <text:p> (not inside a span)
      curText += xml.text().toString();

    } else if (xml.isEndElement()) {
      QString ns   = xml.namespaceUri().toString();
      QString name = xml.name().toString();
      if (ns == kTextNs && name == QLatin1String("p") && inPara) {
        inPara = false;
        // Always append — empty paras become blank separator lines.
        paras.append({curType, curText});
      }
    }
  }

  if (xml.hasError()) return tr("XML parse error in .odt file.");

  if (!hasKnownStyles) {
    QString flat;
    for (const Para &p : paras) flat += p.text + "\n";
    QString tmp = QDir::tempPath() + "/ztory_odt_import.txt";
    QFile tf(tmp);
    if (tf.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream ts(&tf);
      ts.setCodec("UTF-8");
      ts << flat;
    }
    return parseTxt(tmp);
  }

  QString result;
  for (const Para &p : paras)
    if (!p.text.trimmed().isEmpty())
      result += ScreenplayStyle::format(p.type, p.text);
  return result;
}

//-----------------------------------------------------------------------------
// Search
//-----------------------------------------------------------------------------

void ZtoryScriptView::onSearchTextChanged(const QString &text) {
  applySearch(text);
}

void ZtoryScriptView::applySearch(const QString &text) {
  // Rimuovi highlight precedente
  QTextCursor cursor = m_textEdit->textCursor();
  cursor.select(QTextCursor::Document);
  QTextCharFormat plainFormat;
  plainFormat.setBackground(Qt::transparent);
  cursor.setCharFormat(plainFormat);
  cursor.clearSelection();
  m_textEdit->setTextCursor(cursor);

  m_matchPositions.clear();
  m_currentMatch = -1;
  m_matchLabel->clear();

  if (text.isEmpty()) {
    m_searchPrevBtn->setEnabled(false);
    m_searchNextBtn->setEnabled(false);
    return;
  }

  // Trova tutte le occorrenze
  QTextDocument *doc = m_textEdit->document();
  QTextCharFormat highlightFormat;
  highlightFormat.setBackground(QColor("#4a4a00"));
  highlightFormat.setForeground(QColor("#ffff88"));

  QTextCursor search(doc);
  while (!search.isNull() && !search.atEnd()) {
    search = doc->find(text, search, QTextDocument::FindCaseSensitively);
    if (!search.isNull()) {
      m_matchPositions.append(search.anchor());
      search.mergeCharFormat(highlightFormat);
    }
  }

  int count = m_matchPositions.size();
  if (count > 0) {
    m_currentMatch = 0;
    navigateMatch(true);
    m_matchLabel->setText(QString("1/%1").arg(count));
    m_searchPrevBtn->setEnabled(true);
    m_searchNextBtn->setEnabled(true);
  } else {
    m_matchLabel->setText(tr("0 found"));
    m_searchPrevBtn->setEnabled(false);
    m_searchNextBtn->setEnabled(false);
  }
  m_lastSearch = text;
}

void ZtoryScriptView::onSearchNext() {
  if (m_matchPositions.isEmpty()) return;
  m_currentMatch = (m_currentMatch + 1) % m_matchPositions.size();
  navigateMatch(true);
  m_matchLabel->setText(
      QString("%1/%2").arg(m_currentMatch + 1).arg(m_matchPositions.size()));
}

void ZtoryScriptView::onSearchPrev() {
  if (m_matchPositions.isEmpty()) return;
  m_currentMatch = (m_currentMatch - 1 + m_matchPositions.size())
                   % m_matchPositions.size();
  navigateMatch(false);
  m_matchLabel->setText(
      QString("%1/%2").arg(m_currentMatch + 1).arg(m_matchPositions.size()));
}

void ZtoryScriptView::navigateMatch(bool forward) {
  if (m_matchPositions.isEmpty() || m_currentMatch < 0) return;

  int pos = m_matchPositions[m_currentMatch];
  QTextDocument *doc = m_textEdit->document();

  // Highlight corrente in giallo brillante
  QTextCursor cur(doc);
  cur.setPosition(pos);
  cur.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                   m_lastSearch.length());

  QTextCharFormat currentFormat;
  currentFormat.setBackground(QColor("#aaaa00"));
  currentFormat.setForeground(QColor("#ffffff"));
  cur.mergeCharFormat(currentFormat);

  m_textEdit->setTextCursor(cur);
  m_textEdit->ensureCursorVisible();
}

//=============================================================================
// ZtoryScriptPanel
//=============================================================================

ZtoryScriptPanel::ZtoryScriptPanel(QWidget *parent)
    : TPanel(parent) {
  setWindowTitle(tr("Ztoryc Script"));
  setObjectName("ZtoryScriptPanel");

  m_view = new ZtoryScriptView(this);
  setWidget(m_view);

  // Dimensione di default ragionevole
  setMinimumSize(320, 400);
  resize(420, 600);
}

//=============================================================================
// Factory
//=============================================================================

class ZtoryScriptPanelFactory final : public TPanelFactory {
public:
  ZtoryScriptPanelFactory() : TPanelFactory("ZtoryScriptPanel") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryScriptPanel(parent);
    panel->setObjectName("ZtoryScriptPanel");
    panel->setWindowTitle("Ztoryc Script");
    return panel;
  }
  void initialize(TPanel *) override { assert(0); }
} ztoryScriptPanelFactory;
