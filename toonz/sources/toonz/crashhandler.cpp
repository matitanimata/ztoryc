#include "crashhandler.h"

#include <inttypes.h>
#include <signal.h>
#include <exception>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <winbase.h>
#include <dbghelp.h>
#include <psapi.h>
#else
#ifdef MACOSX
#include <mach-o/dyld.h>
#else
#include <link.h>
#include <ucontext.h>
#endif
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <atomic>
#endif

#include "tgl.h"
#include "tapp.h"
#include "tenv.h"
#include "tconvert.h"
#include "texception.h"
#include "tfilepath_io.h"
#include "toonz/toonzfolders.h"
#include "toonz/tproject.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "tsystem.h"

#include <QOperatingSystemVersion>
#include <QDesktopServices>
#include <QApplication>
#include <QClipboard>
#include <QThread>
#include <QMainWindow>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDialog>
#include <QLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QProcess>
#include <QRegularExpression>

#include <QDebug>

static QWidget *s_parentWindow  = NULL;
static bool s_reportProjInfo    = false;
static volatile bool s_appExiting = false;
#ifdef _WIN32
static PEXCEPTION_POINTERS s_exceptionPtr = NULL;
#endif

//-----------------------------------------------------------------------------

static const char *filenameOnly(const char *path) {
  for (int i = strlen(path); i >= 0; --i) {
    if (path[i] == '\\' || path[i] == '/') return path + i + 1;
  }
  return path;
}

//-----------------------------------------------------------------------------
static QString obfuscateUsername(QString path) {
  QString username = TSystem::getUserName();
  QString newPath  = path;
  newPath.replace("/" + username + "/", "/USER/");
  newPath.replace("\\" + username + "\\", "\\USER\\");
  return newPath;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Windows platform functions

#ifdef _WIN32

#define HAS_MINIDUMP
static bool generateMinidump(TFilePath dumpFile,
                             PEXCEPTION_POINTERS exceptionInfo) {
  HANDLE hDumpFile = CreateFileW(dumpFile.getWideString().c_str(),
                                 GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (hDumpFile == INVALID_HANDLE_VALUE) return false;

  MINIDUMP_EXCEPTION_INFORMATION mdei;
  mdei.ThreadId          = GetCurrentThreadId();
  mdei.ExceptionPointers = exceptionInfo;
  mdei.ClientPointers    = TRUE;

  if (MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile,
                        MiniDumpNormal, &mdei, 0, NULL)) {
    CloseHandle(hDumpFile);
    return true;
  }

  return false;
}

#define HAS_MODULES
static void printModules(std::string &out) {
  HANDLE hProcess = GetCurrentProcess();

  HMODULE modules[1024];
  DWORD size;
  if (EnumProcessModules(hProcess, modules, sizeof(modules), &size)) {
    for (unsigned int i = 0; i < size / sizeof(HMODULE); i++) {
      char moduleName[512];
      GetModuleFileNameA(modules[i], moduleName, 512);
      QString path = obfuscateUsername(QString(moduleName));
      out.append(path.toStdString());
      out.append("\n");
    }
  }
}

#define HAS_BACKTRACE
static void printBacktrace(std::string &out) {
  int frameStack = 0;
  int frameSkip  = 3;

  HANDLE hProcess = GetCurrentProcess();

  SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES |
                SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES |
                SYMOPT_UNDNAME);

  SymInitialize(hProcess, NULL, TRUE);

  CONTEXT context;
  RtlCaptureContext(&context);

  char sourceSymMem[sizeof(IMAGEHLP_SYMBOL64) + 1025];
  PIMAGEHLP_SYMBOL64 sourceSym = (PIMAGEHLP_SYMBOL64)&sourceSymMem;
  memset(sourceSymMem, 0, sizeof(sourceSymMem));

  IMAGEHLP_LINE64 sourceInfo;
  memset(&sourceInfo, 0, sizeof(IMAGEHLP_LINE64));
  sourceInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

  IMAGEHLP_MODULE64 moduleInfo;
  memset(&moduleInfo, 0, sizeof(moduleInfo));
  moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);

  STACKFRAME64 stackframe;
  memset(&stackframe, 0, sizeof(STACKFRAME64));

#ifdef _WIN64
  int machineType             = IMAGE_FILE_MACHINE_AMD64;
  stackframe.AddrPC.Offset    = context.Rip;
  stackframe.AddrPC.Mode      = AddrModeFlat;
  stackframe.AddrStack.Offset = context.Rsp;
  stackframe.AddrStack.Mode   = AddrModeFlat;
  stackframe.AddrFrame.Offset = context.Rbp;
  stackframe.AddrFrame.Mode   = AddrModeFlat;
#else
  int machineType             = IMAGE_FILE_MACHINE_I386;
  stackframe.AddrPC.Offset    = context.Eip;
  stackframe.AddrPC.Mode      = AddrModeFlat;
  stackframe.AddrStack.Offset = context.Esp;
  stackframe.AddrStack.Mode   = AddrModeFlat;
  stackframe.AddrFrame.Offset = context.Ebp;
  stackframe.AddrFrame.Mode   = AddrModeFlat;
#endif

  HANDLE hThread = GetCurrentThread();
  while (StackWalk64(machineType, hProcess, hThread, &stackframe, &context,
                     NULL, SymFunctionTableAccess64, SymGetModuleBase64,
                     NULL)) {
    // Skip first frames since they point to this function
    if (frameStack++ < frameSkip) continue;
    char numStr[32];
    memset(numStr, 0, sizeof(numStr));
    sprintf(numStr, "%3i> ", frameStack - frameSkip);
    out.append(numStr);

    sourceSym->SizeOfStruct  = sizeof(IMAGEHLP_SYMBOL64);
    sourceSym->MaxNameLength = 1024;

    // Get symbol name
    DWORD64 displacement64;
    if (SymGetSymFromAddr64(hProcess, (ULONG64)stackframe.AddrPC.Offset,
                            &displacement64, sourceSym)) {
      out.append(sourceSym->Name);

      // Get module filename
      char moduleFile[512];
      MEMORY_BASIC_INFORMATION mbi;
      VirtualQuery((LPCVOID)stackframe.AddrPC.Offset, &mbi, sizeof(mbi));
      GetModuleFileNameA((HMODULE)mbi.AllocationBase, moduleFile, 512);

      // Get source filename and line
      DWORD displacement32;
      if (SymGetLineFromAddr64(hProcess, stackframe.AddrPC.Offset,
                               &displacement32, &sourceInfo) != FALSE) {
        out.append(" {");
        out.append(filenameOnly(sourceInfo.FileName));
        out.append(":");
        out.append(std::to_string(sourceInfo.LineNumber));
        out.append("}");
      } else {
        memset(numStr, 0, sizeof(numStr));
        sprintf(numStr, " [0x%" PRIx64 "]", stackframe.AddrPC.Offset);
        out.append(numStr);
      }
      out.append(" <");
      out.append(filenameOnly(moduleFile));
      out.append(">");
    }

    out.append("\n");
  }

  SymCleanup(hProcess);
}

//-----------------------------------------------------------------------------

LONG WINAPI exceptionHandler(PEXCEPTION_POINTERS info) {
  static volatile bool handling = false;

  const char *reason = "Unknown";
  switch (info->ExceptionRecord->ExceptionCode) {
  case EXCEPTION_ACCESS_VIOLATION:
    reason = "EXCEPTION_ACCESS_VIOLATION";
    break;
  case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    reason = "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    break;
  case EXCEPTION_DATATYPE_MISALIGNMENT:
    reason = "EXCEPTION_DATATYPE_MISALIGNMENT";
    break;
  case EXCEPTION_FLT_DENORMAL_OPERAND:
    reason = "EXCEPTION_FLT_DENORMAL_OPERAND";
    break;
  case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    reason = "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    break;
  case EXCEPTION_FLT_INEXACT_RESULT:
    reason = "EXCEPTION_FLT_INEXACT_RESULT";
    break;
  case EXCEPTION_FLT_INVALID_OPERATION:
    reason = "EXCEPTION_FLT_INVALID_OPERATION";
    break;
  case EXCEPTION_FLT_OVERFLOW:
    reason = "EXCEPTION_FLT_OVERFLOW";
    break;
  case EXCEPTION_FLT_STACK_CHECK:
    reason = "EXCEPTION_FLT_STACK_CHECK";
    break;
  case EXCEPTION_FLT_UNDERFLOW:
    reason = "EXCEPTION_FLT_UNDERFLOW";
    break;
  case EXCEPTION_ILLEGAL_INSTRUCTION:
    reason = "EXCEPTION_ILLEGAL_INSTRUCTION";
    break;
  case EXCEPTION_IN_PAGE_ERROR:
    reason = "EXCEPTION_IN_PAGE_ERROR";
    break;
  case EXCEPTION_INT_DIVIDE_BY_ZERO:
    reason = "EXCEPTION_INT_DIVIDE_BY_ZERO";
    break;
  case EXCEPTION_INVALID_DISPOSITION:
    reason = "EXCEPTION_INVALID_DISPOSITION";
    break;
  case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    reason = "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    break;
  case EXCEPTION_PRIV_INSTRUCTION:
    reason = "EXCEPTION_PRIV_INSTRUCTION";
    break;
  case EXCEPTION_STACK_OVERFLOW:
    reason = "EXCEPTION_STACK_OVERFLOW";
    break;
  default:
    return EXCEPTION_CONTINUE_SEARCH;
  }

  // Avoid new exceptions inside the crash handler
  if (handling) return EXCEPTION_CONTINUE_SEARCH;

  handling = true;
  s_exceptionPtr = info;
  if (CrashHandler::trigger(reason, true)) _Exit(1);
  handling = false;

  return EXCEPTION_CONTINUE_SEARCH;
}

#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Linux and Mac OS X platform functions

#ifndef _WIN32

static bool sh(std::string &out, const char *cmd) {
  char buffer[128];
  FILE *p = popen(cmd, "r");
  if (p == NULL) return false;
  while (fgets(buffer, 128, p)) out.append(buffer);
  pclose(p);
  return true;
}

//-----------------------------------------------------------------------------

static bool addr2line(std::string &out, const char *exepath, const size_t addr,
                      const size_t laddr) {
  char cmd[512];
#ifdef MACOSX
  sprintf(cmd, "atos -o \"%.400s\" -l %p %p 2>&1", exepath, laddr, addr);
#else
  sprintf(cmd, "addr2line -fpCis -e \"%.400s\" %p 2>&1", exepath, addr);
#endif
  return sh(out, cmd);
}

//-----------------------------------------------------------------------------

static bool generateMinidump(TFilePath dumpFile) { return false; }

//-----------------------------------------------------------------------------

#define HAS_MODULES
#ifdef LINUX
static int printModuleInfo(struct dl_phdr_info *i, size_t size, void *data) {
  std::string *out = (std::string *)data;
  char addr[512];
  sprintf(addr, "%p", (long)i->dlpi_addr);
  out->append(std::string(addr) + " - " + std::string(i->dlpi_name) + "\n");
  return 0;
}
#endif

static void printModules(std::string &out) {
#ifdef MACOSX
  char str[512];
  int c = _dyld_image_count();
  for (int i = 0; i < c; i++) {
    const char *image_name           = _dyld_get_image_name(i);
    const struct mach_header *header = _dyld_get_image_header(i);
    uintptr_t loadaddr               = (uintptr_t)header;
    sprintf(str, "%p", loadaddr);

    QString path = obfuscateUsername(QString(image_name));
    out.append(std::string(str) + " - " + path.toStdString());
    out.append("\n");
  }
#else
  dl_iterate_phdr(printModuleInfo, &out);
#endif
}

//-----------------------------------------------------------------------------

#define HAS_BACKTRACE
static void printBacktrace(std::string &out) {
  int frameStack = 0;
  int frameSkip  = 3;

  const int size = 256;
  void *buffer[size];

  // Back trace
  int nptrs = backtrace(buffer, size);
  for (int i = 0; i < nptrs; ++i) {
    // Skip first frames since they point to this function
    if (frameStack++ < frameSkip) continue;
    char numStr[32];
    memset(numStr, 0, sizeof(numStr));
    sprintf(numStr, "%3i> ", frameStack - frameSkip);
    out.append(numStr);

    std::string line;
    std::string sym    = "??? at ??:?";
    std::string module = "???";
    size_t addr        = (size_t)buffer[i];

    bool found = false;

    Dl_info info;
    int infoFound = dladdr(buffer[i], &info);

    if (infoFound) module = std::string(info.dli_fname);

#ifdef MACOSX
    if (module.find("Ztoryc.app/Contents/MacOS") != std::string::npos ||
        module.find("Tahoma2D.app/Contents/MacOS") != std::string::npos) {
      std::string dsym = module;
      int pos          = dsym.find("Contents");
      dsym.replace(pos, 14, "DSYM/Contents/Resources/DWARF");
      TFilePath file(dsym);
      if (TFileStatus(file).doesExist()) module = dsym;
    }

    char addrstr[512];
    sprintf(addrstr, "%p", addr);
    sym = std::string(addrstr) + " (in " + module + ")";
#else
    link_map *link_map;
    dladdr1(buffer[i], &info, (void **)&link_map, RTLD_DL_LINKMAP);
    addr -= link_map->l_addr;
#endif

    if (infoFound &&
        addr2line(line, module.c_str(), addr, (size_t)info.dli_fbase)) {
      found = (line.rfind("??", 0) != 0) &&
              (line.find("cannot load symbol", 0) != 0) &&
              (line.find("command not found") != 0);
    }

    out.append(found ? line : (sym + "\n"));
  }
}

// Ztoryc (2026-11-04, from groundzero PR #13): the signal handler used to
// build the report in place — atos/addr2line through popen(), std::string,
// QString and a modal Qt dialog. After a SIGSEGV the heap or Qt can be broken,
// and the handler then hung or crashed again: the lzo deadlock blocked signals
// around fork() for this reason. Now the handler only writes, with
// async-signal-safe calls, a raw report (prepared text, the return addresses,
// where every library was loaded) to PendingCrashReport.log. The next launch
// turns the addresses into function names and lines, files it as
// Crash-<time>.log and shows it (prepareReport / showPendingReport).

static char s_pendingReportPath[1024] = {0};
static char s_reportIntro[4096]       = {0};
static size_t s_reportIntroLen        = 0;
static char s_sceneInfo[2048]         = {0};
static std::atomic<size_t> s_sceneInfoLen(0);
static QString s_previousReport;
static TFilePath s_previousReportFile;

#ifdef MACOSX
// Every image dyld loads, as "0x<header> <path>\n": dladdr() is not safe in
// the handler, so the table is filled when an image is loaded.
static char s_imageTable[256 * 1024] = {0};
static std::atomic<size_t> s_imageTableLen(0);

static void onImageAdded(const struct mach_header *mh, intptr_t) {
  Dl_info info;
  if (!dladdr(mh, &info) || !info.dli_fname) return;
  char line[1100];
  int n = snprintf(line, sizeof(line), "%p %s\n", (const void *)mh,
                   info.dli_fname);
  size_t len = s_imageTableLen.load();
  if (n <= 0 || len + n >= sizeof(s_imageTable)) return;
  memcpy(s_imageTable + len, line, n);
  s_imageTableLen.store(len + n);
}
#endif

static size_t copyToBuffer(char *dst, size_t capacity, const char *src,
                           size_t len) {
  if (len > capacity - 1) len = capacity - 1;
  memcpy(dst, src, len);
  dst[len] = 0;
  return len;
}

static void writeAll(int fd, const char *data, size_t len) {
  while (len > 0) {
    ssize_t n = write(fd, data, len);
    if (n <= 0) {
      if (n < 0 && errno == EINTR) continue;
      return;
    }
    data += n;
    len -= (size_t)n;
  }
}

static void writeText(int fd, const char *text) {
  writeAll(fd, text, strlen(text));
}

static void writeNumber(int fd, unsigned long long value, int minDigits) {
  char buf[24];
  int pos = sizeof(buf);
  do {
    buf[--pos] = (char)('0' + value % 10);
    value /= 10;
    --minDigits;
  } while (pos > 0 && (value > 0 || minDigits > 0));
  writeAll(fd, buf + pos, sizeof(buf) - pos);
}

static void writeHex(int fd, unsigned long long value) {
  char buf[20];
  int pos = sizeof(buf);
  do {
    buf[--pos] = "0123456789abcdef"[value & 15];
    value >>= 4;
  } while (pos > 2 && value > 0);
  buf[--pos] = 'x';
  buf[--pos] = '0';
  writeAll(fd, buf + pos, sizeof(buf) - pos);
}

// localtime() and strftime() are not async-signal-safe: a UTC date with plain
// arithmetic (days-to-civil, Howard Hinnant).
static void writeUtcTime(int fd, time_t t) {
  long long days = (long long)t / 86400;
  long long secs = (long long)t % 86400;
  if (secs < 0) {
    secs += 86400;
    --days;
  }
  days += 719468;
  const long long era  = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe   = (unsigned)(days - era * 146097);
  const unsigned yoe   = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy   = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp    = (5 * doy + 2) / 153;
  const unsigned day   = doy - (153 * mp + 2) / 5 + 1;
  const unsigned month = mp < 10 ? mp + 3 : mp - 9;
  const long long year = (long long)yoe + era * 400 + (month <= 2 ? 1 : 0);

  writeNumber(fd, (unsigned long long)year, 4);
  writeText(fd, "-");
  writeNumber(fd, month, 2);
  writeText(fd, "-");
  writeNumber(fd, day, 2);
  writeText(fd, " ");
  writeNumber(fd, (unsigned long long)(secs / 3600), 2);
  writeText(fd, ":");
  writeNumber(fd, (unsigned long long)(secs / 60 % 60), 2);
  writeText(fd, ":");
  writeNumber(fd, (unsigned long long)(secs % 60), 2);
  writeText(fd, " UTC");
}

// The handler runs on this stack, so that a stack overflow is reported too.
alignas(16) static char s_signalStack[65536];

// Where the crash happened, from the context the kernel saved. backtrace() does
// not give it: it walks return addresses from the signal frame, so the function
// that crashed — when it has no frame of its own yet — is not among them.
static uintptr_t faultingAddress(void *context) {
  if (!context) return 0;
  ucontext_t *uc = static_cast<ucontext_t *>(context);
#if defined(__APPLE__) && defined(__arm64__)
  return (uintptr_t)__darwin_arm_thread_state64_get_pc(uc->uc_mcontext->__ss);
#elif defined(__APPLE__) && defined(__x86_64__)
  return (uintptr_t)uc->uc_mcontext->__ss.__rip;
#elif defined(__linux__) && defined(__x86_64__)
  return (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__linux__) && defined(__aarch64__)
  return (uintptr_t)uc->uc_mcontext.pc;
#else
  (void)uc;
  return 0;
#endif
}

void signalHandler(int sig, siginfo_t *, void *context) {
  static std::atomic_flag handling = ATOMIC_FLAG_INIT;

  const char *reason = "Unknown";
  switch (sig) {
  case SIGABRT:
    reason = "(SIGABRT) Usually caused by an abort() or assert()";
    break;
  case SIGFPE:
    reason = "(SIGFPE) Arithmetic exception, such as divide by zero";
    break;
  case SIGILL:
    reason = "(SIGILL) Illegal instruction";
    break;
  case SIGSEGV:
    reason = "(SIGSEGV) Segmentation Fault";
    break;
#ifdef SIGBUS
  case SIGBUS:
    reason = "(SIGBUS) Bus error, such as an invalid memory access";
    break;
#endif
  }

  // A crash in the static destructors at shutdown is not reported, as before.
  if (s_appExiting) _Exit(0);

  // Only async-signal-safe calls below. A second crash (another thread, or
  // this handler) waits for the report to be written, then dies.
  if (handling.test_and_set()) {
    struct timespec step = {0, 100000000};  // 0.1 s
    for (int i = 0; i < 50; ++i) nanosleep(&step, NULL);
  } else if (s_pendingReportPath[0]) {
    int fd = open(s_pendingReportPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
      writeAll(fd, s_reportIntro, s_reportIntroLen);
      writeText(fd, "\nReport Date: ");
      writeUtcTime(fd, time(NULL));
      writeText(fd, "\nCrash Reason: ");
      writeText(fd, reason);
      writeText(fd, "\n");
      writeAll(fd, s_sceneInfo, s_sceneInfoLen.load());

      writeText(fd, "\n==== Raw Frames ====\n");
      void *frames[256];
      int nptrs = backtrace(frames, 256);
      // Frame 0 is this handler and frame 1 the system's signal trampoline
      // (a private symbol, named at random: _os_semaphore_wait on macOS). The
      // first line is the address that crashed; if the walk also found it, as
      // it does on Linux, it starts right after, so it is not listed twice.
      const uintptr_t pc = faultingAddress(context);
      int first          = 2;
      if (pc) {
        writeHex(fd, (unsigned long long)pc);
        writeText(fd, "\n");
        for (int i = 2; i < nptrs; ++i)
          if ((uintptr_t)frames[i] == pc) {
            first = i + 1;
            break;
          }
      }
      for (int i = first; i < nptrs; ++i) {
        writeHex(fd, (unsigned long long)(uintptr_t)frames[i]);
        writeText(fd, "\n");
      }
      writeText(fd, "==== Raw Images ====\n");
#ifdef MACOSX
      writeAll(fd, s_imageTable, s_imageTableLen.load());
#else
      int maps = open("/proc/self/maps", O_RDONLY);
      if (maps >= 0) {
        char buf[4096];
        ssize_t n;
        while ((n = read(maps, buf, sizeof(buf))) > 0 ||
               (n < 0 && errno == EINTR))
          if (n > 0) writeAll(fd, buf, (size_t)n);
        close(maps);
      }
#endif
      writeText(fd, "==== Raw End ====\n");
      close(fd);
    }
  }

  // The OS still does its own crash handling (crash report, exit status).
  signal(sig, SIG_DFL);
  raise(sig);
}

//-----------------------------------------------------------------------------

// The next launch: the raw addresses of the pending report become function
// names and lines, the way printBacktrace() writes them.
static QString runTool(const QString &tool, const QStringList &args) {
  QProcess p;
  p.start(tool, args);
  if (!p.waitForFinished(20000)) {
    p.kill();
    return QString();
  }
  return QString::fromLocal8Bit(p.readAllStandardOutput());
}

static QString symbolicate(const QString &raw) {
  const int framesAt = raw.indexOf("\n==== Raw Frames ====\n");
  const int imagesAt = raw.indexOf("==== Raw Images ====\n");
  const int endAt    = raw.indexOf("==== Raw End ====");
  if (framesAt < 0 || imagesAt < framesAt) return raw;  // cut short: as it is

  const QString head = raw.left(framesAt + 1);
  QVector<quint64> frames;
  for (const QString &l :
       raw.mid(framesAt + 22, imagesAt - framesAt - 22).split('\n'))
    if (l.startsWith("0x")) frames << l.mid(2).toULongLong(nullptr, 16);
  const QStringList imageLines =
      raw.mid(imagesAt + 21, (endAt < 0 ? raw.size() : endAt) - imagesAt - 21)
          .split('\n', Qt::SkipEmptyParts);

  // the image of every frame: base and path
  struct Image {
    quint64 base;
    QString path;
  };
  QVector<Image> images;
#ifdef MACOSX
  for (const QString &l : imageLines) {
    int sp = l.indexOf(' ');
    if (sp > 2) images.push_back({l.left(sp).mid(2).toULongLong(nullptr, 16),
                                  l.mid(sp + 1)});
  }
#else
  // /proc/self/maps: "start-end perms offset dev inode path"; the base of a
  // file is the start of its mapping at offset 0
  struct Range {
    quint64 from, to;
    QString path;
  };
  QVector<Range> ranges;
  QHash<QString, quint64> bases;
  for (const QString &l : imageLines) {
    const QStringList f = l.split(' ', Qt::SkipEmptyParts);
    if (f.size() < 6 || !f[5].startsWith('/')) continue;
    const QStringList r = f[0].split('-');
    if (r.size() != 2) continue;
    Range range{r[0].toULongLong(nullptr, 16), r[1].toULongLong(nullptr, 16),
                f.mid(5).join(' ')};
    ranges << range;
    if (f[2].toULongLong(nullptr, 16) == 0 && !bases.contains(range.path))
      bases[range.path] = range.from;
  }
#endif

  QVector<int> imageOf(frames.size(), -1);
  QStringList lines;
  for (int i = 0; i < frames.size(); ++i) lines << QString();
#ifdef MACOSX
  QHash<int, QVector<int>> byImage;
  for (int i = 0; i < frames.size(); ++i) {
    int best = -1;
    for (int k = 0; k < images.size(); ++k)
      if (images[k].base <= frames[i] &&
          (best < 0 || images[k].base > images[best].base))
        best = k;
    imageOf[i] = best;
    if (best >= 0) byImage[best] << i;
  }
  for (auto it = byImage.begin(); it != byImage.end(); ++it) {
    QString module = images[it.key()].path;
    if (module.contains("Ztoryc.app/Contents/MacOS") ||
        module.contains("Tahoma2D.app/Contents/MacOS")) {
      QString dsym = module;
      dsym.replace(dsym.indexOf("Contents"), 14,
                   "DSYM/Contents/Resources/DWARF");
      if (QFileInfo::exists(dsym)) module = dsym;
    }
    QStringList args{"-o", module, "-l",
                     "0x" + QString::number(images[it.key()].base, 16)};
    for (int i : it.value()) args << "0x" + QString::number(frames[i], 16);
    const QStringList out = runTool("atos", args).split('\n');
    for (int j = 0; j < it.value().size() && j < out.size(); ++j)
      if (!out[j].startsWith("0x")) lines[it.value()[j]] = out[j];
  }
#else
  for (int i = 0; i < frames.size(); ++i)
    for (const Range &r : ranges)
      if (r.from <= frames[i] && frames[i] < r.to) {
        const QString line = runTool(
            "addr2line",
            {"-fpCis", "-e", r.path,
             "0x" + QString::number(frames[i] - bases.value(r.path, 0), 16)})
                                 .trimmed();
        if (!line.isEmpty() && !line.startsWith("??")) lines[i] = line;
        break;
      }
#endif

  QString out = head + "\n==== Backtrace ====\n";
  for (int i = 0; i < frames.size(); ++i) {
    QString line = lines[i];
    if (line.isEmpty()) {
      line = "0x" + QString::number(frames[i], 16);
#ifdef MACOSX
      if (imageOf[i] >= 0) line += " (in " + images[imageOf[i]].path + ")";
#endif
    }
    out += QString("%1> ").arg(i + 1, 3) + line + "\n";
  }
  out += "==== End ====\n\n==== Modules ====\n";
  for (const QString &l : imageLines) out += l + "\n";
  out += "==== End ====\n";
  return obfuscateUsername(out);
}

#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

static void printSysInfo(std::string &out) {
  out.append("Build ABI: " + QSysInfo::buildAbi().toStdString() + "\n");
  out.append("Operating System: " + QSysInfo::prettyProductName().toStdString() + "\n");
  out.append("OS Kernel: " + QSysInfo::kernelVersion().toStdString() + "\n");
  out.append("CPU Threads: " + std::to_string(QThread::idealThreadCount()) + "\n");
}

//-----------------------------------------------------------------------------

static void printGPUInfo(std::string &out) {
  const char *gpuVendorName = (const char *)glGetString(GL_VENDOR);
  const char *gpuModelName  = (const char *)glGetString(GL_RENDERER);
  const char *gpuVersion    = (const char *)glGetString(GL_VERSION);
  if (gpuVendorName)
    out.append("GPU Vendor: " + std::string(gpuVendorName) + "\n");
  if (gpuModelName)
    out.append("GPU Model: " + std::string(gpuModelName) + "\n");
  if (gpuVersion)
    out.append("GPU Version: " + std::string(gpuVersion) + "\n");
}

//-----------------------------------------------------------------------------

CrashHandler::CrashHandler(QWidget *parent, TFilePath crashFile,
                           QString crashReport, bool previousSession)
    : QDialog(parent)
    , m_crashFile(crashFile)
    , m_crashReport(crashReport)
    , m_previousSession(previousSession) {
  setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint |
                 (previousSession ? Qt::WindowCloseButtonHint
                                  : Qt::WindowFlags()));

  QStringList sl;
  sl.append(previousSession
                ? tr("<b>Ztoryc crashed during the last session.</b>")
                : tr("<b>Ztoryc crashed unexpectedly.</b>"));
  sl.append("");
  sl.append(tr("A crash report has been generated."));
  sl.append(
      tr("To report, click 'Open Issue Webpage' to access Ztoryc's Issues "
         "page on GitHub."));
  sl.append(tr("Click on the 'New issue' button and fill out the form."));
  sl.append("");
  sl.append(tr("System Configuration and Problem Details:"));
  
  QLabel *headtext = new QLabel(sl.join("<br>"));
  headtext->setTextFormat(Qt::RichText);

  QTextEdit *reportTxt = new QTextEdit();
  reportTxt->setText(crashReport);
  reportTxt->setReadOnly(true);
  reportTxt->setLineWrapMode(QTextEdit::LineWrapMode::NoWrap);
  reportTxt->setStyleSheet(
      "background:white;\ncolor:black;\nborder:1 solid black;");

  QVBoxLayout *mainLayout = new QVBoxLayout();
  QHBoxLayout *buttonsLay = new QHBoxLayout();

  QPushButton *copyBtn   = new QPushButton(tr("Copy to Clipboard"));
  QPushButton *webBtn    = new QPushButton(tr("Open Issue Webpage"));
  QPushButton *folderBtn = new QPushButton(tr("Open Reports Folder"));
  QPushButton *closeBtn  = new QPushButton(
      previousSession ? tr("Close") : tr("Close Application"));
  buttonsLay->addWidget(copyBtn);
  buttonsLay->addWidget(webBtn);
  buttonsLay->addWidget(folderBtn);
  buttonsLay->addWidget(closeBtn);

  mainLayout->addWidget(headtext);
  mainLayout->addWidget(reportTxt);
  mainLayout->addLayout(buttonsLay);

  bool ret = connect(copyBtn, SIGNAL(clicked()), this, SLOT(copyClipboard()));
  ret = ret && connect(webBtn, SIGNAL(clicked()), this, SLOT(openWebpage()));
  ret = ret && connect(folderBtn, SIGNAL(clicked()), this, SLOT(openFolder()));
  ret = ret && connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
  if (!ret) throw TException();

  setWindowTitle(tr("Ztoryc crashed!"));
  setLayout(mainLayout);
}

void CrashHandler::reject() {
  // the report of the last session is only information: Esc closes it
  if (m_previousSession) QDialog::reject();
/*
  QStringList sl;
  sl.append(tr("Application is in unstable state and must be restarted."));
  sl.append(tr("Resuming is not recommended and may lead to an unrecoverable crash."));
  sl.append(tr("Ignore advice and try to resume program?"));

  QMessageBox::StandardButton reply =
      QMessageBox::question(this, tr("Ignore crash?"), sl.join("\n"),
                            QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    QDialog::reject();
  }
*/
}

//-----------------------------------------------------------------------------

void CrashHandler::copyClipboard() {
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(m_crashReport);
}

//-----------------------------------------------------------------------------

void CrashHandler::openWebpage() {
  QDesktopServices::openUrl(QUrl("https://github.com/matitanimata/ztoryc/issues"));
}  

//-----------------------------------------------------------------------------

void CrashHandler::openFolder() {
  TFilePath fp = ToonzFolder::getCrashReportFolder();
  QDesktopServices::openUrl(QUrl("file:///" + fp.getQString()));
}

//-----------------------------------------------------------------------------

// An uncaught C++ exception is NOT a structured exception: it never reaches the
// vectored/signal handlers above — the CRT just calls std::terminate → abort,
// killing the app with no dialog and no crash log. Many Tahoma paths throw
// (TException on project/level/file errors), so this was a silent-death hole.
// Grab the pending exception's message (the usual root cause) and write a report
// before exiting.
static void terminateHandler() {
  static volatile bool handling = false;
  if (handling) _Exit(1);  // a throw inside the handler → don't recurse
  handling = true;

  QString reason = "Unhandled C++ exception (std::terminate)";
  if (std::exception_ptr ep = std::current_exception()) {
    try {
      std::rethrow_exception(ep);
    } catch (const TException &e) {
      reason += ": " + QString::fromStdWString(e.getMessage());
    } catch (const std::exception &e) {
      reason += QString(": ") + e.what();
    } catch (...) {
      reason += ": (non-standard exception)";
    }
  }
  CrashHandler::trigger(reason, true);
  _Exit(1);
}

#ifdef _WIN32
// The CRT calls these on a bad library argument (e.g. an invalid iterator or an
// out-of-range index in a hardened STL) and on a pure-virtual call. Both default
// to a silent abort() that skips the vectored handler — route them through the
// same report path.
static void invalidParameterHandler(const wchar_t *, const wchar_t *,
                                    const wchar_t *, unsigned int, uintptr_t) {
  static volatile bool handling = false;
  if (!handling) {
    handling = true;
    CrashHandler::trigger("CRT invalid parameter", true);
  }
  _Exit(1);
}
static void pureCallHandler() {
  CrashHandler::trigger("Pure virtual function call", true);
  _Exit(1);
}
#endif

void CrashHandler::install() {
  // Cross-platform: catch uncaught C++ exceptions (silent std::terminate/abort).
  std::set_terminate(terminateHandler);
#ifdef _WIN32
  // std library seems to override this
  //SetUnhandledExceptionFilter(exceptionHandler);

  void *handler = AddVectoredExceptionHandler(0, exceptionHandler);
  assert(handler != NULL);
  //RemoveVectoredExceptionHandler(handler);
  _set_invalid_parameter_handler(invalidParameterHandler);
  _set_purecall_handler(pureCallHandler);
#else
  // backtrace() may allocate while loading the unwinder on its first call:
  // not in the handler, then
  void *frames[1];
  backtrace(frames, 1);
#ifdef MACOSX
  // called at once for the images already loaded, then for every new one
  _dyld_register_func_for_add_image(onImageAdded);
#endif

  // the alternate stack covers the thread calling install(), the main one
  stack_t ss;
  memset(&ss, 0, sizeof(ss));
  ss.ss_sp   = s_signalStack;
  ss.ss_size = sizeof(s_signalStack);
  sigaltstack(&ss, NULL);

  // SIGINT and SIGTERM are not crashes (Ctrl+C, logging out): they keep their
  // default action and leave no report
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = signalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGSEGV, &sa, NULL);
#ifdef SIGBUS
  sigaction(SIGBUS, &sa, NULL);
#endif
#endif
}

//-----------------------------------------------------------------------------

void CrashHandler::prepareReport() {
#ifndef _WIN32
  // everything the handler writes besides the addresses, gathered while Qt
  // and the heap can still be used
  TFilePath folder = ToonzFolder::getCrashReportFolder();
  if (folder.isEmpty()) return;

  std::string intro;
  try {
    intro.append(TEnv::getApplicationFullName() + " (Tahoma2D base " +
                 TEnv::getTahomaBaseVersionString() + ")  (Build " __DATE__
                 ")\n");
    intro.append("\n");
    printSysInfo(intro);
    intro.append(
        "\nReport Type: written at crash time with async-signal-safe calls, "
        "names resolved at the next launch; no GPU details\n");
  } catch (...) {
  }
  s_reportIntroLen = copyToBuffer(s_reportIntro, sizeof(s_reportIntro),
                                  intro.c_str(), intro.size());

  // the report of a crash in the last session, taken before a new crash can
  // overwrite it; it is filed and shown by showPendingReport()
  const QString pendingPath = (folder + "PendingCrashReport.log").getQString();
  QFile file(pendingPath);
  if (file.exists()) {
    if (file.open(QIODevice::ReadOnly)) {
      s_previousReport = QString::fromUtf8(file.readAll());
      file.close();
    }
    const QDateTime crashTime = QFileInfo(pendingPath).lastModified();
    s_previousReportFile =
        folder + ("Crash-" + crashTime.toString("yyyyMMdd-HHmmss") + ".log")
                     .toStdString();
    QFile::remove(pendingPath);
  }

  // the path last: the handler writes nothing while it is empty
  const QByteArray path = QFile::encodeName(pendingPath);
  if (path.size() < (int)sizeof(s_pendingReportPath))
    copyToBuffer(s_pendingReportPath, sizeof(s_pendingReportPath),
                 path.constData(), path.size());
#endif
}

//-----------------------------------------------------------------------------

void CrashHandler::showPendingReport() {
#ifndef _WIN32
  if (s_previousReport.isEmpty()) return;
  const QString report = symbolicate(s_previousReport);
  s_previousReport.clear();

  QFile out(s_previousReportFile.getQString());
  if (out.open(QIODevice::WriteOnly)) {
    out.write(report.toUtf8());
    out.close();
  }
  CrashHandler crashdialog(s_parentWindow, s_previousReportFile, report, true);
  crashdialog.exec();
#endif
}

//-----------------------------------------------------------------------------

void CrashHandler::reportProjectInfo(bool enableReport) {
  s_reportProjInfo = enableReport;
#ifndef _WIN32
  // the handler cannot ask the scene: it is told here whenever it changes
  static bool connected = false;
  if (!enableReport || connected) return;
  connected       = true;
  auto noteScene = []() {
    std::string info;
    try {
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      auto project      = TProjectManager::instance()->getCurrentProject();
      info.append("\nProject Name: " +
                  project->getName().getQString().toStdString());
      info.append("\nScene Path: " +
                  obfuscateUsername(scene->getScenePath().getQString())
                      .toStdString());
      info.append("\n");
    } catch (...) {
    }
    s_sceneInfoLen.store(0);
    s_sceneInfoLen.store(copyToBuffer(s_sceneInfo, sizeof(s_sceneInfo),
                                      info.c_str(), info.size()));
  };
  TSceneHandle *sh = TApp::instance()->getCurrentScene();
  QObject::connect(sh, &TSceneHandle::sceneSwitched, noteScene);
  QObject::connect(sh, &TSceneHandle::nameSceneChanged, noteScene);
  noteScene();
#endif
}

//-----------------------------------------------------------------------------

void CrashHandler::attachParentWindow(QWidget *parent) {
  s_parentWindow = parent;
}

void CrashHandler::setAppExiting() {
  s_appExiting = true;
}

//-----------------------------------------------------------------------------

bool CrashHandler::trigger(const QString reason, bool showDialog) {
  char fileName[128];
  char dumpName[128];
  char dateName[128];
  std::string out;


  // Get time and build filename
  time_t acc_time;
  time(&acc_time);
  struct tm *tm = localtime(&acc_time);
  strftime(dateName, 128, "%Y-%m-%d %H:%M:%S", tm);
  strftime(fileName, 128, "Crash-%Y%m%d-%H%M%S.log", tm);
  strftime(dumpName, 128, "Crash-%Y%m%d-%H%M%S.dmp", tm);
  TFilePath fpCrsh = ToonzFolder::getCrashReportFolder() + fileName;
  TFilePath fpDump = ToonzFolder::getCrashReportFolder() + dumpName;

  // Generate minidump
#ifdef _WIN32
  bool minidump = generateMinidump(fpDump, s_exceptionPtr);
#else
  bool minidump = generateMinidump(fpDump);
#endif

  // Generate report
  try {
    out.append(TEnv::getApplicationFullName() + " (Tahoma2D base " +
               TEnv::getTahomaBaseVersionString() + ")  (Build " + __DATE__ ")\n");
    out.append("\nReport Date: ");
    out.append(dateName);
    out.append("\nCrash Reason: ");
    out.append(reason.toStdString());
    out.append("\n\n");
    printSysInfo(out);
    out.append("\n");
    printGPUInfo(out);
    out.append("\nCrash File: ");
    out.append(obfuscateUsername(fpCrsh.getQString()).toStdString());
#ifdef HAS_MINIDUMP
    out.append("\nMini Dump File: ");
    if (minidump)
      out.append(obfuscateUsername(fpDump.getQString()).toStdString());
    else
      out.append("Failed");
#endif
    out.append("\n");
  } catch (...) {
  }
  try {
    if (s_reportProjInfo) {
      auto currentProject = TProjectManager::instance()->getCurrentProject();
      TFilePath projectPath    = currentProject->getProjectPath();

      ToonzScene *currentScene = TApp::instance()->getCurrentScene()->getScene();
      std::wstring sceneName   = currentScene->getSceneName();

      out.append("\nApplication Dir: ");
      out.append(obfuscateUsername(QCoreApplication::applicationDirPath())
                     .toStdString());
      out.append("\nStuff Dir: ");
      out.append(
          obfuscateUsername(TEnv::getStuffDir().getQString()).toStdString());
      out.append("\n");
      out.append("\nProject Name: ");
      out.append(currentProject->getName().getQString().toStdString());
      out.append("\nScene Name: ");
      out.append(QString::fromStdWString(sceneName).toStdString());
      out.append("\nProject Path: ");
      out.append(obfuscateUsername(projectPath.getQString()).toStdString());
      out.append("\nScene Path: ");
      out.append(obfuscateUsername(currentScene->getScenePath().getQString())
                     .toStdString());
      out.append("\n");
    }
  } catch (...) {
  }
#ifdef HAS_BACKTRACE
  try {
    out.append("\n==== Backtrace ====\n");
    printBacktrace(out);
    out.append("==== End ====\n");
  } catch (...) {
  }
#endif
#ifdef HAS_MODULES
  try {
    out.append("\n==== Modules ====\n");
    printModules(out);
    out.append("==== End ====\n");
  } catch (...) {
  }
#endif

  // Save to crash information to file
  FILE *fw = fopen(fpCrsh, "w");
  if (fw != NULL) {
    fwrite(out.c_str(), 1, out.size(), fw);
    fclose(fw);
  }

  // On macOS (and generally), creating a QDialog (NSWindow) from a non-main
  // thread is illegal and throws an ObjC exception. When the crash originates
  // on a render thread, we still write the log file above, but skip the dialog.
  if (showDialog &&
      QThread::currentThread() != QCoreApplication::instance()->thread()) {
    showDialog = false;
  }

  if (showDialog) {
    // Show crash handler dialog
    CrashHandler crashdialog(s_parentWindow, fpCrsh, QString::fromStdString(out));
    return crashdialog.exec() != QDialog::Rejected;
  }

  return true;
}

//-----------------------------------------------------------------------------
