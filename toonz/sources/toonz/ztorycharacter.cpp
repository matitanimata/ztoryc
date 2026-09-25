#include "ztorycharacter.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

//============================================================================

QString ZtoryCharacter::sidecarPathFor(const QString &scenePath) {
  if (scenePath.isEmpty()) return QString();
  if (scenePath.endsWith(".ztoryc", Qt::CaseInsensitive)) return scenePath;
  QString p = scenePath;
  p.replace(QRegularExpression("\\.tnz$", QRegularExpression::CaseInsensitiveOption),
            ".ztoryc");
  // Una scena senza estensione .tnz non ha un sidecar da indovinare: meglio
  // niente che un file con due estensioni appiccicate.
  return p.endsWith(".ztoryc", Qt::CaseInsensitive) ? p : QString();
}

//----------------------------------------------------------------------------

//! Legge il solo attributo `role` del sidecar, senza costruire il documento.
//! Stringa vuota se il file non c'e' o non e' un `.ztoryc`.
static QString readRole(const QString &sidecar) {
  QFile f(sidecar);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
  QXmlStreamReader xml(&f);
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement() && xml.name() == QLatin1String("ztoryc"))
      return xml.attributes().value("role").toString();
  }
  return QString();
}

QString ZtoryCharacter::roleOf(const QString &scenePath) {
  const QString sidecar = sidecarPathFor(scenePath);
  if (sidecar.isEmpty() || !QFile::exists(sidecar)) return QString();
  const QString r = readRole(sidecar);
  return r.isEmpty() ? QString("storyboard") : r;
}

bool ZtoryCharacter::setRole(const QString &scenePath, const QString &role,
                             QString *error) {
  auto fail = [&](const QString &msg) {
    if (error) *error = msg;
    return false;
  };
  if (error) error->clear();

  const QString sidecar = sidecarPathFor(scenePath);
  if (sidecar.isEmpty())
    return fail(QObject::tr("not a scene path: %1").arg(scenePath));
  if (!QFile::exists(sidecar)) {
    // Nessun sidecar: lo si CREA con il solo ruolo, invece di rifiutare.
    // Il file nasce comunque alla prima apertura della scena, quindi non e' un
    // artefatto nuovo: e' lo stesso file, scritto prima. Rifiutare qui voleva
    // dire «il ruolo lo puoi correggere solo DOPO aver aperto la scena col
    // ruolo sbagliato» — cioe' dopo che il danno e' fatto (una scena letta
    // come storyboard pubblica i suoi shot nel tracker).
    QFile nf(sidecar);
    if (!nf.open(QIODevice::WriteOnly | QIODevice::Text))
      return fail(
          QObject::tr("cannot create %1").arg(QFileInfo(sidecar).fileName()));
    nf.write(QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     "<ztoryc version=\"2\" role=\"%1\">\n</ztoryc>\n")
                 .arg(role)
                 .toUtf8());
    nf.close();
    if (nf.error() != QFile::NoError)
      return fail(
          QObject::tr("writing %1 failed").arg(QFileInfo(sidecar).fileName()));
    return true;
  }

  QFile f(sidecar);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return fail(QObject::tr("cannot read %1").arg(QFileInfo(sidecar).fileName()));
  QString text = QString::fromUtf8(f.readAll());
  f.close();

  // Si tocca SOLO l'elemento radice: un `role=` che comparisse piu' in basso
  // (in un attributo di un altro elemento, oggi o domani) non deve essere
  // riscritto per sbaglio.
  QRegularExpression re("(<ztoryc\\b[^>]*?)\\brole=\"[^\"]*\"");
  QRegularExpressionMatch m = re.match(text);
  if (m.hasMatch()) {
    text.replace(m.capturedStart(), m.capturedLength(),
                 m.captured(1) + "role=\"" + role + "\"");
  } else {
    // Sidecar vecchio senza l'attributo: glielo si aggiunge invece di
    // rifiutare, o le scene di prima del ruolo resterebbero non correggibili.
    QRegularExpression open("<ztoryc\\b");
    QRegularExpressionMatch om = open.match(text);
    if (!om.hasMatch())
      return fail(QObject::tr("%1 is not a Ztoryc file")
                      .arg(QFileInfo(sidecar).fileName()));
    text.insert(om.capturedEnd(), " role=\"" + role + "\"");
  }

  QFile w(sidecar);
  if (!w.open(QIODevice::WriteOnly | QIODevice::Text))
    return fail(QObject::tr("cannot write %1").arg(QFileInfo(sidecar).fileName()));
  w.write(text.toUtf8());
  w.close();
  if (w.error() != QFile::NoError)
    return fail(QObject::tr("writing %1 failed").arg(QFileInfo(sidecar).fileName()));
  return true;
}

void ZtoryCharacter::characterRef(const QString &scenePath, QString *uuid,
                                  QString *name) {
  if (uuid) uuid->clear();
  if (name) name->clear();
  const QString sidecar = sidecarPathFor(scenePath);
  if (sidecar.isEmpty()) return;
  QFile f(sidecar);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QXmlStreamReader xml(&f);
  while (!xml.atEnd()) {
    xml.readNext();
    if (!xml.isStartElement()) continue;
    if (xml.name() != QLatin1String("character")) continue;
    const QXmlStreamAttributes a = xml.attributes();
    if (uuid) *uuid = a.value("uuid").toString();
    if (name) *name = a.value("name").toString();
    return;
  }
}

bool ZtoryCharacter::isCharacterScene(const QString &scenePath) {
  const QString sidecar = sidecarPathFor(scenePath);
  if (sidecar.isEmpty()) return false;
  return readRole(sidecar) == QLatin1String("character");
}

//----------------------------------------------------------------------------

//----------------------------------------------------------------------------

//! Scrive <character uuid name/> nel sidecar ESISTENTE \p sidecar toccando solo
//! quell'elemento: sostituisce il primo <character> (lo stesso che legge
//! characterRef), o lo aggiunge subito dopo l'apertura di <ztoryc>.
static bool setCharacterRefInSidecar(const QString &sidecar, const QString &uuid,
                                     const QString &name, QString *error) {
  auto fail = [&](const QString &msg) {
    if (error) *error = msg;
    return false;
  };
  QFile f(sidecar);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return fail(QObject::tr("cannot read %1").arg(QFileInfo(sidecar).fileName()));
  QString text = QString::fromUtf8(f.readAll());
  f.close();

  QString elem = "<character";
  if (!uuid.isEmpty()) elem += " uuid=\"" + uuid.toHtmlEscaped() + "\"";
  if (!name.isEmpty()) elem += " name=\"" + name.toHtmlEscaped() + "\"";
  elem += "/>";

  QRegularExpression existing("<character\\b[^>]*/>");
  QRegularExpressionMatch m = existing.match(text);
  if (m.hasMatch()) {
    text.replace(m.capturedStart(), m.capturedLength(), elem);
  } else {
    QRegularExpression open("<ztoryc\\b[^>]*>");
    QRegularExpressionMatch om = open.match(text);
    if (!om.hasMatch())
      return fail(QObject::tr("%1 is not a Ztoryc file")
                      .arg(QFileInfo(sidecar).fileName()));
    text.insert(om.capturedEnd(), "\n    " + elem);
  }

  QFile w(sidecar);
  if (!w.open(QIODevice::WriteOnly | QIODevice::Text))
    return fail(QObject::tr("cannot write %1").arg(QFileInfo(sidecar).fileName()));
  w.write(text.toUtf8());
  w.close();
  if (w.error() != QFile::NoError)
    return fail(QObject::tr("writing %1 failed").arg(QFileInfo(sidecar).fileName()));
  return true;
}

bool ZtoryCharacter::declareCharacterScene(const QString &scenePath,
                                           const QString &assetUuid,
                                           const QString &assetName,
                                           QString *error) {
  auto fail = [&](const QString &msg) {
    if (error) *error = msg;
    return false;
  };
  if (error) error->clear();

  const QString sidecar = sidecarPathFor(scenePath);
  if (sidecar.isEmpty())
    return fail(QObject::tr("not a scene path: %1").arg(scenePath));

  // Se il sidecar c'e' gia' si cambiano il ruolo e il riferimento al
  // personaggio, senza riscrivere il file: potrebbe contenere roba che non
  // conosciamo, e buttarla via sarebbe un prezzo assurdo.
  // ⚠️ Prima si cambiava SOLO il ruolo: le scene dichiarate su un sidecar gia'
  // esistente (FATINA, SOFIA) restavano personaggi senza sapere QUALE, e le
  // loro mappe delle bocche nascevano senza personaggio (Franco, 2026-09-25).
  if (QFile::exists(sidecar)) {
    if (!setRole(scenePath, "character", error)) return false;
    if (assetUuid.isEmpty() && assetName.isEmpty()) return true;
    return setCharacterRefInSidecar(sidecar, assetUuid, assetName, error);
  }

  QFile f(sidecar);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    return fail(QObject::tr("cannot write %1").arg(QFileInfo(sidecar).fileName()));

  QXmlStreamWriter xml(&f);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztoryc");
  xml.writeAttribute("version", "2");
  xml.writeAttribute("role", "character");
  // Il riferimento all'asset di progetto: e' cio' che lega la scena al
  // personaggio del tracker anche se il file viene rinominato.
  if (!assetUuid.isEmpty() || !assetName.isEmpty()) {
    xml.writeStartElement("character");
    if (!assetUuid.isEmpty()) xml.writeAttribute("uuid", assetUuid);
    if (!assetName.isEmpty()) xml.writeAttribute("name", assetName);
    xml.writeEndElement();
  }
  xml.writeEndElement();  // ztoryc
  xml.writeEndDocument();

  f.close();
  if (f.error() != QFile::NoError)
    return fail(QObject::tr("writing %1 failed").arg(QFileInfo(sidecar).fileName()));
  return true;
}
