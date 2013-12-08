/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_DEBUGGING_H
#define MTX_COMMON_DEBUGGING_H

#include "common/common_pch.h"

bool debugging_requested(const char *option, std::string *arg = nullptr);
bool debugging_requested(const std::string &option, std::string *arg = nullptr);
void request_debugging(const std::string &options);
void init_debugging();

int parse_debug_interval_arg(const std::string &option, int default_value = 1000, int invalid_value = -1);

class debugging_option_c {
  struct option_c {
    boost::tribool m_requested;
    std::string m_option;

    option_c(std::string const &option)
      : m_requested{boost::logic::indeterminate}
      , m_option{option}
    {
    }

    bool get() {
      if (boost::logic::indeterminate(m_requested))
        m_requested = debugging_requested(m_option);

      return m_requested;
    }
  };

protected:
  size_t m_registered_idx;
  std::string m_option;

private:
  static std::vector<option_c> ms_registered_options;

public:
  debugging_option_c(std::string const &option)
    : m_registered_idx{std::numeric_limits<size_t>::max()}
    , m_option{option}
  {
  }

  operator bool() {
    if (m_registered_idx == std::numeric_limits<size_t>::max())
      m_registered_idx = register_option(m_option);

    return ms_registered_options[m_registered_idx].get();
  }

public:
  static size_t register_option(std::string const &option);
  static void invalidate_cache();
};

#endif  // MTX_COMMON_DEBUGGING_H
