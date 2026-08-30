#include "ztorysecret.h"

#include <QByteArray>

#include <string>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif

namespace {

// One flat name per entry. Both backends want a single string, and keeping the
// same shape on each keeps the entries recognisable when a user goes looking
// for them in Credential Manager or Keychain Access.
QString entryName(const QString &service, const QString &account) {
  return service + QLatin1Char('/') + account;
}

#ifdef __APPLE__
CFStringRef toCFString(const QByteArray &utf8) {
  return CFStringCreateWithBytes(nullptr,
                                 reinterpret_cast<const UInt8 *>(utf8.constData()),
                                 utf8.size(), kCFStringEncodingUTF8, false);
}

// The three attributes that identify one generic-password item.
CFDictionaryRef makeQuery(CFStringRef service, CFStringRef account) {
  const void *keys[]   = {kSecClass, kSecAttrService, kSecAttrAccount};
  const void *values[] = {kSecClassGenericPassword, service, account};
  return CFDictionaryCreate(nullptr, keys, values, 3,
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}
#endif

}  // namespace

//----------------------------------------------------------------------------

bool ZtorySecret::isAvailable() {
#if defined(_WIN32) || defined(__APPLE__)
  return true;
#else
  return false;
#endif
}

//----------------------------------------------------------------------------

#ifdef _WIN32

bool ZtorySecret::store(const QString &service, const QString &account,
                        const QString &secret) {
  std::wstring target = entryName(service, account).toStdWString();
  std::wstring user   = account.toStdWString();
  QByteArray blob     = secret.toUtf8();

  CREDENTIALW cred    = {};
  cred.Type           = CRED_TYPE_GENERIC;
  cred.TargetName     = const_cast<LPWSTR>(target.c_str());
  cred.UserName       = const_cast<LPWSTR>(user.c_str());
  cred.CredentialBlobSize = static_cast<DWORD>(blob.size());
  cred.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
  // LOCAL_MACHINE, not SESSION: the point is that it survives a logout the way
  // the old registry value did, otherwise "remember me" stops meaning anything.
  cred.Persist        = CRED_PERSIST_LOCAL_MACHINE;

  return CredWriteW(&cred, 0) == TRUE;
}

QString ZtorySecret::retrieve(const QString &service, const QString &account) {
  std::wstring target = entryName(service, account).toStdWString();

  PCREDENTIALW cred = nullptr;
  if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) return QString();

  QString secret =
      QString::fromUtf8(reinterpret_cast<const char *>(cred->CredentialBlob),
                        static_cast<int>(cred->CredentialBlobSize));
  CredFree(cred);
  return secret;
}

void ZtorySecret::remove(const QString &service, const QString &account) {
  std::wstring target = entryName(service, account).toStdWString();
  CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
}

#elif defined(__APPLE__)

bool ZtorySecret::store(const QString &service, const QString &account,
                        const QString &secret) {
  CFStringRef svc = toCFString(entryName(service, account).toUtf8());
  CFStringRef acc = toCFString(account.toUtf8());
  QByteArray blob = secret.toUtf8();
  CFDataRef data  = CFDataCreate(
      nullptr, reinterpret_cast<const UInt8 *>(blob.constData()), blob.size());

  CFDictionaryRef query = makeQuery(svc, acc);

  // Update first: SecItemAdd refuses an item that already exists, and the
  // common case here is a password being changed, not added.
  const void *updateKeys[]   = {kSecValueData};
  const void *updateValues[] = {data};
  CFDictionaryRef update =
      CFDictionaryCreate(nullptr, updateKeys, updateValues, 1,
                         &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);

  OSStatus status = SecItemUpdate(query, update);
  if (status == errSecItemNotFound) {
    CFMutableDictionaryRef add =
        CFDictionaryCreateMutableCopy(nullptr, 0, query);
    CFDictionarySetValue(add, kSecValueData, data);
    status = SecItemAdd(add, nullptr);
    CFRelease(add);
  }

  CFRelease(update);
  CFRelease(query);
  CFRelease(data);
  CFRelease(acc);
  CFRelease(svc);
  return status == errSecSuccess;
}

QString ZtorySecret::retrieve(const QString &service, const QString &account) {
  CFStringRef svc = toCFString(entryName(service, account).toUtf8());
  CFStringRef acc = toCFString(account.toUtf8());

  const void *keys[]   = {kSecClass,      kSecAttrService, kSecAttrAccount,
                          kSecReturnData, kSecMatchLimit};
  const void *values[] = {kSecClassGenericPassword, svc, acc, kCFBooleanTrue,
                          kSecMatchLimitOne};
  CFDictionaryRef query =
      CFDictionaryCreate(nullptr, keys, values, 5, &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);

  CFTypeRef found = nullptr;
  QString secret;
  if (SecItemCopyMatching(query, &found) == errSecSuccess && found) {
    CFDataRef data = static_cast<CFDataRef>(found);
    secret = QString::fromUtf8(
        reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
        static_cast<int>(CFDataGetLength(data)));
    CFRelease(found);
  }

  CFRelease(query);
  CFRelease(acc);
  CFRelease(svc);
  return secret;
}

void ZtorySecret::remove(const QString &service, const QString &account) {
  CFStringRef svc       = toCFString(entryName(service, account).toUtf8());
  CFStringRef acc       = toCFString(account.toUtf8());
  CFDictionaryRef query = makeQuery(svc, acc);
  SecItemDelete(query);
  CFRelease(query);
  CFRelease(acc);
  CFRelease(svc);
}

#else

// No keychain here. Refusing is the whole point: the caller falls back to
// asking for the password each time, which is what plain text was buying us.
bool ZtorySecret::store(const QString &, const QString &, const QString &) {
  return false;
}

QString ZtorySecret::retrieve(const QString &, const QString &) {
  return QString();
}

void ZtorySecret::remove(const QString &, const QString &) {}

#endif
