#pragma once

#include <string_view>

// Diathic: crash report for testers. On a crash the game writes gra\dane\raporty\crash-<date>\:
// kontekst.txt (exception, module + offset for addr2line, world, hero position, stack walked from the exception),
// crash.log (OpenGothic's trace), crash.dmp (minidump, Windows) and log.txt (flushed, so the last lines are there).
// The launcher zips the newest one with system info ("Zgłoś błąd").
namespace ArpgCrash {
  // main: the log file to flush and copy into the report; Windows: stack kept for the handler (stack overflow)
  void setup(const char* logPath, void (*flush)());
  // GameSession::tick: where the hero is (only stored, read on a crash)
  void setPlace(std::string_view world, float x, float y, float z, bool arpg);
  // Windows exception filter: EXCEPTION_POINTERS of the crash
  void setException(void* exceptionPointers);
  // CrashLog::dumpStack, after crash.log is written: the report folder (once)
  void report(const char* sig);
}
