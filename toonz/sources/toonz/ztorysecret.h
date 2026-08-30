#pragma once

#include <QString>

// Somewhere to keep a password that is not plain text.
//
// QSettings puts values wherever the platform keeps application settings: on
// Windows that is the registry, which anyone with access to the user account
// can read without tools, and which every profile backup carries along. The
// systems already provide the right place — Credential Manager on Windows,
// Keychain Services on macOS — so use it.
//
// Where there is no keychain (Linux, for now) these functions fail instead of
// falling back to plain text: a password we cannot protect is one we do not
// keep. Callers must therefore treat store() failing as "not remembered", not
// as an error to report.
namespace ZtorySecret {

// False where no secure store exists, so callers can avoid offering to
// remember a password they would then have to forget.
bool isAvailable();

// `service` groups the entries (e.g. "Ztoryc/Kitsu"), `account` identifies one
// within it (e.g. the login email).
bool store(const QString &service, const QString &account,
           const QString &secret);
QString retrieve(const QString &service, const QString &account);
void remove(const QString &service, const QString &account);

}  // namespace ZtorySecret
