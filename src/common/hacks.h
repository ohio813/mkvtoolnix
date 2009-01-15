/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   some hacks that the author might want to use

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __HACKS_H
#define __HACKS_H

#include "os.h"

#include <string>

using namespace std;

// Some hacks that are configurable via command line but which should ONLY!
// be used by the author.
#define ENGAGE_SPACE_AFTER_CHAPTERS "space_after_chapters"
#define ENGAGE_NO_CHAPTERS_IN_META_SEEK "no_chapters_in_meta_seek"
#define ENGAGE_NO_META_SEEK "no_meta_seek"
#define ENGAGE_LACING_XIPH "lacing_xiph"
#define ENGAGE_LACING_EBML "lacing_ebml"
#define ENGAGE_NATIVE_MPEG4 "native_mpeg4"
#define ENGAGE_NO_VARIABLE_DATA "no_variable_data"
#define ENGAGE_NO_DEFAULT_HEADER_VALUES "no_default_header_values"
#define ENGAGE_FORCE_PASSTHROUGH_PACKETIZER "force_passthrough_packetizer"
#define ENGAGE_WRITE_HEADERS_TWICE "write_headers_twice"
#define ENGAGE_ALLOW_AVC_IN_VFW_MODE "allow_avc_in_vfw_mode"
#define ENGAGE_KEEP_BITSTREAM_AR_INFO "keep_bitstream_ar_info"
#define ENGAGE_USE_SIMPLE_BLOCK "use_simpleblock"
#define ENGAGE_OLD_AAC_CODECID "old_aac_codecid"
#define ENGAGE_USE_CODEC_STATE "use_codec_state"
#define ENGAGE_ENABLE_TIMECODE_WARNING "enable_timecode_warning"

void MTX_DLL_API engage_hacks(const string &hacks);
bool MTX_DLL_API hack_engaged(const string &hack);

#endif // __HACKS_H
