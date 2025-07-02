#include <utility>

#ifdef VBOX
# include <iprt/log.h>
#endif

#include "log.h"

#include "../util_env.h"

namespace dxvk {
  
  Logger::Logger(const std::string& fileName)
  : m_minLevel(getMinLogLevel()), m_fileName(fileName) {

  }
  
  
  Logger::~Logger() { }
  
  
  void Logger::trace(const std::string& message) {
#ifndef VBOX
    s_instance.emitMsg(LogLevel::Trace, message);
#else
    LogRel(("%s\n", message.c_str()));
#endif
  }
  
  
  void Logger::debug(const std::string& message) {
#ifndef VBOX
    s_instance.emitMsg(LogLevel::Debug, message);
#else
    Log(("%s\n", message.c_str()));
#endif
  }
  
  
  void Logger::info(const std::string& message) {
#ifndef VBOX
    s_instance.emitMsg(LogLevel::Info, message);
#else
    LogRel(("%s\n", message.c_str()));
#endif
  }
  
  
  void Logger::warn(const std::string& message) {
#ifndef VBOX
    s_instance.emitMsg(LogLevel::Warn, message);
#else
    LogRel(("%s\n", message.c_str()));
#endif
  }
  
  
  void Logger::err(const std::string& message) {
#ifndef VBOX
    s_instance.emitMsg(LogLevel::Error, message);
#else
    LogRel(("%s\n", message.c_str()));
#endif
  }
  
  
  void Logger::log(LogLevel level, const std::string& message) {
#ifndef VBOX
    s_instance.emitMsg(level, message);
#else
    Log(("%s\n", message.c_str()));
#endif
  }

#ifndef VBOX
  
  void Logger::emitMsg(LogLevel level, const std::string& message) {
    if (level >= m_minLevel) {
      std::lock_guard<dxvk::mutex> lock(m_mutex);
      
      static std::array<const char*, 5> s_prefixes
        = {{ "trace: ", "debug: ", "info:  ", "warn:  ", "err:   " }};
      
      const char* prefix = s_prefixes.at(static_cast<uint32_t>(level));

      if (!std::exchange(m_initialized, true)) {
#ifdef _WIN32
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");

        if (ntdll)
          m_wineLogOutput = reinterpret_cast<PFN_wineLogOutput>(GetProcAddress(ntdll, "__wine_dbg_output"));
#endif
        auto path = getFileName(m_fileName);

        if (!path.empty())
          m_fileStream = std::ofstream(str::topath(path.c_str()).c_str());
      }

      std::stringstream stream(message);
      std::string line;

      while (std::getline(stream, line, '\n')) {
        std::stringstream outstream;
        outstream << prefix << line << std::endl;

        std::string adjusted = outstream.str();

        if (!adjusted.empty()) {
#ifdef _WIN32
          if (m_wineLogOutput) {
            // __wine_dbg_output tries to buffer lines up to 1020 characters
            // including null terminator, and will cause a hang if we submit
            // anything longer than that even in consecutive calls. Work
            // around this by splitting long lines into multiple lines.
            constexpr size_t MaxDebugBufferLength = 1018;

            if (adjusted.size() <= MaxDebugBufferLength) {
              m_wineLogOutput(adjusted.c_str());
            } else {
              std::array<char, MaxDebugBufferLength + 2u> buffer;

              for (size_t i = 0; i < adjusted.size(); i += MaxDebugBufferLength) {
                size_t size = std::min(adjusted.size() - i, MaxDebugBufferLength);

                std::strncpy(buffer.data(), &adjusted[i], size);
                if (buffer[size - 1u] != '\n')
                  buffer[size++] = '\n';

                buffer[size] = '\0';
                m_wineLogOutput(buffer.data());
              }
            }
          }

          // Don't log anything to stderr if we're not on wine. Usually games are
          // compiled as gui apps anyway, and emitting anything to the standard
          // output streams can crash certain games.
#else
          // For native builds, logging to stderr should be fine.
          std::cerr << adjusted;
#endif
        }

        if (m_fileStream)
          m_fileStream << adjusted;
      }
    }
  }
#endif

  
  std::string Logger::getFileName(const std::string& base) {
#ifndef VBOX
    std::string path = env::getEnvVar("DXVK_LOG_PATH");
    
    if (path == "none")
      return std::string();

#ifdef _WIN32
    // Don't create a log file if we're writing to wine's console output
    if (path.empty() && m_wineLogOutput)
      return std::string();
#endif

    if (!path.empty() && *path.rbegin() != '/')
      path += '/';

    std::string exeName = env::getExeBaseName();
    path += exeName + "_" + base;
    return path;
#else
    return "";
#endif
  }


  LogLevel Logger::getMinLogLevel() {
#ifndef VBOX
    const std::array<std::pair<const char*, LogLevel>, 6> logLevels = {{
      { "trace", LogLevel::Trace },
      { "debug", LogLevel::Debug },
      { "info",  LogLevel::Info  },
      { "warn",  LogLevel::Warn  },
      { "error", LogLevel::Error },
      { "none",  LogLevel::None  },
    }};
    
    const std::string logLevelStr = env::getEnvVar("DXVK_LOG_LEVEL");
    
    for (const auto& pair : logLevels) {
      if (logLevelStr == pair.first)
        return pair.second;
    }
#endif
    return LogLevel::Info;
  }
  
}
