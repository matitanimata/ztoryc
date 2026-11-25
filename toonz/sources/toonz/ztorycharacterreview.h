#pragma once

#include <QString>

// Ztoryc: sending a character's rig for approval when its scene is closed
// (Franco, 2026-09-26). Kept apart from ztorycharacter, which is plain file
// and XML work used by the mouth sets too: this one opens dialogs, renders a
// frame and talks to Kitsu.

namespace ZtoryCharacterReview {

// Called while the current scene is being closed (new scene, another scene,
// quit), after the user has answered the save question. If it is a character
// scene whose task (Rigging) is in WIP, asks whether to send it for approval:
// on yes, renders the current frame to <project>/previews/, uploads it to
// the Kitsu task — which sets WFA there — and mirrors WFA locally. Without
// Kitsu in the project, WFA is set locally and the preview is kept on disk.
// Does nothing if `changesDiscarded`: a preview would show discarded work.
void offerWfaOnClose(bool changesDiscarded);

}  // namespace ZtoryCharacterReview

class TFilePath;

// Ztoryc: a shot's render sent to its Kitsu task for approval (Franco,
// 2026-11-24). Asked for in Output Name from Project Pattern; the render is
// the same file used for the final edit, so nothing is burnt in.
namespace ZtoryShotReview {

// Called when an output render is done. If its output name is one marked for
// upload (setRenderUpload), uploads it to that shot's task in a comment that
// sets WFA there, and mirrors WFA locally. Otherwise does nothing.
void renderCompleted(const TFilePath &fp);

// Marks the output `file` (decoded path) for upload after every render, to
// `taskType` of the shot — kept in the app settings, so it holds across
// renders, scenes and restarts until unticked (Franco, 2026-11-25: the
// second render of the same name uploaded nothing). Empty `taskType` unmarks.
void setRenderUpload(const QString &file, const QString &shotUuid,
                     const QString &taskType);

// At the START of a render: if `fp` is marked for upload, makes sure Kitsu is
// connected now, not after minutes of rendering (Franco, 2026-11-25: the name
// can be set already, without opening Name from Project Pattern). Declined:
// the render goes on, and the upload asks again at the end.
void renderStarting(const TFilePath &fp);

// Uploads `file` to the shot's `taskType` on Kitsu (WFA there and here),
// with the progress box. Also for a render made earlier (Render Settings,
// «Upload render to Kitsu»).
void uploadShotRender(const QString &file, const QString &shotUuid,
                      const QString &taskType);

// Connected to Kitsu, or offers to connect now (the dialog logs in by itself
// when the password is saved). True if connected at the end.
bool ensureConnected();

}  // namespace ZtoryShotReview
