#pragma once

#ifndef PEGBARCMD_INCLUDED
#define PEGBARCMD_INCLUDED

#include "tcommon.h"
#include <QPair>
#include <QPointF>
#include <QString>
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TStageObjectId;
class TXsheetHandle;
class TObjectHandle;
class TColumnHandle;
class TFxHandle;
class TStageObjectSpline;
class TStageObject;

namespace TStageObjectCmd {

DVAPI void rename(const TStageObjectId &id, std::string name,
                  TXsheetHandle *xshHandle);
DVAPI void resetOffset(const TStageObjectId &id, TXsheetHandle *xshHandle);
DVAPI void resetCenterAndOffset(const TStageObjectId &id,
                                TXsheetHandle *xshHandle);
DVAPI void resetPosition(const TStageObjectId &id, TXsheetHandle *xshHandle);
DVAPI void setHandle(const TStageObjectId &id, std::string handle,
                     TXsheetHandle *xshHandle);
DVAPI void setParentHandle(const std::vector<TStageObjectId> &ids,
                           std::string handle, TXsheetHandle *xshHandle);
DVAPI void setParent(const TStageObjectId &id, TStageObjectId parentId,
                     std::string parentHandle, TXsheetHandle *xshHandle,
                     bool doUndo = true);
DVAPI void setSplineParent(TStageObjectSpline *spline, TStageObject *parentObj,
                           TXsheetHandle *xshHandle);

//! Draws a motion path through the object's positions at \e frames and puts
//! the object on it, turning those keys into posPath keys at the same frames.
//!
//! Three x/y keys make a movement that goes right, then up, then down again --
//! and it arrives at the middle key one way and leaves it another, so the
//! trajectory has a CORNER there. What one wanted was an arc. Drawing the
//! spline by hand to get it is awkward enough that motion paths go unused.
//!
//! So: the same Catmull-Rom the Auto Bezier tangents use, in space instead of
//! in time. The curve passes exactly through every key position, the timing is
//! preserved key by key (each key keeps its frame, and its posPath value is the
//! percentage of length where its point sits), and what changes is only the
//! shape of the movement BETWEEN the keys. Spreading the intermediate keys for
//! constant speed is a separate question, answered by Even Speed Along Path.
//!
//! The x and y curves are left untouched: on a path they are simply not read
//! (see TStageObject::computeLocalPlacement), so detaching the spline brings
//! the original movement back.
//!
//! An object can only carry one path, so an existing one is detached and stays
//! in the schematic as a free spline node.
//! Returns false and fills \e error when the request makes no sense.
DVAPI bool generatePathFromKeys(const TStageObjectId &id,
                                const std::set<int> &frames,
                                TXsheetHandle *xshHandle,
                                QString *error = 0);

DVAPI void addNewCamera(TXsheetHandle *xshHandle, TObjectHandle *objHandle,
                        QPointF initialPos = QPointF());
DVAPI void addNewPegbar(TXsheetHandle *xshHandle, TObjectHandle *objHandle,
                        QPointF initialPos = QPointF(), int col = 0);
DVAPI void setAsActiveCamera(TXsheetHandle *xshHandle,
                             TObjectHandle *objHandle);
DVAPI TStageObjectSpline *addNewSpline(TXsheetHandle *xshHandle,
                                       TObjectHandle *objHandle,
                                       TColumnHandle *colHandle,
                                       QPointF initialPos = QPointF(),
                                       bool setActive     = false);
DVAPI void deleteSelection(
    const std::vector<TStageObjectId> &objIds,
    const std::list<QPair<TStageObjectId, TStageObjectId>> &links,
    const std::list<int> &splineIds, TXsheetHandle *xshHandle,
    TObjectHandle *objHandle, TFxHandle *fxHandle, bool doUndo = true);
DVAPI void group(const QList<TStageObjectId> ids, TXsheetHandle *xshHandle);
DVAPI void ungroup(int groupId, TXsheetHandle *xshHandle);
DVAPI void renameGroup(const QList<TStageObject *> objs,
                       const std::wstring &name, bool fromEditor,
                       TXsheetHandle *xshHandle);
DVAPI void duplicateObject(const QList<TStageObjectId> ids,
                           TXsheetHandle *xshHandle);
DVAPI void enableSplineAim(TStageObject *obj, int state,
                           TXsheetHandle *xshHandle);
DVAPI void enableSplineUppk(TStageObject *obj, bool toggled,
                            TXsheetHandle *xshHandle);
}

#endif
