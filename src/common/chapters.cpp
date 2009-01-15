/** \brief chapter parser and helper functions

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <ctype.h>
#include <stdarg.h>

#include <cassert>
#include <string>
#include <boost/regex.hpp>

#include <matroska/KaxChapters.h>

#include "chapters.h"
#include "commonebml.h"
#include "error.h"

using namespace std;
using namespace libmatroska;

/** The default language for all chapter entries that don't have their own. */
string default_chapter_language;
/** The default country for all chapter entries that don't have their own. */
string default_chapter_country;

#define SIMCHAP_RE_TIMECODE_LINE "^\\s*CHAPTER\\d+\\s*=\\s*(\\d+)\\s*:\\s*(\\d+)\\s*:\\s*(\\d+)\\s*[\\.,]\\s*(\\d+)"
#define SIMCHAP_RE_TIMECODE      "^\\s*CHAPTER\\d+\\s*=(.*)"
#define SIMCHAP_RE_NAME_LINE     "^\\s*CHAPTER\\d+NAME\\s*=(.*)"

/** \brief Throw a special chapter parser exception.

   \param error The error message.
*/
inline void
chapter_error(const std::string &error) {
  throw error_c(boost::format(Y("Simple chapter parser: %1%\n")) % error);
}

inline void
chapter_error(const boost::format &format) {
  chapter_error(format.str());
}

/** \brief Reads the start of a file and checks for OGM style comments.

   The first lines are read. OGM style comments are recognized if the first
   non-empty line contains <tt>CHAPTER01=...</tt> and the first non-empty
   line afterwards contains <tt>CHAPTER01NAME=...</tt>.

   The parameters are checked for validity.

   \param in The file to read from.

   \return \c true if the file contains OGM style comments and \c false
     otherwise.
*/
bool
probe_simple_chapters(mm_text_io_c *in) {
  boost::regex timecode_line_re(SIMCHAP_RE_TIMECODE_LINE, boost::regex::perl);
  boost::regex name_line_re(    SIMCHAP_RE_NAME_LINE,     boost::regex::perl);
  boost::match_results<string::const_iterator> matches;

  string line;

  assert(in != NULL);

  in->setFilePointer(0);
  while (in->getline2(line)) {
    strip(line);
    if (line.empty())
      continue;

    if (!boost::regex_search(line, timecode_line_re))
      return false;

    while (in->getline2(line)) {
      strip(line);
      if (line.empty())
        continue;

      return boost::regex_search(line, name_line_re);
    }

    return false;
  }

  return false;
}

//           1         2
// 012345678901234567890123
//
// CHAPTER01=00:00:00.000
// CHAPTER01NAME=Hallo Welt

/** \brief Parse simple OGM style comments

   The file \a in is read. The content is assumed to be OGM style comments.

   The parameters are checked for validity.

   \param in The text file to read from.
   \param min_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param max_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param offset An optional offset that is subtracted from all start and
     end timecodes after the timerange check has been made.
   \param language This language is added as the \c KaxChapterLanguage
     for all entries.
   \param charset The charset the chapters are supposed to be it. The entries
     will be converted to UTF-8 if necessary.
   \param exception_on_error If set to \c true then an exception is thrown
     if an error occurs. Otherwise \c NULL will be returned.

   \return The chapters parsed from the file or \c NULL if an error occured.
*/
KaxChapters *
parse_simple_chapters(mm_text_io_c *in,
                      int64_t min_tc,
                      int64_t max_tc,
                      int64_t offset,
                      const string &language,
                      const string &charset,
                      bool exception_on_error) {
  KaxChapters *chaps;
  KaxEditionEntry *edition;
  KaxChapterAtom *atom;
  KaxChapterDisplay *display;
  int64_t start, hour, minute, second, msecs;
  string name, line, use_language, s_timecode;
  int mode, num, cc_utf8;
  bool do_convert;
  UTFstring wchar_string;

  assert(in != NULL);

  in->setFilePointer(0);
  chaps = new KaxChapters;

  mode = 0;
  atom = NULL;
  edition = NULL;
  num = 0;
  start = 0;
  cc_utf8 = 0;

  // The core now uses ns precision timecodes.
  if (min_tc > 0)
    min_tc /= 1000000;
  if (max_tc > 0)
    max_tc /= 1000000;
  if (offset > 0)
    offset /= 1000000;

  if (in->get_byte_order() == BO_NONE) {
    do_convert = true;
    cc_utf8 = utf8_init(charset);

  } else
    do_convert = false;

  if (language == "") {
    if (default_chapter_language.length() > 0)
      use_language = default_chapter_language;
    else
      use_language = "eng";
  } else
    use_language = language;

  boost::regex timecode_line_re(SIMCHAP_RE_TIMECODE_LINE, boost::regex::perl);
  boost::regex timecode_re(     SIMCHAP_RE_TIMECODE,      boost::regex::perl);
  boost::regex name_line_re(    SIMCHAP_RE_NAME_LINE,     boost::regex::perl);
  boost::match_results<string::const_iterator> matches;

  try {
    while (in->getline2(line)) {
      strip(line);
      if (line.length() == 0)
        continue;

      if (mode == 0) {
        if (!boost::regex_match(line, matches, timecode_line_re))
          chapter_error(boost::format(Y("'%1%' is not a CHAPTERxx=... line.")) % line);
        parse_int(matches[1].str(), hour);
        parse_int(matches[2].str(), minute);
        parse_int(matches[3].str(), second);
        parse_int(matches[4].str(), msecs);
        if (minute > 59)
          chapter_error(boost::format(Y("Invalid minute: %1%")) % minute);
        if (second > 59)
          chapter_error(boost::format(Y("Invalid second: %1%")) % second);
        start = msecs + second * 1000 + minute * 1000 * 60 +
          hour * 1000 * 60 * 60;
        mode = 1;

        if (!boost::regex_match(line, matches, timecode_re))
          chapter_error(boost::format(Y("'%1%' is not a CHAPTERxx=... line.")) % line);
        s_timecode = matches[1].str();

      } else {
        if (!boost::regex_match(line, matches, name_line_re))
          chapter_error(boost::format(Y("'%1%' is not a CHAPTERxxNAME=... line.")) % line);
        name = matches[1].str();
        if (name.empty())
          name = s_timecode;
        mode = 0;

        if ((start >= min_tc) && ((start <= max_tc) || (max_tc == -1))) {
          if (edition == NULL)
            edition = &GetChild<KaxEditionEntry>(*chaps);
          if (atom == NULL)
            atom = &GetChild<KaxChapterAtom>(*edition);
          else
            atom = &GetNextChild<KaxChapterAtom>(*edition, *atom);

          *static_cast<EbmlUInteger *>(&GetChild<KaxChapterUID>(*atom)) =
            create_unique_uint32(UNIQUE_CHAPTER_IDS);

          *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(*atom)) =
            (start - offset) * 1000000;

          display = &GetChild<KaxChapterDisplay>(*atom);

          if (do_convert)
            wchar_string = cstrutf8_to_UTFstring(to_utf8(cc_utf8, name));
          else
            wchar_string = cstrutf8_to_UTFstring(name);
          *static_cast<EbmlUnicodeString *>
            (&GetChild<KaxChapterString>(*display)) = wchar_string;

          *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(*display)) =
            use_language;

          if (default_chapter_country.length() > 0)
            *static_cast<EbmlString *>
              (&GetChild<KaxChapterCountry>(*display)) =
              default_chapter_country;

          num++;
        }
      }
    }
  } catch (error_c e) {
    delete chaps;
    throw error_c(e);
  }

  if (num == 0) {
    delete chaps;
    return NULL;
  }

  return chaps;
}

/** \brief Probe a file for different chapter formats and parse the file.

   The file \a file_name is opened and checked for supported chapter formats.
   These include simple OGM style chapters, CUE sheets and mkvtoolnix' own
   XML chapter format.

   Its parameters don't have to be checked for validity.

   \param file_name The name of the text file to read from.
   \param min_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param max_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param offset An optional offset that is subtracted from all start and
     end timecodes after the timerange check has been made.
   \param language This language is added as the \c KaxChapterLanguage
     for entries that don't specifiy it.
   \param charset The charset the chapters are supposed to be it. The entries
     will be converted to UTF-8 if necessary. This parameter is ignored for XML
     chapter files.
   \param exception_on_error If set to \c true then an exception is thrown
     if an error occurs. Otherwise \c NULL will be returned.
   \param is_simple_format This boolean will be set to \c true if the chapter
     format is either the OGM style format or a CUE sheet. May be \c NULL if
     the caller is not interested in the result.
   \param tags When parsing a CUE sheet tags will be created along with the
     chapter entries. These tags will be stored in this parameter.

   \return The chapters parsed from the file or \c NULL if an error occured.

   \see ::parse_chapters(mm_text_io_c *in,int64_t min_tc,int64_t max_tc, int64_t offset,const string &language,const string &charset,bool exception_on_error,bool *is_simple_format,KaxTags **tags)
*/
KaxChapters *
parse_chapters(const string &file_name,
               int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               const string &language,
               const string &charset,
               bool exception_on_error,
               bool *is_simple_format,
               KaxTags **tags) {
  mm_text_io_c *in;
  KaxChapters *result;

  in = NULL;
  result = NULL;
  try {

    in = new mm_text_io_c(new mm_file_io_c(file_name));
    result = parse_chapters(in, min_tc, max_tc, offset, language, charset,
                            exception_on_error, is_simple_format, tags);
    delete in;
  } catch (error_c &e) {
    if (in != NULL)
      delete in;
    if (exception_on_error)
      throw;
    mxerror(boost::format(Y("Could not parse the chapters in '%1%': %2%\n")) % file_name % e.get_error());
  } catch (...) {
    if (in != NULL)
      delete in;
    if (exception_on_error)
      throw error_c(boost::format(Y("Could not open '%1%' for reading.\n")) % file_name);
    else
      mxerror(boost::format(Y("Could not open '%1%' for reading.\n")) % file_name);
  }
  return result;
}

/** \brief Probe a file for different chapter formats and parse the file.

   The file \a in is checked for supported chapter formats. These include
   simple OGM style chapters, CUE sheets and mkvtoolnix' own XML chapter
   format.

   The parameters are checked for validity.

   \param in The text file to read from.
   \param min_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param max_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param offset An optional offset that is subtracted from all start and
     end timecodes after the timerange check has been made.
   \param language This language is added as the \c KaxChapterLanguage
     for entries that don't specifiy it.
   \param charset The charset the chapters are supposed to be it. The entries
     will be converted to UTF-8 if necessary. This parameter is ignored for XML
     chapter files.
   \param exception_on_error If set to \c true then an exception is thrown
     if an error occurs. Otherwise \c NULL will be returned.
   \param is_simple_format This boolean will be set to \c true if the chapter
     format is either the OGM style format or a CUE sheet. May be \c NULL if
     the caller is not interested in the result.
   \param tags When parsing a CUE sheet tags will be created along with the
     chapter entries. These tags will be stored in this parameter.

   \return The chapters parsed from the file or \c NULL if an error occured.

   \see ::parse_chapters(const string &file_name,int64_t min_tc,int64_t max_tc, int64_t offset,const string &language,const string &charset,bool exception_on_error,bool *is_simple_format,KaxTags **tags)
*/
KaxChapters *
parse_chapters(mm_text_io_c *in,
               int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               const string &language,
               const string &charset,
               bool exception_on_error,
               bool *is_simple_format,
               KaxTags **tags) {
  assert(in != NULL);

  try {
    if (probe_simple_chapters(in)) {
      if (is_simple_format != NULL)
        *is_simple_format = true;
      return parse_simple_chapters(in, min_tc, max_tc, offset, language,
                                   charset, exception_on_error);
    } else if (probe_cue_chapters(in)) {
      if (is_simple_format != NULL)
        *is_simple_format = true;
      return parse_cue_chapters(in, min_tc, max_tc, offset, language,
                                charset, exception_on_error, tags);
    } else if (is_simple_format != NULL)
      *is_simple_format = false;

    if (probe_xml_chapters(in))
      return parse_xml_chapters(in, min_tc, max_tc, offset,
                                exception_on_error);

    throw error_c(boost::format(Y("Unknown chapter file format in '%1%'. It does not contain a supported chapter format.\n")) % in->get_file_name());
  } catch (error_c e) {
    if (exception_on_error)
      throw e;
    mxerror(e.get_error());
  }

  return NULL;
}

/** \brief Get the start timecode for a chapter atom.

   Its parameters don't have to be checked for validity.

   \param atom The atom for which the start timecode should be returned.
   \param value_if_not_found The value to return if no start timecode child
     element was found. Defaults to -1.

   \return The start timecode or \c value_if_not_found if the atom doesn't
     contain such a child element.
*/
int64_t
get_chapter_start(KaxChapterAtom &atom,
                  int64_t value_if_not_found) {
  KaxChapterTimeStart *start;

  start = FINDFIRST(&atom, KaxChapterTimeStart);
  if (NULL == start)
    return value_if_not_found;
  return uint64(*start);
}

/** \brief Get the end timecode for a chapter atom.

   Its parameters don't have to be checked for validity.

   \param atom The atom for which the end timecode should be returned.
   \param value_if_not_found The value to return if no end timecode child
     element was found. Defaults to -1.

   \return The start timecode or \c value_if_not_found if the atom doesn't
     contain such a child element.
*/
int64_t
get_chapter_end(KaxChapterAtom &atom,
                int64_t value_if_not_found) {
  KaxChapterTimeEnd *end;

  end = FINDFIRST(&atom, KaxChapterTimeEnd);
  if (NULL == end)
    return value_if_not_found;
  return uint64(*end);
}

/** \brief Get the name for a chapter atom.

   Its parameters don't have to be checked for validity.

   \param atom The atom for which the name should be returned.

   \return The atom's name UTF-8 coded or \c "" if the atom doesn't contain
     such a child element.
*/
string
get_chapter_name(KaxChapterAtom &atom) {
  KaxChapterDisplay *display;
  KaxChapterString *name;

  display = FINDFIRST(&atom, KaxChapterDisplay);
  if (display == NULL)
    return "";
  name = FINDFIRST(display, KaxChapterString);
  if (name == NULL)
    return "";
  return UTFstring_to_cstrutf8(UTFstring(*name));
}

/** \brief Get the unique ID for a chapter atom.

   Its parameters don't have to be checked for validity.

   \param atom The atom for which the unique ID should be returned.

   \return The ID or \c -1 if the atom doesn't contain such a
     child element.
*/
int64_t
get_chapter_uid(KaxChapterAtom &atom) {
  KaxChapterUID *uid;

  uid = FINDFIRST(&atom, KaxChapterUID);
  if (uid == NULL)
    return -1;
  return uint64(*static_cast<EbmlUInteger *>(uid));
}

/** \brief Add missing mandatory elements

   The Matroska specs and \c libmatroska say that several elements are
   mandatory. This function makes sure that they all exist by adding them
   with their default values if they're missing. It works recursively. See
   <a href="http://www.matroska.org/technical/specs/chapters/index.html">
   the Matroska chapter specs</a> for a list or mandatory elements.

   The parameters are checked for validity.

   \param e An element that really is an \c EbmlMaster. \a e's children
     should be checked.
*/
void
fix_mandatory_chapter_elements(EbmlElement *e) {
  if (e == NULL)
    return;

  if (dynamic_cast<KaxEditionEntry *>(e) != NULL) {
    KaxEditionEntry &ee = *static_cast<KaxEditionEntry *>(e);
    GetChild<KaxEditionFlagDefault>(ee);
    GetChild<KaxEditionFlagHidden>(ee);
    if (FINDFIRST(&ee, KaxEditionUID) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxEditionUID>(ee)) =
        create_unique_uint32(UNIQUE_EDITION_IDS);

  } else if (dynamic_cast<KaxChapterAtom *>(e) != NULL) {
    KaxChapterAtom &a = *static_cast<KaxChapterAtom *>(e);

    GetChild<KaxChapterFlagHidden>(a);
    GetChild<KaxChapterFlagEnabled>(a);
    if (FINDFIRST(&a, KaxChapterUID) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterUID>(a)) =
        create_unique_uint32(UNIQUE_CHAPTER_IDS);
    if (FINDFIRST(&a, KaxChapterTimeStart) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(a)) = 0;

  } else if (dynamic_cast<KaxChapterTrack *>(e) != NULL) {
    KaxChapterTrack &t = *static_cast<KaxChapterTrack *>(e);

    if (FINDFIRST(&t, KaxChapterTrackNumber) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTrackNumber>(t)) = 0;

  } else if (dynamic_cast<KaxChapterDisplay *>(e) != NULL) {
    KaxChapterDisplay &d = *static_cast<KaxChapterDisplay *>(e);

    if (FINDFIRST(&d, KaxChapterString) == NULL)
      *static_cast<EbmlUnicodeString *>(&GetChild<KaxChapterString>(d)) = L"";
    if (FINDFIRST(&d, KaxChapterLanguage) == NULL)
      *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(d)) = "und";

  } else if (dynamic_cast<KaxChapterProcess *>(e) != NULL) {
    KaxChapterProcess &p = *static_cast<KaxChapterProcess *>(e);

    GetChild<KaxChapterProcessCodecID>(p);

  } else if (dynamic_cast<KaxChapterProcessCommand *>(e) != NULL) {
    KaxChapterProcessCommand &c = *static_cast<KaxChapterProcessCommand *>(e);

    GetChild<KaxChapterProcessTime>(c);
    GetChild<KaxChapterProcessData>(c);

  }

  if (dynamic_cast<EbmlMaster *>(e) != NULL) {
    EbmlMaster *m;
    int i;

    m = static_cast<EbmlMaster *>(e);
    for (i = 0; i < m->ListSize(); i++)
      fix_mandatory_chapter_elements((*m)[i]);
  }
}

/** \brief Remove all chapter atoms that are outside of a time range

   All chapter atoms that lie completely outside the timecode range
   given with <tt>[min_tc..max_tc]</tt> are deleted. This is the workhorse
   for ::select_chapters_in_timeframe

   Chapters which start before the window but end inside or after the window
   are kept as well, and their start timecode is adjusted.

   Its parameters don't have to be checked for validity.

   \param min_tc The minimum timecode to accept.
   \param max_tc The maximum timecode to accept.
   \param offset This value is subtracted from both the start and end timecode
     for each chapter after the decision whether or not to keep it has been
     made.
   \param m The master containing the elements to check.
*/
static void
remove_entries(int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               EbmlMaster &m) {
  struct chapter_entry_t {
    bool remove, spans, is_atom;
    int64_t start, end;

    chapter_entry_t():
      remove(false), spans(false), is_atom(false), start(0), end(-1) {}
  } *entries;

  int i, last_atom_at;
  KaxChapterAtom *atom;
  KaxChapterTimeStart *cts;
  KaxChapterTimeEnd *cte;
  EbmlMaster *m2;

  if (0 == m.ListSize())
    return;

  entries = new chapter_entry_t[m.ListSize()];
  last_atom_at = -1;

  // Determine whether or not an entry has to be removed. Also retrieve
  // the start and end timecodes.
  for (i = 0; m.ListSize() > i; ++i) {
    atom = dynamic_cast<KaxChapterAtom *>(m[i]);
    if (NULL == atom)
      continue;

    last_atom_at = i;
    entries[i].is_atom = true;

    cts = static_cast<KaxChapterTimeStart *>
      (atom->FindFirstElt(KaxChapterTimeStart::ClassInfos, false));

    if (NULL != cts)
      entries[i].start = uint64(*static_cast<EbmlUInteger *>(cts));

    cte = static_cast<KaxChapterTimeEnd *>
      (atom->FindFirstElt(KaxChapterTimeEnd::ClassInfos, false));

    if (NULL != cte)
      entries[i].end = uint64(*static_cast<EbmlUInteger *>(cte));
  }

  // We can return if we don't have a single atom to work with.
  if (-1 == last_atom_at)
    return;

  for (i = 0; m.ListSize() > i; ++i) {
    atom = dynamic_cast<KaxChapterAtom *>(m[i]);
    if (NULL == atom)
      continue;

    // Calculate the end timestamps and determine whether or not an entry spans
    // several segments.
    if (-1 == entries[i].end) {
      if (i == last_atom_at)
        entries[i].end = 1LL << 62;

      else {
        int next_atom = i + 1;

        while (!entries[next_atom].is_atom)
          ++next_atom;

        entries[i].end = entries[next_atom].start;
      }
    }

    if ((entries[i].start < min_tc)
        ||
        ((max_tc >= 0) && (entries[i].start > max_tc)))
      entries[i].remove = true;

    if (entries[i].remove && (entries[i].start < min_tc) &&
        (entries[i].end > min_tc))
      entries[i].spans = true;

    mxverb(3,
           boost::format("remove_chapters: entries[%1%]: remove %2% spans %3% start %4% end %5%\n")
           % i % entries[i].remove % entries[i].spans % entries[i].start % entries[i].end);

    // Spanning entries must be kept, and their start timecode must be
    // adjusted. Entries that are to be deleted will be deleted later and
    // have to be skipped for now.
    if (entries[i].remove && !entries[i].spans)
      continue;

    cts = static_cast<KaxChapterTimeStart *>
      (atom->FindFirstElt(KaxChapterTimeStart::ClassInfos, false));
    cte = static_cast<KaxChapterTimeEnd *>
      (atom->FindFirstElt(KaxChapterTimeEnd::ClassInfos, false));

    if (entries[i].spans)
      *static_cast<EbmlUInteger *>(cts) = min_tc;

    *static_cast<EbmlUInteger *>(cts) =
      uint64(*static_cast<EbmlUInteger *>(cts)) - offset;
    if (NULL != cte) {
      int64_t end_tc =  uint64(*static_cast<EbmlUInteger *>(cte));
      if ((max_tc >= 0) && (end_tc > max_tc))
        end_tc = max_tc;
      end_tc -= offset;
      *static_cast<EbmlUInteger *>(cte) = end_tc;
    }

    m2 = dynamic_cast<EbmlMaster *>(m[i]);
    if (m2 != NULL)
      remove_entries(min_tc, max_tc, offset, *m2);
  }

  // Now really delete those entries.
  for (i = m.ListSize() - 1; 0 <= i; --i)
    if (entries[i].remove && !entries[i].spans) {
      delete m[i];
      m.Remove(i);
    }

  delete []entries;
}

/** \brief Merge all chapter atoms sharing the same UID

   If two or more chapters with the same UID are encountered on the same
   level then those are merged into a single chapter. The start timecode
   is the minimum start timecode of all the chapters, and the end timecode
   is the maximum end timecode of all the chapters.

   The parameters do not have to be checked for validity.

   \param master The master containing the elements to check.
*/
void
merge_chapter_entries(EbmlMaster &master) {
  int i, k;

  // Iterate over all children of the atomaster.
  for (i = 0; master.ListSize() > i; ++i) {
    KaxChapterAtom *atom, *merge_this;
    int64_t uid, start_tc, end_tc;

    // Not every child is a chapter atomaster. Skip those.
    atom = dynamic_cast<KaxChapterAtom *>(master[i]);
    if (NULL == atom)
      continue;

    uid = get_chapter_uid(*atom);
    if (-1 == uid)
      continue;

    // First get the start and end time, if present.
    start_tc = get_chapter_start(*atom, 0);
    end_tc = get_chapter_end(*atom);

    mxverb(3, boost::format("chapters: merge_entries: looking for %1% with %2%, %3%\n") % uid % start_tc % end_tc);

    // Now iterate over all remaining atoms and find those with the same
    // UID.
    k = i + 1;
    while (true) {
      int64_t merge_start_tc, merge_end_tc;

      merge_this = NULL;
      for (; master.ListSize() > k; ++k) {
        KaxChapterAtom *cmp_atom;

        cmp_atom = dynamic_cast<KaxChapterAtom *>(master[k]);
        if (NULL == cmp_atom)
          continue;

        if (get_chapter_uid(*cmp_atom) == uid) {
          merge_this = cmp_atom;
          break;
        }
      }

      // If we haven't found an atom with the same UID then we're done here.
      if (NULL == merge_this)
        break;

      // Do the merger! First get the start and end timecodes if present.
      merge_start_tc = get_chapter_start(*merge_this, 0);
      merge_end_tc = get_chapter_end(*merge_this);

      // Then compare them to the ones we have for the soon-to-be merged
      // chapter and assign accordingly.
      if (merge_start_tc < start_tc)
        start_tc = merge_start_tc;
      if ((-1 == end_tc) || (merge_end_tc > end_tc))
        end_tc = merge_end_tc;

      mxverb(3, boost::format("chapters: merge_entries:   found one at %1% with %2%, %3%; merged to %4%, %5%\n") % k % merge_start_tc % merge_end_tc % start_tc % end_tc);

      // Finally remove the entry itself.
      delete master[k];
      master.Remove(k);
    }

    // Assign the start and end timecode to the chapter. Only assign an
    // end timecode if one was present in at least one of the merged
    // chapter atoms.
    *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(*atom)) =
      start_tc;

    if (-1 != end_tc)
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeEnd>(*atom)) =
        end_tc;
  }

  // Recusively merge atoms.
  for (i = 0; master.ListSize() > i; ++i)
    if (NULL != dynamic_cast<EbmlMaster *>(master[i]))
      merge_chapter_entries(*static_cast<EbmlMaster *>(master[i]));
}

/** \brief Remove all chapter atoms that are outside of a time range

   All chapter atoms that lie completely outside the timecode range
   given with <tt>[min_tc..max_tc]</tt> are deleted.

   Chapters which start before the window but end inside or after the window
   are kept as well, and their start timecode is adjusted.

   If two or more chapters with the same UID are encountered on the same
   level then those are merged into a single chapter. The start timecode
   is the minimum start timecode of all the chapters, and the end timecode
   is the maximum end timecode of all the chapters.

   The parameters are checked for validity.

   \param chapters The chapters to check.
   \param min_tc The minimum timecode to accept.
   \param max_tc The maximum timecode to accept.
   \param offset This value is subtracted from both the start and end timecode
     for each chapter after the decision whether or not to keep it has been
     made.

   \return \a chapters if there are entries left and \c NULL otherwise.
*/
KaxChapters *
select_chapters_in_timeframe(KaxChapters *chapters,
                             int64_t min_tc,
                             int64_t max_tc,
                             int64_t offset) {
  uint32_t i, k, num_atoms;
  KaxEditionEntry *eentry;

  // Check the parameters.
  if (chapters == NULL)
    return NULL;

  // Remove the atoms that are outside of the requested range.
  for (i = 0; i < chapters->ListSize(); i++) {
    if (dynamic_cast<KaxEditionEntry *>((*chapters)[i]) == NULL)
      continue;
    remove_entries(min_tc, max_tc, offset,
                   *static_cast<EbmlMaster *>((*chapters)[i]));
  }

  // Count the number of atoms in each edition. Delete editions without
  // any atom in them.
  i = 0;
  while (i < chapters->ListSize()) {
    if (dynamic_cast<KaxEditionEntry *>((*chapters)[i]) == NULL) {
      i++;
      continue;
    }
    eentry = static_cast<KaxEditionEntry *>((*chapters)[i]);
    num_atoms = 0;
    for (k = 0; k < eentry->ListSize(); k++)
      if (dynamic_cast<KaxChapterAtom *>((*eentry)[k]) != NULL)
        num_atoms++;

    if (num_atoms == 0) {
      chapters->Remove(i);
      delete eentry;

    } else
      i++;
  }

  // If we don't even have one edition then delete the chapters themselves.
  if (chapters->ListSize() == 0) {
    delete chapters;
    chapters = NULL;
  }

  return chapters;
}

/** \brief Find an edition with a specific UID.

   Its parameters don't have to be checked for validity.

   \param chapters The chapters in which to look for the edition.
   \param uid The requested unique edition ID. The special value \c 0
     results in the first edition being returned.

   \return A pointer to the edition or \c NULL if none has been found.
*/
KaxEditionEntry *
find_edition_with_uid(KaxChapters &chapters,
                      uint64_t uid) {
  KaxEditionEntry *eentry;
  KaxEditionUID *euid;
  int eentry_idx;

  if (uid == 0)
    return FINDFIRST(&chapters, KaxEditionEntry);

  for (eentry_idx = 0; eentry_idx < chapters.ListSize(); eentry_idx++) {
    eentry = dynamic_cast<KaxEditionEntry *>(chapters[eentry_idx]);
    if (eentry == NULL)
      continue;
    euid = FINDFIRST(eentry, KaxEditionUID);
    if ((euid != NULL) && (uint64(*euid) == uid))
      return eentry;
  }

  return NULL;
}

/** \brief Find a chapter atom with a specific UID.

   Its parameters don't have to be checked for validity.

   \param chapters The chapters in which to look for the atom.
   \param uid The requested unique atom ID. The special value \c 0 results in
     the first atom in the first edition being returned.

   \return A pointer to the atom or \c NULL if none has been found.
*/
KaxChapterAtom *
find_chapter_with_uid(KaxChapters &chapters,
                      uint64_t uid) {
  KaxEditionEntry *eentry;
  KaxChapterAtom *atom;
  KaxChapterUID *cuid;
  int eentry_idx, atom_idx;

  if (uid == 0) {
    eentry = FINDFIRST(&chapters, KaxEditionEntry);
    if (eentry == NULL)
      return NULL;
    return FINDFIRST(eentry, KaxChapterAtom);
  }

  for (eentry_idx = 0; eentry_idx < chapters.ListSize(); eentry_idx++) {
    eentry = dynamic_cast<KaxEditionEntry *>(chapters[eentry_idx]);
    if (eentry == NULL)
      continue;
    for (atom_idx = 0; atom_idx < eentry->ListSize(); atom_idx++) {
      atom = dynamic_cast<KaxChapterAtom *>((*eentry)[atom_idx]);
      if (atom == NULL)
        continue;
      cuid = FINDFIRST(atom, KaxChapterUID);
      if ((cuid != NULL) && (uint64(*cuid) == uid))
        return atom;
    }
  }

  return NULL;
}

/** \brief Move all chapter atoms to another container keeping editions intact

   This function moves all chapter atoms from \a src to \a dst.
   If there's already an edition in \a dst with the same UID as the current
   one in \a src, then all atoms will be put into that edition. Otherwise
   the complete edition will simply be moved over.

   After processing \a src will be empty.

   Its parameters don't have to be checked for validity.

   \param dst The container the atoms and editions will be put into.
   \param src The container the atoms and editions will be taken from.
*/
void
move_chapters_by_edition(KaxChapters &dst,
                         KaxChapters &src) {
  int i, k;

  for (i = 0; i < src.ListSize(); i++) {
    KaxEditionEntry *ee_dst;
    KaxEditionUID *euid_src;
    EbmlMaster *m;

    m = static_cast<EbmlMaster *>(src[i]);

    // Find an edition to which these atoms will be added.
    ee_dst = NULL;
    euid_src = FINDFIRST(m, KaxEditionUID);
    if (euid_src != NULL)
      ee_dst = find_edition_with_uid(dst, uint32(*euid_src));

    // No edition with the same UID found as the one we want to handle?
    // Then simply move the complete edition over.
    if (ee_dst == NULL)
      dst.PushElement(*m);
    else {
      // Move all atoms from the old edition to the new one.
      for (k = 0; k < m->ListSize(); k++)
        if (is_id((*m)[k], KaxChapterAtom))
          ee_dst->PushElement(*(*m)[k]);
        else
          delete (*m)[k];
      m->RemoveAll();
      delete m;
    }
  }
  src.RemoveAll();
}

/** \brief Adjust all start and end timecodes by an offset

   All start and end timecodes are adjusted by an offset. This is done
   recursively.

   Its parameters don't have to be checked for validity.

   \param master A master containint the elements to adjust. This can be
     a KaxChapters, KaxEditionEntry or KaxChapterAtom object.
   \param offset The offset to add to each timecode. Can be negative. If
     the resulting timecode would be smaller than zero then it will be set
     to zero.
*/
void
adjust_chapter_timecodes(EbmlMaster &master,
                         int64_t offset) {
  int i;

  for (i = 0; i < master.ListSize(); i++) {
    KaxChapterAtom *atom;
    KaxChapterTimeStart *start;
    KaxChapterTimeEnd *end;
    int64_t new_value;

    if (!is_id(master[i], KaxChapterAtom))
      continue;

    atom = static_cast<KaxChapterAtom *>(master[i]);
    start = FINDFIRST(atom, KaxChapterTimeStart);
    if (start != NULL) {
      new_value = uint64(*start);
      new_value += offset;
      if (new_value < 0)
        new_value = 0;
      *static_cast<EbmlUInteger *>(start) = new_value;
    }

    end = FINDFIRST(atom, KaxChapterTimeEnd);
    if (end != NULL) {
      new_value = uint64(*end);
      new_value += offset;
      if (new_value < 0)
        new_value = 0;
      *static_cast<EbmlUInteger *>(end) = new_value;
    }
  }

  for (i = 0; i < master.ListSize(); i++) {
    EbmlMaster *m;

    m = dynamic_cast<EbmlMaster *>(master[i]);
    if (m != NULL)
      adjust_chapter_timecodes(*m, offset);
  }
}
