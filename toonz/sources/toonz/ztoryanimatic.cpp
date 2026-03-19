
#include "ztoryanimatic.h"
#include "ztorymodel.h"
#include "toonz/tcolumnhandle.h"
#include "subscenecommand.h"
#include "toonzqt/menubarcommand.h"
#include "columncommand.h"
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/txsheet.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "iocommand.h"
#include "toonzqt/gutil.h"
#include "orientation.h"
#include <QPainter>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QMenu>
#include <QLabel>
#include <QFileDialog>
#include <QContextMenuEvent>

// ---- ZtoryAnimaticController ----

ZtoryAnimaticController::ZtoryAnimaticController() : QObject() {
  m_frameHandle = new TFrameHandle();
  m_frameHandle->setFrame(0);
}

ZtoryAnimaticController *ZtoryAnimaticController::instance() {
  static ZtoryAnimaticController ctrl;
  return &ctrl;
}

TXsheet *ZtoryAnimaticController::mainXsheet() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return nullptr;
  return scene->getChildStack()->getTopXsheet();
}

void ZtoryAnimaticController::setCurrentFrame(int frame) {
  if (frame < 0) frame = 0;
  m_frameHandle->setFrame(frame);
}

int ZtoryAnimaticController::currentFrame() const {
  return m_frameHandle->getFrame();
}

// ---- ZtoryAnimaticRuler ----

ZtoryAnimaticRuler::ZtoryAnimaticRuler(QWidget *parent) : QWidget(parent) {
  setFixedHeight(24);
  setMouseTracking(true);
}

void ZtoryAnimaticRuler::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(40, 40, 40));
  p.setPen(QColor(180, 180, 180));
  int w = width();
  for (int f = 0; f * m_ppf < w; f++) {
    int x = (int)(f * m_ppf);
    if (f % 24 == 0) {
      p.drawLine(x, 0, x, 16);
      p.drawText(x + 2, 14, QString::number(f));
    } else if (f % 6 == 0) {
      p.drawLine(x, 8, x, 16);
    }
  }
  // Playhead
  int px = (int)(m_currentFrame * m_ppf);
  p.setPen(QColor(255, 100, 0));
  p.drawLine(px, 0, px, height());
}

void ZtoryAnimaticRuler::mousePressEvent(QMouseEvent *e) {
  m_currentFrame = (int)(e->x() / m_ppf);
  update();
  emit frameChanged(m_currentFrame);
}

void ZtoryAnimaticRuler::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons() & Qt::LeftButton) {
    m_currentFrame = qMax(0, (int)(e->x() / m_ppf));
    update();
    emit frameChanged(m_currentFrame);
  }
}

// ---- ZtoryAudioTrack ----

ZtoryAudioTrack::ZtoryAudioTrack(int col, const QString &name, QWidget *parent)
    : QWidget(parent), m_col(col), m_name(name) {
  setFixedHeight(m_trackHeight);
  setMinimumWidth(100);
}

void ZtoryAudioTrack::paintEvent(QPaintEvent *) {
  QPainter p(this);
  int labelW = 80;

  // Sfondo
  p.fillRect(rect(), QColor(25, 25, 25));

  // Label colonna
  p.fillRect(0, 0, labelW, height(), QColor(40, 40, 40));
  p.setPen(QColor(200, 200, 200));
  p.setFont(QFont("Arial", 9));
  p.drawText(4, 0, labelW - 4, height(), Qt::AlignVCenter | Qt::AlignLeft, m_name);

  // Separatore verticale
  p.setPen(QColor(60, 60, 60));
  p.drawLine(labelW, 0, labelW, height());

  // Disegna waveform
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  TXshColumn *column = xsh->getColumn(m_col);
  if (!column) return;
  TXshSoundColumn *sc = column->getSoundColumn();
  if (!sc) return;

  int maxFrame = sc->getMaxFrame();
  if (maxFrame < 0) return;

  int trackH = height();
  int center = trackH / 2;
  int trackW = width() - labelW;
  const Orientation *o = Orientations::leftToRight();

  // Linea centrale
  p.setPen(QColor(70, 70, 70));
  p.drawLine(labelW, center, width(), center);

  // Waveform — scorriamo pixel per pixel nell'area track
  p.setPen(QColor(80, 200, 120));

  for (int px = 0; px < trackW; px++) {
    double frame = px / m_ppf;
    int row = (int)frame;
    if (row > maxFrame) break;

    TXshCell cell = sc->getSoundCell(row);
    if (cell.isEmpty()) continue;
    TXshSoundLevelP soundLevel = cell.getSoundLevel();
    if (!soundLevel) continue;

    // soundPixel = pixel dall'inizio del clip
    int offset = row - cell.getFrameId().getNumber();
    int soundPixel = (int)(frame / 1.0 * 1.0) - o->rowToFrameAxis(offset);
    // Semplificazione: usiamo px * ppf come pixel audio
    soundPixel = (int)(px);

    DoublePair minmax;
    soundLevel->getValueAtPixel(o, soundPixel, minmax);

    double pmin = minmax.first;
    double pmax = minmax.second;

    // Scala al centro della traccia
    int halfH = (trackH - 4) / 2;
    int yMin = center - (int)(pmax * halfH / 128.0);
    int yMax = center - (int)(pmin * halfH / 128.0);
    yMin = qBound(2, yMin, trackH - 2);
    yMax = qBound(2, yMax, trackH - 2);

    p.drawLine(labelW + px, yMin, labelW + px, yMax);
  }

  // Playhead
  int px = labelW + (int)(m_currentFrame * m_ppf);
  p.setPen(QColor(255, 100, 0));
  p.drawLine(px, 0, px, height());
}

// ---- ZtoryAnimaticTrack ----

ZtoryAnimaticTrack::ZtoryAnimaticTrack(QWidget *parent) : QWidget(parent) {
  setFixedHeight(80);
  setMouseTracking(true);
}

void ZtoryAnimaticTrack::updateCursor() {
  if (m_tool == RazorTool)
    setCursor(Qt::CrossCursor);
  else
    unsetCursor();
}

void ZtoryAnimaticTrack::refreshFromScene() {
  m_blocks.clear();
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  int numCols = mainXsh->getColumnCount();
  for (int col = 0; col < numCols; col++) {
    // Usa getMinFrame/getMaxFrame per avere durata reale incluse celle vuote
    TXshColumn *column = mainXsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    if (r1 < r0) continue;
    // Cerca child level per identificare lo shot
    TXshChildLevel *cl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = mainXsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        cl = cell.m_level->getChildLevel();
        break;
      }
    }
    if (!cl) continue;
    int startFrame = r0;
    int duration = r1 - r0 + 1;

    ShotBlock b;
    b.col = col;
    b.startFrameInMain = startFrame;
    b.f0 = 0;
    b.f1 = duration - 1;
    // Legge il numero shot dal nome della colonna (impostato da StoryboardPanel)
    QString colName = QString::fromStdString(
        mainXsh->getStageObject(mainXsh->getColumnObjectId(col))->getName());
    b.shotNumber = colName.isEmpty() ? QString("%1").arg(col + 1, 2, 10, QChar('0')) : colName;
    m_blocks.push_back(b);
  }
  // Ordina per startFrameInMain per garantire ordine corretto nel ripple
  std::sort(m_blocks.begin(), m_blocks.end(),
    [](const ShotBlock &a, const ShotBlock &b) {
      return a.startFrameInMain < b.startFrameInMain;
    });

  // Calcola larghezza totale
  int totalFrames = 0;
  for (auto &b : m_blocks)
    totalFrames = qMax(totalFrames, b.startFrameInMain + (b.f1 - b.f0 + 1));
  setMinimumWidth((int)(totalFrames * m_ppf) + 100);
  update();
}

void ZtoryAnimaticTrack::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(30, 30, 30));

  for (auto &b : m_blocks) {
    int duration = b.f1 - b.f0 + 1;
    int x = (int)(b.startFrameInMain * m_ppf);
    int w = (int)(duration * m_ppf);
    int h = height() - 4;
    bool selected = m_selectedCols.count(b.col) > 0;

    // Sfondo shot
    QColor bg = selected ? QColor(80, 110, 160) : QColor(60, 90, 130);
    p.fillRect(x + 1, 2, w - 2, h, bg);
    p.setPen(selected ? QColor(255, 160, 0) : QColor(100, 140, 200));
    p.drawRect(x + 1, 2, w - 2, h);
    if (selected) {
      // Doppio bordo arancione
      p.setPen(QPen(QColor(255, 160, 0), 2));
      p.drawRect(x + 2, 3, w - 4, h - 2);
    }

    // Thumbnail
    if (!b.thumbnail.isNull())
      p.drawPixmap(x + 2, 4, qMin(w - 4, 60), h - 4, b.thumbnail);

    // Numero shot
    p.setPen(Qt::white);
    p.drawText(x + 4, 2, w - 8, h, Qt::AlignBottom | Qt::AlignLeft, b.shotNumber);

    // Handle resize (bordo destro)
    p.fillRect(x + w - 4, 2, 4, h, QColor(180, 180, 80));
  }

  // Playhead
  int px = (int)(m_currentFrame * m_ppf);
  p.setPen(QColor(255, 100, 0));
  p.drawLine(px, 0, px, height());
}

void ZtoryAnimaticTrack::mousePressEvent(QMouseEvent *e) {
  int mx = e->x();

  // Razor tool: split shot at click position
  if (m_tool == RazorTool && e->button() == Qt::LeftButton) {
    for (auto &b : m_blocks) {
      int duration = b.f1 - b.f0 + 1;
      int x = (int)(b.startFrameInMain * m_ppf);
      int w = (int)(duration * m_ppf);
      if (mx >= x && mx < x + w) {
        int splitFrame = (int)((mx - x) / m_ppf); // relativo all'inizio del blocco
        if (splitFrame > 0 && splitFrame < duration - 1)
          emit razorRequested(b.col, b.startFrameInMain + splitFrame);
        return;
      }
    }
    return;
  }

  // Tasto destro — context menu
  if (e->button() == Qt::RightButton) {
    for (auto &b : m_blocks) {
      int duration = b.f1 - b.f0 + 1;
      int x = (int)(b.startFrameInMain * m_ppf);
      int w = (int)(duration * m_ppf);
      if (mx >= x && mx < x + w) {
        QMenu menu(this);
        QAction *matchAct = menu.addAction(tr("Match Subscene Duration"));
        QAction *chosen = menu.exec(e->globalPos());
        if (chosen == matchAct)
          emit matchSubsceneDuration(b.col);
        return;
      }
    }
    return;
  }

  for (auto &b : m_blocks) {
    int duration = b.f1 - b.f0 + 1;
    int x = (int)(b.startFrameInMain * m_ppf);
    int w = (int)(duration * m_ppf);
    // Check handle resize
    if (mx >= x + w - 6 && mx <= x + w + 2) {
      m_draggingCol = b.col;
      m_dragStartX = mx;
      m_dragOrigF1 = b.f1;
      // Refresh prima di salvare originals per avere dati aggiornati
      refreshFromScene();
      m_origStarts.clear();
      m_origDurations.clear();
      for (auto &sb : m_blocks) {
        m_origStarts[sb.col] = sb.startFrameInMain;
        m_origDurations[sb.col] = sb.f1 - sb.f0 + 1;
      }
      return;
    }
    // Click su shot
    if (mx >= x && mx < x + w) {
      Qt::KeyboardModifiers mods = e->modifiers();
      if (mods & Qt::ShiftModifier && m_lastClickedCol >= 0) {
        // Selezione a range: seleziona tutti i blocchi tra m_lastClickedCol e b.col
        bool inRange = false;
        for (auto &sb : m_blocks) {
          if (sb.col == m_lastClickedCol || sb.col == b.col) {
            inRange = !inRange;
            m_selectedCols.insert(sb.col);
          } else if (inRange) {
            m_selectedCols.insert(sb.col);
          }
        }
        // Garantisce che anche il punto finale sia incluso
        m_selectedCols.insert(b.col);
      } else if (mods & Qt::ControlModifier) {
        // Toggle selezione
        if (m_selectedCols.count(b.col)) m_selectedCols.erase(b.col);
        else m_selectedCols.insert(b.col);
      } else {
        // Click semplice: seleziona solo questo
        m_selectedCols.clear();
        m_selectedCols.insert(b.col);
      }
      m_lastClickedCol = b.col;
      update();
      emit selectionChanged(m_selectedCols);
      emit shotClicked(b.col);
      return;
    }
  }
  // Click su area vuota: deseleziona tutto
  if (!(e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
    m_selectedCols.clear();
    m_lastClickedCol = -1;
    update();
    emit selectionChanged(m_selectedCols);
  }
}

void ZtoryAnimaticTrack::mouseMoveEvent(QMouseEvent *e) {
  if (m_draggingCol >= 0) {
    int dx = e->x() - m_dragStartX;
    int delta = (int)(dx / m_ppf);
    int newF1 = qMax(m_dragOrigF1 + delta, 0);
    int newDuration = newF1 + 1;
    int durationDelta = newDuration - (m_dragOrigF1 + 1);

    // Aggiorna durata shot draggato e ricalcola posizioni successive
    int nextStart = -1;
    for (int i = 0; i < (int)m_blocks.size(); i++) {
      if (m_blocks[i].col == m_draggingCol) {
        m_blocks[i].f1 = newF1;
        nextStart = m_origStarts[m_blocks[i].col] + newDuration;
      } else if (nextStart >= 0) {
        int origDuration = m_origDurations.value(m_blocks[i].col, m_blocks[i].f1 - m_blocks[i].f0 + 1);
        m_blocks[i].startFrameInMain = nextStart;
        nextStart += origDuration;
      }
    }
    update();
  }
}

void ZtoryAnimaticTrack::mouseReleaseEvent(QMouseEvent *) {
  if (m_draggingCol >= 0) {
    bool found = false;
    for (auto &b : m_blocks) {
      if (b.col == m_draggingCol) {
        emit shotDurationChanged(m_draggingCol, b.f1);
        found = true;
      } else if (found) {
        // Ripple: aggiorna posizione xsheet degli shot successivi
        emit shotMoved(b.col, b.startFrameInMain);
      }
    }
    m_draggingCol = -1;
  }
}

void ZtoryAnimaticTrack::mouseDoubleClickEvent(QMouseEvent *e) {
  int mx = e->x();
  for (auto &b : m_blocks) {
    int duration = b.f1 - b.f0 + 1;
    int x = (int)(b.startFrameInMain * m_ppf);
    int w = (int)(duration * m_ppf);
    if (mx >= x && mx < x + w) {
      emit shotDoubleClicked(b.col);
      return;
    }
  }
}

void ZtoryAnimaticTrack::wheelEvent(QWheelEvent *e) {
  int delta = e->angleDelta().y();
  double factor = (delta > 0) ? 1.15 : (1.0 / 1.15);
  double newPpf = qBound(2.0, m_ppf * factor, 64.0);
  emit zoomChanged(newPpf);
  e->accept();
}

// ---- ZtoryStoryStrip ----

ZtoryStoryStrip::ZtoryStoryStrip(QWidget *parent) : QWidget(parent) {
  setFixedHeight(kThumbH + 8);
  setMinimumWidth(100);
  setStyleSheet("background:#1a1a1a;");
}

void ZtoryStoryStrip::refreshFromScene() {
  m_entries.clear();
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) { update(); return; }
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) { update(); return; }

  int numCols = xsh->getColumnCount();
  for (int col = 0; col < numCols; col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    TXshChildLevel *cl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        cl = cell.m_level->getChildLevel();
        break;
      }
    }
    if (!cl) continue;
    ThumbEntry e;
    e.col = col;
    e.shotNumber = ZtoryModel::instance()->shotCount() > col
        ? QString::fromStdString("")  // will use column name below
        : QString("%1").arg(col + 1, 2, 10, QChar('0'));
    QString colName = QString::fromStdString(
        xsh->getStageObject(xsh->getColumnObjectId(col))->getName());
    if (!colName.isEmpty()) e.shotNumber = colName;
    // Grab preview from ZtoryModel if available
    for (int si = 0; si < ZtoryModel::instance()->shotCount(); si++) {
      if (ZtoryModel::instance()->shot(si).xsheetColumn == col) {
        e.thumb = ZtoryModel::instance()->preview(si, 0);
        break;
      }
    }
    m_entries.push_back(e);
  }
  update();
}

void ZtoryStoryStrip::setCurrentCol(int col) {
  m_currentCol = col;
  // Ensure the current shot is visible (auto-scroll)
  int idx = -1;
  for (int i = 0; i < (int)m_entries.size(); i++)
    if (m_entries[i].col == col) { idx = i; break; }
  if (idx >= 0) {
    int x = idx * (kThumbW + kSpacing);
    if (x < m_scrollOffset) m_scrollOffset = x;
    else if (x + kThumbW > m_scrollOffset + width()) m_scrollOffset = x + kThumbW - width();
    m_scrollOffset = qMax(0, m_scrollOffset);
  }
  update();
}

void ZtoryStoryStrip::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(25, 25, 25));

  int x = 4 - m_scrollOffset;
  for (auto &e : m_entries) {
    bool current = (e.col == m_currentCol);
    QRect r(x, 4, kThumbW, kThumbH);

    // Background
    p.fillRect(r, current ? QColor(70, 100, 150) : QColor(45, 45, 45));

    // Thumbnail
    if (!e.thumb.isNull())
      p.drawPixmap(r, e.thumb.scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // Border
    p.setPen(current ? QColor(255, 160, 0) : QColor(80, 80, 80));
    p.drawRect(r);

    // Shot number overlay
    p.setPen(Qt::white);
    p.setFont(QFont("Arial", 8, QFont::Bold));
    p.drawText(r.adjusted(2, 2, -2, -2), Qt::AlignBottom | Qt::AlignLeft, e.shotNumber);

    x += kThumbW + kSpacing;
  }
}

void ZtoryStoryStrip::mousePressEvent(QMouseEvent *e) {
  int mx = e->x();
  int x = 4 - m_scrollOffset;
  for (auto &entry : m_entries) {
    if (mx >= x && mx < x + kThumbW) {
      setCurrentCol(entry.col);
      emit shotClicked(entry.col);
      return;
    }
    x += kThumbW + kSpacing;
  }
}

void ZtoryStoryStrip::wheelEvent(QWheelEvent *e) {
  m_scrollOffset -= e->angleDelta().y() / 2;
  int maxOffset = qMax(0, (int)m_entries.size() * (kThumbW + kSpacing) - width());
  m_scrollOffset = qBound(0, m_scrollOffset, maxOffset);
  update();
  e->accept();
}

// ---- ZtoryAnimaticViewer (standalone) ----

ZtoryAnimaticViewer::ZtoryAnimaticViewer(QWidget *parent)
    : BaseViewerPanel(parent) {
  m_sceneViewer->setAlwaysMainXsheet(true);

  // --- Dedicated frame handle from the animatic controller ---
  auto *ctrl = ZtoryAnimaticController::instance();
  // Override FlipConsole so playback uses the animatic frame, not the global
  m_flipConsole->setFrameHandle(ctrl->frameHandle());
  // Override SceneViewer so rendering reads the animatic frame
  m_sceneViewer->setCustomFrameHandle(ctrl->frameHandle());
  // Disconnect playback from global frame handle, connect to ours
  disconnect(m_flipConsole, SIGNAL(playStateChanged(bool)),
             TApp::instance()->getCurrentFrame(), SLOT(setPlaying(bool)));
  connect(m_flipConsole, SIGNAL(playStateChanged(bool)),
          ctrl->frameHandle(), SLOT(setPlaying(bool)));

  // Don't let this viewer become the global active viewer
  disconnect(TApp::instance(), SIGNAL(activeViewerChanged()),
             this, SLOT(onActiveViewerChanged()));

  // Repaint when the animatic frame changes
  connect(ctrl->frameHandle(), &TFrameHandle::frameSwitched, this, [this]() {
    if (m_sceneViewer) m_sceneViewer->update();
  });

  // Set up layout: scene viewer + flip console (like SceneViewerPanel)
  m_mainLayout->insertWidget(0, m_fsWidget, 1);
  setLayout(m_mainLayout);
  m_visiblePartsFlag = VPPARTS_ALL;
}

void ZtoryAnimaticViewer::showEvent(QShowEvent *e) {
  BaseViewerPanel::showEvent(e);
  if (m_sceneViewer) m_sceneViewer->update();
}

// ---- ZtoryAnimaticViewerPanel ----

ZtoryAnimaticViewerPanel::ZtoryAnimaticViewerPanel(QWidget *parent)
    : TPanel(parent) {
  setWindowTitle("Ztory Viewer");
  m_viewer = new ZtoryAnimaticViewer(this);
  m_viewer->setMinimumHeight(120);
  setWidget(m_viewer);
}

void ZtoryAnimaticViewerPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  m_viewer->show();
}

// ---- ZtoryStoryStripPanel ----

ZtoryStoryStripPanel::ZtoryStoryStripPanel(QWidget *parent)
    : TPanel(parent) {
  setWindowTitle("Ztory Story Strip");
  m_strip = new ZtoryStoryStrip(this);
  setWidget(m_strip);

  // Refresh when scene or xsheet changes
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryStoryStripPanel::refreshFromScene);
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, [this]() {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene && scene->getChildStack()->getAncestorCount() == 0)
      refreshFromScene();
  });
  // Highlight current column when column selection changes
  connect(TApp::instance()->getCurrentColumn(), &TColumnHandle::columnIndexSwitched,
          this, [this]() {
    m_strip->setCurrentCol(TApp::instance()->getCurrentColumn()->getColumnIndex());
  });
  // Clicking a shot in the strip selects its column
  connect(m_strip, &ZtoryStoryStrip::shotClicked,
          this, [](int col) {
    TApp::instance()->getCurrentColumn()->setColumnIndex(col);
  });
}

void ZtoryStoryStripPanel::refreshFromScene() {
  m_strip->refreshFromScene();
}

void ZtoryStoryStripPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  refreshFromScene();
}

// ---- ZtoryAnimaticPanel (timeline only) ----

ZtoryAnimaticPanel::ZtoryAnimaticPanel(QWidget *parent) : TPanel(parent) {
  setWindowTitle("Ztory Animatic");

  QWidget *container = new QWidget(this);
  QVBoxLayout *lay = new QVBoxLayout(container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  m_ruler = new ZtoryAnimaticRuler(container);
  m_track = new ZtoryAnimaticTrack(container);

  // Toolbar
  QWidget *toolbar = new QWidget(container);
  toolbar->setFixedHeight(28);
  toolbar->setStyleSheet("background:#2a2a2a;");
  QHBoxLayout *tbLay = new QHBoxLayout(toolbar);
  tbLay->setContentsMargins(6, 2, 6, 2);
  tbLay->setSpacing(4);
  QLabel *zoomLabel = new QLabel("Zoom:", toolbar);
  zoomLabel->setStyleSheet("color:#ccc; font-size:11px;");
  m_zoomSlider = new QSlider(Qt::Horizontal, toolbar);
  m_zoomSlider->setRange(20, 640);
  m_zoomSlider->setValue((int)(m_ppf * 10));
  m_zoomSlider->setMaximumWidth(160);
  m_zoomSlider->setToolTip("Zoom (pixels per frame)");
  tbLay->addWidget(zoomLabel);
  tbLay->addWidget(m_zoomSlider);
  tbLay->addSpacing(12);

  QPushButton *selectBtn = new QPushButton("Select", toolbar);
  selectBtn->setCheckable(true);
  selectBtn->setChecked(true);
  selectBtn->setMaximumWidth(60);
  selectBtn->setStyleSheet(
    "QPushButton{background:#3a3a3a;color:#ccc;border-radius:3px;padding:2px 6px;font-size:11px;}"
    "QPushButton:checked{background:#555;color:white;}"
    "QPushButton:hover{background:#4a4a4a;}");

  QPushButton *razorBtn = new QPushButton("Razor", toolbar);
  razorBtn->setCheckable(true);
  razorBtn->setMaximumWidth(60);
  razorBtn->setStyleSheet(
    "QPushButton{background:#3a3a3a;color:#ccc;border-radius:3px;padding:2px 6px;font-size:11px;}"
    "QPushButton:checked{background:#6a3a2a;color:white;}"
    "QPushButton:hover{background:#4a4a4a;}");

  QPushButton *linkBtn = new QPushButton("A|V Link", toolbar);
  linkBtn->setCheckable(true);
  linkBtn->setChecked(true);
  linkBtn->setMaximumWidth(70);
  linkBtn->setStyleSheet(
    "QPushButton{background:#3a3a3a;color:#ccc;border-radius:3px;padding:2px 6px;font-size:11px;}"
    "QPushButton:checked{background:#2a5a3a;color:white;}"
    "QPushButton:hover{background:#4a4a4a;}");
  linkBtn->setToolTip("Link/Unlink audio and video tracks");

  QPushButton *mergeBtn = new QPushButton("Merge", toolbar);
  mergeBtn->setMaximumWidth(60);
  mergeBtn->setStyleSheet(
    "QPushButton{background:#3a3a3a;color:#ccc;border-radius:3px;padding:2px 6px;font-size:11px;}"
    "QPushButton:hover{background:#5a4a2a;color:white;}");
  mergeBtn->setToolTip("Merge selected shots into the first selected shot");

  tbLay->addWidget(selectBtn);
  tbLay->addWidget(razorBtn);
  tbLay->addSpacing(8);
  tbLay->addWidget(linkBtn);
  tbLay->addSpacing(8);
  tbLay->addWidget(mergeBtn);
  tbLay->addStretch(1);

  connect(selectBtn, &QPushButton::clicked, this, [this, selectBtn, razorBtn](){
    m_track->setTool(ZtoryAnimaticTrack::SelectTool);
    selectBtn->setChecked(true);
    razorBtn->setChecked(false);
  });
  connect(razorBtn, &QPushButton::clicked, this, [this, selectBtn, razorBtn](){
    m_track->setTool(ZtoryAnimaticTrack::RazorTool);
    razorBtn->setChecked(true);
    selectBtn->setChecked(false);
  });
  connect(linkBtn, &QPushButton::toggled, this, [this](bool checked){
    m_audioLinked = checked;
  });
  connect(mergeBtn, &QPushButton::clicked, this, &ZtoryAnimaticPanel::onMergeShots);

  // Scroll area with ruler + track + audio
  QScrollArea *scroll = new QScrollArea(container);
  m_scrollContent = new QWidget();
  m_scrollLay = new QVBoxLayout(m_scrollContent);
  m_scrollLay->setContentsMargins(0, 0, 0, 0);
  m_scrollLay->setSpacing(0);
  m_scrollLay->addWidget(m_ruler);
  m_scrollLay->addWidget(m_track);
  m_scrollLay->addStretch(1);
  scroll->setWidget(m_scrollContent);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  // Layout: toolbar on top, scroll below
  lay->addWidget(toolbar);
  lay->addWidget(scroll, 1);
  setWidget(container);

  connect(m_ruler, &ZtoryAnimaticRuler::frameChanged,
          this, &ZtoryAnimaticPanel::onFrameChanged);
  connect(m_track, &ZtoryAnimaticTrack::shotClicked,
          this, &ZtoryAnimaticPanel::onShotClicked);
  connect(m_track, &ZtoryAnimaticTrack::shotDoubleClicked,
          this, &ZtoryAnimaticPanel::onShotDoubleClicked);
  connect(m_track, &ZtoryAnimaticTrack::zoomChanged,
          this, &ZtoryAnimaticPanel::onZoomChanged);
  connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v){
    onZoomChanged(v / 10.0);
  });
  connect(m_track, &ZtoryAnimaticTrack::matchSubsceneDuration,
          this, &ZtoryAnimaticPanel::onMatchSubsceneDuration);
  connect(m_track, &ZtoryAnimaticTrack::razorRequested,
          this, &ZtoryAnimaticPanel::onRazorRequested);
  connect(m_track, &ZtoryAnimaticTrack::shotDurationChanged,
          this, &ZtoryAnimaticPanel::onShotDurationChanged);
  connect(m_track, &ZtoryAnimaticTrack::shotMoved,
          this, &ZtoryAnimaticPanel::onShotMoved);

  // BUG-02 fix: sceneSwitched fires when entering a sub-scene too.
  // Without this guard, refreshFromScene() would call getTopXsheet() which
  // returns the sub-scene, causing the timeline to show its columns instead
  // of the main shot-list.  Same guard already used for xsheetChanged below.
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, [this]() {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    if (scene->getChildStack()->getAncestorCount() != 0) return;
    refreshFromScene();
  });
  // Animatic panel listens to the controller's dedicated frame handle,
  // NOT the global TApp frame.  This decouples the animatic playhead from
  // the native timeline cursor (BUG-03 fix).
  auto *animCtrl = ZtoryAnimaticController::instance();
  connect(animCtrl->frameHandle(), &TFrameHandle::frameSwitched,
          this, [this](){
    int frame = ZtoryAnimaticController::instance()->currentFrame();
    m_ruler->setCurrentFrame(frame);
    m_track->setCurrentFrame(frame);
    for (auto *at : m_audioTracks)
      at->setCurrentFrame(frame);
  });
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, [this](){
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene && scene->getChildStack()->getAncestorCount() == 0)
      refreshFromScene();
  });
}

void ZtoryAnimaticPanel::refreshFromScene() {
  m_track->refreshFromScene();
  refreshAudioTracks();
}

void ZtoryAnimaticPanel::refreshAudioTracks() {
  // Rimuovi tracce audio esistenti
  for (auto *at : m_audioTracks) {
    m_scrollLay->removeWidget(at);
    delete at;
  }
  m_audioTracks.clear();

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Trova tutte le colonne audio nel main xsheet
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    TXshSoundColumn *sc = column->getSoundColumn();
    if (!sc || sc->isEmpty()) continue;

    QString name = QString::fromStdString(xsh->getStageObject(xsh->getColumnObjectId(col))->getName());
    if (name.isEmpty()) name = tr("Audio %1").arg(col + 1);

    ZtoryAudioTrack *at = new ZtoryAudioTrack(col, name, m_scrollContent);
    at->setPixelsPerFrame(m_ppf);
    at->setCurrentFrame(m_track->property("currentFrame").toInt());

    // Inserisci prima dello stretch finale
    int insertIdx = m_scrollLay->count() - 1;
    if (insertIdx < 0) insertIdx = 0;
    m_scrollLay->insertWidget(insertIdx, at);
    m_audioTracks.append(at);
  }
}

void ZtoryAnimaticPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  refreshFromScene();
}

void ZtoryAnimaticPanel::onShotClicked(int col) {
  TApp::instance()->getCurrentColumn()->setColumnIndex(col);
}

void ZtoryAnimaticPanel::onShotDoubleClicked(int col) {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  // Close any open sub-scene first
  while (scene->getChildStack()->getAncestorCount() > 0)
    CommandManager::instance()->execute("MI_CloseChild");
  app->getCurrentColumn()->setColumnIndex(col);
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsh->getColumn(col);
  if (!column || column->isEmpty()) return;
  // BUG-01 fix: shots 2+ start at row r0 > 0; getCell(0,col) returned an
  // empty cell for them, so MI_OpenChild never fired.
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  TXshCell cell = xsh->getCell(r0, col);
  if (cell.m_level && cell.m_level->getChildLevel()) {
    app->getCurrentFrame()->setFrame(r0);
    CommandManager::instance()->execute("MI_OpenChild");
    // Keep the animatic controller's frame at the shot's main-xsheet row
    // so the animatic viewer renders at the correct position.
    ZtoryAnimaticController::instance()->setCurrentFrame(r0);
  }
}

void ZtoryAnimaticPanel::onShotMoved(int col, int newStartFrame) {
  if (!m_audioLinked) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Compute delta from old start frame
  TXshColumn *column = xsh->getColumn(col);
  if (!column) return;
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  int delta = newStartFrame - r0;
  if (delta == 0) return;

  // Shift all audio columns by the same delta
  for (auto *at : m_audioTracks) {
    int audioCol = at->columnIndex();
    TXshColumn *ac = xsh->getColumn(audioCol);
    if (!ac) continue;
    int ar0 = 0, ar1 = 0;
    ac->getRange(ar0, ar1);
    int duration = ar1 - ar0 + 1;
    // Collect cells
    std::vector<TXshCell> cells(duration);
    for (int r = ar0; r <= ar1; r++)
      cells[r - ar0] = xsh->getCell(r, audioCol);
    // Clear and rewrite at new position
    for (int r = ar0; r <= ar1; r++) xsh->clearCells(r, audioCol);
    int newAr0 = qMax(0, ar0 + delta);
    for (int r = 0; r < duration; r++) {
      if (!cells[r].isEmpty())
        xsh->setCell(newAr0 + r, audioCol, cells[r]);
    }
  }

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
}

void ZtoryAnimaticPanel::onMergeShots() {
  const std::set<int> &sel = m_track->selectedCols();
  if (sel.size() < 2) return;

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Sort selected cols by their start frame in main xsheet
  std::vector<int> sortedCols(sel.begin(), sel.end());
  std::sort(sortedCols.begin(), sortedCols.end(), [&](int a, int b){
    int r0a = 0, r1a = 0, r0b = 0, r1b = 0;
    if (xsh->getColumn(a)) xsh->getColumn(a)->getRange(r0a, r1a);
    if (xsh->getColumn(b)) xsh->getColumn(b)->getRange(r0b, r1b);
    return r0a < r0b;
  });

  int dstCol = sortedCols[0];
  TXshColumn *dstColumn = xsh->getColumn(dstCol);
  if (!dstColumn) return;
  int dstR0 = 0, dstR1 = 0;
  dstColumn->getRange(dstR0, dstR1);

  // Find child level of destination
  TXshChildLevel *dstCl = nullptr;
  for (int r = dstR0; r <= dstR1; r++) {
    TXshCell cell = xsh->getCell(r, dstCol);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      dstCl = cell.m_level->getChildLevel();
      break;
    }
  }
  if (!dstCl) return;

  // Append frames from each subsequent shot into dstCol
  int appendAt = dstR1 + 1;
  int lastFrameNum = dstR1 - dstR0 + 1; // 1-based frame index for continuation

  for (int i = 1; i < (int)sortedCols.size(); i++) {
    int srcCol = sortedCols[i];
    TXshColumn *srcColumn = xsh->getColumn(srcCol);
    if (!srcColumn) continue;
    int r0 = 0, r1 = 0;
    srcColumn->getRange(r0, r1);
    int duration = r1 - r0 + 1;
    // Append cells pointing to dstCl (same sub-scene, extended)
    for (int r = 0; r < duration; r++)
      xsh->setCell(appendAt + r, dstCol, TXshCell(dstCl, TFrameId(++lastFrameNum)));
    appendAt += duration;
  }

  // Delete source columns in reverse order (skip first = destination)
  for (int i = (int)sortedCols.size() - 1; i >= 1; i--)
    ColumnCmd::deleteColumn(sortedCols[i]);

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();
  m_track->refreshFromScene();
}

void ZtoryAnimaticPanel::onRazorRequested(int col, int splitFrame) {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  TXshColumn *srcColumn = mainXsh->getColumn(col);
  if (!srcColumn) return;
  int r0 = 0, r1 = 0;
  srcColumn->getRange(r0, r1);
  // splitFrame is absolute; compute relative position within shot
  int splitRel = splitFrame - r0;
  if (splitRel <= 0 || splitRel >= r1 - r0 + 1) return;

  // Find the child level of the source column
  TXshChildLevel *cl = nullptr;
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      cl = cell.m_level->getChildLevel();
      break;
    }
  }
  if (!cl) return;

  // Insert a new column right after 'col' for the second half
  int newCol = col + 1;
  mainXsh->insertColumn(newCol);

  // Move cells from splitFrame..r1 to the new column (starting at row 0 of new col)
  for (int r = splitFrame; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty())
      mainXsh->setCell(r - splitFrame + r0, newCol, cell);
    mainXsh->clearCells(r, col);
  }

  mainXsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();
  m_track->refreshFromScene();
}

void ZtoryAnimaticPanel::onShotDurationChanged(int col, int newF1) {
  int newDuration = newF1 + 1;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  TXshColumn *column = mainXsh->getColumn(col);
  if (!column || column->isEmpty()) return;
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  int currentDuration = r1 - r0 + 1;

  // Trova tipo cella
  TXshCell typeCell;
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      typeCell = cell;
      break;
    }
  }
  if (typeCell.isEmpty()) return;

  if (newDuration > currentDuration) {
    // Trova l'ultimo frame index usato
    int lastFrameNum = currentDuration; // frame index dell'ultima cella (0-based -> 1-based)
    for (int r = r1; r >= r0; r--) {
      TXshCell cell = mainXsh->getCell(r, col);
      if (!cell.isEmpty()) {
        lastFrameNum = cell.m_frameId.getNumber();
        break;
      }
    }
    for (int r = r0 + currentDuration; r < r0 + newDuration; r++) {
      TXshCell newCell = typeCell;
      newCell.m_frameId = TFrameId(++lastFrameNum);
      mainXsh->setCell(r, col, newCell);
    }
  } else if (newDuration < currentDuration) {
    mainXsh->removeCells(r0 + newDuration, col, currentDuration - newDuration);
  }

  resequenceXsheet();
  m_track->refreshFromScene();
}

void ZtoryAnimaticPanel::resequenceXsheet() {
  ZtoryModel::instance()->resequenceXsheet();
}

void ZtoryAnimaticPanel::onMatchSubsceneDuration(int col) {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  // Trova la prima riga reale della colonna
  TXshColumn *mainCol = mainXsh->getColumn(col);
  if (!mainCol) return;
  int r0 = 0, r1 = 0;
  mainCol->getRange(r0, r1);
  // Entra nella sottoscena per trovare l'ultimo frame non vuoto
  TXshCell cell = mainXsh->getCell(r0, col);
  if (!cell.m_level || !cell.m_level->getChildLevel()) return;
  TXsheet *subXsh = cell.m_level->getChildLevel()->getXsheet();
  if (!subXsh) return;

  // Trova l'ultimo frame non vuoto in tutta la sottoscena
  int lastFrame = 0;
  for (int c = 0; c < subXsh->getColumnCount(); c++) {
    int r0 = 0, r1 = 0;
    TXshColumn *column = subXsh->getColumn(c);
    if (!column) continue;
    column->getRange(r0, r1);
    // Cerca l'ultimo frame con cella non vuota
    for (int r = r1; r >= r0; r--) {
      TXshCell sc = subXsh->getCell(r, c);
      if (!sc.isEmpty()) {
        lastFrame = qMax(lastFrame, r);
        break;
      }
    }
  }

  if (lastFrame <= 0) return;
  int newDuration = lastFrame + 1;

  // Applica la nuova durata
  emit m_track->shotDurationChanged(col, newDuration - 1);
  onShotDurationChanged(col, newDuration - 1);
}

void ZtoryAnimaticPanel::onZoomChanged(double ppf) {
  m_ppf = ppf;
  m_ruler->setPixelsPerFrame(ppf);
  m_track->setPixelsPerFrame(ppf);
  for (auto *at : m_audioTracks)
    at->setPixelsPerFrame(ppf);
  // Sync slider without triggering another valueChanged
  if (m_zoomSlider) {
    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue((int)(ppf * 10));
    m_zoomSlider->blockSignals(false);
  }
}

void ZtoryAnimaticPanel::onFrameChanged(int frame) {
  m_ruler->setCurrentFrame(frame);
  m_track->setCurrentFrame(frame);
  for (auto *at : m_audioTracks)
    at->setCurrentFrame(frame);
  // Write to the dedicated animatic frame handle — this drives the animatic
  // viewer without touching TApp's global frame (BUG-03 fix).
  ZtoryAnimaticController::instance()->setCurrentFrame(frame);
}



void ZtoryAnimaticPanel::contextMenuEvent(QContextMenuEvent *e) {
  QMenu menu(this);
  QAction *loadAudio = menu.addAction(tr("Load Audio..."));
  QAction *chosen = menu.exec(e->globalPos());
  if (chosen == loadAudio) {
    // Formati nativi: wav, aiff. mp3/ogg richiedono FFmpeg configurato.
    QString path = QFileDialog::getOpenFileName(
        this, tr("Load Audio"),
        QString(),
        tr("Audio Files (*.wav *.aiff *.aif *.mp3 *.ogg);;WAV (*.wav);;AIFF (*.aiff *.aif);;All Files (*)"));
    if (path.isEmpty()) return;

    TApp *app = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    if (!scene) return;
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    if (!xsh) return;

    // Inserisci nella prima colonna libera dopo le colonne esistenti
    int insertCol = xsh->getColumnCount();

    IoCmd::LoadResourceArguments args(TFilePath(path.toStdWString()));
    args.row0 = 0;
    args.col0 = insertCol;
    args.expose = true;
    IoCmd::loadResources(args);
    refreshAudioTracks();
  }
}

class ZtoryAnimaticPanelFactory final : public TPanelFactory {
public:
  ZtoryAnimaticPanelFactory() : TPanelFactory("ZtoryAnimatic") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryAnimaticPanel(parent);
    panel->setObjectName("ZtoryAnimatic");
    panel->setWindowTitle("Ztoryc Animatic");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryAnimaticPanelFactory;

class ZtoryAnimaticViewerPanelFactory final : public TPanelFactory {
public:
  ZtoryAnimaticViewerPanelFactory() : TPanelFactory("ZtoryAnimaticViewer") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryAnimaticViewerPanel(parent);
    panel->setObjectName("ZtoryAnimaticViewer");
    panel->setWindowTitle("Ztoryc Viewer");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryAnimaticViewerPanelFactory;

class ZtoryStoryStripPanelFactory final : public TPanelFactory {
public:
  ZtoryStoryStripPanelFactory() : TPanelFactory("ZtoryStoryStrip") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryStoryStripPanel(parent);
    panel->setObjectName("ZtoryStoryStrip");
    panel->setWindowTitle("Ztoryc Story Strip");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryStoryStripPanelFactory;


