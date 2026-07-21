

#include "toonzqt/keyframenavigator.h"
#include "toonzqt/styleselection.h"
#include "toonzqt/ztorykeydiamond.h"

#include "toonzqt/gutil.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/tstageobjectkeyframe.h"
#include "toonz/stageobjectutil.h"
#include "toonz/tapplication.h"

#include "tpixelutils.h"
#include "tfx.h"
#include "tparamcontainer.h"
#include "tspectrumparam.h"
#include "ttonecurveparam.h"

#include <QPainter>
#include <QPixmap>
#include <QIcon>

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

#include <memory>

#include <QAction>
#include <QMouseEvent>

using namespace std;

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

const int KEY_ICON_SIZE = 20;

//! Ztoryc: i .qss dei temi forzano `image: url(transparent.svg)` sui tre
//! bottoni chiave, cioe' le icone su file NON si vedono e gli stati si
//! distinguono solo dal fondo (arancio = c'e' una chiave, punto). Con sei stati
//! da dire quello non basta piu': queste regole inline — che hanno la
//! precedenza su quelle applicative — riabilitano l'icona (disegnata a codice)
//! e mettono il magenta al posto dell'arancio, che divorava l'oro della posa.
QString keyButtonStyleSheet(bool hasKey) {
  if (!hasKey)
    // Nessuna chiave: solo l'icona: fondo e hover restano quelli del tema.
    return QStringLiteral("QToolButton { image: none; }");

  const QString bg    = ZtoryTheme::keyBackground().name();
  const QString hover = ZtoryTheme::keyBackgroundHover().name();
  return QStringLiteral(
             "QToolButton { image: none; background-color: %1; "
             "border: 1px solid %1; }"
             "QToolButton:hover { background-color: %2; border-color: %2; }")
      .arg(bg, hover);
}

//! Suggerimento in hover: il glifo dice lo STATO (verita' del frame), la frase
//! dice l'AZIONE del click — che dipende dal Global Key scope (tetto): il click
//! avvicina la chiave al completo-nello-scope e solo da li' rimuove. Il
//! suggerimento nomina sempre l'asse che manca, mai contraddicendo il diamante
//! (una posa completa e' "Key: pose", non "partial", anche se il click deve
//! ancora aggiungere la trasformazione).
QString keyHint(bool hasKey, bool stageFull, bool poseAny, bool poseFull,
                bool poseInScope) {
  if (!hasKey)
    return poseInScope ? QObject::tr("Set Key (transform + pose)")
                       : QObject::tr("Set Key");

  const bool fullInScope = stageFull && (!poseInScope || poseFull);
  if (fullInScope) {
    if (poseInScope)
      return QObject::tr("Key: transform + pose — click to remove");
    if (poseAny)  // rig con chiavi di posa ma scope=Stage: il glifo mostra oro, il
                  // click non lo tocca — dirlo evita la sorpresa
      return QObject::tr(
          "Key: transform — click to remove (pose not in Key scope)");
    return QObject::tr("Key: transform — click to remove");
  }

  if (!poseInScope)
    return poseAny ? QObject::tr(
                         "Partial key: transform — click to complete "
                         "(pose not in Key scope)")
                   : QObject::tr(
                         "Partial key: transform — click to complete");
  if (stageFull)
    return poseAny
               ? QObject::tr(
                     "Key: transform + partial pose — click to complete pose")
               : QObject::tr("Key: transform — click to add pose");
  if (poseFull) return QObject::tr("Key: pose — click to add transform");
  if (poseAny)
    return QObject::tr("Partial key: transform + pose — click to complete");
  return QObject::tr("Partial key: transform — click to complete");
}

//-----------------------------------------------------------------------------
}  // namespace
//=============================================================================

//=============================================================================
// KeyframeNavigator
//-----------------------------------------------------------------------------

KeyframeNavigator::KeyframeNavigator(QWidget *parent, TFrameHandle *frameHandle)
    : QToolBar(parent), m_frameHandle(frameHandle), m_panel(0) {
  setLayoutDirection(Qt::LeftToRight);
  setIconSize(QSize(20, 20));
  setObjectName("keyFrameNavigator");

  // previous key button
  QIcon prevKeyIcon = createQIcon("prevkey");
  m_actPreviewKey   = new QAction(prevKeyIcon, tr("Previous Key"), this);
  connect(m_actPreviewKey, SIGNAL(triggered()), SLOT(togglePrevKeyAct()));
  addAction(m_actPreviewKey);
  QWidget *prevWidget = widgetForAction(
      m_actPreviewKey);  // obtain a widget generated from QAction
  prevWidget->setObjectName("PreviousKey");

  // key off button
  m_actKeyNo = new QAction(tr("Set Key"), this);
  connect(m_actKeyNo, SIGNAL(triggered()), SLOT(toggleKeyAct()));
  addAction(m_actKeyNo);
  QWidget *keyNoWidget =
      widgetForAction(m_actKeyNo);  // obtain a widget generated from QAction
  keyNoWidget->setObjectName("KeyNo");
  keyNoWidget->setStyleSheet(keyButtonStyleSheet(false));

  // key partial button
  m_actKeyPartial = new QAction(tr("Set Key"), this);
  connect(m_actKeyPartial, SIGNAL(triggered()), SLOT(toggleKeyAct()));
  addAction(m_actKeyPartial);
  QWidget *keyPartialWidget = widgetForAction(
      m_actKeyPartial);  // obtain a widget generated from QAction
  keyPartialWidget->setObjectName("KeyPartial");
  keyPartialWidget->setStyleSheet(keyButtonStyleSheet(true));

  // key total button
  m_actKeyTotal = new QAction(tr("Set Key"), this);
  connect(m_actKeyTotal, SIGNAL(triggered()), SLOT(toggleKeyAct()));
  addAction(m_actKeyTotal);
  QWidget *keyTotalWidget =
      widgetForAction(m_actKeyTotal);  // obtain a widget generated from QAction
  keyTotalWidget->setObjectName("KeyTotal");
  keyTotalWidget->setStyleSheet(keyButtonStyleSheet(true));

  // Le icone dei tre bottoni sono dipinte a codice in updateKeyGlyph(): sei
  // stati non stanno in tre file svg, e i file svg qui non si vedrebbero
  // comunque (vedi keyButtonStyleSheet).

  // next key button
  QIcon nextKeyIcon = createQIcon("nextkey");
  m_actNextKey      = new QAction(nextKeyIcon, tr("Next Key"), this);
  connect(m_actNextKey, SIGNAL(triggered()), SLOT(toggleNextKeyAct()));
  addAction(m_actNextKey);
  QWidget *nextWidget =
      widgetForAction(m_actNextKey);  // obtain a widget generated from QAction
  nextWidget->setObjectName("NextKey");
}

//-----------------------------------------------------------------------------

void KeyframeNavigator::update() {
  // Prev button
  if (hasPrev())
    m_actPreviewKey->setDisabled(false);
  else
    m_actPreviewKey->setDisabled(true);

  bool isFullKey = isFullKeyframe();
  bool isKey     = isKeyframe();

  if (isKey && !isFullKey) {
    m_actKeyNo->setVisible(false);
    m_actKeyTotal->setVisible(false);
    m_actKeyPartial->setVisible(true);
    m_actKeyPartial->setDisabled(false);
    updateKeyGlyph(m_actKeyPartial);
  }
  if (isFullKey) {
    m_actKeyNo->setVisible(false);
    m_actKeyPartial->setVisible(false);
    m_actKeyTotal->setVisible(true);
    m_actKeyTotal->setDisabled(false);
    updateKeyGlyph(m_actKeyTotal);
  }
  if (!isKey && !isFullKey) {
    m_actKeyPartial->setVisible(false);
    m_actKeyTotal->setVisible(false);
    m_actKeyNo->setVisible(true);
    m_actKeyNo->setDisabled(false);
    updateKeyGlyph(m_actKeyNo);
  }

  // Next button
  if (hasNext())
    m_actNextKey->setDisabled(false);
  else
    m_actNextKey->setDisabled(true);
}

//-----------------------------------------------------------------------------

void KeyframeNavigator::updateKeyGlyph(QAction *visibleAct) {
  const bool hasKey    = isKeyframe() || isFullKeyframe();
  const bool stageFull = isFullKeyframe();
  const bool poseAny   = isPoseKeyframe();
  const bool poseFull  = isFullPoseKeyframe();

  // Senza chiave il diamante e' solo contorno: prima il bottone era del tutto
  // invisibile (icona trasparente su fondo trasparente) e per trovarlo si
  // doveva sapere che c'era. Il glifo mostra la verita' del frame qualunque
  // sia lo scope; e' il suggerimento a raccontare l'azione scope-dipendente.
  ZtoryTheme::KeyDiamond d =
      hasKey ? ZtoryTheme::keyDiamond(stageFull, poseAny, poseFull)
             : ZtoryTheme::keyDiamondSolid(QColor());
  const QColor outline = hasKey ? QColor(0, 0, 0) : QColor(150, 150, 150);

  visibleAct->setIcon(QIcon(ZtoryTheme::keyDiamondPixmap(
      d, KEY_ICON_SIZE, devicePixelRatioF(), outline)));
  visibleAct->setToolTip(
      keyHint(hasKey, stageFull, poseAny, poseFull, isPoseInScope()));
}

//-----------------------------------------------------------------------------

void KeyframeNavigator::showEvent(QShowEvent *e) {
  update();
  if (!m_frameHandle) return;
  connect(m_frameHandle, SIGNAL(frameSwitched()), this, SLOT(update()));

  connect(m_frameHandle, SIGNAL(triggerNextKeyframe(QWidget *)), this,
          SLOT(onNextKeyframe(QWidget *)));
  connect(m_frameHandle, SIGNAL(triggerPrevKeyframe(QWidget *)), this,
          SLOT(onPrevKeyframe(QWidget *)));
  if (!m_panel || m_panel == nullptr) {
    QWidget *panel = this->parentWidget();
    while (panel) {
      if (panel->windowType() == Qt::WindowType::SubWindow ||
          panel->windowType() == Qt::WindowType::Tool) {
        m_panel = panel;
        break;
      }
      panel = panel->parentWidget();
    }
  }
}

//-----------------------------------------------------------------------------

void KeyframeNavigator::hideEvent(QHideEvent *e) {
  if (!m_frameHandle) return;
  disconnect(m_frameHandle);

  disconnect(m_frameHandle, SIGNAL(triggerNextKeyframe(QWidget *)), this,
             SLOT(onNextKeyframe(QWidget *)));
  disconnect(m_frameHandle, SIGNAL(triggerPrevKeyframe(QWidget *)), this,
             SLOT(onPrevKeyframe(QWidget *)));
  m_panel = nullptr;
}

void KeyframeNavigator::onNextKeyframe(QWidget *panel) {
  if (!m_panel || m_panel != panel) return;
  toggleNextKeyAct();
}

void KeyframeNavigator::onPrevKeyframe(QWidget *panel) {
  if (!m_panel || m_panel != panel) return;
  togglePrevKeyAct();
}

//=============================================================================
// ViewerKeyframeNavigator
//-----------------------------------------------------------------------------

TStageObject *ViewerKeyframeNavigator::getStageObject() const {
  if (!m_xsheetHandle || !m_objectHandle) return 0;

  TStageObjectId objectId = m_objectHandle->getObjectId();
  TXsheet *xsh            = m_xsheetHandle->getXsheet();
  // Se e' una colonna sound non posso settare chiavi
  if (objectId.isColumn()) {
    TXshColumn *column = xsh->getColumn(objectId.getIndex());
    if (column && (column->getSoundColumn() || column->getFolderColumn()))
      return 0;
  }
  return xsh->getStageObject(objectId);
}

//-----------------------------------------------------------------------------

bool ViewerKeyframeNavigator::hasNext() const {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return false;
  int r0, r1;
  pegbar->getKeyframeRange(r0, r1);
  return r0 <= r1 && getCurrentFrame() < r1;
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::hasPrev() const {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return false;
  int r0, r1;
  pegbar->getKeyframeRange(r0, r1);
  return r0 <= r1 && getCurrentFrame() > r0;
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::hasKeyframes() const {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return false;
  int r0, r1;
  pegbar->getKeyframeRange(r0, r1);
  return r0 <= r1;
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::isKeyframe() const {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return false;
  return pegbar->isKeyframe(getCurrentFrame());
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::isFullKeyframe() const {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return false;
  return pegbar->isFullKeyframe(getCurrentFrame());
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::isPoseKeyframe() const {
  bool any = false, full = false;
  ZtoryTheme::plasticPoseState(getStageObject(), getCurrentFrame(), any, full);
  return any;
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::isFullPoseKeyframe() const {
  bool any = false, full = false;
  ZtoryTheme::plasticPoseState(getStageObject(), getCurrentFrame(), any, full);
  return full;
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::isPoseInScope() const {
  TStageObject *pegbar = getStageObject();
  return pegbar && pegbar->getPlasticSkeletonDeformation() &&
         Preferences::instance()->getIntValue(GlobalKeyScope) != 0;  // != Stage
}

//-------------------------------------------------------------------

void ViewerKeyframeNavigator::toggle() {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return;
  int frame = getCurrentFrame();

  TXshColumn *pegbarColumn =
      m_xsheetHandle->getXsheet()->getColumnForPegbarObjectId(pegbar->getId());
  if (pegbarColumn && pegbarColumn->isLocked()) return;

  // Both branches go through the undo's redo(): that is where the Ztoryc
  // plastic-pose handling lives (Set Key on a rigged column also keys/unkeys
  // the pose, pref-gated), and doing the operations inline here would bypass
  // it — the navigator key would stay white while the xsheet Z key turns gold.
  //
  // Il click cicla verso il completo-NELLO-SCOPE, e solo da li' rimuove:
  // transform pieno ma posa mancante (scope All) non e' "completo" — il click
  // aggiunge la posa invece di cancellare. Con scope Stage la posa non conta
  // (ne' viene toccata: gli undo leggono la stessa preferenza), quindi una
  // colonna riggata si comporta come una liscia.
  const bool includePose = isPoseInScope();
  bool poseFull          = includePose && isFullPoseKeyframe();
  const bool fullWithinScope =
      pegbar->isFullKeyframe(frame) && (!includePose || poseFull);

  if (fullWithinScope) {
    TStageObject::Keyframe key = pegbar->getKeyframe(frame);
    TPointD center, offset;
    pegbar->getCenterAndOffset(center, offset);

    UndoRemoveKeyFrame *undo = new UndoRemoveKeyFrame(
        pegbar->getId(), frame, key, center, offset, m_xsheetHandle);
    undo->setObjectHandle(m_objectHandle);
    TUndoManager::manager()->add(undo);
    undo->redo();
  } else {
    UndoSetKeyFrame *undo =
        new UndoSetKeyFrame(pegbar->getId(), frame, m_xsheetHandle);
    undo->setObjectHandle(m_objectHandle);
    TUndoManager::manager()->add(undo);
    undo->redo();
  }
  m_xsheetHandle->notifyXsheetChanged();
  m_objectHandle->notifyObjectIdChanged(false);
}

//-------------------------------------------------------------------

void ViewerKeyframeNavigator::goNext() {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return;
  int frame = getCurrentFrame();
  TStageObject::KeyframeMap keyframes;
  pegbar->getKeyframes(keyframes);
  TStageObject::KeyframeMap::iterator it;
  for (it = keyframes.begin(); it != keyframes.end(); ++it)
    if (it->first > frame) {
      setCurrentFrame(it->first);
      return;
    }
}

//-------------------------------------------------------------------

void ViewerKeyframeNavigator::goPrev() {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return;
  int frame = getCurrentFrame();
  TStageObject::KeyframeMap keyframes;
  pegbar->getKeyframes(keyframes);
  TStageObject::KeyframeMap::reverse_iterator it;
  for (it = keyframes.rbegin(); it != keyframes.rend(); ++it)
    if (it->first < frame) {
      setCurrentFrame(it->first);
      return;
    }
}

//-----------------------------------------------------------------------------

void ViewerKeyframeNavigator::showEvent(QShowEvent *e) {
  if (!m_objectHandle) return;
  connect(m_objectHandle, SIGNAL(objectSwitched()), this, SLOT(update()));
  connect(m_objectHandle, SIGNAL(objectChanged(bool)), this, SLOT(update()));
  KeyframeNavigator::showEvent(e);
}

//-----------------------------------------------------------------------------

void ViewerKeyframeNavigator::hideEvent(QHideEvent *e) {
  if (!m_objectHandle) return;
  disconnect(m_objectHandle);
  KeyframeNavigator::hideEvent(e);
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

class UndoPaletteSetKeyFrame final : public TUndo {
  int m_frame;
  int m_styleId;
  TPaletteHandle *m_paletteHandle;

public:
  UndoPaletteSetKeyFrame(int styleId, int frame, TPaletteHandle *paletteHandle)
      : m_frame(frame), m_styleId(styleId), m_paletteHandle(paletteHandle) {}

  void undo() const override { setKeyFrame(); }
  void redo() const override { setKeyFrame(); }
  int getSize() const override { return sizeof(*this); }

protected:
  void setKeyFrame() const {
    TPalette *palette = m_paletteHandle->getPalette();
    if (palette->isKeyframe(m_styleId, m_frame))
      palette->clearKeyframe(m_styleId, m_frame);
    else
      palette->setKeyframe(m_styleId, m_frame);
    m_paletteHandle->notifyPaletteChanged();
  }
};
//-----------------------------------------------------------------------------
}  // namespace
//=============================================================================

//=============================================================================
// PaletteKeyframeNavigator
//-----------------------------------------------------------------------------

bool PaletteKeyframeNavigator::hasNext() const {
  TPalette *palette = getPalette();
  if (!palette || palette->getStyleCount() < 1) return false;
  int styleId = getStyleIndex();
  int frame   = getCurrentFrame();
  int n       = palette->getKeyframeCount(styleId);
  for (int i = n - 1; i >= 0; i--) {
    int f = palette->getKeyframe(styleId, i);
    if (f > frame)
      return true;
    else if (f <= frame)
      return false;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool PaletteKeyframeNavigator::hasPrev() const {
  TPalette *palette = getPalette();
  if (!palette || palette->getStyleCount() < 1) return false;
  int styleId = getStyleIndex();
  int frame   = getCurrentFrame();
  int n       = palette->getKeyframeCount(styleId);
  for (int i = 0; i < n; i++) {
    int f = palette->getKeyframe(styleId, i);
    if (f < frame)
      return true;
    else if (f >= frame)
      return false;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool PaletteKeyframeNavigator::hasKeyframes() const {
  TPalette *palette = getPalette();
  if (!palette) return false;
  return palette->getKeyframeCount(getStyleIndex()) > 0;
}

//-----------------------------------------------------------------------------

bool PaletteKeyframeNavigator::isKeyframe() const {
  TPalette *palette = getPalette();
  if (!palette || palette->getStyleCount() < 1) return false;
  int frame = getCurrentFrame();
  return palette->isKeyframe(getStyleIndex(), frame);
}

//-----------------------------------------------------------------------------

void PaletteKeyframeNavigator::toggle() {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleId = getStyleIndex(), frame = getCurrentFrame();

  std::unique_ptr<UndoPaletteSetKeyFrame> undo(
      new UndoPaletteSetKeyFrame(styleId, frame, m_paletteHandle));
  undo->redo();

  TUndoManager::manager()->add(undo.release());
}

//-----------------------------------------------------------------------------

void PaletteKeyframeNavigator::goNext() {
  TPalette *palette = getPalette();
  if (!palette) return;
  int styleId = getStyleIndex();
  int frame   = getCurrentFrame();
  int n       = palette->getKeyframeCount(styleId);
  for (int i = 0; i < n; i++) {
    int f = palette->getKeyframe(styleId, i);
    if (f > frame) {
      setCurrentFrame(f);
      break;
    }
  }
}

//-----------------------------------------------------------------------------

void PaletteKeyframeNavigator::goPrev() {
  TPalette *palette = getPalette();
  if (!palette) return;
  int styleId = getStyleIndex();
  int frame   = getCurrentFrame();
  int n       = palette->getKeyframeCount(styleId);
  for (int i = n - 1; i >= 0; i--) {
    int f = palette->getKeyframe(styleId, i);
    if (f < frame) {
      setCurrentFrame(f);
      break;
    }
  }
}

//-----------------------------------------------------------------------------

void PaletteKeyframeNavigator::showEvent(QShowEvent *e) {
  if (!m_paletteHandle) return;
  connect(m_paletteHandle, SIGNAL(paletteSwitched()), this, SLOT(update()));
  connect(m_paletteHandle, SIGNAL(paletteChanged()), this, SLOT(update()));
  connect(m_paletteHandle, SIGNAL(colorStyleSwitched()), this, SLOT(update()));
  KeyframeNavigator::showEvent(e);
}

//-----------------------------------------------------------------------------

void PaletteKeyframeNavigator::hideEvent(QHideEvent *e) {
  if (!m_paletteHandle) return;
  disconnect(m_paletteHandle);
  KeyframeNavigator::hideEvent(e);
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

//! Se non c'e' un keyframe successivo ritorna frame
int getNextKeyframe(TFxP fx, int frame) {
  if (!fx) return frame;
  int targetFrame = frame;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    int j         = param->getNextKeyframe(frame);
    if (j < 0) continue;
    int f = (int)param->keyframeIndexToFrame(j);
    if (targetFrame == frame || f < targetFrame) targetFrame = f;
  }
  return targetFrame;
}

//-----------------------------------------------------------------------------

//! Se non c'e' un keyframe precedente ritorna frame
int getPrevKeyframe(TFxP fx, int frame) {
  if (!fx) return frame;
  int targetFrame = frame;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    int j         = param->getPrevKeyframe(frame);
    if (j < 0) continue;
    int f = (int)param->keyframeIndexToFrame(j);
    if (targetFrame == frame || f > targetFrame) targetFrame = f;
  }
  return targetFrame;
}

//-----------------------------------------------------------------------------

void setKeyframe(TFxP fx, int frame, bool on) {
  if (fx)
    for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
      TParamP param = fx->getParams()->getParam(i);
      if (TDoubleParamP dp = param) {
        if (on)
          dp->setValue(frame, dp->getValue(frame));
        else
          dp->deleteKeyframe(frame);
      }
    }
}

//-----------------------------------------------------------------------------
}  // anonymous namespace
//=============================================================================

//=============================================================================
// FxKeyframeNavigator
//-----------------------------------------------------------------------------

bool FxKeyframeNavigator::hasNext() const {
  TFx *fx = getFx();
  if (!fx)
    return false;
  else
    return getNextKeyframe(fx, getCurrentFrame()) > getCurrentFrame();
}

//-----------------------------------------------------------------------------

bool FxKeyframeNavigator::hasPrev() const {
  TFx *fx = getFx();
  if (!fx)
    return false;
  else
    return getPrevKeyframe(fx, getCurrentFrame()) < getCurrentFrame();
}

//-----------------------------------------------------------------------------

bool FxKeyframeNavigator::hasKeyframes() const {
  TFx *fx = getFx();
  if (!fx) return false;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    if (param->hasKeyframes()) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool FxKeyframeNavigator::isKeyframe() const {
  TFx *fx = getFx();
  if (!fx) return false;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    if (param->isKeyframe(getCurrentFrame())) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool FxKeyframeNavigator::isFullKeyframe() const {
  TFx *fx = getFx();
  if (!fx) return false;
  int keyFrameCount        = 0;
  int animatableParamCount = 0;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    if (param->isAnimatable()) {
      animatableParamCount++;
      if (param->isKeyframe(getCurrentFrame())) keyFrameCount++;
    }
  }
  return animatableParamCount > 0 && keyFrameCount == animatableParamCount;
}

//-----------------------------------------------------------------------------

void FxKeyframeNavigator::toggle() {
  TFx *fx = getFx();
  if (!fx) return;
  int i, paramCount = fx->getParams()->getParamCount();

  // determino in quale caso ci troviamo:
  // il frame corrente non e' keyframe di nessun parametro (isKeyframe=false),
  // di qualche parametro o di tutti i parametri (isFullKeyframe=true)
  bool isFullKeyframe = true;
  bool isKeyframe     = false;
  int frame           = getCurrentFrame();
  for (i = 0; i < paramCount; i++) {
    TParamP param = fx->getParams()->getParam(i);
    if (!param->isAnimatable()) continue;
    if (param->isKeyframe(frame))
      isKeyframe = true;
    else
      isFullKeyframe = false;
  }
  if (!isKeyframe) isFullKeyframe = false;

  // modifico lo stato: nokeyframe->full, full->no, partial->full
  bool on = (!isKeyframe || (isKeyframe && !isFullKeyframe));
  for (i = 0; i < fx->getParams()->getParamCount();
       i++) {  // TODO. spostare questo codice in TParam
    TParamP param = fx->getParams()->getParam(i);
    if (TDoubleParamP dp = param) {
      if (on)
        dp->setValue(frame, dp->getValue(frame));
      else
        dp->deleteKeyframe(frame);
    } else if (TRangeParamP rp = param) {
      if (on)
        rp->setValue(frame, rp->getValue(frame));
      else
        rp->deleteKeyframe(frame);
    } else if (TPointParamP pp = param) {
      if (on)
        pp->setValue(frame, pp->getValue(frame));
      else
        pp->deleteKeyframe(frame);
    } else if (TPixelParamP pip = param) {
      if (on)
        pip->setValue(frame, pip->getValue(frame));
      else
        pip->deleteKeyframe(frame);
    } else if (TSpectrumParamP sp = param) {
      if (on)
        sp->setValue(frame, sp->getValue(frame), false);
      else
        sp->deleteKeyframe(frame);
    } else if (TToneCurveParamP tcp = param) {
      if (on)
        tcp->setValue(frame, tcp->getValue(frame), false);
      else
        tcp->deleteKeyframe(frame);
    }
  }
  m_fxHandle->notifyFxChanged();
}

//-----------------------------------------------------------------------------

void FxKeyframeNavigator::goNext() {
  TFx *fx = getFx();
  if (!fx) return;
  int frame = getNextKeyframe(fx, getCurrentFrame());
  if (frame > getCurrentFrame()) {
    setCurrentFrame(frame);
    //		m_fxHandle->notifyFxChanged();
  }
}

//-----------------------------------------------------------------------------

void FxKeyframeNavigator::goPrev() {
  TFx *fx = getFx();
  if (!fx) return;
  int frame = getPrevKeyframe(fx, getCurrentFrame());
  if (frame < getCurrentFrame()) {
    setCurrentFrame(frame);
    //		m_fxHandle->notifyFxChanged();
  }
}

//-----------------------------------------------------------------------------

void FxKeyframeNavigator::showEvent(QShowEvent *e) {
  if (!m_fxHandle) return;
  connect(m_fxHandle, SIGNAL(fxSwitched()), this, SLOT(update()));
  connect(m_fxHandle, SIGNAL(fxChanged()), this, SLOT(update()));
  KeyframeNavigator::showEvent(e);
}

//-----------------------------------------------------------------------------

void FxKeyframeNavigator::hideEvent(QHideEvent *e) {
  if (!m_fxHandle) return;
  disconnect(m_fxHandle);
  KeyframeNavigator::hideEvent(e);
}
