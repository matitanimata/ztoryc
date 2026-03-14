
#include "ztoryanimatic.h"
#include "toonz/tcolumnhandle.h"
#include "subscenecommand.h"
#include "toonz/tcolumnhandle.h"
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/txsheet.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/gutil.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsoundlevel.h"
#include "iocommand.h"
#include "tundo.h"
#include "historytypes.h"
#include "orientation.h"
#include "xsheetviewer.h"
#include "toonz/columnfan.h"
#include "toonz/preferences.h"
#include "toonz/preferencesitemids.h"
#include <cmath>
#include <QPainter>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QFileDialog>
#include <QMenu>
#include <QContextMenuEvent>
#include <QToolButton>

namespace {

class ZtorySoundColumnUndo final : public TUndo {
public:
  ZtorySoundColumnUndo(int col, TXshSoundColumn *oldColumn)
      : m_col(col)
      , m_oldColumn(oldColumn ? dynamic_cast<TXshSoundColumn *>(oldColumn->clone()) : nullptr) {}

  void setNewColumn(TXshSoundColumn *newColumn) {
    m_newColumn = newColumn ? dynamic_cast<TXshSoundColumn *>(newColumn->clone()) : nullptr;
  }

  void undo() const override { apply(m_oldColumn.getPointer()); }
  void redo() const override { apply(m_newColumn.getPointer()); }

  int getSize() const override { return sizeof(*this); }
  int getHistoryType() override { return HistoryType::Xsheet; }

private:
  void apply(TXshSoundColumn *src) const {
    if (!src) return;
    TApp *app = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    if (!scene) return;
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    if (!xsh) return;
    TXshColumn *col = xsh->getColumn(m_col);
    if (!col || !col->getSoundColumn()) return;

    col->getSoundColumn()->assignLevels(src);
    xsh->updateFrameCount();
    app->getCurrentXsheet()->notifyXsheetSoundChanged();
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentScene()->setDirtyFlag(true);
  }

  int m_col = -1;
  TXshSoundColumnP m_oldColumn;
  TXshSoundColumnP m_newColumn;
};

static TXsheet *getMainXsheet() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return nullptr;
  return scene->getChildStack()->getTopXsheet();
}

} // namespace

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

// ---- ZtoryAnimaticAudioTrack ----

ZtoryAnimaticAudioTrack::ZtoryAnimaticAudioTrack(QWidget *parent)
    : QWidget(parent) {
  setMouseTracking(true);
  setMinimumHeight(m_trackHeight + 2 * m_clipPadding);
}

QSize ZtoryAnimaticAudioTrack::sizeHint() const {
  int trackCount = (int)m_tracks.size();
  int h = qMax(1, trackCount) * m_trackHeight + 2 * m_clipPadding;
  int maxFrame = 0;
  for (const auto &track : m_tracks) {
    for (const auto &clip : track.clips)
      maxFrame = qMax(maxFrame, clip.r1 + 1);
  }
  int w = m_headerWidth + (int)(maxFrame * m_ppf) + 100;
  return QSize(w, h);
}

void ZtoryAnimaticAudioTrack::refreshFromScene() {
  m_tracks.clear();
  TXsheet *mainXsh = getMainXsheet();
  if (!mainXsh) {
    updateGeometry();
    update();
    return;
  }

  int numCols = mainXsh->getColumnCount();
  for (int col = 0; col < numCols; col++) {
    TXshColumn *column = mainXsh->getColumn(col);
    if (!column || !column->getSoundColumn()) continue;
    TXshSoundColumn *sc = column->getSoundColumn();
    if (!sc) continue;

    AudioTrack track;
    track.col = col;

    int maxFrame = sc->getMaxFrame();
    int row = sc->getFirstRow();
    if (row < 0) row = 0;
    while (row <= maxFrame) {
      if (sc->isCellEmpty(row)) {
        row++;
        continue;
      }
      int r0 = 0, r1 = 0;
      if (!sc->getLevelRange(row, r0, r1)) {
        row++;
        continue;
      }
      AudioClip clip;
      clip.col = col;
      clip.r0 = r0;
      clip.r1 = r1;
      track.clips.push_back(clip);
      row = r1 + 1;
    }

    if (!track.clips.empty()) m_tracks.push_back(track);
  }

  updateGeometry();
  update();
}

void ZtoryAnimaticAudioTrack::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(30, 30, 30));

  const Orientation *o = Orientations::leftToRight();
  int frameHeight = o ? o->dimension(PredefinedDimension::FRAME) : 1;
  if (frameHeight <= 0) frameHeight = 1;

  int y = m_clipPadding;
  for (size_t ti = 0; ti < m_tracks.size(); ti++) {
    const AudioTrack &track = m_tracks[ti];
    QRect trackRect(0, y, width(), m_trackHeight);
    p.fillRect(trackRect, QColor(35, 35, 35));

    QRect labelRect(0, y, m_headerWidth, m_trackHeight);
    p.fillRect(labelRect, QColor(45, 45, 45));
    p.setPen(QColor(200, 200, 200));
    p.drawText(labelRect.adjusted(6, 0, -6, 0),
               Qt::AlignVCenter | Qt::AlignLeft,
               QString("Audio %1").arg((int)ti + 1));

    TXsheet *mainXsh = getMainXsheet();
    TXshSoundColumn *sc =
        (mainXsh && mainXsh->getColumn(track.col))
            ? mainXsh->getColumn(track.col)->getSoundColumn()
            : nullptr;

    for (const auto &clip : track.clips) {
      int x0 = m_headerWidth + (int)(clip.r0 * m_ppf);
      int x1 = m_headerWidth + (int)((clip.r1 + 1) * m_ppf);
      QRect clipRect(x0, y + 4, qMax(1, x1 - x0), m_trackHeight - 8);
      p.fillRect(clipRect, QColor(70, 90, 120));
      p.setPen(QColor(120, 150, 190));
      p.drawRect(clipRect.adjusted(0, 0, -1, -1));

      if (!sc) continue;
      TXshCell cell = sc->getSoundCell(clip.r0);
      TXshSoundLevelP level = cell.getSoundLevel();
      if (!level) continue;
      level->computeValues();

      int centerY = clipRect.center().y();
      int amp = qMax(1, clipRect.height() / 2 - 2);
      int step = 2;
      for (int x = clipRect.left(); x <= clipRect.right(); x += step) {
        double framePos = (x - m_headerWidth) / m_ppf;
        int frameIndex = qMax(0, (int)floor(framePos));
        int row = clip.r0 + frameIndex;
        TXshCell c = sc->getSoundCell(row);
        if (c.isEmpty()) continue;
        int frameId = c.m_frameId.getNumber();
        double local = framePos - frameIndex;
        int soundPixel = (int)(frameId * frameHeight + local * frameHeight);
        DoublePair minmax;
        level->getValueAtPixel(o, soundPixel, minmax);
        int minV = qBound(-amp, (int)minmax.first, amp);
        int maxV = qBound(-amp, (int)minmax.second, amp);
        p.setPen(QColor(180, 200, 220));
        p.drawLine(x, centerY - maxV, x, centerY - minV);
      }
    }

    y += m_trackHeight;
  }

  int px = m_headerWidth + (int)(m_currentFrame * m_ppf);
  p.setPen(QColor(255, 100, 0));
  p.drawLine(px, 0, px, height());
}

bool ZtoryAnimaticAudioTrack::hitTestClip(
    const QPoint &pos, int &outTrackIdx, int &outClipIdx,
    QRect &outClipRect, DragMode &outMode) const {
  int y = m_clipPadding;
  for (size_t ti = 0; ti < m_tracks.size(); ti++) {
    const AudioTrack &track = m_tracks[ti];
    QRect trackRect(0, y, width(), m_trackHeight);
    if (!trackRect.contains(pos)) {
      y += m_trackHeight;
      continue;
    }
    for (size_t ci = 0; ci < track.clips.size(); ci++) {
      const AudioClip &clip = track.clips[ci];
      int x0 = m_headerWidth + (int)(clip.r0 * m_ppf);
      int x1 = m_headerWidth + (int)((clip.r1 + 1) * m_ppf);
      QRect clipRect(x0, y + 4, qMax(1, x1 - x0), m_trackHeight - 8);
      if (!clipRect.contains(pos)) continue;

      int edge = 6;
      if (pos.x() <= clipRect.left() + edge)
        outMode = DragTrimStart;
      else if (pos.x() >= clipRect.right() - edge)
        outMode = DragTrimEnd;
      else
        outMode = DragMove;

      outTrackIdx = (int)ti;
      outClipIdx = (int)ci;
      outClipRect = clipRect;
      return true;
    }
    y += m_trackHeight;
  }
  return false;
}

void ZtoryAnimaticAudioTrack::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::RightButton) return;
  int trackIdx = -1, clipIdx = -1;
  QRect clipRect;
  DragMode mode = DragNone;
  if (!hitTestClip(e->pos(), trackIdx, clipIdx, clipRect, mode)) return;

  m_dragMode = mode;
  m_dragTrackIdx = trackIdx;
  m_dragClipIdx = clipIdx;
  m_dragStartX = e->x();
  m_dragChanged = false;

  const AudioClip &clip = m_tracks[trackIdx].clips[clipIdx];
  m_origR0 = clip.r0;
  m_origR1 = clip.r1;

  m_undoCol = clip.col;
  m_oldColumn = TXshSoundColumnP();
  TXsheet *mainXsh = getMainXsheet();
  if (mainXsh) {
    TXshColumn *col = mainXsh->getColumn(clip.col);
    if (col && col->getSoundColumn())
      m_oldColumn = dynamic_cast<TXshSoundColumn *>(col->getSoundColumn()->clone());
  }
}

void ZtoryAnimaticAudioTrack::mouseMoveEvent(QMouseEvent *e) {
  if (m_dragMode == DragNone || m_dragTrackIdx < 0 || m_dragClipIdx < 0)
    return;

  int deltaFrames = qRound((e->x() - m_dragStartX) / m_ppf);
  if (deltaFrames == 0) return;

  const AudioClip &clip = m_tracks[m_dragTrackIdx].clips[m_dragClipIdx];
  int col = clip.col;

  if (m_dragMode == DragMove) {
    int length = m_origR1 - m_origR0 + 1;
    int newR0 = qMax(0, m_origR0 + deltaFrames);
    applyMoveClip(col, m_origR0, m_origR1, newR0);
  } else if (m_dragMode == DragTrimStart) {
    applyTrimClip(col, m_origR0, deltaFrames, true);
  } else if (m_dragMode == DragTrimEnd) {
    applyTrimClip(col, m_origR1, deltaFrames, false);
  }

  m_dragChanged = true;
  refreshFromScene();
}

void ZtoryAnimaticAudioTrack::mouseReleaseEvent(QMouseEvent *) {
  if (m_dragMode == DragNone) return;
  if (m_dragChanged && m_oldColumn) {
    TXsheet *mainXsh = getMainXsheet();
    if (mainXsh) {
      TXshColumn *col = mainXsh->getColumn(m_undoCol);
      if (col && col->getSoundColumn()) {
        ZtorySoundColumnUndo *undo =
            new ZtorySoundColumnUndo(m_undoCol, m_oldColumn.getPointer());
        undo->setNewColumn(col->getSoundColumn());
        TUndoManager::manager()->add(undo);
      }
    }
  }

  m_dragMode = DragNone;
  m_dragTrackIdx = -1;
  m_dragClipIdx = -1;
  m_dragChanged = false;
  m_undoCol = -1;
  m_oldColumn = TXshSoundColumnP();
}

void ZtoryAnimaticAudioTrack::contextMenuEvent(QContextMenuEvent *e) {
  QMenu menu(this);
  QAction *importAct = menu.addAction(tr("Import Audio..."));
  QAction *chosen = menu.exec(e->globalPos());
  if (chosen == importAct) importAudio();
}

void ZtoryAnimaticAudioTrack::applyMoveClip(int col, int r0, int r1, int newR0) {
  TXsheet *mainXsh = getMainXsheet();
  if (!mainXsh) return;
  TXshColumn *column = mainXsh->getColumn(col);
  if (!column || !column->getSoundColumn()) return;
  TXshSoundColumn *sc = column->getSoundColumn();
  if (!sc) return;

  if (m_oldColumn) sc->assignLevels(m_oldColumn.getPointer());

  int length = r1 - r0 + 1;
  if (length <= 0) return;
  std::vector<TXshCell> cells(length);
  for (int i = 0; i < length; i++) cells[i] = sc->getSoundCell(r0 + i);
  sc->clearCells(r0, length);
  sc->setCells(newR0, length, cells.data());

  mainXsh->updateFrameCount();
  TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

void ZtoryAnimaticAudioTrack::applyTrimClip(int col, int anchorRow, int delta,
                                            bool trimStart) {
  TXsheet *mainXsh = getMainXsheet();
  if (!mainXsh) return;
  TXshColumn *column = mainXsh->getColumn(col);
  if (!column || !column->getSoundColumn()) return;
  TXshSoundColumn *sc = column->getSoundColumn();
  if (!sc) return;

  if (m_oldColumn) sc->assignLevels(m_oldColumn.getPointer());

  sc->modifyCellRange(anchorRow, delta, trimStart);
  mainXsh->updateFrameCount();
  TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

void ZtoryAnimaticAudioTrack::importAudio() {
  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Import Audio"),
      QString(),
      tr("Audio Files (*.wav *.aiff *.aif *.mp3 *.flac);;All Files (*.*)"));
  if (fileName.isEmpty()) return;

  TXsheet *mainXsh = getMainXsheet();
  if (!mainXsh) return;

  int col = mainXsh->getColumnCount();
  for (int i = 0; i < mainXsh->getColumnCount(); i++) {
    TXshColumn *c = mainXsh->getColumn(i);
    if (!c || c->isEmpty()) {
      col = i;
      break;
    }
  }

  IoCmd::LoadResourceArguments args(TFilePath(fileName.toStdWString()));
  args.expose = true;
  args.row0 = 0;
  args.col0 = col;
  args.importPolicy = IoCmd::LoadResourceArguments::ASK_USER;
  IoCmd::loadResources(args, false);

  refreshFromScene();
}

// ---- ZtoryAnimaticTrack ----

ZtoryAnimaticTrack::ZtoryAnimaticTrack(QWidget *parent) : QWidget(parent) {
  setFixedHeight(80);
  setMouseTracking(true);
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
    b.shotNumber = QString("%1").arg(col + 1, 2, 10, QChar('0'));
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

    // Sfondo shot
    QColor col = QColor(60, 90, 130);
    p.fillRect(x + 1, 2, w - 2, h, col);
    p.setPen(QColor(100, 140, 200));
    p.drawRect(x + 1, 2, w - 2, h);

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
      emit shotClicked(b.col);
      return;
    }
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

void ZtoryAnimaticTrack::wheelEvent(QWheelEvent *e) {
  int delta = e->angleDelta().y();
  double factor = (delta > 0) ? 1.15 : (1.0 / 1.15);
  double newPpf = qBound(2.0, m_ppf * factor, 64.0);
  emit zoomChanged(newPpf);
  e->accept();
}

// ---- ZtoryAnimaticPanel ----

ZtoryAnimaticPanel::ZtoryAnimaticPanel(QWidget *parent) : TPanel(parent) {
  setWindowTitle("Ztory Animatic");

  QWidget *container = new QWidget(this);
  QVBoxLayout *lay = new QVBoxLayout(container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  QWidget *toolbar = new QWidget(container);
  QHBoxLayout *toolLay = new QHBoxLayout(toolbar);
  toolLay->setContentsMargins(6, 4, 6, 4);
  toolLay->setSpacing(6);
  QToolButton *loadAudioBtn = new QToolButton(toolbar);
  loadAudioBtn->setText(tr("Load Audio..."));
  loadAudioBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
  toolLay->addWidget(loadAudioBtn);
  toolLay->addStretch();

  m_ruler = new ZtoryAnimaticRuler(container);
  m_track = new ZtoryAnimaticTrack(container);
  m_animViewer = new ZtoryAnimaticViewer(container);
  m_animViewer->setMinimumHeight(120);
  m_audioViewer = new XsheetViewer(container);

  QScrollArea *scroll = new QScrollArea(container);
  QWidget *scrollContent = new QWidget();
  QVBoxLayout *scrollLay = new QVBoxLayout(scrollContent);
  scrollLay->setContentsMargins(0, 0, 0, 0);
  scrollLay->setSpacing(0);
  scrollLay->addWidget(m_ruler);
  scrollLay->addWidget(m_track);
  scroll->setWidget(scrollContent);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  QSplitter *splitter = new QSplitter(Qt::Vertical, container);
  splitter->addWidget(m_animViewer);
  splitter->addWidget(scroll);
  splitter->addWidget(m_audioViewer);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);
  splitter->setStretchFactor(2, 2);
  lay->addWidget(toolbar);
  lay->addWidget(splitter);
  setWidget(container);

  connect(loadAudioBtn, &QToolButton::clicked, this,
          &ZtoryAnimaticPanel::importAudio);

  connect(m_ruler, &ZtoryAnimaticRuler::frameChanged,
          this, &ZtoryAnimaticPanel::onFrameChanged);
  connect(m_track, &ZtoryAnimaticTrack::shotClicked,
          this, &ZtoryAnimaticPanel::onShotClicked);
  connect(m_track, &ZtoryAnimaticTrack::zoomChanged,
          this, &ZtoryAnimaticPanel::onZoomChanged);
  connect(m_track, &ZtoryAnimaticTrack::matchSubsceneDuration,
          this, &ZtoryAnimaticPanel::onMatchSubsceneDuration);
  connect(m_track, &ZtoryAnimaticTrack::shotDurationChanged,
          this, &ZtoryAnimaticPanel::onShotDurationChanged);

  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryAnimaticPanel::refreshFromScene);
  connect(TApp::instance()->getCurrentFrame(), &TFrameHandle::frameSwitched,
          this, [this](){
    // Aggiorna playhead solo se siamo al main xsheet
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    if (scene->getChildStack()->getAncestorCount() == 0) {
      int frame = TApp::instance()->getCurrentFrame()->getFrame();
      m_track->setCurrentFrame(frame);
      m_ruler->update();
    }
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
}

void ZtoryAnimaticPanel::importAudio() {
  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Import Audio"), QString(),
      tr("Audio Files (*.wav *.aiff *.aif *.mp3 *.flac);;All Files (*.*)"));
  if (fileName.isEmpty()) return;

  TXsheet *mainXsh = getMainXsheet();
  if (!mainXsh) return;

  int col = mainXsh->getColumnCount();
  for (int i = 0; i < mainXsh->getColumnCount(); i++) {
    TXshColumn *c = mainXsh->getColumn(i);
    if (!c || c->isEmpty()) {
      col = i;
      break;
    }
  }

  IoCmd::LoadResourceArguments args(TFilePath(fileName.toStdWString()));
  args.expose = true;
  args.row0 = 0;
  args.col0 = col;
  args.importPolicy = IoCmd::LoadResourceArguments::ASK_USER;
  IoCmd::loadResources(args, false);

  refreshFromScene();
}

void ZtoryAnimaticPanel::applyAudioColumnFilter(bool enable) {
  TXsheet *mainXsh = getMainXsheet();
  if (!mainXsh) return;

  ColumnFan *fan = mainXsh->getColumnFan(Orientations::leftToRight());
  if (!fan) return;

  if (enable) {
    if (m_audioFilterApplied) return;
    m_prevColVisibility.clear();
    int colCount = mainXsh->getColumnCount();
    for (int col = 0; col < colCount; col++) {
      m_prevColVisibility[col] = fan->isVisible(col);
      TXshColumn *column = mainXsh->getColumn(col);
      if (column && column->getSoundColumn())
        fan->show(col);
      else
        fan->hide(col);
    }
    m_prevCameraVisible =
        Preferences::instance()->getBoolValue(showXsheetCameraColumn);
    Preferences::instance()->setValue(showXsheetCameraColumn, false);
    m_audioFilterApplied = true;
  } else {
    if (!m_audioFilterApplied) return;
    for (const auto &it : m_prevColVisibility) {
      if (it.second)
        fan->show(it.first);
      else
        fan->hide(it.first);
    }
    Preferences::instance()->setValue(showXsheetCameraColumn,
                                      m_prevCameraVisible);
    m_audioFilterApplied = false;
  }

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

void ZtoryAnimaticPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  refreshFromScene();
  TXsheet *mainXsh = getMainXsheet();
  if (mainXsh) TApp::instance()->getCurrentXsheet()->setXsheet(mainXsh);
  applyAudioColumnFilter(true);
}

void ZtoryAnimaticPanel::hideEvent(QHideEvent *e) {
  TPanel::hideEvent(e);
  applyAudioColumnFilter(false);
}

void ZtoryAnimaticPanel::onShotClicked(int col) {
  TApp::instance()->getCurrentColumn()->setColumnIndex(col);
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
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;
  int numCols = xsh->getColumnCount();
  int maxFrames = xsh->getFrameCount() + 200;
  int startFrame = 0;
  for (int col = 0; col < numCols; col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    int duration = r1 - r0 + 1;
    TXshChildLevel *cl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        cl = cell.m_level->getChildLevel();
        break;
      }
    }
    if (!cl) { startFrame += duration; continue; }
    for (int r = 0; r <= maxFrames; r++) xsh->clearCells(r, col);
    for (int r = 0; r < duration; r++)
      xsh->setCell(startFrame + r, col, TXshCell(cl, TFrameId(r + 1)));
    startFrame += duration;
  }
  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
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
}

void ZtoryAnimaticPanel::onFrameChanged(int frame) {
  TApp::instance()->getCurrentFrame()->setFrame(frame);
  m_track->setCurrentFrame(frame);
  m_ruler->update();
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
