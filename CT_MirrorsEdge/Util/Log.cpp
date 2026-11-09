#include "Util.h"
#include <mutex>
#include <share.h>
#include <stdarg.h>
#include <stdio.h>

using namespace util;

std::mutex g_logMutex;

namespace
{
  HANDLE hstdin, hstdout;
  FILE* pfstdin;
  FILE* pfstdout;
  FILE* pfileout;
  bool hasConsole = false;

  static void PrintTimeStamp()
  {
    SYSTEMTIME now;
    GetLocalTime(&now);

    char sTimeStamp[32];
    sprintf_s(sTimeStamp, "[%02d:%02d:%02d] ", now.wHour, now.wMinute, now.wSecond);

    if (hasConsole)
    {
      SetConsoleTextAttribute(hstdout, FOREGROUND_RED | FOREGROUND_INTENSITY);
      fputs(sTimeStamp, stdout);
    }
    if (pfileout)
      fputs(sTimeStamp, pfileout);
  }

  // With g_logMutex held
  static void PrintHeld(WORD color, const char* type, const char* format, va_list args)
  {
    PrintTimeStamp();
    std::string finalFormat = std::string(type) + format + "\n";

    va_list argsCopy;
    va_copy(argsCopy, args);
    if (hasConsole)
    {
      SetConsoleTextAttribute(hstdout, color);
      vfprintf(stdout, finalFormat.c_str(), args);
    }
    if (pfileout)
    {
      vfprintf(pfileout, finalFormat.c_str(), argsCopy);
      fflush(pfileout);
    }
    va_end(argsCopy);
  }

  static void PrintMessage(WORD color, const char* type, const char* format, va_list args)
  {
    // Block other threads from writing at the same time
    std::lock_guard<std::mutex> lock(g_logMutex);
    PrintHeld(color, type, format, args);
  }
}

void log::Init(bool showConsole)
{
  // The last session's log is kept, so it's still there after a crash and a restart
  MoveFileExA(".\\Cinematic Tools\\CT.log", ".\\Cinematic Tools\\CT.previous.log", MOVEFILE_REPLACE_EXISTING);
  // Shared for reading, so the log can be opened while the game runs
  // (fopen_s would lock it)
  pfileout = _fsopen(".\\Cinematic Tools\\CT.log", "w", _SH_DENYWR);
  if (!showConsole)
    return;

  hasConsole = true;
  AllocConsole();
  freopen_s(&pfstdout, "CONOUT$", "w", stdout);
  freopen_s(&pfstdin, "CONIN$", "r", stdin);
  hstdin = GetStdHandle(STD_INPUT_HANDLE);
  hstdout = GetStdHandle(STD_OUTPUT_HANDLE);

  // Clicking a console with QuickEdit on puts it in "Select" mode, which
  // blocks every thread that writes to it, including the game's own threads
  // when they log from a hook. Turn it off so the game can't be frozen that way.
  DWORD consoleMode = 0;
  if (GetConsoleMode(hstdin, &consoleMode))
    SetConsoleMode(hstdin, (consoleMode & ~ENABLE_QUICK_EDIT_MODE) | ENABLE_EXTENDED_FLAGS);
}

void log::Write(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  PrintMessage(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "", format, args);
  va_end(args);
}

void log::Warning(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  PrintMessage(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "[WARNING] ", format, args);
  va_end(args);
}

void log::Error(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  PrintMessage(FOREGROUND_RED | FOREGROUND_INTENSITY, "[ERROR] ", format, args);
  va_end(args);
}

void log::Ok(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  PrintMessage(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "[OK] ", format, args);
  va_end(args);
}
void log::CrashNote(const char* text)
{
  // No lock: the crashing thread might be the one holding it
  if (pfileout)
  {
    fputs(text, pfileout);
    fputs("\n", pfileout);
    fflush(pfileout);
  }
}

void log::ExitNote(const char* text)
{
  // Not even the file's own lock: a thread Windows stopped mid-write still
  // holds it, and nothing else runs any more
  if (!pfileout)
    return;
  SYSTEMTIME now;
  GetLocalTime(&now);
  char stamp[32];
  sprintf_s(stamp, "[%02d:%02d:%02d] ", now.wHour, now.wMinute, now.wSecond);
  _fwrite_nolock(stamp, 1, strlen(stamp), pfileout);
  _fwrite_nolock(text, 1, strlen(text), pfileout);
  _fwrite_nolock("\n", 1, 1, pfileout);
  _fflush_nolock(pfileout);
}

bool log::TryWarning(const char* format, ...)
{
  std::unique_lock<std::mutex> lock(g_logMutex, std::try_to_lock);
  if (!lock.owns_lock())
    return false;
  va_list args;
  va_start(args, format);
  PrintHeld(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "[WARNING] ", format, args);
  va_end(args);
  return true;
}
