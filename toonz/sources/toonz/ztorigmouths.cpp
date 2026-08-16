#include "ztorigmouths.h"

#include "tapp.h"
#include "ztorycharacter.h"
#include "ztorymodel.h"

#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/levelset.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshcolumn.h"
#include "toonz/tstageobject.h"
#include "toonz/txshcell.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/icongenerator.h"

#include <QHash>
#include <QSet>

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalMapper>
#include <QMouseEvent>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

namespace {

//! Le etichette dei dieci viseme, nell'ordine di ZtoryMouthMap::kShapes.
//! Sono quelle che l'animatore riconosce (Preston Blair), non le sigle
//! interne: «M B P» dice cosa disegnare, «mbp» no.
const char *const kLabels[10] = {"A I", "E",     "O",  "U",  "F V",
                                 "L",   "M B P", "W Q", "etc", "Rest"};

//! Dimensione di partenza dei riquadri. Non e' un limite: c'e' lo zoom.
const QSize kThumbBase(92, 72);

//! Rende il disegno in GRANDE e ne trova il riquadro utile.
//!
//! Il render sta a parte dall'inquadratura perche' trascinare deve costare un
//! ritaglio, non dieci render: a 30 volte al secondo si vedrebbe.
//!
//! Il riquadro utile e' il ritaglio automatico sul disegno. Senza, la bocca si
//! vede «piccolissima» (Franco, 2026-08-16): il livello e' grande quanto la
//! camera e la bocca ne occupa un angolo. Serve come INQUADRATURA DI PARTENZA,
//! poi la aggiusta l'utente.
void renderSource(TXshLevel *level, const TFrameId &fid, const QSize &thumb,
                  QImage *outImg, QRect *outBox) {
  *outImg = QImage();
  *outBox = QRect();
  if (!level) return;
  // Si zooma DENTRO, quindi servono pixel veri anche ingrandendo — ma non
  // troppi, e per le SOTTO-SCENE ancora meno.
  //
  // ⚠️ Per un child level l'id di cache e' `sub:<ptr>_<riga>`: NON contiene la
  // dimensione ne' il suffisso, quindi la nostra immagine e quella delle
  // miniature dell'xsheet sono la STESSA voce. Chiederla enorme costa il
  // render a tutti, e occupa quella memoria per ogni fotogramma della
  // sotto-scena. Un livello semplice invece ha il suo id (il suffisso
  // «_ztorigmouth»), quindi li' si puo' essere generosi.
  const int mult = level->getChildLevel() ? 3 : 6;
  const TDimension big(thumb.width() * mult, thumb.height() * mult);
  QPixmap pm =
      IconGenerator::instance()->getSizedIcon(level, fid, "_ztorigmouth", big);
  // Nullo = render ancora in coda: ripassera' iconGenerated().
  if (pm.isNull()) return;

  QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
  int x0 = img.width(), y0 = img.height(), x1 = -1, y1 = -1;
  for (int y = 0; y < img.height(); y++) {
    const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
    for (int x = 0; x < img.width(); x++) {
      // Soglia bassa: l'antialiasing del bordo va TENUTO, o si taglia il
      // contorno della bocca e resta una macchia.
      if (qAlpha(line[x]) < 8) continue;
      if (x < x0) x0 = x;
      if (x > x1) x1 = x;
      if (y < y0) y0 = y;
      if (y > y1) y1 = y;
    }
  }
  if (x1 < x0 || y1 < y0) {
    // Niente trasparenza: e' il caso delle SOTTO-SCENE, rese su fondo bianco
    // pieno. Senza un secondo tentativo la bocca resta un puntino in mezzo al
    // fotogramma (Franco, 2026-08-16: «le bocche si vedono piccolissime»).
    // Si riprova cercando cio' che NON e' quasi-bianco: su un disegno al
    // tratto e' esattamente il disegno.
    x0 = img.width(); y0 = img.height(); x1 = -1; y1 = -1;
    for (int y = 0; y < img.height(); y++) {
      const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
      for (int x = 0; x < img.width(); x++) {
        const QRgb p = line[x];
        if (qAlpha(p) < 8) continue;
        // Soglia alta: si vuole scartare il fondo, non le parti chiare del
        // disegno. Un grigio a 240 e' ancora segno.
        if (qRed(p) > 246 && qGreen(p) > 246 && qBlue(p) > 246) continue;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
      }
    }
  }

  QRect box;
  if (x1 < x0 || y1 < y0) {
    // Nemmeno cosi': immagine vuota o tutta di un colore. Si mostra intera.
    box = img.rect();
  } else {
    box = QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    const int pad = qMax(2, qMin(box.width(), box.height()) / 12);
    box.adjust(-pad, -pad, pad, pad);
    box = box.intersected(img.rect());
  }
  *outImg = img;
  *outBox = box;
}

//! Applica inquadratura (zoom + spostamento) a una sorgente gia' resa.
QPixmap framed(const QImage &img, const QRect &base, double zoom,
               const QPointF &pan, const QSize &thumb) {
  if (img.isNull() || base.isEmpty()) return QPixmap();
  // Zoom = si guarda una porzione PIU' PICCOLA del riquadro di partenza,
  // ingrandita fino al riquadro: e' il contenuto che cresce, non il box.
  QSizeF s(base.width() / zoom, base.height() / zoom);
  QPointF centre(base.center().x() + pan.x(), base.center().y() + pan.y());
  QRect view(qRound(centre.x() - s.width() / 2),
             qRound(centre.y() - s.height() / 2),
             qMax(1, qRound(s.width())), qMax(1, qRound(s.height())));
  // Fuori dall'immagine non c'e' niente da mostrare: si riporta dentro invece
  // di restituire una banda vuota che sembrerebbe un disegno mancante.
  view = view.intersected(img.rect());
  if (view.isEmpty()) return QPixmap();
  return QPixmap::fromImage(img.copy(view))
      .scaled(thumb, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

//! I nomi di colonna di TUTTI i livelli, in una sola passata sull'albero.
//!
//! Prima si chiamava columnNameIn() una volta per livello, e ognuna riscendeva
//! l'intero albero: su uno storyboard con molti shot diventa il quadrato del
//! lavoro necessario, e si sentiva uscendo da una sotto-scena.
void collectColumnNames(TXsheet *xsh, int depth,
                        QHash<TXshLevel *, QString> &out,
                        QSet<TXshLevel *> &visited) {
  if (!xsh || depth > 8) return;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *c = xsh->getColumn(col);
    if (!c || c->isEmpty()) continue;
    TStageObject *so = xsh->getStageObject(xsh->getColumnObjectId(col));
    QString colName  = so ? QString::fromStdString(so->getName()) : QString();
    // Un nome di default («Col1») non dice niente piu' del nome del livello.
    if (colName.startsWith("Col")) colName.clear();

    int r0 = 0, r1 = 0;
    c->getRange(r0, r1);
    TXshLevel *lastSeen = nullptr;
    for (int r = r0; r <= r1; r++) {
      const TXshCell cell = xsh->getCell(r, col);
      if (cell.isEmpty() || !cell.m_level) continue;
      TXshLevel *lv = cell.m_level.getPointer();
      // Una colonna espone quasi sempre lo stesso livello per molte celle di
      // fila: rifare il lavoro a ogni riga non aggiunge niente.
      if (lv == lastSeen) continue;
      lastSeen = lv;

      if (!colName.isEmpty() && !out.contains(lv)) out.insert(lv, colName);

      // ⚠️ UNA VOLTA SOLA PER SOTTO-SCENA, non una per cella.
      //
      // Prima si ricorreva a ogni cella: uno shot esposto per 100 fotogrammi
      // faceva entrare 100 volte nella sua sotto-scena, e annidato si
      // moltiplicava. Su uno storyboard vero questo caricava la scena in un
      // minuto e mezzo, tutto dentro questa funzione (misurato con `sample`,
      // 2026-08-16 — non dedotto).
      if (TXshChildLevel *cl = lv->getChildLevel()) {
        if (visited.contains(lv)) continue;
        visited.insert(lv);
        collectColumnNames(cl->getXsheet(), depth + 1, out, visited);
      }
    }
  }
}

}  // namespace

//============================================================================

ZtoRigMouthsTab::ZtoRigMouthsTab(QWidget *parent) : QWidget(parent) {
  auto *lay = new QVBoxLayout(this);
  lay->setContentsMargins(4, 4, 4, 4);
  lay->setSpacing(4);

  // ── riga del livello ──────────────────────────────────────────────────
  {
    auto *row = new QHBoxLayout();
    row->setSpacing(4);
    row->addWidget(new QLabel(tr("Mouth level:"), this));
    m_levelCombo = new QComboBox(this);
    m_levelCombo->setToolTip(
        tr("Which level holds the mouths. Levels that already have a mapping\n"
           "are listed first and marked — the others are just the scene's\n"
           "levels, since nothing in the name says which one is the mouth."));
    row->addWidget(m_levelCombo, 1);
    lay->addLayout(row);
  }

  // ── riga del set ──────────────────────────────────────────────────────
  {
    auto *row = new QHBoxLayout();
    row->setSpacing(4);
    row->addWidget(new QLabel(tr("Set:"), this));
    m_setCombo = new QComboBox(this);
    m_setCombo->setToolTip(
        tr("A level can hold several sets — the same mouth drawn happy and\n"
           "sad, for instance. At lip sync time you only pick which one."));
    row->addWidget(m_setCombo, 1);
    m_newBt    = new QPushButton(tr("New"), this);
    m_saveBt   = new QPushButton(tr("Save"), this);
    m_deleteBt = new QPushButton(tr("Delete"), this);
    m_newBt->setToolTip(tr("Start a new set from the current assignment."));
    m_saveBt->setToolTip(
        tr("Write the ten boxes onto the level, next to it — like a palette.\n"
           "From then on the set travels with the character."));
    row->addWidget(m_newBt);
    row->addWidget(m_saveBt);
    row->addWidget(m_deleteBt);
    lay->addLayout(row);
  }

  m_emptyLabel = new QLabel(
      tr("No level in this scene to map.\n\n"
         "Open the character scene, then pick the level that holds its\n"
         "mouth drawings."),
      this);
  m_emptyLabel->setWordWrap(true);
  m_emptyLabel->setAlignment(Qt::AlignTop);
  lay->addWidget(m_emptyLabel);

  // ── zoom delle anteprime ──────────────────────────────────────────────
  // Quanto debba essere grande una bocca per riconoscerla dipende da come e'
  // disegnata e da quanto occupa del fotogramma: nessun valore fisso va bene
  // per tutti, quindi lo decide chi guarda.
  {
    auto *row = new QHBoxLayout();
    row->setSpacing(4);
    row->addWidget(new QLabel(tr("Zoom:"), this));
    m_zoom = new QSlider(Qt::Horizontal, this);
    m_zoom->setRange(100, 800);  // percentuale
    m_zoom->setValue(100);
    m_zoom->setToolTip(
        tr("Zoom INSIDE the boxes — the box stays put, the mouth gets bigger.\n"
           "Drag a box to move the framing; the wheel zooms too.\n"
           "One framing for all ten, so they can be compared."));
    row->addWidget(m_zoom, 1);
    lay->addLayout(row);
    connect(m_zoom, &QSlider::valueChanged, this,
            &ZtoRigMouthsTab::onZoomChanged);
  }

  // ── i dieci riquadri, 5 + 5 ───────────────────────────────────────────
  m_thumb      = kThumbBase;
  m_grid       = new QWidget(this);
  auto *g      = new QGridLayout(m_grid);
  g->setContentsMargins(0, 0, 0, 0);
  g->setSpacing(3);

  auto *mapper = new QSignalMapper(this);
  for (int i = 0; i < 10; i++) {
    const int row = (i / 5) * 4;
    const int col = i % 5;

    g->addWidget(new QLabel(tr(kLabels[i]), m_grid), row, col,
                 Qt::AlignHCenter);

    m_preview[i] = new QLabel(m_grid);
    m_preview[i]->setFixedSize(m_thumb);
    m_preview[i]->setFrameShape(QFrame::Box);
    m_preview[i]->setAlignment(Qt::AlignCenter);
    // Il tasto destro apre i bersagli sugli altri livelli: sta sul riquadro
    // perche' e' li' che si guarda la casella, non in un menu lontano.
    m_preview[i]->setContextMenuPolicy(Qt::CustomContextMenu);
    m_preview[i]->setProperty("slotIndex", i);
    connect(m_preview[i], &QWidget::customContextMenuRequested, this,
            &ZtoRigMouthsTab::onSlotContextMenu);
    m_preview[i]->installEventFilter(this);
    m_preview[i]->setCursor(Qt::OpenHandCursor);
    g->addWidget(m_preview[i], row + 1, col, Qt::AlignHCenter);

    m_caption[i] = new QLabel(m_grid);
    m_caption[i]->setAlignment(Qt::AlignCenter);
    QFont f = m_caption[i]->font();
    f.setPixelSize(10);
    m_caption[i]->setFont(f);
    g->addWidget(m_caption[i], row + 2, col, Qt::AlignHCenter);

    auto *navBox = new QWidget(m_grid);
    auto *nl     = new QHBoxLayout(navBox);
    nl->setContentsMargins(0, 0, 0, 0);
    nl->setSpacing(2);
    for (int d = 0; d < 2; d++) {
      const int id = i * 2 + d;
      m_nav[id]    = new QPushButton(d ? ">" : "<", navBox);
      m_nav[id]->setFixedWidth(28);
      m_nav[id]->setToolTip(d ? tr("Next drawing") : tr("Previous drawing"));
      nl->addWidget(m_nav[id]);
      connect(m_nav[id], SIGNAL(clicked()), mapper, SLOT(map()));
      mapper->setMapping(m_nav[id], id);
    }
    g->addWidget(navBox, row + 3, col, Qt::AlignHCenter);
  }
  connect(mapper, SIGNAL(mapped(int)), this, SLOT(onNavClicked(int)));

  // ⚠️ NIENTE area scorrevole. C'era da quando lo zoom ingrandiva i riquadri;
  // ora lo zoom e' INTERNO e i riquadri non cambiano mai misura, quindi la
  // zona scorrevole restava solo a rubare spazio e a mettere una barra dove
  // c'era posto per vedere tutte e dieci le anteprime insieme (Franco,
  // 2026-08-16). La griglia sta nel layout e si prende cio' che le serve.
  lay->addWidget(m_grid);

  m_note = new QLabel(this);
  m_note->setWordWrap(true);
  lay->addWidget(m_note);
  lay->addStretch(1);

  connect(m_levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ZtoRigMouthsTab::onLevelChanged);
  connect(m_setCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ZtoRigMouthsTab::onSetChanged);
  connect(m_newBt, &QPushButton::clicked, this, &ZtoRigMouthsTab::onNewSet);
  connect(m_saveBt, &QPushButton::clicked, this, &ZtoRigMouthsTab::onSaveSet);
  connect(m_deleteBt, &QPushButton::clicked, this,
          &ZtoRigMouthsTab::onDeleteSet);

  // Le icone arrivano in differita: quando ne e' pronta una si ridisegna.
  connect(IconGenerator::instance(), &IconGenerator::iconGenerated, this,
          &ZtoRigMouthsTab::onIconGenerated);

  // Lo xsheet cambia a raffica (ogni rinomina, ogni cella): si ricostruisce
  // una volta sola quando si e' fermato, o la scheda si ricostruirebbe cento
  // volte mentre l'utente digita il nome di una colonna.
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setSingleShot(true);
  m_refreshTimer->setInterval(250);
  connect(m_refreshTimer, &QTimer::timeout, this, &ZtoRigMouthsTab::rebuild);
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, &ZtoRigMouthsTab::onXsheetChanged);
  // ⚠️ xsheetChanged NON copre la rinomina di un livello: e' un'altra
  // notifica. Senza queste due, una sotto-scena rinominata restava nella
  // tendina col nome vecchio (Franco, 2026-08-16).
  connect(TApp::instance()->getCurrentLevel(),
          &TXshLevelHandle::xshLevelTitleChanged, this,
          &ZtoRigMouthsTab::onXsheetChanged);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::castChanged,
          this, &ZtoRigMouthsTab::onXsheetChanged);

  rebuild();
}

//----------------------------------------------------------------------------

QString ZtoRigMouthsTab::subSceneName() const {
  if (!m_level || !m_level->getChildLevel()) return QString();
  return QString::fromStdWString(m_level->getName());
}

TFilePath ZtoRigMouthsTab::ownerPath() const {
  if (!m_level) return TFilePath();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return TFilePath();
  // Una SOTTO-SCENA non ha un file: vive dentro il .tnz, quindi la sua mappa
  // sta accanto alla SCENA. Un livello semplice ce l'ha, e la mappa gli sta
  // accanto — decodificata, perche' il suo percorso e' `+extras/…` e il file
  // vero sta altrove.
  if (m_level->getChildLevel()) return scene->getScenePath();
  if (TXshSimpleLevel *sl = m_level->getSimpleLevel())
    return scene->decodeFilePath(sl->getPath());
  return TFilePath();
}

void ZtoRigMouthsTab::reloadFrames() {
  m_fids.clear();
  if (!m_level) return;
  if (TXshSimpleLevel *sl = m_level->getSimpleLevel()) {
    sl->getFids(m_fids);
    return;
  }
  if (TXshChildLevel *cl = m_level->getChildLevel()) {
    // Una sotto-scena non ha fid propri: i suoi «disegni» sono i fotogrammi
    // del sub-xsheet, 1-based come li espone la cella.
    TXsheet *xsh = cl->getXsheet();
    const int n  = xsh ? xsh->getFrameCount() : 0;
    for (int i = 1; i <= n; i++) m_fids.push_back(TFrameId(i));
  }
}

void ZtoRigMouthsTab::readCharacterFromScene(QString *uuid,
                                             QString *name) const {
  if (uuid) uuid->clear();
  if (name) name->clear();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  const QString tnz =
      QString::fromStdWString(scene->getScenePath().getWideString());
  if (tnz.isEmpty()) return;
  if (ZtoryCharacter::roleOf(tnz) != QLatin1String("character")) return;
  ZtoryCharacter::characterRef(tnz, uuid, name);
}

//----------------------------------------------------------------------------

void ZtoRigMouthsTab::rebuild() {
  // Il lavoro vero (leggere le mappe, scandire l'albero delle sotto-scene) non
  // si fa per un pannello che nessuno sta guardando: si segna e si recupera
  // allo showEvent.
  if (!isVisible()) {
    m_dirty = true;
    return;
  }
  m_dirty = false;
  refreshLevelCombo();
}

void ZtoRigMouthsTab::showEvent(QShowEvent *) {
  if (m_dirty) rebuild();
}

void ZtoRigMouthsTab::onXsheetChanged() {
  if (!isVisible()) {
    m_dirty = true;
    return;
  }
  m_refreshTimer->start();
}

void ZtoRigMouthsTab::onIconGenerated() {
  if (!isVisible()) return;  // nessuno le sta guardando
  // Non si ricostruisce niente: si rirende e basta. Le icone arrivano una per
  // volta e una ricostruzione per ognuna resetterebbe la scelta corrente.
  refreshSources();
}

void ZtoRigMouthsTab::onZoomChanged(int value) {
  m_viewZoom = value / 100.0;
  refreshPreviews();  // solo ritaglio: le sorgenti non cambiano
}

//----------------------------------------------------------------------------

bool ZtoRigMouthsTab::eventFilter(QObject *obj, QEvent *event) {
  // Trascinare un riquadro sposta l'inquadratura di TUTTI: e' una sola, e
  // spostarne uno solo li renderebbe non confrontabili.
  if (event->type() == QEvent::MouseButtonPress) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
      m_dragging    = true;
      m_dragFrom    = me->pos();
      m_dragPanFrom = m_viewPan;
      return true;
    }
  } else if (event->type() == QEvent::MouseMove && m_dragging) {
    auto *me = static_cast<QMouseEvent *>(event);
    // Lo spostamento e' in pixel del RIQUADRO: va convertito in pixel della
    // sorgente, o trascinando a zoom alto la bocca schizzerebbe via.
    double scale = 1.0;
    for (int i = 0; i < 10; i++)
      if (!m_srcBox[i].isEmpty()) {
        scale = double(m_srcBox[i].width()) / (m_thumb.width() * m_viewZoom);
        break;
      }
    const QPoint d = me->pos() - m_dragFrom;
    m_viewPan      = m_dragPanFrom - QPointF(d.x() * scale, d.y() * scale);
    refreshPreviews();
    return true;
  } else if (event->type() == QEvent::MouseButtonRelease) {
    m_dragging = false;
    return true;
  } else if (event->type() == QEvent::Wheel) {
    auto *we      = static_cast<QWheelEvent *>(event);
    const int step = we->angleDelta().y() > 0 ? 25 : -25;
    m_zoom->setValue(m_zoom->value() + step);  // passa da onZoomChanged
    return true;
  }
  return QWidget::eventFilter(obj, event);
}

void ZtoRigMouthsTab::refreshLevelCombo() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  // ── SI FA QUALCOSA SOLO SE E' CAMBIATO QUALCOSA ────────────────────────
  // La firma costa una passata in memoria sui nomi dei livelli. Il lavoro che
  // evita costa una scansione ricorsiva dell'albero piu' un accesso a file per
  // livello, su un volume esterno. Navigare fra sotto-scene non tocca nessuno
  // dei due, quindi qui si esce subito.
  QString sig;
  if (scene) {
    sig = QString::number(quintptr(scene));
    TLevelSet *ls = scene->getLevelSet();
    sig += "/" + QString::number(ls->getLevelCount());
    for (int i = 0; i < ls->getLevelCount(); i++)
      if (TXshLevel *lv = ls->getLevel(i))
        sig += "|" + QString::fromStdWString(lv->getName());
  }
  if (sig == m_levelsSignature && m_levelCombo->count() > 0) return;
  // Cambiati i livelli, cio' che si sapeva dei loro file non vale piu'.
  if (sig != m_levelsSignature) m_mapCache.clear();
  m_levelsSignature = sig;

  m_filling = true;
  const QString keep = m_levelCombo->currentData().toString();
  m_levelCombo->clear();

  QVector<QPair<QString, TXshLevel *>> mapped, others;
  if (scene) {
    // UNA passata per i nomi di colonna, e UNA lettura del file di scena.
    // Prima si rileggeva e riparsificava lo stesso .zmouth una volta per
    // sotto-scena, e si riscendeva l'albero una volta per livello: su una scena
    // vera diventava il lavoro al quadrato.
    QHash<TXshLevel *, QString> colNames;
    QSet<TXshLevel *> visitedSubs;
    collectColumnNames(scene->getChildStack()->getTopXsheet(), 0, colNames,
                       visitedSubs);

    // DOVE stanno i set: la stessa ricerca del popup di «Apply», non una
    // seconda copia. Copre l'annidamento e il recupero dei set arrivati con un
    // personaggio importato, quindi il pallino compare anche nello shot.
    m_sources = ZtoryMouthApply::findTargets(scene);
    QSet<TXshLevel *> withSets;
    for (const MouthApplyTarget &t : m_sources) withSets.insert(t.level);

    TLevelSet *ls = scene->getLevelSet();
    for (int i = 0; i < ls->getLevelCount(); i++) {
      TXshLevel *lv = ls->getLevel(i);
      if (!lv) continue;
      // Livelli semplici E SOTTO-SCENE: un labiale costruito con piu' livelli
      // sta in una sotto-scena, ed e' un modo di lavorare, non un caso strano.
      TXshSimpleLevel *sl = lv->getSimpleLevel();
      TXshChildLevel  *cl = lv->getChildLevel();
      if (!sl && !cl) continue;
      const QString name = QString::fromStdWString(lv->getName());
      // Si mostra il nome della COLONNA quando c'e' (e' quello che l'utente
      // ha scritto e che vede nell'xsheet), col nome del livello accanto —
      // che resta la chiave della mappa, quindi nasconderlo del tutto
      // renderebbe incomprensibile un set che «non si trova piu'».
      const QString colName = colNames.value(lv);
      QString label = colName.isEmpty() ? name
                                        : QString("%1  [%2]").arg(colName, name);
      if (cl) label = tr("%1  (sub-scene)").arg(label);

      const bool hasMap = withSets.contains(lv);
      if (hasMap)
        mapped.append(qMakePair(label, lv));
      else
        others.append(qMakePair(label, lv));
    }
  }

  // I livelli gia' mappati per primi, col pallino: sono l'unica informazione
  // certa su quale sia la bocca. Il resto e' l'elenco della scena, perche' i
  // nomi (CH_nome#7#group) non dicono niente e indovinare sarebbe peggio.
  for (const auto &p : mapped)
    m_levelCombo->addItem("● " + p.first,
                          QVariant::fromValue(quintptr(p.second)));
  for (const auto &p : others)
    m_levelCombo->addItem(p.first, QVariant::fromValue(quintptr(p.second)));
  m_filling = false;

  const bool any = m_levelCombo->count() > 0;
  m_emptyLabel->setVisible(!any);
  m_grid->setVisible(any);
  m_setCombo->setEnabled(any);
  m_newBt->setEnabled(any);
  m_saveBt->setEnabled(any);

  if (!any) {
    m_level = nullptr;
    return;
  }
  // Quale livello proporre, in ordine di quanto e' probabile che sia quello
  // giusto:
  //   1. quello di prima, se c'e' ancora — non si cambia sotto le mani;
  //   2. quello corrente nell'xsheet SE E' GIA' MAPPATO — ci stai lavorando ed
  //      e' delle bocche, non c'e' candidato migliore;
  //   3. il primo MAPPATO. Aprire su un livello qualsiasi mentre uno ha gia' i
  //      suoi set costringe a cercarlo ogni volta: il pallino lo indica, ma
  //      indicarlo e non sceglierlo e' meta' lavoro (Franco, 2026-08-16);
  //   4. quello corrente, o il primo.
  const int mappedCount = mapped.size();
  int idx = keep.isEmpty() ? -1 : m_levelCombo->findData(keep);
  int currentIdx = -1;
  if (TXshLevelHandle *lh = TApp::instance()->getCurrentLevel())
    if (TXshLevel *cur = lh->getLevel())
      currentIdx = m_levelCombo->findData(QVariant::fromValue(quintptr(cur)));
  if (idx < 0) {
    // I mappati stanno in testa all'elenco, quindi «indice < mappedCount»
    // vuol dire «ha una mappa».
    if (currentIdx >= 0 && currentIdx < mappedCount) idx = currentIdx;
    else if (mappedCount > 0) idx = 0;
    else idx = currentIdx;
  }
  m_filling = true;
  m_levelCombo->setCurrentIndex(idx >= 0 ? idx : 0);
  m_filling = false;

  // ⚠️ Si RICARICA solo se il livello e' davvero cambiato. Questa funzione ora
  // gira anche a ogni modifica dello xsheet (rinomina di una colonna, nuova
  // sotto-scena): ricaricare comunque azzererebbe l'assegnazione in corso e non
  // ancora salvata, che e' il lavoro che si stava facendo.
  TXshLevel *selected = reinterpret_cast<TXshLevel *>(
      m_levelCombo->currentData().value<quintptr>());
  if (selected != m_level)
    onLevelChanged(m_levelCombo->currentIndex());
  else {
    // Stesso livello: i fotogrammi pero' possono essere cambiati (una
    // sotto-scena si allunga mentre ci si lavora), quindi si rilegge quello.
    reloadFrames();
    refreshSources();
  }
}

//----------------------------------------------------------------------------

void ZtoRigMouthsTab::onLevelChanged(int) {
  if (m_filling) return;
  m_level = reinterpret_cast<TXshLevel *>(
      m_levelCombo->currentData().value<quintptr>());
  m_fids.clear();
  m_map = MouthMap();
  for (int i = 0; i < 10; i++) {
    m_targets[i].clear();
    m_src[i]      = QImage();
    m_srcLevel[i] = nullptr;
    m_srcFid[i]   = TFrameId();
  }
  m_currentSetName.clear();

  if (m_level) {
    reloadFrames();
    QString why;
    // «Non c'e'» e' il caso normale: quasi nessun livello e' bocche. Si parte
    // da una mappa vuota senza dire niente, e il primo Salva la crea.
    ZtoryMouthMap::load(ownerPath(), subSceneName(), m_map, &why);

    // ── I SET ARRIVATI COL PERSONAGGIO ───────────────────────────────────
    // La mappa di una sotto-scena vive accanto alla scena che la contiene.
    // Importando il personaggio in uno shot arriva la sotto-scena ma non la
    // mappa: quella resta accanto alla scena di libreria. Si ritrova per la
    // catena asset Character -> scena -> mappa, agganciandosi al NOME della
    // sotto-scena, che l'import non cambia.
    //
    // ⚠️ Questo recupero c'era gia' nel popup di «Apply», e NON qui: percio'
    // l'apply trovava i set e la scheda diceva che non ce n'erano.
    if (m_map.sets.isEmpty() && !subSceneName().isEmpty()) {
      ZtoryModel *model = ZtoryModel::instance();
      for (const Asset &a : model->assets()) {
        if (a.type.compare("Character", Qt::CaseInsensitive) != 0) continue;
        const QString lib = model->resolveAssetFile(a);
        if (lib.isEmpty()) continue;
        MouthMap m;
        if (!ZtoryMouthMap::load(TFilePath(lib.toStdWString()),
                                 subSceneName(), m))
          continue;
        if (m.sets.isEmpty()) continue;
        m_map = m;
        m_note->setText(tr("Sets read from %1 — saving writes them here, "
                           "on this scene.").arg(a.name));
        break;
      }
    }
  }
  refreshSetCombo();
}

void ZtoRigMouthsTab::refreshSetCombo() {
  m_filling = true;
  m_setCombo->clear();

  // ── TUTTI i set della scena, non solo quelli del livello scelto ────────
  // Franco (2026-08-16): «nella tendina dove si selezionano i set si vedono
  // tutti e quando selezioni un set il livello si seleziona automaticamente
  // senza doverlo cercare». Ha ragione: si sa quale set si vuole, non su quale
  // livello sta — chiedere prima il livello e' far rispondere a una domanda
  // che non ci si e' posti.
  //
  // Il livello resta scegliibile sopra, perche' serve per MAPPARNE uno nuovo,
  // che di set non ne ha ancora e quindi qui non comparirebbe.
  const bool several = m_sources.size() > 1;
  for (const MouthApplyTarget &src : m_sources) {
    for (const MouthSet &ms : src.map.sets) {
      QStringList attrs;
      if (!ms.view.isEmpty()) attrs << ms.view;
      if (!ms.expression.isEmpty()) attrs << ms.expression;
      if (!ms.variant.isEmpty()) attrs << ms.variant;
      QString label = attrs.isEmpty()
                          ? ms.name
                          : QString("%1  (%2)").arg(ms.name, attrs.join(", "));
      // Il livello si nomina solo se ce n'e' piu' d'uno con dei set: con uno
      // solo sarebbe rumore ripetuto su ogni riga.
      if (several) label += QString("  —  %1").arg(src.label);
      // Chi lo possiede viaggia con la voce: e' cosi' che scegliere il set
      // porta anche il livello.
      m_setCombo->addItem(label,
                          QVariant::fromValue(quintptr(src.level)));
      m_setCombo->setItemData(m_setCombo->count() - 1, ms.name, Qt::UserRole + 1);
    }
  }
  m_filling = false;

  // ⚠️ SOLO un set DI QUESTO LIVELLO puo' essere selezionato da qui.
  //
  // La tendina contiene i set di tutta la scena, ed e' voluto: scegliere un
  // set porta al suo livello. Ma il viaggio deve partire da un clic
  // dell'utente. Selezionando d'ufficio il primo della lista, un livello
  // ancora SENZA set veniva rimbalzato subito sul livello che possiede quel
  // primo set — e non si riusciva piu' a scegliere un livello per mapparlo da
  // zero (Franco, 2026-08-16).
  int pick = -1;
  for (int i = 0; i < m_setCombo->count(); i++)
    if (reinterpret_cast<TXshLevel *>(
            m_setCombo->itemData(i).value<quintptr>()) == m_level) {
      pick = i;
      break;
    }
  m_deleteBt->setEnabled(pick >= 0);
  if (pick >= 0) {
    m_filling = true;
    m_setCombo->setCurrentIndex(pick);
    m_filling = false;
    const QString name =
        m_setCombo->itemData(pick, Qt::UserRole + 1).toString();
    const int idx = m_map.indexOfSet(name);
    if (idx >= 0) loadSet(idx);
    return;
  }

  // Questo livello non ha set: la tendina resta senza selezione e si parte dai
  // primi dieci disegni, che e' il punto di partenza naturale per mapparlo.
  m_filling = true;
  m_setCombo->setCurrentIndex(-1);
  m_filling = false;
  for (int i = 0; i < 10; i++) {
    m_targets[i].clear();
    if (i < (int)m_fids.size()) {
      MouthTarget t;
      t.frameId = m_fids[i];
      m_targets[i] << t;
    }
  }
  m_currentSetName.clear();
  refreshSources();
}

//----------------------------------------------------------------------------

//! L'indice della voce che porta il set chiamato \p name sul livello corrente,
//! o -1.
//!
//! ⚠️ Nella tendina dei set il dato principale e' ora il LIVELLO (e' cosi' che
//! scegliere un set porta con se' dove sta); il nome del set e' in
//! Qt::UserRole + 1. Cercarlo con findData() prenderebbe il campo sbagliato e
//! non troverebbe mai niente.
int ZtoRigMouthsTab::setRow(const QString &name) const {
  for (int i = 0; i < m_setCombo->count(); i++) {
    if (m_setCombo->itemData(i, Qt::UserRole + 1).toString() != name) continue;
    if (reinterpret_cast<TXshLevel *>(m_setCombo->itemData(i).value<quintptr>())
        == m_level)
      return i;
  }
  return -1;
}

void ZtoRigMouthsTab::selectLevel(TXshLevel *level) {
  if (!level || level == m_level) return;
  m_level = level;
  reloadFrames();
  for (int i = 0; i < 10; i++) {
    m_src[i]      = QImage();
    m_srcLevel[i] = nullptr;
    m_srcFid[i]   = TFrameId();
  }
  // La tendina dei livelli segue, cosi' si vede DOVE si e' finiti — senza
  // rilanciare onLevelChanged, che ricaricherebbe la mappa e ributterebbe via
  // la scelta appena fatta.
  m_filling  = true;
  const int i = m_levelCombo->findData(QVariant::fromValue(quintptr(level)));
  if (i >= 0) m_levelCombo->setCurrentIndex(i);
  m_filling = false;
}

void ZtoRigMouthsTab::onSetChanged(int index) {
  if (m_filling) return;
  if (index < 0 || index >= m_setCombo->count()) return;

  TXshLevel *owner = reinterpret_cast<TXshLevel *>(
      m_setCombo->itemData(index).value<quintptr>());
  const QString name = m_setCombo->itemData(index, Qt::UserRole + 1).toString();

  // Scegliere un set porta con se' il suo livello: e' il punto di tutta
  // l'inversione.
  if (owner && owner != m_level) {
    selectLevel(owner);
    // La mappa in lavorazione e' quella del livello dove si e' finiti.
    for (const MouthApplyTarget &src : m_sources)
      if (src.level == owner) { m_map = src.map; break; }
  }

  const int idx = m_map.indexOfSet(name);
  if (idx < 0) {
    // Set nuovo, non ancora nel file: le caselle restano quelle che sono.
    m_currentSetName = name;
    return;
  }
  loadSet(idx);
}

//----------------------------------------------------------------------------

void ZtoRigMouthsTab::loadSet(int index) {
  if (index < 0 || index >= m_map.sets.size()) return;
  const MouthSet &ms = m_map.sets[index];
  m_currentSetName   = ms.name;
  for (int i = 0; i < 10; i++) m_targets[i] = ms.mouths[i];
  m_filling = true;
  m_setCombo->setCurrentIndex(index);
  m_filling = false;
  // Rirendere, non solo riquadrare: cambiare set cambia tutti e dieci i
  // fotogrammi, quindi le sorgenti non sono piu' quelle.
  refreshSources();
}

//----------------------------------------------------------------------------

MouthTarget *ZtoRigMouthsTab::anchorTarget(int i) {
  if (i < 0 || i >= 10) return nullptr;
  for (MouthTarget &t : m_targets[i])
    if (t.isAnchorLevel() && !t.isPose()) return &t;
  return nullptr;
}

void ZtoRigMouthsTab::refreshSources() {
  for (int i = 0; i < 10; i++) {
    const MouthTarget *anchor = anchorTarget(i);
    if (!m_level || !anchor) {
      m_src[i]      = QImage();
      m_srcBox[i]   = QRect();
      m_srcLevel[i] = nullptr;
      m_srcFid[i]   = TFrameId();
      continue;
    }
    // Gia' resa e ancora buona: non si richiede. E' la differenza fra dieci
    // render e una coda che non finisce piu' — vedi il commento su m_srcLevel.
    if (!m_src[i].isNull() && m_srcLevel[i] == m_level &&
        m_srcFid[i] == anchor->frameId)
      continue;

    renderSource(m_level, anchor->frameId, m_thumb, &m_src[i], &m_srcBox[i]);
    // Si segna cosa si e' ottenuto SOLO se e' arrivato qualcosa: se il render
    // e' ancora in coda, m_src resta nullo e iconGenerated ripassera'.
    if (!m_src[i].isNull()) {
      m_srcLevel[i] = m_level;
      m_srcFid[i]   = anchor->frameId;
    }
  }
  refreshPreviews();
}

void ZtoRigMouthsTab::refreshPreviews() {
  for (int i = 0; i < 10; i++) {
    const MouthTarget *anchor = anchorTarget(i);

    QPixmap pm = framed(m_src[i], m_srcBox[i], m_viewZoom, m_viewPan, m_thumb);
    if (pm.isNull()) {
      // Sorgente non ancora pronta (il render e' in coda) oppure casella
      // vuota: riquadro grigio, e iconGenerated() ripassera'.
      QPixmap blank(m_thumb);
      blank.fill(QColor(60, 60, 60));
      pm = blank;
    }
    m_preview[i]->setPixmap(pm);

    // La didascalia dice DOVE si e' finiti: il fotogramma sull'ancora, e
    // quanti bersagli in piu' ci sono su altri livelli. Senza il conteggio, un
    // bersaglio sui denti sarebbe invisibile e si crederebbe di averlo perso.
    QString cap;
    if (anchor)
      cap = tr("f%1").arg(anchor->frameId.getNumber());
    else if (!m_targets[i].isEmpty())
      cap = tr("(no drawing)");
    else
      cap = "—";
    const int extra = m_targets[i].size() - (anchor ? 1 : 0);
    if (extra > 0) cap += tr(" +%1").arg(extra);
    m_caption[i]->setText(cap);

    const bool navUsable = m_level && !m_fids.empty();
    m_nav[i * 2]->setEnabled(navUsable);
    m_nav[i * 2 + 1]->setEnabled(navUsable);
  }
}

//----------------------------------------------------------------------------

void ZtoRigMouthsTab::onNavClicked(int id) {
  if (!m_level || m_fids.empty()) return;
  const int slot      = id / 2;
  const int direction = (id % 2) ? 1 : -1;

  MouthTarget *anchor = anchorTarget(slot);
  if (!anchor) {
    // Casella senza disegno sull'ancora: la freccia la CREA sul primo
    // fotogramma, invece di non fare niente e sembrare rotta.
    MouthTarget t;
    t.frameId = m_fids.front();
    m_targets[slot] << t;
  } else {
    auto it = std::find(m_fids.begin(), m_fids.end(), anchor->frameId);
    int idx = (it == m_fids.end()) ? 0 : int(std::distance(m_fids.begin(), it));
    idx     = (idx + direction + int(m_fids.size())) % int(m_fids.size());
    anchor->frameId = m_fids[idx];
  }
  refreshSources();
}

//----------------------------------------------------------------------------

void ZtoRigMouthsTab::onSlotContextMenu(const QPoint &pos) {
  auto *w = qobject_cast<QWidget *>(sender());
  if (!w) return;
  const int slot = w->property("slotIndex").toInt();
  if (slot < 0 || slot >= 10) return;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;

  QMenu menu(this);
  menu.addAction(tr("Targets for %1").arg(tr(kLabels[slot])))->setEnabled(false);
  menu.addSeparator();

  // I bersagli gia' presenti su ALTRI livelli, per toglierli.
  for (int k = 0; k < m_targets[slot].size(); k++) {
    const MouthTarget &t = m_targets[slot][k];
    if (t.isAnchorLevel()) continue;
    QAction *a = menu.addAction(
        tr("Remove  %1  f%2").arg(t.levelName).arg(t.frameId.getNumber()));
    a->setData(QVariantList{QString("remove"), k});
  }

  // Aggiungerne uno: gli altri livelli della scena, quelli veri.
  QMenu *add = menu.addMenu(tr("Add target on…"));
  TLevelSet *ls = scene->getLevelSet();
  for (int i = 0; i < ls->getLevelCount(); i++) {
    TXshLevel *lv = ls->getLevel(i);
    if (!lv) continue;
    TXshSimpleLevel *sl = lv->getSimpleLevel();
    if (!sl || sl == m_level) continue;  // l'ancora si edita con le frecce
    QAction *a = add->addAction(QString::fromStdWString(sl->getName()));
    a->setData(QVariantList{QString("add"),
                            QString::fromStdWString(sl->getName())});
  }
  if (add->isEmpty()) add->setEnabled(false);

  QAction *chosen = menu.exec(w->mapToGlobal(pos));
  if (!chosen) return;
  const QVariantList d = chosen->data().toList();
  if (d.size() != 2) return;

  if (d[0].toString() == "remove") {
    m_targets[slot].remove(d[1].toInt());
  } else {
    // Il fotogramma si chiede: su un altro livello non c'e' un'anteprima da
    // scorrere qui, e inventarne uno darebbe un disegno a caso.
    bool ok = false;
    const int frame = QInputDialog::getInt(
        this, tr("Add Target"),
        tr("Frame on %1:").arg(d[1].toString()), 1, 1, 9999, 1, &ok);
    if (!ok) return;
    MouthTarget t;
    t.levelName = d[1].toString();
    t.frameId   = TFrameId(frame);
    m_targets[slot] << t;
  }
  refreshPreviews();
}

//----------------------------------------------------------------------------

void ZtoRigMouthsTab::onNewSet() {
  bool ok = false;
  const QString name = QInputDialog::getText(
      this, tr("New Mouth Set"),
      tr("Name this set — front, profile, happy, sad…"), QLineEdit::Normal,
      QString(), &ok);
  if (!ok || name.trimmed().isEmpty()) return;
  m_currentSetName = name.trimmed();

  // Il nome nuovo deve COMPARIRE nella tendina. Lasciarci quello di prima
  // direbbe che si sta modificando l'altro set, ed e' falso: i prossimi
  // ritocchi e il Salva vanno su questo.
  const int existing = setRow(m_currentSetName);
  m_filling          = true;
  if (existing >= 0) {
    // Nome gia' in uso: non si crea un doppione, si va su quello — e infatti
    // Salva lo aggiornera'.
    m_setCombo->setCurrentIndex(existing);
  } else {
    // Marcato finche' non e' su disco, cosi' si vede che manca il Salva.
    m_setCombo->addItem(tr("%1  (not saved)").arg(m_currentSetName),
                        QVariant::fromValue(quintptr(m_level)));
    m_setCombo->setItemData(m_setCombo->count() - 1, m_currentSetName,
                            Qt::UserRole + 1);
    m_setCombo->setCurrentIndex(m_setCombo->count() - 1);
  }
  m_filling = false;

  // Le caselle NON si azzerano: quasi sempre un set nuovo nasce da uno che
  // c'era gia' («come il frontale, ma triste»), e ricominciare da zero sarebbe
  // dieci assegnazioni buttate.
  m_note->setText(tr("New set «%1» — adjust the drawings, then Save.")
                      .arg(m_currentSetName));
}

void ZtoRigMouthsTab::onSaveSet() {
  if (!m_level) return;
  if (m_currentSetName.isEmpty()) {
    onNewSet();
    if (m_currentSetName.isEmpty()) return;
  }

  MouthSet ms;
  ms.name     = m_currentSetName;
  // Marca a quale sotto-scena appartiene: e' cio' che tiene separati i set di
  // «testa» da quelli di «testa_profilo» dentro lo stesso file di scena.
  ms.subScene = subSceneName();
  for (int i = 0; i < 10; i++) ms.mouths[i] = m_targets[i];
  if (!ms.isUsable()) {
    DVGui::warning(tr("This set has no drawing assigned — nothing to save."));
    return;
  }
  // Gli attributi si ricavano dal nome invece di chiedere quattro campi che si
  // compilerebbero a caso. Scrivere «profilo triste» basta a ritrovarlo, ed e'
  // come i set si chiamano davvero.
  const QString low = ms.name.toLower();
  if (low.contains("profil")) ms.view = "profile";
  else if (low.contains("3-4") || low.contains("three")) ms.view = "threequarter";
  else if (low.contains("front")) ms.view = "front";
  if (low.contains("felic") || low.contains("happy")) ms.expression = "happy";
  else if (low.contains("trist") || low.contains("sad")) ms.expression = "sad";

  const int existing = m_map.indexOfSet(ms.name);
  if (existing >= 0)
    m_map.sets[existing] = ms;
  else
    m_map.sets.push_back(ms);

  QString uuid, name;
  readCharacterFromScene(&uuid, &name);
  if (!uuid.isEmpty()) m_map.characterUuid = uuid;
  if (!name.isEmpty()) m_map.characterName = name;

  QString why;
  if (!ZtoryMouthMap::save(ownerPath(), subSceneName(), m_map, &why)) {
    DVGui::warning(tr("Could not save the mouth set: %1").arg(why));
    return;
  }
  // ⚠️ La firma che evita le ricostruzioni inutili guarda scena e nomi dei
  // livelli: salvando un set NON cambia nessuno dei due, quindi senza
  // invalidarla la scheda resta convinta di sapere gia' tutto e il set appena
  // salvato non compare (Franco, 2026-08-16). Un'ottimizzazione va invalidata
  // anche quando a cambiare i dati siamo noi.
  m_levelsSignature.clear();
  m_mapCache.clear();
  refreshLevelCombo();  // rilegge dove stanno i set, pallino compreso
  refreshSetCombo();    // ora l'elenco contiene quello nuovo

  m_filling = true;
  m_setCombo->setCurrentIndex(setRow(ms.name));
  m_filling = false;
  loadSet(m_map.indexOfSet(ms.name));
  m_note->setText(
      tr("Saved next to the level — it travels with the character."));
}

void ZtoRigMouthsTab::onDeleteSet() {
  const QString name =
      m_setCombo->itemData(m_setCombo->currentIndex(), Qt::UserRole + 1)
          .toString();
  const int idx = m_map.indexOfSet(name);
  // Un set mai salvato non sta nel file: si toglie solo dalla tendina.
  if (idx < 0) {
    const int row = setRow(name);
    if (row >= 0) {
      m_filling = true;
      m_setCombo->removeItem(row);
      m_filling = false;
    }
    m_currentSetName.clear();
    refreshSetCombo();
    return;
  }

  const int answer =
      DVGui::MsgBox(tr("Delete the mouth set «%1»?").arg(name), tr("Delete"),
                    tr("Cancel"), 1);
  if (answer != 1) return;  // 2 = Annulla, 0 = finestra chiusa

  m_map.sets.remove(idx);
  QString why;
  if (!ZtoryMouthMap::save(ownerPath(), subSceneName(), m_map, &why)) {
    DVGui::warning(tr("Could not update the level: %1").arg(why));
    return;
  }
  m_currentSetName.clear();
  // Stessa ragione del salvataggio: abbiamo scritto noi, la firma non lo sa.
  m_levelsSignature.clear();
  m_mapCache.clear();
  refreshLevelCombo();
  refreshSetCombo();
  m_note->setText(tr("Deleted."));
}
