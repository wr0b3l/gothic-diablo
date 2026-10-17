#include "arpgcrash.h"

#include <Tempest/Platform>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>

#ifdef __WINDOWS__
#include <windows.h>
#include <dbghelp.h>
#endif

namespace {

char   worldName[64] = "-";
size_t worldLen      = 0;
float  heroPos[3]    = {};
bool   heroValid     = false;
bool   arpgMode      = false;

const char* logPath  = nullptr;
void      (*logFlush)() = nullptr;

#ifdef __WINDOWS__
EXCEPTION_POINTERS* exPtrs = nullptr;

const char* exceptionName(DWORD code) {
  switch(code) {
    case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
    case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_PRIV_INSTRUCTION:      return "PRIV_INSTRUCTION";
    case EXCEPTION_BREAKPOINT:            return "BREAKPOINT";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_IN_PAGE_ERROR:         return "IN_PAGE_ERROR";
    case 0xC0000409:                      return "STACK_BUFFER_OVERRUN";
    case 0x20474343:                      return "GCC C++ exception";
    case 0xE06D7363:                      return "MSVC C++ exception";
    }
  return "?";
  }

struct Module {
  char    name[MAX_PATH] = "?";
  DWORD64 base = 0; // where it is loaded
  DWORD64 pref = 0; // image base in the file: addr2line address = pref + offset
  bool    exe  = false;
  };

Module moduleOf(DWORD64 addr) {
  Module  m;
  HMODULE h = nullptr;
  if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(addr), &h) || h==nullptr)
    return m;
  char path[MAX_PATH] = {};
  GetModuleFileNameA(h, path, MAX_PATH);
  const char* slash = std::max(std::strrchr(path,'\\'), std::strrchr(path,'/'));
  std::snprintf(m.name, sizeof(m.name), "%s", slash!=nullptr ? slash+1 : path);
  m.base = reinterpret_cast<DWORD64>(h);
  m.exe  = (h==GetModuleHandleA(nullptr));
  auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(h);
  auto nt  = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const char*>(h)+dos->e_lfanew);
  m.pref = DWORD64(nt->OptionalHeader.ImageBase);
  return m;
  }

void printAddr(std::FILE* f, DWORD64 addr) {
  const Module m = moduleOf(addr);
  if(m.base==0) {
    std::fprintf(f, "0x%llx", (unsigned long long)addr);
    return;
    }
  const DWORD64 off = addr-m.base;
  std::fprintf(f, "%s+0x%llx", m.name, (unsigned long long)off);
  if(m.exe)
    std::fprintf(f, "   (addr2line 0x%llx)", (unsigned long long)(m.pref+off));
  }

void writeDump(const char* path) {
  HMODULE dbg = LoadLibraryA("dbghelp.dll");
  if(dbg==nullptr)
    return;
  using Fn = BOOL (WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                            PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
  auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(dbg, "MiniDumpWriteDump")));
  if(fn==nullptr)
    return;
  HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if(file==INVALID_HANDLE_VALUE)
    return;
  MINIDUMP_EXCEPTION_INFORMATION ei = {};
  ei.ThreadId          = GetCurrentThreadId();
  ei.ExceptionPointers = exPtrs;
  ei.ClientPointers    = FALSE;
  const auto type = MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
  fn(GetCurrentProcess(), GetCurrentProcessId(), file, type, exPtrs!=nullptr ? &ei : nullptr, nullptr, nullptr);
  CloseHandle(file);
  }

void writeStack(std::FILE* f) {
#if defined(__x86_64__) || defined(_M_X64)
  if(exPtrs==nullptr || exPtrs->ContextRecord==nullptr)
    return;
  // the stack of the crash itself, unwound from the exception context (.pdata of every module)
  std::fprintf(f, "\nstos w chwili bledu:\n");
  std::fflush(f);
  CONTEXT ctx = *exPtrs->ContextRecord;
  for(int i=0; i<48 && ctx.Rip!=0; ++i) {
    std::fprintf(f, "#%02d ", i);
    printAddr(f, ctx.Rip);
    std::fprintf(f, "\n");
    std::fflush(f); // a broken stack can crash the walk: keep what is written
    DWORD64 base = 0;
    auto    fe   = RtlLookupFunctionEntry(ctx.Rip, &base, nullptr);
    if(fe==nullptr) {
      // leaf function: the return address is on top of the stack
      ctx.Rip  = *reinterpret_cast<const DWORD64*>(ctx.Rsp);
      ctx.Rsp += 8;
      continue;
      }
    void*   handlerData = nullptr;
    DWORD64 establisher = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx.Rip, fe, &ctx, &handlerData, &establisher, nullptr);
    }
#else
  (void)f;
#endif
  }
#endif

void stamp(char* out, size_t n) {
#ifdef __WINDOWS__
  SYSTEMTIME t = {};
  GetLocalTime(&t);
  std::snprintf(out, n, "%04d-%02d-%02d_%02d-%02d-%02d", int(t.wYear), int(t.wMonth), int(t.wDay),
                int(t.wHour), int(t.wMinute), int(t.wSecond));
#else
  const std::time_t now = std::time(nullptr);
  std::tm tmv = {};
  localtime_r(&now, &tmv);
  std::strftime(out, n, "%Y-%m-%d_%H-%M-%S", &tmv);
#endif
  }

void copyFile(const char* from, const std::filesystem::path& to) {
  std::error_code ec;
  if(from!=nullptr && std::filesystem::exists(from, ec))
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
  }
}

void ArpgCrash::setup(const char* path, void (*flush)()) {
  logPath  = path;
  logFlush = flush;
#ifdef __WINDOWS__
  // stack overflow: keep some stack for the crash handler on the main thread
  ULONG guarantee = 64*1024;
  SetThreadStackGuarantee(&guarantee);
#endif
  }

void ArpgCrash::setPlace(std::string_view world, float x, float y, float z, bool arpg) {
  if(world.size()!=worldLen || std::memcmp(world.data(), worldName, std::min(world.size(), sizeof(worldName)-1))!=0) {
    worldLen = std::min(world.size(), sizeof(worldName)-1);
    std::memcpy(worldName, world.data(), worldLen);
    worldName[worldLen] = '\0';
    }
  heroPos[0] = x;
  heroPos[1] = y;
  heroPos[2] = z;
  heroValid  = true;
  arpgMode   = arpg;
  }

void ArpgCrash::setException(void* exceptionPointers) {
#ifdef __WINDOWS__
  exPtrs = reinterpret_cast<EXCEPTION_POINTERS*>(exceptionPointers);
#else
  (void)exceptionPointers;
#endif
  }

void ArpgCrash::report(const char* sig) {
  static bool done = false; // abort() after std::terminate comes here again
  if(done)
    return;
  done = true;

  char ts[32] = {};
  stamp(ts, sizeof(ts));
  const std::filesystem::path dir = std::filesystem::path("raporty") / (std::string("crash-")+ts);
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if(ec)
    return;

  // the most important first: a second crash inside the handler ends everything
  std::FILE* f = std::fopen((dir/"kontekst.txt").string().c_str(), "w");
  if(f!=nullptr) {
    std::fprintf(f, "Diathic - raport bledu %s\n", ts);
    std::fprintf(f, "sygnal: %s\n", sig!=nullptr ? sig : "?");
#ifdef __WINDOWS__
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::fprintf(f, "exe: %s\n", exe);
    const Module self = moduleOf(reinterpret_cast<DWORD64>(GetModuleHandleA(nullptr)));
    std::fprintf(f, "exe w pamieci: 0x%llx, baza w pliku: 0x%llx\n", (unsigned long long)self.base, (unsigned long long)self.pref);
    if(exPtrs!=nullptr && exPtrs->ExceptionRecord!=nullptr) {
      const auto* r = exPtrs->ExceptionRecord;
      std::fprintf(f, "wyjatek: 0x%08lx %s pod ", (unsigned long)r->ExceptionCode, exceptionName(r->ExceptionCode));
      printAddr(f, reinterpret_cast<DWORD64>(r->ExceptionAddress));
      std::fprintf(f, "\n");
      if(r->ExceptionCode==EXCEPTION_ACCESS_VIOLATION && r->NumberParameters>=2) {
        const auto op = r->ExceptionInformation[0];
        std::fprintf(f, "dostep: %s adresu 0x%llx\n", op==0 ? "odczyt" : (op==1 ? "zapis" : "wykonanie"),
                     (unsigned long long)r->ExceptionInformation[1]);
        }
      }
#endif
    std::fprintf(f, "tryb: %s\n", arpgMode ? "ARPG" : "klasyczny");
    std::fprintf(f, "swiat: %s\n", worldName);
    if(heroValid)
      std::fprintf(f, "bohater: %.0f %.0f %.0f\n", double(heroPos[0]), double(heroPos[1]), double(heroPos[2]));
    std::fflush(f);
    }

  copyFile("crash.log", dir/"crash.log");
#ifdef __WINDOWS__
  writeDump((dir/"crash.dmp").string().c_str());
#endif
  // the log is buffered (flushed only on errors): flush and copy, the next start overwrites log.txt
  if(logFlush!=nullptr)
    logFlush();
  copyFile(logPath, dir/"log.txt");

#ifdef __WINDOWS__
  if(f!=nullptr)
    writeStack(f);
#endif
  if(f!=nullptr)
    std::fclose(f);
  }
