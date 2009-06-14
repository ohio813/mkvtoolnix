/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "input" tab -- "Format specific track options" sub-tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <wx/wxprec.h>

#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/file.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/regex.h>
#include <wx/statline.h>

#include "common/common.h"
#include "common/extern_data.h"
#include "common/iso639.h"
#include "merge/mkvmerge.h"
#include "mmg/mmg.h"
#include "mmg/mmg_dialog.h"
#include "mmg/tabs/input.h"
#include "mmg/tabs/global.h"


tab_input_format::tab_input_format(wxWindow *parent,
                                   tab_input *ti)
  : wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400), wxTAB_TRAVERSAL)
  , input(ti) {

  wxFlexGridSizer *siz_fg;
  wxBoxSizer *siz_all, *siz_line;
  int i;

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(TOPBOTTOMSPACING);

  siz_fg = new wxFlexGridSizer(4);
  siz_fg->AddGrowableCol(1);
  siz_fg->AddGrowableCol(3);

  rb_aspect_ratio = new wxRadioButton(this, ID_RB_ASPECTRATIO, Z("Aspect ratio:"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  rb_aspect_ratio->SetValue(true);
  siz_fg->Add(rb_aspect_ratio, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_aspect_ratio = new wxMTX_COMBOBOX_TYPE(this, ID_CB_ASPECTRATIO, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);

  for (i = 0; NULL != predefined_aspect_ratios[i]; ++i)
    cob_aspect_ratio->Append(predefined_aspect_ratios[i]);

  cob_aspect_ratio->SetToolTip(TIP("Sets the display aspect ratio of the track. The format can be either 'a/b' in which case both numbers must be integer "
                                   "(e.g. 16/9) or just a single floting point number 'f' (e.g. 2.35)."));
  cob_aspect_ratio->SetSizeHints(0, -1);
  siz_fg->Add(cob_aspect_ratio, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  rb_display_dimensions = new wxRadioButton(this, ID_RB_DISPLAYDIMENSIONS, Z("Display width/height:"));
  rb_display_dimensions->SetValue(false);
  siz_fg->Add(rb_display_dimensions, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  tc_display_width = new wxTextCtrl(this, ID_TC_DISPLAYWIDTH, wxEmptyString);
  tc_display_width->SetToolTip(TIP("Sets the display width of the track. The height must be set as well, or this field will be ignored."));
  tc_display_width->SetSizeHints(0, -1);
  siz_line->Add(tc_display_width, 1, wxGROW | wxALL, STDSPACING);

  st_x = new wxStaticText(this, wxID_STATIC, wxT("x"));
  st_x->Enable(false);
  siz_line->Add(st_x, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  tc_display_height = new wxTextCtrl(this, ID_TC_DISPLAYHEIGHT, wxEmptyString);
  tc_display_height->SetToolTip(TIP("Sets the display height of the track. The width must be set as well, or this field will be ignored."));
  tc_display_height->SetSizeHints(0, -1);
  siz_line->Add(tc_display_height, 1, wxGROW | wxALL, STDSPACING);
  siz_fg->Add(siz_line, 1, wxGROW, 0);

  st_fourcc = new wxStaticText(this, -1, Z("FourCC:"));
  st_fourcc->Enable(false);
  siz_fg->Add(st_fourcc, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_fourcc = new wxMTX_COMBOBOX_TYPE(this, ID_CB_FOURCC, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
  cob_fourcc->SetToolTip(TIP("Forces the FourCC of the video track to this value. Note that this only works for video "
                             "tracks that use the AVI compatibility mode or for QuickTime video tracks. This option CANNOT be used to change Matroska's CodecID."));
  cob_fourcc->SetSizeHints(0, -1);
  siz_fg->Add(cob_fourcc, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_stereo_mode = new wxStaticText(this, -1, Z("Stereoscopy:"));
  st_stereo_mode->Enable(false);
  siz_fg->Add(st_stereo_mode, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_stereo_mode = new wxMTX_COMBOBOX_TYPE(this, ID_CB_STEREO_MODE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
  cob_stereo_mode->SetToolTip(TIP("Sets the stereo mode of the video track to this value. If left empty then the track's original stereo mode will be kept or, if "
                                  "it didn't have one, none will be set at all."));
  cob_stereo_mode->SetSizeHints(0, -1);
  siz_fg->Add(cob_stereo_mode, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_fps = new wxStaticText(this, -1, Z("FPS:"));
  st_fps->Enable(false);
  siz_fg->Add(st_fps, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_fps = new wxMTX_COMBOBOX_TYPE(this, ID_CB_FPS, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
  cob_fps->SetToolTip(TIP("Sets the default duration or number of frames per second for a track. This is only possible "
                          "for video tracks. For AVC/h.264 elementary streams this option must be given. "
                          "This can either be a floating point number or a fraction."));
  cob_fps->SetSizeHints(0, -1);
  siz_fg->Add(cob_fps, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_nalu_size_length = new wxStaticText(this, -1, Z("NALU size length:"));
  st_nalu_size_length->Enable(false);
  siz_fg->Add(st_nalu_size_length, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_nalu_size_length = new wxMTX_COMBOBOX_TYPE(this, ID_CB_NALU_SIZE_LENGTH, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
  cob_nalu_size_length->SetToolTip(TIP("Forces the NALU size length to a certain number of bytes. This parameter is only available for AVC/h.264 elementary "
                                       "streams read from AVC/h.264 ES files, AVIs or Matroska files created with '--engage allow_avc_in_vwf_mode'. "
                                       "It defaults to 4 bytes, but there are files which do not contain a frame or slice that is bigger than 65535 bytes. "
                                       "For such files you can use this parameter and decrease the size to 2."));
  cob_nalu_size_length->SetSizeHints(0, -1);
  siz_fg->Add(cob_nalu_size_length, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_delay = new wxStaticText(this, wxID_STATIC, Z("Delay (in ms):"));
  st_delay->Enable(false);
  siz_fg->Add(st_delay, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  tc_delay = new wxTextCtrl(this, ID_TC_DELAY, wxEmptyString);
  tc_delay->SetToolTip(TIP("Delay this track's timecodes by a couple of ms. Can be negative. Works best on video and subtitle tracks."));
  tc_delay->SetSizeHints(0, -1);
  siz_fg->Add(tc_delay, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_stretch = new wxStaticText(this, wxID_STATIC, Z("Stretch by:"));
  st_stretch->Enable(false);
  siz_fg->Add(st_stretch, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  tc_stretch = new wxTextCtrl(this, ID_TC_STRETCH, wxEmptyString);
  tc_stretch->SetToolTip(TIP("Stretch this track's timecodes. This entry can have two formats. "
                             "It is either a positive floating point number, or a fraction like e.g. 1200/1253. "
                             "Works best on video and subtitle tracks."));
  tc_stretch->SetSizeHints(0, -1);
  siz_fg->Add(tc_stretch, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_sub_charset = new wxStaticText(this, wxID_STATIC, Z("Charset:"));
  st_sub_charset->Enable(false);
  siz_fg->Add(st_sub_charset, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_sub_charset = new wxMTX_COMBOBOX_TYPE(this, ID_CB_SUBTITLECHARSET, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_sub_charset->SetToolTip(TIP("Selects the character set a subtitle file or chapter information was written with. Only needed for non-UTF encoded "
                                  "subtitle files or for files with chapter information (e.g. OGM, MP4) for which mkvmerge does not detect the encoding correctly."));
  cob_sub_charset->Append(Z("default"));
  cob_sub_charset->SetSizeHints(0, -1);
  siz_fg->Add(cob_sub_charset, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_compression = new wxStaticText(this, -1, Z("Compression:"));
  st_compression->Enable(false);
  siz_fg->Add(st_compression, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_compression = new wxMTX_COMBOBOX_TYPE(this, ID_CB_COMPRESSION, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_compression->SetToolTip(TIP("Sets the compression used for VobSub subtitles. If nothing is chosen then the "
                                  "VobSubs will be automatically compressed with zlib. 'none' results is files that are a lot larger."));
  cob_compression->SetSizeHints(0, -1);
  siz_fg->Add(cob_compression, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  siz_all->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  cb_aac_is_sbr = new wxCheckBox(this, ID_CB_AACISSBR, Z("AAC is SBR/HE-AAC/AAC+"));
  cb_aac_is_sbr->SetValue(false);
  cb_aac_is_sbr->SetToolTip(TIP("This track contains SBR AAC/HE-AAC/AAC+ data. Only needed for AAC input files, because SBR AAC cannot be detected automatically for "
                                "these files. Not needed for AAC tracks read from MP4 or Matroska files."));

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(cb_aac_is_sbr, 0, wxALL, STDSPACING);
  siz_all->Add(siz_line, 0, wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  siz_all->AddSpacer(TOPBOTTOMSPACING);

  SetSizer(siz_all);

  setup_control_contents();
}

void
tab_input_format::setup_control_contents() {
  int i;

  for (i = 0; NULL != predefined_fourccs[i]; ++i)
    cob_fourcc->Append(predefined_fourccs[i]);

  cob_stereo_mode->Append(wxEmptyString);
  cob_stereo_mode->Append(Z("None"));
  cob_stereo_mode->Append(Z("Left eye"));
  cob_stereo_mode->Append(Z("Right eye"));
  cob_stereo_mode->Append(Z("Both eyes"));

  cob_fps->Append(wxEmptyString);
  cob_fps->Append(wxT("24"));
  cob_fps->Append(wxT("25"));
  cob_fps->Append(wxT("30"));
  cob_fps->Append(wxT("60000/1001"));
  cob_fps->Append(wxT("30000/1001"));
  cob_fps->Append(wxT("24000/1001"));

  cob_nalu_size_length->Append(Z("Default"));
  cob_nalu_size_length->Append(Z("2 bytes"));
  cob_nalu_size_length->Append(Z("4 bytes"));

  if (sorted_charsets.Count() == 0) {
    for (i = 0; sub_charsets[i] != NULL; i++)
      sorted_charsets.Add(wxU(sub_charsets[i]));
    sorted_charsets.Sort();
  }

  for (i = 0; i < sorted_charsets.Count(); i++)
    cob_sub_charset->Append(sorted_charsets[i]);

  cob_sub_charset_translations.add(wxT("default"), Z("default"));

  cob_compression->Append(wxEmptyString);
  cob_compression->Append(Z("none"));
  cob_compression->Append(wxT("zlib"));
  if (capabilities[wxT("BZ2")] == wxT("true"))
    cob_compression->Append(wxT("bz2"));
  if (capabilities[wxT("LZO")] == wxT("true"))
    cob_compression->Append(wxT("lzo"));

  cob_compression_translations.add(wxT("none"), Z("none"));
}

void
tab_input_format::set_track_mode(mmg_track_t *t) {
  char type         = t ? t->type      : 'n';
  wxString ctype    = t ? t->ctype     : wxT("");
  bool appending    = t ? t->appending : false;
  bool video        = ('v' == type) && !appending;
  bool audio_app    = ('a' == type);
  bool subs_app     = ('s' == type);
  bool chapters_app = ('c' == type);
  bool avc_es       = video && (t->packetizer == wxT("mpeg4_p10_es_video"));
  bool avc          = video && (t->packetizer == wxT("mpeg4_p10_video"));
  bool normal_track = video || audio_app || subs_app;

  ctype = ctype.Lower();

  st_delay->Enable((NULL != t) && normal_track);
  tc_delay->Enable((NULL != t) && normal_track);
  st_stretch->Enable((NULL != t) && normal_track);
  tc_stretch->Enable((NULL != t) && normal_track);
  st_sub_charset->Enable(chapters_app || (subs_app && (ctype.Find(wxT("vobsub")) < 0)));
  cob_sub_charset->Enable(chapters_app || (subs_app && (ctype.Find(wxT("vobsub")) < 0)));
  st_fourcc->Enable(video);
  cob_fourcc->Enable(video);
  st_stereo_mode->Enable(video);
  cob_stereo_mode->Enable(video);
  st_fps->Enable(video);
  cob_fps->Enable(video);
  st_nalu_size_length->Enable(avc || avc_es);
  cob_nalu_size_length->Enable(avc || avc_es);
  st_compression->Enable((ctype.Find(wxT("vobsub")) >= 0) && !appending);
  cob_compression->Enable((ctype.Find(wxT("vobsub")) >= 0) && !appending);

  bool ar_enabled = normal_track && (NULL != t) && !t->display_dimensions_selected;
  rb_aspect_ratio->Enable(video);
  cob_aspect_ratio->Enable(video && ar_enabled);
  rb_display_dimensions->Enable(video);
  tc_display_width->Enable(video && !ar_enabled);
  st_x->Enable(video && !ar_enabled);
  tc_display_height->Enable(video && !ar_enabled);

  rb_aspect_ratio->SetValue(video && ar_enabled);
  rb_display_dimensions->SetValue(video && !ar_enabled);

  cb_aac_is_sbr->Enable(audio_app && ((ctype.Find(wxT("aac")) >= 0) || (ctype.Find(wxT("mp4a")) >= 0)));

  if (NULL != t)
    return;

  bool saved_dcvn             = input->dont_copy_values_now;
  input->dont_copy_values_now = true;

  set_combobox_selection(cob_aspect_ratio, wxEmptyString);
  tc_display_width->SetValue(wxEmptyString);
  tc_display_height->SetValue(wxEmptyString);
  set_combobox_selection(cob_fourcc, wxEmptyString);
  set_combobox_selection(cob_fps, wxEmptyString);
  cob_nalu_size_length->SetSelection(0);
  set_combobox_selection(cob_stereo_mode, wxEmptyString);
  tc_delay->SetValue(wxEmptyString);
  tc_stretch->SetValue(wxEmptyString);
  set_combobox_selection(cob_sub_charset, wxU(""));
  set_combobox_selection(cob_compression, wxEmptyString);
  cb_aac_is_sbr->SetValue(false);

  input->dont_copy_values_now = saved_dcvn;
}

void
tab_input_format::on_aac_is_sbr_clicked(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->aac_is_sbr = cb_aac_is_sbr->GetValue();
}

void
tab_input_format::on_subcharset_selected(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->sub_charset = cob_sub_charset_translations.to_english(cob_sub_charset->GetStringSelection());
}

void
tab_input_format::on_delay_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->delay = tc_delay->GetValue();
}

void
tab_input_format::on_stretch_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->stretch = tc_stretch->GetValue();
}

void
tab_input_format::on_aspect_ratio_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->aspect_ratio = cob_aspect_ratio->GetStringSelection();
}

void
tab_input_format::on_fourcc_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->fourcc = cob_fourcc->GetStringSelection();
}

void
tab_input_format::on_fps_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->fps = cob_fps->GetStringSelection();
}

void
tab_input_format::on_nalu_size_length_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->nalu_size_length = cob_nalu_size_length->GetSelection() * 2;
}

void
tab_input_format::on_stereo_mode_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->stereo_mode = cob_stereo_mode->GetSelection();
}

void
tab_input_format::on_compression_selected(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->compression = cob_compression_translations.to_english(cob_compression->GetStringSelection());
}

void
tab_input_format::on_display_width_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->dwidth = tc_display_width->GetValue();
}

void
tab_input_format::on_display_height_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->dheight = tc_display_height->GetValue();
}

void
tab_input_format::on_aspect_ratio_selected(wxCommandEvent &evt) {
  mmg_track_t *track;

  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  track = tracks[input->selected_track];
  track->display_dimensions_selected = false;
  set_track_mode(track);
}

void
tab_input_format::on_display_dimensions_selected(wxCommandEvent &evt) {
  mmg_track_t *track;

  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  track = tracks[input->selected_track];
  track->display_dimensions_selected = true;
  set_track_mode(track);
}

IMPLEMENT_CLASS(tab_input_format, wxPanel);
BEGIN_EVENT_TABLE(tab_input_format, wxPanel)
  EVT_CHECKBOX(ID_CB_AACISSBR,             tab_input_format::on_aac_is_sbr_clicked)
  EVT_COMBOBOX(ID_CB_SUBTITLECHARSET,      tab_input_format::on_subcharset_selected)
  EVT_COMBOBOX(ID_CB_ASPECTRATIO,          tab_input_format::on_aspect_ratio_changed)
  EVT_COMBOBOX(ID_CB_FOURCC,               tab_input_format::on_fourcc_changed)
  EVT_COMBOBOX(ID_CB_FPS,                  tab_input_format::on_fps_changed)
  EVT_COMBOBOX(ID_CB_NALU_SIZE_LENGTH,     tab_input_format::on_nalu_size_length_changed)
  EVT_COMBOBOX(ID_CB_STEREO_MODE,          tab_input_format::on_stereo_mode_changed)
  EVT_TEXT(ID_CB_SUBTITLECHARSET,          tab_input_format::on_subcharset_selected)
  EVT_TEXT(ID_TC_DELAY,                    tab_input_format::on_delay_changed)
  EVT_TEXT(ID_TC_STRETCH,                  tab_input_format::on_stretch_changed)
  EVT_RADIOBUTTON(ID_RB_ASPECTRATIO,       tab_input_format::on_aspect_ratio_selected)
  EVT_RADIOBUTTON(ID_RB_DISPLAYDIMENSIONS, tab_input_format::on_display_dimensions_selected)
  EVT_TEXT(ID_TC_DISPLAYWIDTH,             tab_input_format::on_display_width_changed)
  EVT_TEXT(ID_TC_DISPLAYHEIGHT,            tab_input_format::on_display_height_changed)
  EVT_COMBOBOX(ID_CB_COMPRESSION,          tab_input_format::on_compression_selected)
END_EVENT_TABLE();
