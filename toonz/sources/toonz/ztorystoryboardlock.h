#pragma once

#ifndef ZTORYSTORYBOARDLOCK_H
#define ZTORYSTORYBOARDLOCK_H

// Ztoryc: Lock Storyboard (Franco, 2026-10-23).
//
// A storyboard is locked once every shot's Storyboard task is approved (Done:
// Kitsu's Approved reads as Done). Locked, the file only exports: shots cannot
// be entered, the scene and its levels cannot be saved over, and the Board and
// Animatic refuse edits. Unlocking asks to retype the file name and creates a
// new version, «name_vN», a full copy (levels in a folder of their own) that
// takes over the shots in the Production Tracker, in Keep # numbering; the
// locked original stays as it was.

#include <QString>
#include <QStringList>

class QWidget;

namespace ZtoryStoryboardLock {

bool isLocked();

//! When the storyboard is locked: explains it (unless silent) and returns
//! true, so the caller stops.
bool refuse(bool silent = false);

//! Shot labels whose Storyboard task is not approved yet.
QStringList shotsNotApproved();

//! Lock: checks the approvals, confirms, saves the scene locked.
bool lock(QWidget *parent);
//! Unlock: the file name retyped, then the new version, opened.
bool unlock(QWidget *parent);

//! Lets one save through while the lock itself is being written.
class SaveBypass {
public:
  SaveBypass();
  ~SaveBypass();
  static bool active();
};

}  // namespace ZtoryStoryboardLock

#endif  // ZTORYSTORYBOARDLOCK_H
