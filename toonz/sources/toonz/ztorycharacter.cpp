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
  if (!QFile::exists(sidecar))
    return fail(QObject::tr("%1 has no Ztoryc file to change")
                    .arg(QFileInfo(scenePath).fileName()));

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

  // Se il sidecar c'e' gia' si cambia il solo ruolo, senza riscrivere il file:
  // potrebbe contenere roba che non conosciamo, e buttarla via per dichiarare
  // un ruolo sarebbe un prezzo assurdo.
  if (QFile::exists(sidecar)) return setRole(scenePath, "character", error);

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
