

// TnzCore includes
#include "tsystem.h"
#include "tstream.h"
#include "tfilepath_io.h"
#include "tfunctorinvoker.h"

// TnzBase includes
#include "tunit.h"
#include "tparamcontainer.h"
#include "tparamset.h"
#include "tmacrofx.h"
#include "tparamchange.h"

// TnzExt includes
#include "ext/plasticskeleton.h"

// TnzLib includes
#include "toonz/tstageobjecttree.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/fxdag.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tfxhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/doubleparamcmd.h"

// TnzQt includes
#include "toonzqt/functionviewer.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/plasticvertexselection.h"
#include "tw/stringtable.h"

// Qt includes
#include <QMenu>
#include <QAction>
#include <QItemSelectionModel>
#include <QFileDialog>
#include <QMouseEvent>
#include <QMetaObject>
#include <QColor>
#include <QApplication>  // for drag&drop
#include <QDrag>
#include <QMimeData>
#include <QBitmap>

#include "toonzqt/functiontreeviewer.h"

//*************************************************************************************
//    ChannelGroup specialization  definition
//*************************************************************************************

namespace {

class ParamChannelGroup final : public FunctionTreeModel::ParamWrapper,
                                public FunctionTreeModel::ChannelGroup {
public:
  ParamChannelGroup(TParam *param, const std::wstring &fxId,
                    std::string &paramName);

  void refresh() override;
  void *getInternalPointer() const override;
};

//=============================================================================

class SkVDChannelGroup final : public FunctionTreeModel::ChannelGroup {
public:
  StageObjectChannelGroup *m_stageObjectGroup;  //!< Parent stage object group
  const QString *m_vxName;                      //!< The associated vertex name

public:
  SkVDChannelGroup(const QString *vxName, StageObjectChannelGroup *stageGroup)
      : ChannelGroup(*vxName)
      , m_stageObjectGroup(stageGroup)
      , m_vxName(vxName) {}

  QString getShortName() const override {
    return m_stageObjectGroup->getShortName();
  }
  QString getLongName() const override { return *m_vxName; }

  void *getInternalPointer() const override { return (void *)m_vxName; }

  static inline bool compareStr(const TreeModel::Item *item,
                                const QString &str) {
    const QString &thisStr =
        static_cast<const SkVDChannelGroup *>(item)->getLongName();
    return (QString::localeAwareCompare(thisStr, str) < 0);
  }

  QVariant data(int role) const override;
};

}  // namespace

//=============================================================================
//
// ChannelGroup
//
//-----------------------------------------------------------------------------

FunctionTreeModel::ChannelGroup::ChannelGroup(const QString &name)
    : m_name(name), m_showFilter(ShowAllChannels) {}

//-----------------------------------------------------------------------------

FunctionTreeModel::ChannelGroup::~ChannelGroup() {}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::ChannelGroup::isActive() const {
  // Analyze children. If one is active, this is active too.
  int c, childCount = getChildCount();
  for (c = 0; c != childCount; ++c)
    if (static_cast<Item *>(getChild(c))->isActive()) return true;

  return false;
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::ChannelGroup::isAnimated() const {
  // Same for the animated feature, this is animate if any of its children is.
  int c, childCount = getChildCount();
  for (c = 0; c != childCount; ++c)
    if (static_cast<Item *>(getChild(c))->isAnimated()) return true;

  return false;
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::ChannelGroup::isIgnored() const {
  // Same for the ignored ones, show warning icon if any of its children is.
  int c, childCount = getChildCount();
  for (c = 0; c != childCount; ++c)
    if (static_cast<Item *>(getChild(c))->isIgnored()) return true;

  return false;
}

//-----------------------------------------------------------------------------

QVariant FunctionTreeModel::ChannelGroup::data(int role) const {
  if (role == Qt::DisplayRole)
    return getLongName();
  else if (role == Qt::DecorationRole) {
    bool animated = isAnimated();
    bool active   = isActive();
    bool ignored  = (animated) ? isIgnored() : false;

    if (active) {
      static QIcon folderAnimOpen(createQIcon("folder_anim_on", true));
      static QIcon folderAnimClose(createQIcon("folder_anim", true));
      static QIcon folderOpen(createQIcon("folder_on", true));
      static QIcon folderClose(createQIcon("folder", true));
      static QIcon ignoredOn(":Resources/paramignored_on.svg");

      return animated ? (isOpen() ? folderAnimOpen
                                  : (ignored ? ignoredOn : folderAnimClose))
                      : (isOpen() ? folderOpen : folderClose);
    } else {
      static QIcon folderAnimOpen(createQIcon("folder_anim_inactive_on", true));
      static QIcon folderAnimClose(createQIcon("folder_anim_inactive", true));
      static QIcon folderOpen(createQIcon("folder_inactive_on", true));
      static QIcon folderClose(createQIcon("folder_inactive", true));
      static QIcon ignoredOff(":Resources/paramignored_off.svg");

      return animated ? (isOpen() ? folderAnimOpen
                                  : (ignored ? ignoredOff : folderAnimClose))
                      : (isOpen() ? folderOpen : folderClose);
    }
  } else
    return Item::data(role);
}

//-----------------------------------------------------------------------------

//! \todo     This is \a not recursive - I guess it should be...?
bool FunctionTreeModel::ChannelGroup::nameMatchesSearch(
    const QString &searchString) const {
  if (searchString.isEmpty()) return true;

  // Also the groups ABOVE this one: a hit on the column has to reach the
  // channels of its nested groups, whose long names carry only their own group
  // ("Plastic Skeleton Angle") and never the column's. Tested one by one they
  // would all fail, and the subgroup would sit there looking empty.
  //
  // The walk stops above depth 2 — at the stage object / fx — so that the two
  // fixed roots are left out: searching "FX" should not mean "every parameter
  // of every effect".
  for (const TreeModel::Item *item = this; item && item->getDepth() >= 2;
       item = item->getParent()) {
    const ChannelGroup *group = dynamic_cast<const ChannelGroup *>(item);
    if (group && group->getLongName().contains(searchString, Qt::CaseInsensitive))
      return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::ChannelGroup::matchesSearch(
    const QString &searchString) const {
  if (searchString.isEmpty()) return true;
  if (getLongName().contains(searchString, Qt::CaseInsensitive)) return true;

  int i, itemCount = getChildCount();
  for (i = 0; i < itemCount; i++) {
    const FunctionTreeModel::Channel *channel =
        dynamic_cast<const FunctionTreeModel::Channel *>(getChild(i));
    if (channel) {
      if (channel->getLongName().contains(searchString, Qt::CaseInsensitive))
        return true;
      continue;
    }
    const FunctionTreeModel::ChannelGroup *channelGroup =
        dynamic_cast<const FunctionTreeModel::ChannelGroup *>(getChild(i));
    if (channelGroup && channelGroup->matchesSearch(searchString)) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::ChannelGroup::applyShowFilter() {
  FunctionTreeModel *model = dynamic_cast<FunctionTreeModel *>(getModel());
  const QString search     = model ? model->getSearchFilter() : QString();
  // A group matched by name — its own, or that of a group above it — shows
  // everything it contains, so the channels below need no test of their own.
  const bool groupMatches = nameMatchesSearch(search);

  int i, itemCount = getChildCount();
  for (i = 0; i < itemCount; i++) {
    FunctionTreeModel::Channel *channel =
        dynamic_cast<FunctionTreeModel::Channel *>(getChild(i));
    /*--- ChannelGroupの内部も同じフィルタで更新する ---*/
    if (!channel) {
      FunctionTreeModel::ChannelGroup *channelGroup =
          dynamic_cast<FunctionTreeModel::ChannelGroup *>(getChild(i));
      if (!channelGroup) continue;

      channelGroup->setShowFilter(m_showFilter);
      getModel()->setRowHidden(
          i, createIndex(),
          !(groupMatches || channelGroup->matchesSearch(search)));
      continue;
    }

    bool showItem = (m_showFilter == ShowAllChannels) ||
                    channel->getParam()->hasKeyframes();

    // The animated-channels filter turns what it hides OFF; the search filter
    // must NOT — you would come back from a search with your curves gone.
    if (!showItem) channel->setIsActive(false);

    if (showItem && !groupMatches)
      showItem = channel->getLongName().contains(search, Qt::CaseInsensitive);

    QModelIndex modelIndex = createIndex();
    getModel()->setRowHidden(i, modelIndex, !showItem);
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::ChannelGroup::setShowFilter(ShowFilter showFilter) {
  m_showFilter = showFilter;
  applyShowFilter();
}

//-----------------------------------------------------------------------------

QString FunctionTreeModel::ChannelGroup::getIdName() const {
  QString tmpName = QString(m_name);
  tmpName.remove(QChar(' '), Qt::CaseInsensitive);
  tmpName = tmpName.toLower();

  FunctionTreeModel::ChannelGroup *parentGroup =
      dynamic_cast<FunctionTreeModel::ChannelGroup *>(getParent());
  if (parentGroup) {
    return parentGroup->getIdName() + QString(".") + tmpName;
  }
  return tmpName;
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::ChannelGroup::setChildrenAllActive(bool active) {
  for (int i = 0; i < getChildCount(); i++) {
    // for Channel
    FunctionTreeModel::Channel *channel =
        dynamic_cast<FunctionTreeModel::Channel *>(getChild(i));
    if (channel) {
      channel->setIsActive(active);
      continue;
    }
    // for ChannelGroup
    else {
      FunctionTreeModel::ChannelGroup *channelGroup =
          dynamic_cast<FunctionTreeModel::ChannelGroup *>(getChild(i));
      if (channelGroup) {
        channelGroup->setChildrenAllActive(active);
        continue;
      }
    }
  }
}

//=============================================================================
//
// StageObjectChannelGroup
//
//-----------------------------------------------------------------------------

StageObjectChannelGroup::StageObjectChannelGroup(TStageObject *stageObject)
    : m_stageObject(stageObject), m_plasticGroup() {
  m_stageObject->addRef();
}

//-----------------------------------------------------------------------------

StageObjectChannelGroup::~StageObjectChannelGroup() {
  m_stageObject->release();
}

//-----------------------------------------------------------------------------

QVariant StageObjectChannelGroup::data(int role) const {
  if (role == Qt::DisplayRole) {
    std::string name = (m_stageObject->getId().isTable())
                           ? FunctionTreeView::tr("Table").toStdString()
                           : m_stageObject->getFullName();

    return QString::fromStdString(name);
  } else if (role == Qt::ForegroundRole) {
    FunctionTreeModel *model = dynamic_cast<FunctionTreeModel *>(getModel());
    if (!model) return QColor(Qt::black);
    FunctionTreeView *view = dynamic_cast<FunctionTreeView *>(model->getView());
    if (!view || !model->getCurrentStageObject()) return QColor(Qt::black);
    TStageObjectId currentId = model->getCurrentStageObject()->getId();
    return m_stageObject->getId() == currentId
               ? view->getViewer()->getCurrentTextColor()
               : view->getTextColor();
  } else
    return ChannelGroup::data(role);
}

//-----------------------------------------------------------------------------

QString StageObjectChannelGroup::getShortName() const {
  return QString::fromStdString(m_stageObject->getName());
}

//-----------------------------------------------------------------------------

QString StageObjectChannelGroup::getLongName() const {
  return QString::fromStdString(m_stageObject->getFullName());
}

//-----------------------------------------------------------------------------

QString StageObjectChannelGroup::getIdName() const {
  return QString::fromStdString(m_stageObject->getId().toString()).toLower();
}

//=============================================================================
//
// FxChannelGroup
//
//-----------------------------------------------------------------------------

FxChannelGroup::FxChannelGroup(TFx *fx) : m_fx(fx) {
  if (m_fx) m_fx->addRef();
}

//-----------------------------------------------------------------------------

FxChannelGroup::~FxChannelGroup() {
  if (m_fx) m_fx->release();
  m_fx = 0;
}

//-----------------------------------------------------------------------------

QString FxChannelGroup::getShortName() const {
  return QString::fromStdWString(m_fx->getFxId());
}

//-----------------------------------------------------------------------------

QString FxChannelGroup::getLongName() const {
  std::wstring name = m_fx->getName();
  std::wstring id   = m_fx->getFxId();
  return QString::fromStdWString(id + L" (" + name + L")");
}

//-----------------------------------------------------------------------------

QVariant FxChannelGroup::data(int role) const {
  if (role == Qt::DecorationRole) {
    bool isAnimated                 = false;
    TParamContainer *paramContainer = m_fx->getParams();
    int i;
    for (i = 0; i < paramContainer->getParamCount(); i++) {
      if (!paramContainer->getParam(i)->hasKeyframes()) continue;
      isAnimated = true;
      break;
    }
    bool isOneChildActive = false;
    for (i = 0; i < getChildCount(); i++) {
      FunctionTreeModel::Channel *channel =
          dynamic_cast<FunctionTreeModel::Channel *>(getChild(i));
      if (!channel || !channel->isActive()) continue;
      isOneChildActive = true;
      break;
    }
    bool ignored = (isAnimated) ? isIgnored() : false;
    if (isOneChildActive) {
      static QIcon folderAnimOpen(createQIcon("folder_anim_on", true));
      static QIcon folderAnimClose(createQIcon("folder_anim", true));
      static QIcon folderOpen(createQIcon("folder_on", true));
      static QIcon folderClose(createQIcon("folder", true));
      static QIcon ignoredOn(":Resources/paramignored_on.svg");

      return isAnimated ? (isOpen() ? folderAnimOpen
                                    : (ignored ? ignoredOn : folderAnimClose))
                        : (isOpen() ? folderOpen : folderClose);
    } else {
      static QIcon folderAnimOpen(createQIcon("folder_anim_inactive_on", true));
      static QIcon folderAnimClose(createQIcon("folder_anim_inactive", true));
      static QIcon folderOpen(createQIcon("folder_inactive_on", true));
      static QIcon folderClose(createQIcon("folder_inactive", true));
      static QIcon ignoredOff(":Resources/paramignored_off.svg");

      return isAnimated ? (isOpen() ? folderAnimOpen
                                    : (ignored ? ignoredOff : folderAnimClose))
                        : (isOpen() ? folderOpen : folderClose);
    }
  } else if (role == Qt::DisplayRole) {
    std::wstring name = m_fx->getName();
    std::wstring id   = m_fx->getFxId();
    if (name == id)
      return QString::fromStdWString(name);
    else
      return QString::fromStdWString(id + L" (" + name + L")");
  } else if (role == Qt::ForegroundRole) {
    FunctionTreeModel *model = dynamic_cast<FunctionTreeModel *>(getModel());
    if (!model) return QColor(Qt::black);
    FunctionTreeView *view = dynamic_cast<FunctionTreeView *>(model->getView());
    if (!view) return QColor(Qt::black);
    TFx *currentFx = model->getCurrentFx();
    return m_fx == currentFx ? view->getViewer()->getCurrentTextColor()
                             : view->getTextColor();
  } else
    return Item::data(role);
}

//-----------------------------------------------------------------------------

QString FxChannelGroup::getIdName() const {
  return QString::fromStdWString(m_fx->getFxId()).toLower();
}

//-----------------------------------------------------------------------------

void FxChannelGroup::refresh() {
  TMacroFx *macroFx = dynamic_cast<TMacroFx *>(m_fx);

  int i, childrenCount = getChildCount();
  for (i = 0; i < childrenCount; ++i) {
    FunctionTreeModel::ParamWrapper *wrap =
        dynamic_cast<FunctionTreeModel::ParamWrapper *>(getChild(i));
    assert(wrap);

    TParam *param = 0;
    {
      TParamContainer *paramContainer = 0;
      if (macroFx) {
        const std::wstring &fxId = wrap->getFxId();
        TFx *subFx               = macroFx->getFxById(fxId);
        if (!subFx) continue;

        paramContainer = subFx->getParams();
      } else
        paramContainer = m_fx->getParams();

      param = paramContainer->getParam(wrap->getParam()->getName());
    }

    assert(param);
    wrap->setParam(param);

    ParamChannelGroup *paramGroup = dynamic_cast<ParamChannelGroup *>(wrap);
    if (paramGroup) paramGroup->refresh();
  }
}

//=============================================================================
//
// ParamChannelGroup
//
//-----------------------------------------------------------------------------

ParamChannelGroup::ParamChannelGroup(TParam *param, const std::wstring &fxId,
                                     std::string &paramName)
    : ParamWrapper(param, fxId)
    , ChannelGroup(
          param->hasUILabel()
              ? QString::fromStdString(param->getUILabel())
              : QString::fromStdWString(TStringTable::translate(paramName))) {}

//-----------------------------------------------------------------------------

void *ParamChannelGroup::getInternalPointer() const {
  return this->m_param.getPointer();
}

//-----------------------------------------------------------------------------

void ParamChannelGroup::refresh() {
  TParamSet *paramSet = dynamic_cast<TParamSet *>(m_param.getPointer());
  if (!paramSet) return;

  int c, childrenCount = getChildCount();
  for (c = 0; c < childrenCount; ++c) {
    FunctionTreeModel::ParamWrapper *wrap =
        dynamic_cast<FunctionTreeModel::ParamWrapper *>(getChild(c));
    assert(wrap);

    TParamP currentParam = wrap->getParam();
    assert(currentParam);

    int p = paramSet->getParamIdx(wrap->getParam()->getName());
    assert(p < paramSet->getParamCount());

    TParamP param = paramSet->getParam(p);
    wrap->setParam(param);
  }
}

//=============================================================================
//
// SkVDChannelGroup
//
//-----------------------------------------------------------------------------

QVariant SkVDChannelGroup::data(int role) const {
  if (role == Qt::ForegroundRole) {
    // Check whether current selection is a PlasticVertex one - in case, paint
    // it selection color
    // if this group refers to current vertex
    FunctionTreeModel *model = dynamic_cast<FunctionTreeModel *>(getModel());
    if (!model) return QColor(Qt::black);
    FunctionTreeView *view = dynamic_cast<FunctionTreeView *>(model->getView());
    if (!view || !model->getCurrentStageObject()) return QColor(Qt::black);

    if (PlasticVertexSelection *vxSel =
            dynamic_cast<PlasticVertexSelection *>(TSelection::getCurrent()))
      if (TStageObject *obj = model->getCurrentStageObject())
        if (obj == m_stageObjectGroup->m_stageObject)
          if (const SkDP &sd = obj->getPlasticSkeletonDeformation()) {
            int vIdx = *vxSel;

            if (vIdx >= 0 &&
                sd->skeleton(vxSel->skeletonId())->vertex(vIdx).name() ==
                    getLongName())
              return view->getViewer()->getCurrentTextColor();
          }
    return view->getTextColor();
  } else
    return ChannelGroup::data(role);
}

//=============================================================================
//
// Channel
//
//-----------------------------------------------------------------------------

FunctionTreeModel::Channel::Channel(FunctionTreeModel *model,
                                    TDoubleParam *param,
                                    std::string paramNamePref,
                                    std::wstring fxId)
    : ParamWrapper(param, fxId)
    , m_model(model)
    , m_group(0)
    , m_isActive(false)
    , m_paramNamePref(paramNamePref) {}

//-----------------------------------------------------------------------------

FunctionTreeModel::Channel::~Channel() {
  m_model->onChannelDestroyed(this);
  if (m_isActive) getParam()->removeObserver(this);
}

//-----------------------------------------------------------------------------

void *FunctionTreeModel::Channel::getInternalPointer() const {
  return this->m_param.getPointer();
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::Channel::isAnimated() const {
  return m_param->hasKeyframes();
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::Channel::isIgnored() const {
  if (!isAnimated()) return false;
  TDoubleParam *dp = dynamic_cast<TDoubleParam *>(m_param.getPointer());
  if (!dp) return false;
  FunctionTreeView *view = dynamic_cast<FunctionTreeView *>(m_model->m_view);
  if (!view) return false;
  return view->getXsheetHandle()->getXsheet()->isReferenceManagementIgnored(dp);
}

//-----------------------------------------------------------------------------

QVariant FunctionTreeModel::Channel::data(int role) const {
  if (role == Qt::DecorationRole) {
    static QIcon paramIgnoredOn(":Resources/paramignored_on.svg");
    static QIcon paramIgnoredOff(":Resources/paramignored_off.svg");

    if (isIgnored()) return isActive() ? paramIgnoredOn : paramIgnoredOff;

    QPixmap pixmap(10, 10);
    QColor color;
    QString name        = getShortName();
    std::string strName = name.toStdString();
    if (name == "X")
      color = QColor("firebrick");
    else if (name == "Y")
      color = QColor("limegreen");
    else if (name == "Z")
      color = QColor("deepskyblue");
    else if (name == "SO")
      color = QColor("hotpink");
    else if (name == "Rotation")
      color = QColor("darkorchid");
    else if (name == "Scale")
      color = QColor("gold");
    else if (name == "Scale H")
      color = QColor("gold");
    else if (name == "Scale V")
      color = QColor("gold");
    else if (name == "Shear H")
      color = QColor("darkorange");
    else if (name == "Shear V")
      color = QColor("darkorange");
    else if (name == "Drawing #")
      color = QColor("lightgreen"); 
    else if (name == "posPath")
      color = QColor("darksalmon");
    else
      color = QColor("darkcyan");
    if (!isActive())
      color = QColor("dimgray");
    else if (!m_param->hasKeyframes())
      color.setAlpha(100);
    pixmap.fill(color);

    QIcon param(pixmap);
    return param;

  } else if (role == Qt::DisplayRole) {
    if (m_param->hasUILabel()) {
      return QString::fromStdString(m_param->getUILabel());
    }
    std::string name            = m_paramNamePref + m_param->getName();
    std::wstring translatedName = TStringTable::translate(name);
    if (m_fxId.size() > 0)
      return QString::fromStdWString(translatedName + L" (" + m_fxId + L")");
    return QString::fromStdWString(translatedName);
  } else if (role == Qt::ForegroundRole) {
    // 130221 iwasawa
    FunctionTreeView *view = dynamic_cast<FunctionTreeView *>(m_model->m_view);
    if (!view) return QColor(Qt::black);
    return (isCurrent()) ? view->getViewer()->getCurrentTextColor()
                         : view->getTextColor();
  } else if (role == Qt::ToolTipRole) {
    if (m_param->hasKeyframes()) {
      TDoubleParam *dp = dynamic_cast<TDoubleParam *>(m_param.getPointer());
      FunctionTreeView *view =
          dynamic_cast<FunctionTreeView *>(m_model->m_view);
      if (dp && view &&
          view->getXsheetHandle()->getXsheet()->isReferenceManagementIgnored(
              dp))
        return tr(
            "Some key(s) in this parameter loses original reference in "
            "expression.\nManually changing any keyframe will clear the "
            "warning.");
    }
    return TreeModel::Item::data(role);
  } else
    return TreeModel::Item::data(role);
}

//-----------------------------------------------------------------------------

QString FunctionTreeModel::Channel::getShortName() const {
  if (m_param->hasUILabel()) {
    return QString::fromStdString(m_param->getUILabel());
  }
  std::string name            = m_paramNamePref + m_param->getName();
  std::wstring translatedName = TStringTable::translate(name);
  return QString::fromStdWString(translatedName);
}

//-----------------------------------------------------------------------------

QString FunctionTreeModel::Channel::getLongName() const {
  QString name = getShortName();
  if (getChannelGroup()) name = getChannelGroup()->getLongName() + " " + name;
  return name;
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::Channel::setParam(const TParamP &param) {
  if (param.getPointer() == m_param.getPointer()) return;

  TParamP oldParam = m_param;
  m_param          = param;

  if (m_isActive) {
    oldParam->removeObserver(this);
    param->addObserver(this);
  }
}

//-----------------------------------------------------------------------------
/*! in order to show the expression name in the tooltip
 */
QString FunctionTreeModel::Channel::getExprRefName() const {
  QString tmpName;
  if (m_param->hasUILabel())
    tmpName = QString::fromStdString(m_param->getUILabel());
  else
    tmpName = QString::fromStdWString(
        TStringTable::translate(m_paramNamePref + m_param->getName()));
  /*--- stage
   * objectパラメータの場合、TableにあわせてtmpNameを代表的なExpression名にする---*/
  StageObjectChannelGroup *stageGroup =
      dynamic_cast<StageObjectChannelGroup *>(m_group);
  if (stageGroup) {
    if (tmpName == "Y")
      tmpName = "y";
    else if (tmpName == "X")
      tmpName = "x";
    else if (tmpName == "Z")
      tmpName = "z";
    else if (tmpName == "Rotation")
      tmpName = "rot";
    else if (tmpName == "Scale H")
      tmpName = "sx";
    else if (tmpName == "Scale V")
      tmpName = "sy";
    else if (tmpName == "Shear H")
      tmpName = "shx";
    else if (tmpName == "Shear V")
      tmpName = "shy";
    else if (tmpName == "Drawing #")
      tmpName = "drawingnumber";
    else if (tmpName == "posPath")
      tmpName = "path";
    else if (tmpName == "Scale")
      tmpName = "sc";
    else
      tmpName = tmpName.toLower();

    return stageGroup->getIdName() + QString(".") + tmpName;
  }

  // expression for fx parameters
  // see txsheetexpr.cpp for generation of actual tokens

  tmpName.remove(QChar(' '), Qt::CaseInsensitive);
  tmpName.remove(QChar('/'));
  tmpName.remove(QChar('-'));
  tmpName = tmpName.toLower();

  FunctionTreeModel::ChannelGroup *parentGroup =
      dynamic_cast<FunctionTreeModel::ChannelGroup *>(getParent());
  if (parentGroup) {
    return QString("fx.") + parentGroup->getIdName() + QString(".") + tmpName;
  } else
    return "";
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::Channel::setIsActive(bool active) {
  if (active == m_isActive) return;

  m_isActive = active;
  m_model->refreshActiveChannels();
  if (m_isActive) {
    getParam()->addObserver(this);
    /*--- これが最初にVisibleにしたChannelの場合 ---*/
    if (!m_model->m_currentChannel) {
      setIsCurrent(true);
      m_model->emitCurveSelected(getParam());
    }
  } else {
    getParam()->removeObserver(this);
    if (isCurrent()) {
      setIsCurrent(false);
      m_model->emitCurveSelected(0);
    }
  }

  m_model->emitDataChanged(this);
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::Channel::isCurrent() const {
  return m_model->m_currentChannel == this;
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::Channel::setIsCurrent(bool current) {
  Channel *oldCurrent = m_model->m_currentChannel;
  if (current) {
    // this channel must become the current
    if (oldCurrent == this) return;  // already it is: nothing to do
    m_model->m_currentChannel = this;

    // change the current fx if the FxChannelGroup is clicked
    FxChannelGroup *fxGroup = dynamic_cast<FxChannelGroup *>(m_group);
    if (fxGroup && m_model->getFxHandle()) {
      m_model->getFxHandle()->setFx(fxGroup->getFx());
    }
    // or, change the current object if the stageObjectChannelGroup is clicked
    else {
      StageObjectChannelGroup *stageObjectGroup =
          dynamic_cast<StageObjectChannelGroup *>(m_group);
      if (stageObjectGroup && m_model->getObjectHandle()) {
        m_model->getObjectHandle()->setObjectId(
            stageObjectGroup->getStageObject()->getId());
      }
    }

    // the current channel must be active
    if (!m_isActive) {
      m_isActive = true;
      m_model->refreshActiveChannels();
      getParam()->addObserver(this);
    }
    // refresh the old current (if !=0) and the new one
    if (oldCurrent) m_model->emitDataChanged(oldCurrent);
    m_model->emitDataChanged(this);
    m_model->emitCurveSelected(getParam());
    // scroll the column to ensure visible the current channel
    m_model->emitCurrentChannelChanged(this);
  } else {
    // this channel is not the current anymore
    if (oldCurrent != this) return;  // it was not: nothing to do
    m_model->m_currentChannel = 0;
    // refresh the channel
    m_model->emitDataChanged(this);
  }
}

//-----------------------------------------------------------------------------

bool FunctionTreeModel::Channel::isHidden() const {
  return getChannelGroup()->getShowFilter() ==
             ChannelGroup::ShowAnimatedChannels &&
         !getParam()->hasKeyframes();
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::Channel::onChange(const TParamChange &ch) {
  m_model->onChange(ch);
}

//=============================================================================
//
// FunctionTreeModel
//
//-----------------------------------------------------------------------------

FunctionTreeModel::FunctionTreeModel(FunctionTreeView *parent)
    : TreeModel(parent)
    , m_currentChannel(0)
    , m_stageObjects(0)
    , m_fxs(0)
    , m_currentStageObject(0)
    , m_currentFx(0)
    , m_paramsChanged(false) {}

//-----------------------------------------------------------------------------

FunctionTreeModel::~FunctionTreeModel() {
  // I must delete items here (not in TreeModel::~TreeModel()).
  // Channel::~Channel() refers to the model, which should be alive.
  setRootItem(0);
  if (m_currentFx) m_currentFx->release();
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::refreshData(TXsheet *xsh) {
  m_activeChannels.clear();
  Channel *currentChannel = m_currentChannel;

  beginRefresh();
  {
    if (!getRootItem()) {
      setRootItem(new ChannelGroup("Root"));

      if (xsh) {
        getRootItem()->appendChild(m_stageObjects =
                                       new ChannelGroup(tr("Stage")));
        getRootItem()->appendChild(m_fxs = new ChannelGroup(tr("FX")));

        assert(getRootItem()->getChildCount() == 2);
        assert(getRootItem()->getChild(0) == m_stageObjects);
        assert(getRootItem()->getChild(1) == m_fxs);
      }
    }

    if (xsh) {
      refreshStageObjects(xsh);
      refreshFxs(xsh);
    }

    refreshActiveChannels();
  }
  endRefresh();

  if (m_currentChannel != currentChannel) emit curveSelected(0);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::refreshStageObjects(TXsheet *xsh) {
  static const int channelIds[TStageObject::T_ChannelCount] = {
      TStageObject::T_X,      TStageObject::T_Y,      TStageObject::T_Z,
      TStageObject::T_SO,     TStageObject::T_Path,   TStageObject::T_Angle,
      TStageObject::T_ScaleX, TStageObject::T_ScaleY, TStageObject::T_Scale,
      TStageObject::T_ShearX, TStageObject::T_ShearY, TStageObject::T_DrawingNumber};  // Explicitly ordered
                                                        // channels

  // Retrieve all (not-empty) root stage objects, and add them in the tree model
  QList<TreeModel::Item *> newItems;
  TStageObjectTree *ptree = xsh->getStageObjectTree();

  int i, iCount = ptree->getStageObjectCount();
  for (i = 0; i < iCount; ++i) {
    TStageObject *pegbar = ptree->getStageObject(i);
    TStageObjectId id    = pegbar->getId();
    int col              = id.getIndex();
    if (id.isColumn() && (xsh->isColumnEmpty(col) || xsh->isFolderColumn(col) ||
                          xsh->isPegbarColumn(col)))
      continue;
    if (id == ptree->getMotionPathViewerId()) continue;

    newItems.push_back(new StageObjectChannelGroup(pegbar));
  }

  /*--- newItemsの中で、これまでのChildrenに無いものだけ
  m_stageObjectsの子に追加。既に有るものはnewChildrenから除外---*/
  m_stageObjects->setChildren(newItems);

  // Add channels to the NEW stage entries (see the above call to setChildren())
  iCount = newItems.size();
  for (i = 0; i < iCount; ++i) {
    StageObjectChannelGroup *pegbarItem =
        dynamic_cast<StageObjectChannelGroup *>(newItems[i]);

    TStageObject *stageObject = pegbarItem->getStageObject();
    TStageObjectId stageObjId = stageObject->getId();

    // Add the standard stage object channels
    int j, jCount = TStageObject::T_ChannelCount;
    // making each channel of pegbar
    for (j = 0; j < jCount; ++j) {
      TDoubleParam *param =
          stageObject->getParam((TStageObject::Channel)channelIds[j]);
      if (channelIds[j] == TStageObject::T_DrawingNumber &&
          !stageObjId.isColumn())
        continue;
      Channel *channel = new Channel(this, param);

      pegbarItem->appendChild(channel);
      channel->setChannelGroup(pegbarItem);
    }

    pegbarItem->applyShowFilter();
  }

  // As plastic deformations are stored in stage objects, refresh them if
  // necessary
  refreshPlasticDeformations();
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::refreshFxs(TXsheet *xsh) {
  std::vector<TFx *> fxs;
  xsh->getFxDag()->getFxs(fxs);
  for (int i = 0; i < xsh->getColumnCount(); i++) {
    TXshZeraryFxColumn *zc =
        dynamic_cast<TXshZeraryFxColumn *>(xsh->getColumn(i));
    if (!zc) continue;
    fxs.push_back(zc->getZeraryColumnFx()->getZeraryFx());
  }

  // sort items by fxId
  for (int j = 1; j < (int)fxs.size(); j++) {
    int index = j;
    while (index > 0 &&
           QString::localeAwareCompare(
               QString::fromStdWString(fxs[index - 1]->getFxId()),
               QString::fromStdWString(fxs[index]->getFxId())) > 0) {
      std::swap(fxs[index - 1], fxs[index]);
      index = index - 1;
    }
  }

  QList<TreeModel::Item *> newItems;
  int i;
  for (i = 0; i < (int)fxs.size(); i++) {
    TFx *fx = fxs[i];
    if (!fx) continue;
    TParamContainer *params = fx->getParams();
    bool hasChannel         = false;
    int j;
    for (j = 0; j < params->getParamCount(); j++)
      if (0 != dynamic_cast<TDoubleParam *>(params->getParam(j)) ||
          0 != dynamic_cast<TPointParam *>(params->getParam(j)) ||
          0 != dynamic_cast<TRangeParam *>(params->getParam(j)) ||
          0 != dynamic_cast<TPixelParam *>(params->getParam(j))) {
        hasChannel = true;
        break;
      }
    if (hasChannel) newItems.push_back(new FxChannelGroup(fxs[i]));
  }
  m_fxs->setChildren(newItems);
  // Add channels to new fxs (only for those actually added: see setChildren())

  for (i = 0; i < (int)newItems.size(); i++) {
    TreeModel::Item *item  = newItems[i];
    FxChannelGroup *fxItem = dynamic_cast<FxChannelGroup *>(item);
    assert(fxItem);
    if (!fxItem) continue;
    TFx *fx = fxItem->getFx();
    assert(fx);
    if (!fx) continue;
    TMacroFx *macroFx = dynamic_cast<TMacroFx *>(fx);
    if (macroFx) {
      const std::vector<TFxP> &macroFxs = macroFx->getFxs();
      int j;
      for (j = 0; j < (int)macroFxs.size(); j++) {
        TParamContainer *params = macroFxs[j]->getParams();
        addChannels(macroFxs[j].getPointer(), fxItem, params);
      }
    } else {
      TParamContainer *params = fx->getParams();
      addChannels(fx, fxItem, params);
    }
    fxItem->applyShowFilter();
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::refreshPlasticDeformations() {
  // Refresh ALL stage object items (including OLD ones)
  int i, iCount = m_stageObjects->getChildCount();
  for (i = 0; i < iCount; ++i) {
    // Add the eventual Plastic channels group
    StageObjectChannelGroup *stageItem =
        static_cast<StageObjectChannelGroup *>(m_stageObjects->getChild(i));

    TStageObject *stageObject = stageItem->getStageObject();

    const PlasticSkeletonDeformationP &sd =
        stageObject->getPlasticSkeletonDeformation();
    FunctionTreeModel::ChannelGroup *&plasticGroup = stageItem->m_plasticGroup;

    if (sd || plasticGroup) {
      if (!plasticGroup) {
        // Add a group
        plasticGroup = new ChannelGroup(tr("Plastic Skeleton"));
        stageItem->appendChild(plasticGroup);
      }

      // Prepare each vertex deformation
      QList<TreeModel::Item *> plasticItems;

      if (sd) {
        SkD::vd_iterator vdt, vdEnd;
        sd->vertexDeformations(vdt, vdEnd);

        for (; vdt != vdEnd; ++vdt) {
          const QString *str = (*vdt).first;

          QList<TreeModel::Item *>::iterator it =
              std::lower_bound(plasticItems.begin(), plasticItems.end(), *str,
                               SkVDChannelGroup::compareStr);

          plasticItems.insert(it, new SkVDChannelGroup(str, stageItem));
        }

        // Add the channel corresponding to the skeleton id
        {
          Channel *skelIdsChannel =
              new Channel(this, sd->skeletonIdsParam().getPointer());

          plasticItems.insert(plasticItems.begin(), skelIdsChannel);
          skelIdsChannel->setChannelGroup(plasticGroup);
        }
      }

      if (plasticItems.empty()) plasticGroup->setIsOpen(false);

      // Add the vertex deformations group (this needs to be done BEFORE adding
      // the actual
      // channels, seemingly due to a limitation in the TreeModel
      // implementation)
      plasticGroup->setChildren(plasticItems);

      int pi,
          piCount =
              plasticItems.size();  // NOTE: plasticItems now stores only PART
      for (pi = 0; pi != piCount;
           ++pi)  // of the specified items - those considered
      {           // 'new' by internal pointer comparison...
        SkVDChannelGroup *vxGroup =
            dynamic_cast<SkVDChannelGroup *>(plasticItems[pi]);
        if (!vxGroup) continue;

        SkVD *skvd =
            sd->vertexDeformation(vxGroup->ChannelGroup::getShortName());

        for (int k = 0; k < SkVD::PARAMS_COUNT; ++k) {
          // Add channel in the vertex deformation
          Channel *channel = new Channel(this, skvd->m_params[k].getPointer());
          channel->setChannelGroup(vxGroup);

          vxGroup->appendChild(channel);
        }

        vxGroup->applyShowFilter();
      }

      plasticGroup->applyShowFilter();
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::addParameter(ChannelGroup *group,
                                     const std::string &prefixString,
                                     const std::wstring &fxId, TParam *param) {
  if (TDoubleParam *dp = dynamic_cast<TDoubleParam *>(param)) {
    Channel *channel = new Channel(this, dp, prefixString, fxId);

    group->appendChild(channel);
    channel->setChannelGroup(group);
  } else if (dynamic_cast<TPointParam *>(param) ||
             dynamic_cast<TRangeParam *>(param) ||
             dynamic_cast<TPixelParam *>(param)) {
    TParamSet *paramSet = dynamic_cast<TParamSet *>(param);
    assert(paramSet);

    std::string paramName = prefixString + param->getName();

    ChannelGroup *paramChannel = new ParamChannelGroup(param, fxId, paramName);
    group->appendChild(paramChannel);

    TPixelParam *pixParam = dynamic_cast<TPixelParam *>(param);

    int p, paramCount = paramSet->getParamCount();
    for (p = 0; p != paramCount; ++p) {
      TDoubleParam *dp =
          dynamic_cast<TDoubleParam *>(paramSet->getParam(p).getPointer());
      if (!dp ||
          (pixParam && !pixParam->isMatteEnabled() && p == paramCount - 1))
        continue;

      Channel *channel = new Channel(this, dp, "", fxId);

      paramChannel->appendChild(channel);
      channel->setChannelGroup(group);
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::addChannels(TFx *fx, ChannelGroup *groupItem,
                                    TParamContainer *params) {
  FxChannelGroup *fxItem = static_cast<FxChannelGroup *>(groupItem);

  std::wstring fxId = L"";
  TMacroFx *macro   = dynamic_cast<TMacroFx *>(fxItem->getFx());
  if (macro) fxId = fx->getFxId();

  const std::string &paramNamePref = fx->getFxType() + ".";

  int p, pCount = params->getParamCount();
  for (p = 0; p != pCount; ++p) {
    // hidden parameter are not displayed in the tree
    if (params->isParamHidden(p)) continue;
    addParameter(fxItem, paramNamePref, fxId, params->getParam(p));
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::addActiveChannels(TreeModel::Item *item) {
  assert(item);

  if (Channel *channel = dynamic_cast<Channel *>(item)) {
    if (channel->isActive()) m_activeChannels.push_back(channel);
  } else
    for (int i = 0; i < item->getChildCount(); i++)
      addActiveChannels(item->getChild(i));
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::refreshActiveChannels() {
  m_activeChannels.clear();

  if (m_stageObjects) addActiveChannels(m_stageObjects);

  if (m_fxs) addActiveChannels(m_fxs);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::onChannelDestroyed(Channel *channel) {
  if (channel == m_currentChannel) m_currentChannel = 0;
}

//-----------------------------------------------------------------------------

FunctionTreeModel::Channel *FunctionTreeModel::getActiveChannel(
    int index) const {
  if (index < 0 || index >= (int)m_activeChannels.size())
    return 0;
  else
    return m_activeChannels[index];
}

//-----------------------------------------------------------------------------

int FunctionTreeModel::getColumnIndexByCurve(TDoubleParam *param) const {
  for (int i = 0; i < (int)m_activeChannels.size(); i++) {
    if (m_activeChannels[i]->getParam() == param) return i;
  }
  return -1;
}

//-----------------------------------------------------------------------------

FunctionTreeModel::ChannelGroup *FunctionTreeModel::getStageObjectChannel(
    int index) const {
  if (index < 0 || index >= (int)m_stageObjects->getChildCount())
    return 0;
  else
    return dynamic_cast<FunctionTreeModel::ChannelGroup *>(
        m_stageObjects->getChild(index));
}

//-----------------------------------------------------------------------------

FunctionTreeModel::ChannelGroup *FunctionTreeModel::getFxChannel(
    int index) const {
  if (index < 0 || index >= (int)m_fxs->getChildCount())
    return 0;
  else
    return dynamic_cast<FunctionTreeModel::ChannelGroup *>(
        m_fxs->getChild(index));
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::onChange(const TParamChange &tpc) {
  if (!m_paramsChanged) {
    m_paramsChanged = true;

    struct Func final : public TFunctorInvoker::BaseFunctor {
      FunctionTreeModel *m_obj;
      // Use a copy of 'TParamChange' since callers declare
      // and free this value on the stack,
      // so we can't ensure its valid later on when the notifier executes.
      const TParamChange m_tpc;

      Func(FunctionTreeModel *obj, const TParamChange *tpc)
          : m_obj(obj), m_tpc(*tpc) {}
      void operator()() override { m_obj->onParamChange(m_tpc.m_dragging); }
    };

    QMetaObject::invokeMethod(TFunctorInvoker::instance(), "invoke",
                              Qt::QueuedConnection,
                              Q_ARG(void *, new Func(this, &tpc)));
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::onParamChange(bool isDragging) {
  m_paramsChanged = false;

  emit curveChanged(isDragging);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::resetAll() {
  beginResetModel();
  m_activeChannels.clear();

  TreeModel::Item *root_item = getRootItem();
  setRootItem_NoFree(NULL);

  m_stageObjects = 0;
  m_fxs          = 0;

  beginRefresh();
  refreshActiveChannels();
  endRefresh();

  // postpone until after refresh,
  // since its members are used for reference.
  delete root_item;

  m_currentChannel = 0;

  endResetModel();
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::setCurrentFx(TFx *fx) {
  TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx);
  if (zcfx) fx = zcfx->getZeraryFx();
  if (fx != m_currentFx) {
    if (fx) fx->addRef();
    if (m_currentFx) m_currentFx->release();
    m_currentFx = fx;
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::applyShowFilters() {
  // WARNING: This is implemented BAD - notice that the get*() functions below
  //          DO NOT ACTUALLY RETURN CHANNELS, but rather the child
  //          ChannelGROUPS!
  //
  //          This means that these show filters are presumably applied only to
  //          the FIRST LEVEL OF PARAMETERS...!

  // Every group row is decided on every pass, never left as the last search
  // put it: the tree is rebuilt under us on scene and column changes, so
  // un-hiding by index at the moment the search is cleared would strand any
  // row whose position moved in between.
  const bool searching = !m_searchFilter.isEmpty();

  if (m_stageObjects) {
    int so, soCount = m_stageObjects->getChildCount();
    for (so = 0; so != soCount; ++so) {
      ChannelGroup *group = getStageObjectChannel(so);
      group->applyShowFilter();
      const bool matches = !searching || group->matchesSearch(m_searchFilter);
      setRowHidden(so, m_stageObjects->createIndex(), !matches);
      // Open what matched: a hit on a channel is invisible while its object is
      // still folded, which reads as "the search found nothing".
      if (searching && matches) setExpandedItem(group->createIndex(), true);
    }
  }

  if (m_fxs) {
    int fx, fxCount = m_fxs->getChildCount();
    for (fx = 0; fx != fxCount; ++fx) {
      ChannelGroup *group = getFxChannel(fx);
      group->applyShowFilter();
      const bool matches = !searching || group->matchesSearch(m_searchFilter);
      setRowHidden(fx, m_fxs->createIndex(), !matches);
      if (searching && matches) setExpandedItem(group->createIndex(), true);
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::setAllShowFilters(
    ChannelGroup::ShowFilter showFilter) {
  if (m_stageObjects)
    for (int i = 0; i < m_stageObjects->getChildCount(); ++i)
      getStageObjectChannel(i)->setShowFilter(showFilter);

  if (m_fxs)
    for (int i = 0; i < m_fxs->getChildCount(); ++i)
      getFxChannel(i)->setShowFilter(showFilter);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::setSearchFilter(const QString &searchString) {
  const QString trimmed = searchString.trimmed();
  if (trimmed == m_searchFilter) return;

  m_searchFilter = trimmed;
  applyShowFilters();
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::ChannelGroup::rememberShownChannels() {
  std::vector<FunctionTreeModel::Channel *> channels;
  FunctionTreeModel::collectChannels(this, channels);

  QSet<QString> shown;
  for (FunctionTreeModel::Channel *channel : channels)
    if (channel->isActive()) shown.insert(channel->getLongName());

  // Nothing on screen means there is nothing to learn -- and overwriting here
  // would erase the memory the moment a column is hidden, which is exactly
  // when it has to survive.
  if (!shown.isEmpty()) m_shownChannelNames = shown;
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::ChannelGroup::restoreShownChannels() {
  std::vector<FunctionTreeModel::Channel *> channels;
  FunctionTreeModel::collectChannels(this, channels);

  if (m_shownChannelNames.isEmpty()) {
    // Never shown: the curves that carry keys are the sensible opening move.
    // Switching on all dozen channels of a column would bury them.
    for (FunctionTreeModel::Channel *channel : channels)
      if (channel->getParam() && channel->getParam()->hasKeyframes())
        channel->setIsActive(true);
    return;
  }

  for (FunctionTreeModel::Channel *channel : channels)
    if (m_shownChannelNames.contains(channel->getLongName()))
      channel->setIsActive(true);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::setAllChannelsActive(bool active) {
  // Turning everything ON means bringing each column back to what it was
  // showing, not switching on every curve it owns: a rig has hundreds, and
  // "Show All Columns" is about the columns, not about their contents.
  if (active) {
    if (m_stageObjects)
      for (int i = 0; i < m_stageObjects->getChildCount(); ++i)
        getStageObjectChannel(i)->restoreShownChannels();
    if (m_fxs)
      for (int i = 0; i < m_fxs->getChildCount(); ++i)
        getFxChannel(i)->restoreShownChannels();
    return;
  }

  // Turning them off: remember first, so the trip back is possible.
  if (m_stageObjects)
    for (int i = 0; i < m_stageObjects->getChildCount(); ++i)
      getStageObjectChannel(i)->rememberShownChannels();
  if (m_fxs)
    for (int i = 0; i < m_fxs->getChildCount(); ++i)
      getFxChannel(i)->rememberShownChannels();

  if (m_stageObjects) m_stageObjects->setChildrenAllActive(false);
  if (m_fxs) m_fxs->setChildrenAllActive(false);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::collectChannels(TreeModel::Item *item,
                                        std::vector<Channel *> &channels) {
  if (!item) return;

  if (Channel *channel = dynamic_cast<Channel *>(item)) {
    channels.push_back(channel);
    return;
  }
  // A folder stands for everything under it, nested groups included: picking a
  // column has to mean the column, not the handful of channels that happen to
  // sit at its first level.
  for (int i = 0; i < item->getChildCount(); ++i)
    collectChannels(item->getChild(i), channels);
}

//-----------------------------------------------------------------------------

TreeModel::Item *FunctionTreeModel::columnScopeOf(TreeModel::Item *item) {
  // Depth 2 is the stage object / fx level: Root(0) > Stage(1) > Col1(2) >
  // channels, with nested groups such as Plastic Skeleton sitting below.
  while (item && item->getDepth() > 2) item = item->getParent();
  return (item && item->getDepth() == 2) ? item : 0;
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::showOnlyItems(const QList<TreeModel::Item *> &items,
                                      TreeModel::Item *scope) {
  if (items.isEmpty()) return;

  // Gather first, clear second: a channel that is both inside the scope and in
  // the selection would be switched off again if we cleared as we went.
  std::vector<Channel *> channels;
  for (TreeModel::Item *item : items) collectChannels(item, channels);

  setScopeShown(scope, false);
  for (Channel *channel : channels) channel->setIsActive(true);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::setScopeShown(TreeModel::Item *scope, bool shown) {
  if (!scope) {
    setAllChannelsActive(shown);
    return;
  }
  // Aimed at ONE column, "show all" means all of its curves -- that is what
  // the entry says. The remembering-and-restoring is for "Show All Columns",
  // which is about columns and must not decide their contents for them.
  if (!shown)
    if (ChannelGroup *group = dynamic_cast<ChannelGroup *>(scope))
      group->rememberShownChannels();

  std::vector<Channel *> channels;
  collectChannels(scope, channels);
  for (Channel *channel : channels) channel->setIsActive(shown);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::setInterpolationOfItems(
    const QList<TreeModel::Item *> &items, int keyframeType) {
  std::vector<Channel *> channels;
  for (TreeModel::Item *item : items) collectChannels(item, channels);
  if (channels.empty()) return;

  // One block for the lot: retyping six curves is one decision, and it has to
  // come back with one undo.
  TUndoManager::manager()->beginBlock();
  for (Channel *channel : channels) {
    TDoubleParam *curve = channel ? channel->getParam() : 0;
    if (!curve) continue;
    const int count = curve->getKeyframeCount();
    // Every segment, i.e. every keyframe but the last: the last one governs
    // nothing, and writing a type there would only park it for a later move to
    // turn into a real segment.
    for (int k = 0; k < count - 1; ++k)
      KeyframeSetter(curve, k).setType((TDoubleKeyframe::Type)keyframeType);
  }
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::showAnimatedChannelsOf(TreeModel::Item *scope) {
  if (!scope) return;
  std::vector<Channel *> channels;
  collectChannels(scope, channels);
  // Only what has keyframes: a column carries a dozen channels and most of
  // them are flat, so switching all of them on would bury the two curves you
  // opened the column to look at.
  for (Channel *channel : channels)
    if (channel->getParam() && channel->getParam()->hasKeyframes())
      channel->setIsActive(true);
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::setItemsShown(const QList<TreeModel::Item *> &items,
                                      bool shown) {
  std::vector<Channel *> channels;
  for (TreeModel::Item *item : items) collectChannels(item, channels);
  for (Channel *channel : channels) channel->setIsActive(shown);
}

//-----------------------------------------------------------------------------

FunctionTreeModel::ChannelGroup *
FunctionTreeModel::getStageObjectChannelGroup(TStageObject *obj) const {
  if (!obj || !m_stageObjects) return 0;
  int so, soCount = m_stageObjects->getChildCount();
  for (so = 0; so != soCount; ++so) {
    StageObjectChannelGroup *group =
        dynamic_cast<StageObjectChannelGroup *>(getStageObjectChannel(so));
    if (group && group->getStageObject() == obj) return group;
  }
  return 0;
}

//-----------------------------------------------------------------------------

FunctionTreeModel::ChannelGroup *FunctionTreeModel::getFxChannelGroup(
    TFx *fx) const {
  if (!fx || !m_fxs) return 0;
  int i, count = m_fxs->getChildCount();
  for (i = 0; i != count; ++i) {
    FxChannelGroup *group = dynamic_cast<FxChannelGroup *>(getFxChannel(i));
    if (group && group->getFx() == fx) return group;
  }
  return 0;
}

//-----------------------------------------------------------------------------

void FunctionTreeModel::addParameter(TParam *parameter,
                                     const TFilePath &folder) {
  struct locals {
    static void locateExistingRoot(ChannelGroup *&root, TFilePath &fp) {
      std::wstring firstName;
      TFilePath tempFp;

      while (!fp.isEmpty()) {
        // Get the path's first name
        fp.split(firstName, tempFp);

        // Search a matching channel group in root's children
        int c, cCount = root->getChildCount();
        for (c = 0; c != cCount; ++c) {
          if (ChannelGroup *group =
                  dynamic_cast<ChannelGroup *>(root->getChild(c))) {
            if (group->getShortName().toStdWString() == firstName) {
              root = group, fp = tempFp;
              break;
            }
          }
        }

        if (c == cCount) break;
      }
    }

    static void addFolders(ChannelGroup *&group, TFilePath &fp) {
      std::wstring firstName;
      TFilePath tempFp;

      while (!fp.isEmpty()) {
        fp.split(firstName, tempFp);

        ChannelGroup *newGroup =
            new ChannelGroup(QString::fromStdWString(firstName));
        group->appendChild(newGroup);

        group = newGroup, fp = tempFp;
      }
    }
  };  // locals

  // Search for the furthest existing channel group chain leading to our folder
  ChannelGroup *group = static_cast<ChannelGroup *>(getRootItem());
  assert(group);

  TFilePath path = folder;
  locals::locateExistingRoot(group, path);

  // If the chain interrupts prematurely, create new groups up to the required
  // folder
  if (!path.isEmpty()) locals::addFolders(group, path);

  assert(path.isEmpty());

  // Add the parameter to the last group
  addParameter(group, "", L"", parameter);
}

//=============================================================================
//
// FunctionTreeView
//
//-----------------------------------------------------------------------------

FunctionTreeView::FunctionTreeView(FunctionViewer *parent)
    : TreeView(parent)
    , m_scenePath()
    , m_clickedItem(0)
    , m_draggingChannel(0)
    , m_viewer(parent) {
  assert(parent);

  setModel(new FunctionTreeModel(this));

  setObjectName("FunctionEditorTree");
  // Multi-selection: pick several columns with Shift/Ctrl and the visibility
  // commands act on all of them. TreeView::mousePressEvent already forwards to
  // QTreeView and already has an ExtendedSelection branch, so Qt does the
  // Shift/Ctrl arithmetic and the highlighting for us.
  setSelectionMode(QAbstractItemView::ExtendedSelection);

  connect(this, SIGNAL(pressed(const QModelIndex &)), this,
          SLOT(onActivated(const QModelIndex &)));

  setFocusPolicy(Qt::NoFocus);
  setIndentation(14);
  setAlternatingRowColors(true);
}

//-----------------------------------------------------------------------------

void FunctionTreeView::onActivated(const QModelIndex &index) {
  enum {
    NO_CHANNELS       = 0x0,
    ACTIVE_CHANNELS   = 0x1,
    INACTIVE_CHANNELS = 0x2,
    HAS_CHANNELS      = ACTIVE_CHANNELS | INACTIVE_CHANNELS
  };

  if (!index.isValid()) return;

  FunctionTreeModel *ftModel = (FunctionTreeModel *)model();
  if (!ftModel) return;

  std::vector<FunctionTreeModel::Channel *> childChannels;
  std::vector<FunctionTreeModel::ChannelGroup *> channelGroups;

  // Scan for already active children - to decide whether to activate or
  // deactivate them
  int activeFlag = NO_CHANNELS;

  TreeModel::Item *item =
      static_cast<TreeModel::Item *>(index.internalPointer());
  if (item) {
    int c, cCount = item->getChildCount();
    for (c = 0; c != cCount; ++c) {
      FunctionTreeModel::Channel *channel =
          dynamic_cast<FunctionTreeModel::Channel *>(item->getChild(c));

      if (!channel) {
        FunctionTreeModel::ChannelGroup *channelGroup =
            dynamic_cast<FunctionTreeModel::ChannelGroup *>(item->getChild(c));
        if (!channelGroup) continue;
        channelGroups.push_back(channelGroup);
        continue;
      }
      if (channel->isHidden()) continue;

      childChannels.push_back(channel);

      activeFlag |= (channel->isActive() ? ACTIVE_CHANNELS : INACTIVE_CHANNELS);
    }
  }

  // Open the item (ie show children) if it was closed AND not all its children
  // were active
  bool someInactiveChannels = (activeFlag != ACTIVE_CHANNELS);

//  if (someInactiveChannels && !isExpanded(index)) {
//    setExpanded(index, true);
//    ftModel->onExpanded(index);
//  }

  if (item) {
    if (!childChannels.empty()) {
      // Activate child channels if there is some inactive channel - deactivate
      // otherwise
      int c, cCount = childChannels.size();
      for (c = 0; c != cCount; ++c)
        childChannels[c]->setIsActive(someInactiveChannels);

      for (int i = 0; i < (int)channelGroups.size(); i++)
        channelGroups[i]->setChildrenAllActive(someInactiveChannels);

      update();
    } else {
      // There was no child channel. Try to activate children groups.
      int c, cCount = item->getChildCount();
      for (c = 0; c != cCount; ++c)
        onActivated(item->getChild(c)->createIndex());
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeView::mouseDoubleClickEvent(QMouseEvent *event) {
  emit(fit());
}

//-----------------------------------------------------------------------------

void FunctionTreeView::onClick(TreeModel::Item *item, const QPoint &itemPos,
                               QMouseEvent *e) {
  m_draggingChannel = 0;

  // Right-clicking outside the selection makes that row the selection, as
  // everywhere else: otherwise the menu would act on rows the user cannot see
  // highlighted from where they clicked.
  if (e->button() == Qt::RightButton && item && selectionModel()) {
    const QModelIndex index = item->createIndex();
    if (index.isValid() && !selectionModel()->isSelected(index))
      selectionModel()->select(index, QItemSelectionModel::ClearAndSelect |
                                          QItemSelectionModel::Rows);
  }

  // With Ctrl or Shift down the click is a SELECTION gesture and nothing else.
  // Qt has already done the selecting; going on from here would switch the
  // application's current column on the way to picking a second one, and
  // toggle a curve off just because the pointer landed on its icon.
  if (e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) return;
  FunctionTreeModel::Channel *channel =
      dynamic_cast<FunctionTreeModel::Channel *>(item);
  FxChannelGroup *fxChannelGroup = dynamic_cast<FxChannelGroup *>(item);
  StageObjectChannelGroup *stageObjectChannelGroup =
      dynamic_cast<StageObjectChannelGroup *>(item);

  m_clickedItem = channel;

  if (channel) {
    fxChannelGroup = dynamic_cast<FxChannelGroup *>(channel->getParent());
    stageObjectChannelGroup =
        dynamic_cast<StageObjectChannelGroup *>(channel->getParent());

    int x           = itemPos.x();
    m_clickedOnIcon = (0 <= x && x < 20);
    if (x >= 20)
      channel->setIsCurrent(true);
    else if (0 <= x && x < 20) {
      channel->setIsActive(
          (e->button() == Qt::RightButton) ? true : !channel->isActive());
      update();
    }
  }

  if (fxChannelGroup) {
    TFx *fx = fxChannelGroup->getFx();
    emit switchCurrentFx(fx);
  }

  if (stageObjectChannelGroup) {
    TStageObject *obj = stageObjectChannelGroup->getStageObject();
    emit switchCurrentObject(obj);
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeView::onMidClick(TreeModel::Item *item, const QPoint &itemPos,
                                  QMouseEvent *e) {
  FunctionTreeModel::Channel *channel =
      dynamic_cast<FunctionTreeModel::Channel *>(item);
  if (channel && e->button() == Qt::MiddleButton) {
    m_draggingChannel   = channel;
    m_dragStartPosition = e->pos();
  } else
    m_draggingChannel = 0;
}

//-----------------------------------------------------------------------------

void FunctionTreeView::onDrag(TreeModel::Item *item, const QPoint &itemPos,
                              QMouseEvent *e) {
  // middle drag of the channel item can retrieve expression name
  if ((e->buttons() & Qt::MiddleButton) && m_draggingChannel &&
      (e->pos() - m_dragStartPosition).manhattanLength() >=
          QApplication::startDragDistance()) {
    QDrag *drag         = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    mimeData->setText(m_draggingChannel->getExprRefName());
    drag->setMimeData(mimeData);
    static const QPixmap cursorPixmap(":Resources/dragcursor_exp_text.png");
    drag->setDragCursor(cursorPixmap, Qt::MoveAction);
    Qt::DropAction dropAction = drag->exec();
    return;
  }

  FunctionTreeModel::Channel *channel =
      dynamic_cast<FunctionTreeModel::Channel *>(item);
  if (!channel || !m_clickedItem) return;

  // Only when the gesture started on the visibility icon. Started on the name,
  // the drag belongs to the selection -- QTreeView is already sweeping one out
  // underneath -- and painting the on/off state at the same time would make
  // one gesture do two things.
  if (!m_clickedOnIcon) return;

  // i0: item under the current cursor position
  // i1: clicked item
  QModelIndex i0 = channel->createIndex(), i1 = m_clickedItem->createIndex();
  if (!i0.isValid() || !i1.isValid() || i0.parent() != i1.parent()) return;

  if (i0.row() > i1.row()) std::swap(i0, i1);

  FunctionTreeModel *md = static_cast<FunctionTreeModel *>(model());

  bool active = m_clickedItem->isActive();

  for (int row = i0.row(); row <= i1.row(); ++row) {
    if (isRowHidden(row, i0.parent())) continue;

    QModelIndex index = md->index(row, 0, i0.parent());

    TreeModel::Item *chItem =
        static_cast<TreeModel::Item *>(index.internalPointer());
    FunctionTreeModel::Channel *ch =
        dynamic_cast<FunctionTreeModel::Channel *>(chItem);

    if (ch && ch->isActive() != active) {
      ch->setIsActive(active);
      update();
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeView::onRelease() {
  m_clickedItem   = 0;
  m_clickedOnIcon = false;
}

//-----------------------------------------------------------------------------

void FunctionTreeView::openContextMenu(TreeModel::Item *item,
                                       const QPoint &globalPos) {
  if (FunctionTreeModel::Channel *channel =
          dynamic_cast<FunctionTreeModel::Channel *>(item))
    openContextMenu(channel, globalPos);
  else if (FunctionTreeModel::ChannelGroup *group =
               dynamic_cast<FunctionTreeModel::ChannelGroup *>(item))
    openContextMenu(group, globalPos);
}

//-----------------------------------------------------------------------------

QList<TreeModel::Item *> FunctionTreeView::selectedItems() const {
  QList<TreeModel::Item *> items;
  if (!selectionModel()) return items;
  for (const QModelIndex &index : selectionModel()->selectedIndexes()) {
    if (!index.isValid()) continue;
    TreeModel::Item *item =
        static_cast<TreeModel::Item *>(index.internalPointer());
    if (item && !items.contains(item)) items.append(item);
  }
  return items;
}

//-----------------------------------------------------------------------------

namespace {

//! The six visibility commands of the xsheet's column menu, reused here.
//! "Shown" means drawn in the graph; an item stands for every channel under
//! it, so on a folder these read as columns and on a parameter as curves --
//! which is also how they are worded.
class VisibilityActions {
  FunctionTreeView *m_view;
  TreeModel::Item *m_item;
  //! What these commands may switch off. Aimed at a parameter they stay inside
  //! its column, so tidying up one column never disturbs how the others are
  //! displayed; aimed at a column they range over the whole tree. Null = whole
  //! tree.
  TreeModel::Item *m_scope;
  QList<TreeModel::Item *> m_selection;

  QAction m_showThisOnly, m_showSelected, m_showAll;
  QAction m_hideAll, m_hideSelected;

public:
  VisibilityActions(FunctionTreeView *view, TreeModel::Item *item,
                    bool isColumn)
      : m_view(view)
      , m_item(item)
      , m_scope(isColumn ? 0 : FunctionTreeModel::columnScopeOf(item))
      , m_selection(view->selectedItems())
      , m_showThisOnly(isColumn ? FunctionTreeView::tr("Show This Column Only")
                                : FunctionTreeView::tr("Show This Curve Only"),
                       0)
      , m_showSelected(FunctionTreeView::tr("Show Selected"), 0)
      , m_showAll(isColumn ? FunctionTreeView::tr("Show All Columns")
                           : FunctionTreeView::tr("Show All Curves of This Column"),
                  0)
      , m_hideAll(isColumn ? FunctionTreeView::tr("Hide All Columns")
                           : FunctionTreeView::tr("Hide All Curves of This Column"),
                  0)
      , m_hideSelected(FunctionTreeView::tr("Hide Selected"), 0) {
    // The two selection commands would silently do nothing on their own with
    // one row picked, and reading them greyed out says why.
    const bool haveSelection = m_selection.count() > 1;
    m_showSelected.setEnabled(haveSelection);
    m_hideSelected.setEnabled(haveSelection);
  }

  void addTo(QMenu &menu) {
    menu.addAction(&m_showThisOnly);
    menu.addAction(&m_showSelected);
    menu.addAction(&m_showAll);
    menu.addAction(&m_hideAll);
    menu.addAction(&m_hideSelected);
  }

  //! Returns true when \p action was one of ours and has been carried out.
  bool handle(QAction *action) {
    FunctionTreeModel *model =
        dynamic_cast<FunctionTreeModel *>(m_view->model());
    if (!model || !action) return false;

    // "This" is the row under the cursor even when several are selected: it is
    // what the user pointed at, and it is the only reading that makes the
    // command different from "Show Selected" sitting right below it.
    QList<TreeModel::Item *> thisItem;
    thisItem.append(m_item);

    if (action == &m_showThisOnly)
      model->showOnlyItems(thisItem, m_scope);
    else if (action == &m_showSelected)
      model->setItemsShown(m_selection, true);
    else if (action == &m_showAll)
      model->setScopeShown(m_scope, true);
    else if (action == &m_hideAll)
      model->setScopeShown(m_scope, false);
    else if (action == &m_hideSelected)
      model->setItemsShown(m_selection, false);
    else
      return false;

    m_view->update();
    return true;
  }
};

//! "Interpolation >" submenu: retypes every segment of every selected curve at
//! once. Expression, File and Similar Shape are left out on purpose -- each
//! needs parameters of its own per segment, and stamping them wholesale would
//! leave a trail of half-defined segments.
class InterpolationActions {
  FunctionTreeView *m_view;
  QList<QAction *> m_actions;

public:
  InterpolationActions(FunctionTreeView *view) : m_view(view) {}

  void addTo(QMenu &menu) {
    QMenu *sub = menu.addMenu(FunctionTreeView::tr("Interpolation"));

    struct Entry {
      const char *label;
      TDoubleKeyframe::Type type;
    };
    const Entry entries[] = {
        {QT_TR_NOOP("Constant"), TDoubleKeyframe::Constant},
        {QT_TR_NOOP("Linear"), TDoubleKeyframe::Linear},
        {QT_TR_NOOP("Speed In / Speed Out"), TDoubleKeyframe::SpeedInOut},
        {QT_TR_NOOP("Ease In / Ease Out"), TDoubleKeyframe::EaseInOut},
        {QT_TR_NOOP("Ease In / Ease Out %"),
         TDoubleKeyframe::EaseInOutPercentage},
        {QT_TR_NOOP("Exponential"), TDoubleKeyframe::Exponential}};

    for (const Entry &entry : entries) {
      QAction *action = sub->addAction(FunctionTreeView::tr(entry.label));
      action->setData((int)entry.type);
      m_actions.append(action);
    }
  }

  //! Returns true when \p action was one of ours and has been carried out.
  bool handle(QAction *action) {
    if (!action || !m_actions.contains(action)) return false;
    FunctionTreeModel *model =
        dynamic_cast<FunctionTreeModel *>(m_view->model());
    if (!model) return false;

    // The selection always holds the row that was right-clicked: a right-click
    // outside it makes it the selection (see onClick).
    model->setInterpolationOfItems(m_view->selectedItems(),
                                   action->data().toInt());
    m_view->update();
    return true;
  }
};

}  // namespace

//-----------------------------------------------------------------------------

void FunctionTreeView::openContextMenu(FunctionTreeModel::Channel *channel,
                                       const QPoint &globalPos) {
  assert(channel);

  if (!m_viewer) return;

  QMenu menu;

  VisibilityActions visibility(this, channel, false);
  visibility.addTo(menu);
  menu.addSeparator();

  InterpolationActions interpolation(this);
  interpolation.addTo(menu);
  menu.addSeparator();

  QAction saveCurveAction(tr("Save Curve"), 0);
  QAction loadCurveAction(tr("Load Curve"), 0);
  QAction exportDataAction(tr("Export Data"), 0);
  menu.addAction(&saveCurveAction);
  menu.addAction(&loadCurveAction);
  menu.addAction(&exportDataAction);

  QAction *action = menu.exec(globalPos);

  TDoubleParam *curve = channel->getParam();

  if (visibility.handle(action)) return;
  if (interpolation.handle(action)) return;

  if (action == &saveCurveAction)
    m_viewer->emitIoCurve((int)FunctionViewer::eSaveCurve, curve, "");
  else if (action == &loadCurveAction)
    m_viewer->emitIoCurve((int)FunctionViewer::eLoadCurve, curve, "");
  else if (action == &exportDataAction)
    m_viewer->emitIoCurve((int)FunctionViewer::eExportCurve, curve,
                          channel->getLongName().toStdString());
}

//-----------------------------------------------------------------------------

void FunctionTreeView::openContextMenu(FunctionTreeModel::ChannelGroup *group,
                                       const QPoint &globalPos) {
  assert(group);

  QMenu menu;

  // What the GRAPH draws comes first, and is the same set of commands as the
  // xsheet's column visibility menu.
  VisibilityActions visibility(this, group, true);
  visibility.addTo(menu);
  menu.addSeparator();

  InterpolationActions interpolation(this);
  interpolation.addTo(menu);
  menu.addSeparator();

  // Below the line: what the TREE lists, which is a different question. Worded
  // as "list" so that "show" never means two things in one menu -- it used to
  // say "Show All" here, right next to a "Show All" that means curves.
  QAction showAnimateOnly(tr("List Animated Parameters Only"), 0);
  QAction showAll(tr("List All Parameters"), 0);
  menu.addAction(&showAnimateOnly);
  menu.addAction(&showAll);

  menu.addSeparator();
  QAction openSelectedOnly(tr("Open Selected Column Only"), 0);
  openSelectedOnly.setCheckable(true);
  openSelectedOnly.setChecked(m_openSelectedColumnOnly);
  menu.addAction(&openSelectedOnly);

  // execute menu
  QAction *action = menu.exec(globalPos);

  if (visibility.handle(action)) return;
  if (interpolation.handle(action)) return;

  if (action == &openSelectedOnly) {
    m_openSelectedColumnOnly = openSelectedOnly.isChecked();
    return;
  }

  if (action != &showAll && action != &showAnimateOnly) return;

  FunctionTreeModel::ChannelGroup::ShowFilter showFilter =
      (action == &showAll)
          ? FunctionTreeModel::ChannelGroup::ShowAllChannels
          : FunctionTreeModel::ChannelGroup::ShowAnimatedChannels;

  expand(group->createIndex());
  group->setShowFilter(showFilter);
}

//-----------------------------------------------------------------------------

void FunctionTreeView::collapseSiblingsOf(TreeModel::Item *item) {
  if (!item) return;

  // Close every group at the same level, so following the xsheet selection
  // leaves ONE object open instead of stacking a new one onto everything
  // opened before it. Only the siblings: the branch above stays open, or the
  // item we are about to show would be folded away with them.
  TreeModel::Item *parent = item->getParent();
  if (!parent) return;

  for (int i = 0; i < parent->getChildCount(); ++i) {
    TreeModel::Item *sibling = parent->getChild(i);
    if (!sibling || sibling == item) continue;
    const QModelIndex index = sibling->createIndex();
    if (index.isValid()) setExpanded(index, false);
  }
}

//-----------------------------------------------------------------------------

void FunctionTreeView::scrollToItem(TreeModel::Item *item, bool expandItem) {
  if (!item) return;

  // Top down: setExpanded on a node whose parent is still closed does nothing,
  // and QTreeView cannot scroll to an index that is not laid out.
  QList<TreeModel::Item *> ancestors;
  for (TreeModel::Item *p = item->getParent(); p; p = p->getParent())
    ancestors.prepend(p);
  for (TreeModel::Item *p : ancestors) {
    const QModelIndex idx = p->createIndex();
    if (idx.isValid()) setExpanded(idx, true);
  }

  const QModelIndex index = item->createIndex();
  if (!index.isValid()) return;
  if (expandItem) setExpanded(index, true);
  scrollTo(index, QAbstractItemView::EnsureVisible);
}

//-----------------------------------------------------------------------------

void FunctionTreeView::updateAll() {
  FunctionTreeModel *functionTreeModel =
      dynamic_cast<FunctionTreeModel *>(model());
  assert(functionTreeModel);

  functionTreeModel->applyShowFilters();
  update();
}

//-----------------------------------------------------------------------------
/*! show all the animated channels when the scene switched
 */
void FunctionTreeView::displayAnimatedChannels() {
  FunctionTreeModel *functionTreeModel =
      dynamic_cast<FunctionTreeModel *>(model());
  assert(functionTreeModel);
  int i;
  for (i = 0; i < functionTreeModel->getStageObjectsChannelCount(); i++)
    functionTreeModel->getStageObjectChannel(i)->displayAnimatedChannels();
  for (i = 0; i < functionTreeModel->getFxsChannelCount(); i++)
    functionTreeModel->getFxChannel(i)->displayAnimatedChannels();
  update();
}

//-----------------------------------------------------------------------------
/*! show all the animated channels when the scene switched
 */
void FunctionTreeModel::ChannelGroup::displayAnimatedChannels() {
  int itemCount = getChildCount();
  int i;

  for (i = 0; i < itemCount; i++) {
    FunctionTreeModel::Channel *channel =
        dynamic_cast<FunctionTreeModel::Channel *>(getChild(i));
    if (!channel) {
      FunctionTreeModel::ChannelGroup *channelGroup =
          dynamic_cast<FunctionTreeModel::ChannelGroup *>(getChild(i));
      if (!channelGroup) continue;

      channelGroup->displayAnimatedChannels();
      continue;
    }
    channel->setIsActive(channel->getParam()->hasKeyframes());
  }
}
