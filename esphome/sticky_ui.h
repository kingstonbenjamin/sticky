// Sticky screen layouts and state. Drawn by the display lambda in sticky.yaml.
// Landscape is 800x480, portrait 480x800. Black ink on white paper, or the reverse in dark mode.
#pragma once
#include "esphome.h"
#ifdef USE_ESP32
#include "driver/rtc_io.h"
#include "esp_attr.h"
// WHY: RTC memory keeps its contents through deep sleep (not through power loss)
#define STICKY_RTC RTC_DATA_ATTR
#else
#define STICKY_RTC
#endif
#include <cstring>
#include "history_data.h"
#include <cctype>
#include <string>
#include <vector>
#include <cmath>
#include <ctime>

namespace sticky {

using esphome::display::Display;
using esphome::display::TextAlign;
using esphome::font::Font;
using esphome::Color;

// WHY: on this panel COLOR_ON shows as white, so black "ink" is COLOR_OFF. draw() swaps them for dark mode.
static Color INK = esphome::display::COLOR_OFF;
static Color PAPER = esphome::display::COLOR_ON;

// ---------- fonts (set once at boot) ----------
struct Fonts {
  Font *clock = nullptr;  // tall numerals for the big clock and timer
  Font *huge = nullptr;   // 120 px numerals
  Font *big = nullptr;    // 64 px
  Font *mid = nullptr;    // 44 px
  Font *lbl = nullptr;    // 30 px condensed labels
  Font *txt = nullptr;    // 28 px reading text
  Font *txtb = nullptr;   // 30 px bold reading text
  Font *icon = nullptr;   // 40 px symbols
  Font *iconbig = nullptr;  // 96 px symbols
  Font *sm = nullptr;       // 20 px small labels
};
static Fonts F;
// Etched background pictures (light dots) behind the Today page
struct Pictures { esphome::image::Image *land = nullptr, *port = nullptr; };
static Pictures PIC;

// ---------- pages ----------
enum Page { P_TODAY = 0, P_CAL, P_WX, P_FC, P_REM, P_TIMER, P_HIST, P_YEAR, P_WORLD, P_GH, P_LIFE, P_BIO, P_WIFI, P_PLAY, P_COUNT };
static const char *PAGE_NAMES[P_COUNT] = {"Today", "Calendar", "Weather", "Forecast", "Reminders", "Timer", "History",
                                          "Year", "World clock", "GitHub", "Life in weeks", "Biorhythm", "Wi-Fi", "Playing"};

// ---------- Do side (the AI button flips Glance <-> Do) ----------
// D_PAGE shows one of the Glance pages inside Do, with a back arrow.
// D_DW1..3 are the dishwasher wizard: program, options, start time.
enum DoScreen { D_TILES = 0, D_HOME, D_ROOM, D_DOOR, D_VAC, D_DISH, D_DW1, D_DW2, D_DW3, D_VW1, D_VW2, D_VW3, D_PAGE };
static const char *DO_NAMES[] = {"Tiles", "Home", "AC room", "Door", "Vacuum", "Dishwasher", "Dishwasher program",
                                 "Dishwasher options", "Dishwasher start", "Vacuum mode", "Vacuum power", "Vacuum start", "Page"};

// ---------- state fed from Home Assistant and the device ----------
struct State {
  bool page_on[P_COUNT] = {true, true, true, true, true, true, true, true, true, true, true, true, false, true};
  int page = P_TODAY;

  // Do side
  bool do_mode = false;
  int do_screen = D_TILES;
  int do_page = P_TODAY;  // the page shown when do_screen == D_PAGE
  int room = 0;           // AC room open in D_ROOM
  std::string wiz_prog;   // dishwasher program picked in the wizard
  int vwiz[4] = {3, 1, 1, 1};  // vacuum wizard: mode, suction, mop wetness, route (index into VAC_* lists)

  // device
  float room_t = NAN, room_h = NAN, batt = NAN;
  bool charging = false, on_charger = false, ha_ok = false;

  // home
  std::string lock_state;
  std::string ac_state[6];
  float ac_set[6] = {NAN, NAN, NAN, NAN, NAN, NAN};
  float ac_now[6] = {NAN, NAN, NAN, NAN, NAN, NAN};
  std::string ac_fan[6];
  float vac_batt = NAN;
  std::string vac_last;  // ISO time of the last clean
  std::string reminders; // Apple Home list, "a|b|c" or "none"
  std::string vacuum;
  bool vacuum_problem = false;
  bool watering = false;

  // weather
  std::string wx_cond;
  float wx_t = NAN, wx_h = NAN, wx_wind = NAN;
  std::string forecast;  // "Thu|sunny|31|22;Fri|..."
  std::string agenda;    // "T|07:30|Gym;M|All day|Bin day" or "none"

  // appliances
  std::string dish_state, wash_state, dish_finish, wash_finish;
  std::string dish_door, dish_selected;
  bool dish_remote = false;
  std::string dish_opt[5];  // "on", "off" or "unavailable"
  float dish_prog = NAN, wash_prog = NAN;

  // media
  std::string tv_state, tv_app, tv_title, sb_state;
  float tv_vol = NAN, sb_vol = NAN;

  // alert card
  bool alert = false;
  std::string alert_kind, alert_title, alert_detail;

  // kitchen timer
  bool timer_run = false, timer_paused = false;
  uint32_t timer_total_s = 0, timer_left_s = 0;
  uint32_t timer_end_ms = 0;
  std::string timer_ring_at;

  // sleep note
  bool asleep = false;
  std::string asleep_at;

  // touch setup: 0 off, 1 first dot, 2 second dot
  int touch_setup = 0;
  // touch test: 0 off, 1..15 dot to tap, 16 results; miss per dot in pixels
  int touch_test = 0, tt_drawn = 0;
  int tt_miss[15] = {0};
  uint32_t tt_at = 0, tt_shown_at = 0;

  // white text on black when true
  bool dark = false;

  // Glance extras
  std::string hourly;   // "HH,t,h,w;..." next 12 hours
  std::string birth;    // "YYYY-MM-DD" from input_datetime.sticky_birth_date
  std::string ha_lost_at;  // time Home Assistant went away, empty while connected
  int gh_total = -1, gh_days = 0, gh_first_dow = 0, gh_doy = -1;
  uint8_t gh_count[371] = {0}, gh_level[371] = {0};
  std::string gh_error;
  int64_t wx_at = 0, gh_at = 0;  // when weather / GitHub data last arrived (Unix time, 0 = never)

  // page dots in the strip: shown for DOTS_MS after the Glance page changes, then a quiet redraw hides them
  int dots_page = -1;
  uint32_t dots_ms = 0;
  bool dots_drawn = false;
};
static State S;
static const uint32_t DOTS_MS = 10000;
inline void stamp(int64_t &at, esphome::ESPTime now) { if (now.is_valid()) at = (int64_t) now.timestamp; }

// ---------- house settings (filled at boot from the YAML settings file, see packages/) ----------
static const int MAX_AC = 6, MAX_CITY = 5;
struct Config {
  int ac_count = 0;
  std::string ac_name[MAX_AC], ac_id[MAX_AC];
  int city_count = 0;
  std::string city_name[MAX_CITY], city_tz[MAX_CITY];  // POSIX time zones, e.g. "GMT0BST,M3.5.0/1,M10.5.0"
  std::string tv_label = "TV", soundbar_label = "Soundbar";
  std::string vac_usual = "usual settings";             // shown under "Start usual"
  std::string dish_usual = "dishcare_dishwasher_program_eco_50";
  bool github = false;                                   // GitHub page only when packages/github.yaml is used
};
static Config CFG;
static const char *AC_MODES[4][2] = {{"cool", "Cool"}, {"dry", "Dry"}, {"fan_only", "Fan"}, {"auto", "Auto"}};
static const char *AC_FANS[4][2] = {{"auto", "Auto"}, {"low", "Low"}, {"medium", "Mid"}, {"high", "High"}};

// Dishwasher: Home Connect program ids with short names (same as Inkhouse), and the option switches
static const char *DISH_PROGS[9][2] = {
    {"dishcare_dishwasher_program_eco_50", "Eco 50"},     {"dishcare_dishwasher_program_auto_2", "Auto"},
    {"dishcare_dishwasher_program_quick_45", "Quick 45"}, {"dishcare_dishwasher_program_quick_65", "Quick 65"},
    {"dishcare_dishwasher_program_intensiv_70", "Intensive"}, {"dishcare_dishwasher_program_glas_40", "Glass 40"},
    {"dishcare_dishwasher_program_night_wash", "Night"},  {"dishcare_dishwasher_program_pre_rinse", "Pre-rinse"},
    {"dishcare_dishwasher_program_machine_care", "Care"}};
// Home Connect dishwasher options: short key (HA's "Sticky: do" script maps it to the switch) and label
static const char *DISH_OPTS[5][2] = {
    {"intensive_zone", "Intensive zone"}, {"vario_speed", "Speed+"}, {"hygiene", "Hygiene+"},
    {"half_load", "Half load"}, {"silence_on_demand", "Silence"}};
static const char *DISH_WHEN[4][2] = {{"now", "Now"}, {"22:00", "10 PM"}, {"23:00", "11 PM"}, {"00:00", "Midnight"}};
// Vacuum choices (the Dreame integration's option values); the usual run is set in HA's sticky_do script
static const char *VAC_MODE[4][2] = {{"sweeping", "Sweep"}, {"mopping", "Mop"}, {"sweeping_and_mopping", "Sweep + mop"}, {"mopping_after_sweeping", "Mop after sweep"}};
static const char *VAC_SUCTION[4][2] = {{"quiet", "Quiet"}, {"standard", "Standard"}, {"strong", "Strong"}, {"turbo", "Turbo"}};
static const char *VAC_MOP[3][2] = {{"slightly_dry", "Dry"}, {"moist", "Moist"}, {"wet", "Wet"}};
static const char *VAC_ROUTE[4][2] = {{"quick", "Quick"}, {"standard", "Standard"}, {"intensive", "Intensive"}, {"deep", "Deep"}};
inline const char *dish_prog_name(const std::string &id) {
  for (auto &p : DISH_PROGS) if (id == p[0]) return p[1];
  return "Eco 50";
}

// ---------- small helpers ----------
inline bool is_on_ac(const std::string &s) { return !s.empty() && s != "off" && s != "unavailable" && s != "unknown"; }
inline int ac_on_count() {
  int n = 0;
  for (auto &s : S.ac_state) n += is_on_ac(s);
  return n;
}
inline bool playing() {
  auto act = [](const std::string &s) { return s == "playing" || s == "paused" || s == "on" || s == "idle"; };
  return act(S.tv_state) || act(S.sb_state);
}
inline bool page_available(int p) {
  if (!S.page_on[p]) return false;
  if (p == P_GH && !CFG.github) return false;
  if (p == P_PLAY) return playing();
  return true;
}
inline int next_page(int from, int dir) {
  for (int i = 1; i <= P_COUNT; i++) {
    int p = ((from + dir * i) % P_COUNT + P_COUNT) % P_COUNT;
    if (page_available(p)) return p;
  }
  return P_TODAY;
}
inline std::vector<std::string> split(const std::string &s, char d) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == d) { out.push_back(cur); cur.clear(); } else cur += c;
  }
  if (!cur.empty() || (!s.empty() && s.back() == d)) out.push_back(cur);
  return out;
}
inline std::string fit(const std::string &s, size_t n) { return s.size() <= n ? s : s.substr(0, n - 1) + "."; }
inline std::string title_case(std::string s) {
  bool up = true;
  for (auto &c : s) {
    if (c == '_' || c == '-') { c = ' '; up = true; continue; }
    if (up && c >= 'a' && c <= 'z') c -= 32;
    up = (c == ' ');
  }
  return s;
}
// ---------- dates and times ----------
inline int64_t days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned) (y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t) doe - 719468;
}
// "2026-11-18T19:31:47+00:00" -> seconds since 1970 (UTC), -1 if unreadable
inline int64_t iso_to_epoch(const std::string &iso) {
  int Y, M, D, h, m, sec = 0;
  if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &sec) < 5) return -1;
  int off = 0;
  size_t p = iso.find_last_of("+-");
  if (p != std::string::npos && p > 10 && iso.size() >= p + 6) {
    int oh = 0, om = 0;
    sscanf(iso.c_str() + p + 1, "%d:%d", &oh, &om);
    off = (oh * 60 + om) * 60 * (iso[p] == '-' ? -1 : 1);
  }
  return days_from_civil(Y, M, D) * 86400 + h * 3600 + m * 60 + sec - off;
}
// Local wall clock for a moment, in the device's time zone or in another POSIX zone.
// WHY: the C library's localtime_r knows daylight saving rules; the TZ setting is restored straight after.
inline void local_time(int64_t utc, int &mins, int64_t &day, const std::string &zone = "") {
  std::string keep;
  if (!zone.empty()) {
    const char *old = getenv("TZ");
    keep = old ? old : "";
    setenv("TZ", zone.c_str(), 1);
    tzset();
  }
  time_t t = (time_t) utc;
  struct tm tm;
  localtime_r(&t, &tm);
  if (!zone.empty()) {
    if (keep.empty()) unsetenv("TZ"); else setenv("TZ", keep.c_str(), 1);
    tzset();
  }
  mins = tm.tm_hour * 60 + tm.tm_min;
  day = days_from_civil(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}
// HA ISO time "2026-11-18T23:10:22+00:00" -> local "03:10"
inline std::string iso_to_local_hhmm(const std::string &iso) {
  int64_t t = iso_to_epoch(iso);
  if (t < 0) return "";
  int mins;
  int64_t day;
  local_time(t, mins, day);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", mins / 60, mins % 60);
  return buf;
}

// Material Design Icons code points (UTF-8)
static const char *I_LOCK = "\U000F033E";
static const char *I_UNLOCK = "\U000F0FC6";
static const char *I_SNOW = "\U000F0717";
static const char *I_VACUUM = "\U000F070D";
static const char *I_WATER = "\U000F058C";
static const char *I_DISH = "\U000F0AAC";
static const char *I_WASH = "\U000F072A";
static const char *I_TIMER = "\U000F051B";
static const char *I_CAM = "\U000F07AE";
static const char *I_ALERT = "\U000F0026";
static const char *I_PLAY = "\U000F040A";
static const char *I_PAUSE = "\U000F03E4";
static const char *I_NEXT = "\U000F04AD";
static const char *I_PREV = "\U000F04AE";
static const char *I_VOLUP = "\U000F075D";
static const char *I_VOLDN = "\U000F075E";
static const char *I_BOLT = "\U000F140B";
static const char *I_WIFI = "\U000F05A9";
static const char *I_TAP = "\U000F0741";
static const char *I_BACK = "\U000F0141";
static const char *I_PLUS = "\U000F0415";
static const char *I_MINUS = "\U000F0374";
static const char *I_POWER = "\U000F0425";
static const char *I_HOME = "\U000F02DC";
static const char *I_STOP = "\U000F04DB";
static const char *I_TUNE = "\U000F062E";
static const char *I_NIGHT = "\U000F0594";
static const char *I_CAL = "\U000F00ED";
static const char *I_CLOCK = "\U000F0150";
static const char *I_QR = "\U000F0432";
static const char *I_TV = "\U000F0502";
static const char *I_LIST = "\U000F0756";
static const char *I_CIRCLE = "\U000F0130";
static const char *I_OK = "\U000F05E0";

inline const char *wx_icon(const std::string &c) {
  if (c == "sunny") return "\U000F0599";
  if (c == "clear-night") return "\U000F0594";
  if (c == "partlycloudy") return "\U000F0595";
  if (c == "cloudy") return "\U000F0590";
  if (c == "fog") return "\U000F0591";
  if (c == "rainy") return "\U000F0597";
  if (c == "pouring") return "\U000F0596";
  if (c == "lightning" || c == "lightning-rainy") return "\U000F0593";
  if (c == "windy" || c == "windy-variant") return "\U000F059D";
  if (c == "snowy" || c == "snowy-rainy" || c == "hail") return "\U000F0598";
  if (c == "exceptional") return "\U000F0F30";  // hazy / dust
  return "\U000F0595";
}
inline const char *wx_day_icon(const std::string &c) { return wx_icon(c == "clear-night" ? std::string("sunny") : c); }
inline std::string wx_text(const std::string &c) {
  if (c == "partlycloudy") return "Partly cloudy";
  if (c == "clear-night") return "Clear";
  if (c == "lightning-rainy") return "Thunderstorm";
  if (c == "windy-variant") return "Windy";
  if (c == "snowy-rainy") return "Sleet";
  if (c == "exceptional") return "Dusty";
  if (c.empty() || c == "unknown" || c == "unavailable") return "No weather";
  return title_case(c);
}

// ---------- drawing primitives ----------
// One list of sizes for the Do screens: outer margin, gap between buttons, the NEXT button
static const int M = 14, GAP = 10, NEXT_W = 250, NEXT_H = 96;
struct Rect { int x, y, w, h; bool hit(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; } };
inline int text_w(Display &it, Font *f, const std::string &s) {
  int x1, y1, w, h;
  it.get_text_bounds(0, 0, s.c_str(), f, TextAlign::TOP_LEFT, &x1, &y1, &w, &h);
  return w;
}
// Cut text to max_w pixels, ending in "." (keeps whole UTF-8 letters such as ° and ·)
inline std::string clip(Display &it, Font *f, std::string s, int max_w) {
  if (text_w(it, f, s) <= max_w) return s;
  while (!s.empty()) {
    // WHY: UTF-8 letters like ° are 2-3 bytes; drop the trailing 10xxxxxx bytes, then the first byte, so no letter is cut in half
    while (!s.empty() && (s.back() & 0xC0) == 0x80) s.pop_back();
    if (!s.empty()) s.pop_back();
    while (!s.empty() && s.back() == ' ') s.pop_back();
    if (text_w(it, f, s + ".") <= max_w) return s + ".";
  }
  return s;
}
// Auto-shrinking text: the first font in the list that fits max_w, else the last one, clipped.
// Returns the font used.
inline Font *print_fit(Display &it, int x, int y, int max_w, TextAlign a, Color c, const std::string &s,
                       std::initializer_list<Font *> fonts) {
  Font *use = nullptr;
  for (Font *f : fonts) { use = f; if (text_w(it, f, s) <= max_w) break; }
  it.print(x, y, use, c, a, clip(it, use, s, max_w).c_str());
  return use;
}
inline void box(Display &it, int x, int y, int w, int h, int t = 4) {
  for (int i = 0; i < t; i++) it.rectangle(x + i, y + i, w - 2 * i, h - 2 * i, INK);
}
inline void hline(Display &it, int x, int y, int w, int t = 3) { it.filled_rectangle(x, y, w, t, INK); }
inline void vline(Display &it, int x, int y, int h, int t = 3) { it.filled_rectangle(x, y, t, h, INK); }
// Dotted light line (stands in for the light grey of the mockup)
inline void dotline(Display &it, int x, int y, int w) {
  for (int i = 0; i < w; i += 6) it.filled_rectangle(x + i, y, 3, 3, INK);
}
inline void bar(Display &it, int x, int y, int w, int h, float frac) {
  box(it, x, y, w, h, 3);
  if (!std::isnan(frac)) {
    int fw = (int) ((w - 8) * std::max(0.f, std::min(1.f, frac)));
    if (fw > 0) it.filled_rectangle(x + 4, y + 4, fw, h - 8, INK);
  }
}
inline void pad(Display &it, int x, int y, int w, int h, const char *label, bool inverted = false, Font *f = nullptr,
                bool icon = false) {
  if (inverted) it.filled_rectangle(x, y, w, h, INK); else box(it, x, y, w, h, 4);
  it.print(x + w / 2, y + h / 2, icon ? F.icon : (f ? f : F.mid), inverted ? PAPER : INK, TextAlign::CENTER, label);
}

// ---------- status strip (black band, 58 px) ----------
static const int STRIP = 58;
// Small dots in the strip between x0 and x1: one per step, the current one big. Falls back to "3/12".
inline void strip_dots(Display &it, int x0, int x1, int cur, int n) {
  if (n < 2 || cur < 0 || x1 <= x0) return;
  int pitch = std::min(24, (x1 - x0) / n);
  if (pitch >= 12) {
    for (int i = 0; i < n; i++) it.filled_circle(x0 + pitch / 2 + i * pitch, STRIP / 2, i == cur ? 7 : 3, PAPER);
    return;
  }
  char b[12];
  snprintf(b, sizeof(b), "%d/%d", cur + 1, n);
  if (text_w(it, F.lbl, b) <= x1 - x0) it.print(x0, STRIP / 2, F.lbl, PAPER, TextAlign::CENTER_LEFT, b);
}
// "UPDATED 3 H AGO" (portrait: "3h old") for data that is old_s seconds old
inline std::string age_text(int64_t old_s, bool portrait) {
  const bool mins = old_s < 2 * 3600, hours = !mins && old_s < 2 * 86400;
  int v = (int) (mins ? old_s / 60 : hours ? old_s / 3600 : old_s / 86400);
  char b[32];
  if (portrait) snprintf(b, sizeof(b), "%d%s old", v, mins ? "m" : hours ? "h" : "d");
  else snprintf(b, sizeof(b), "UPDATED %d %s AGO", v, mins ? "MIN" : hours ? "H" : "DAYS");
  return b;
}
// dot_cur / dot_n: optional dots after the page name (wizard steps, or which Glance page this is)
// old_s: when above 0, an "updated ... ago" tag for data that old
inline void strip(Display &it, const char *left, bool left_is_clock, const char *page_name, int dot_cur = -1,
                  int dot_n = 0, int64_t old_s = 0) {
  const int W = it.get_width();
  const bool portrait = W < it.get_height();
  it.filled_rectangle(0, 0, W, STRIP, INK);
  int x = 16;
  // WHY: every Do screen except the tiles goes back with a tap on the left of the strip
  if (S.do_mode && S.do_screen != D_TILES) {
    it.print(2, STRIP / 2, F.icon, PAPER, TextAlign::CENTER_LEFT, I_BACK);
    x = 44;
  }
  int lx;  // where the left side ends
  if (left_is_clock) {
    it.print(x, STRIP / 2, F.mid, PAPER, TextAlign::CENTER_LEFT, left);
    x += text_w(it, F.mid, left) + 16;
    lx = x;
    if (!portrait && page_name) {
      it.print(x, STRIP / 2, F.lbl, PAPER, TextAlign::CENTER_LEFT, page_name);
      lx = x + text_w(it, F.lbl, page_name) + 16;
    }
  } else {
    it.print(x, STRIP / 2, F.lbl, PAPER, TextAlign::CENTER_LEFT, left);
    lx = x + text_w(it, F.lbl, left) + 16;
  }
  // tags left of the icons: Home Assistant unreachable, or this page's data is old
  std::string tag_text = !S.ha_lost_at.empty() ? (portrait ? "HA OFF" : "HA OFFLINE " + S.ha_lost_at)
                         : old_s > 0 ? age_text(old_s, portrait) : "";
  Font *tag_font = portrait ? F.sm : F.lbl;
  // WHY: the tag and wizard step dots matter more than the optional vacuum/water icons, so keep room for them
  int reserve = (tag_text.empty() ? 0 : text_w(it, tag_font, tag_text) + 20) + (S.do_mode && dot_n ? dot_n * 14 + 8 : 0);
  // right side, laid out right-to-left
  int rx = W - 14;
  char buf[16];
  // battery
  if (!std::isnan(S.batt)) {
    snprintf(buf, sizeof(buf), "%d%%", (int) S.batt);
    it.print(rx, STRIP / 2, F.lbl, PAPER, TextAlign::CENTER_RIGHT, buf);
    rx -= text_w(it, F.lbl, buf) + 4;
    if (S.on_charger) {
      it.print(rx, STRIP / 2, F.icon, PAPER, TextAlign::CENTER_RIGHT, I_BOLT);
      rx -= 38;
    }
    rx -= 10;
  }
  // AC count (tap target)
  snprintf(buf, sizeof(buf), "%d", ac_on_count());
  it.print(rx, STRIP / 2, F.mid, PAPER, TextAlign::CENTER_RIGHT, buf);
  rx -= portrait ? 26 : 30;
  it.print(rx, STRIP / 2, F.icon, PAPER, TextAlign::CENTER_RIGHT, I_SNOW);
  rx -= 52;
  // lock: white box when unlocked
  bool locked = S.lock_state == "locked";
  bool known = !S.lock_state.empty() && S.lock_state != "unknown" && S.lock_state != "unavailable";
  if (known && !locked) {
    it.filled_rectangle(rx - 46, 6, 50, STRIP - 12, PAPER);
    it.print(rx - 21, STRIP / 2, F.icon, INK, TextAlign::CENTER, I_UNLOCK);
  } else {
    it.print(rx - 21, STRIP / 2, F.icon, PAPER, TextAlign::CENTER, known ? I_LOCK : "\U000F02D6");
  }
  rx -= 62;
  if ((S.vacuum == "cleaning" || S.vacuum == "returning") && rx - 50 >= lx + reserve) {
    it.print(rx, STRIP / 2, F.icon, PAPER, TextAlign::CENTER_RIGHT, I_VACUUM);
    rx -= 50;
  }
  if (S.watering && rx - 50 >= lx + reserve) {
    it.print(rx, STRIP / 2, F.icon, PAPER, TextAlign::CENTER_RIGHT, I_WATER);
    rx -= 50;
  }
  // a white tag, so a stale number is never mistaken for a live one
  if (!tag_text.empty()) {
    int w = text_w(it, tag_font, tag_text), bx = rx - w - 16;
    if (bx >= lx) {
      it.filled_rectangle(bx, 8, w + 14, STRIP - 16, PAPER);
      it.print(bx + 7, STRIP / 2, tag_font, INK, TextAlign::CENTER_LEFT, tag_text.c_str());
      rx = bx - 4;
    }
  }
  strip_dots(it, lx, rx - 4, dot_cur, dot_n);
}
// Which Glance page this is among the ones switched on, for the page dots
inline void glance_pos(int page, int &cur, int &n) {
  cur = -1; n = 0;
  for (int p = 0; p < P_COUNT; p++) {
    if (!page_available(p)) continue;
    if (p == page) cur = n;
    n++;
  }
}
// Seconds since data arrived when that is more than max_age, else 0
inline int64_t data_age(int64_t at, esphome::ESPTime now, int64_t max_age) {
  if (at <= 0 || !now.is_valid()) return 0;
  int64_t age = (int64_t) now.timestamp - at;
  return age > max_age ? age : 0;
}
// The strip for a Glance page: page dots for a few seconds after the page changes, and the "updated" tag
inline void page_strip(Display &it, const char *clk, const char *name, int page, int64_t old_s = 0) {
  int cur = -1, n = 0;
  if (!S.do_mode && S.dots_drawn) glance_pos(page, cur, n);
  strip(it, clk, true, name, cur, n, old_s);
}

// ---------- shared building blocks ----------
// The big number that leads a page: number, then a label and a smaller second line beside it. Returns its height.
inline int hero(Display &it, int x, int y, const std::string &num, const char *label, const char *sub = nullptr) {
  it.print(x, y, F.big, INK, TextAlign::TOP_LEFT, num.c_str());
  int lx = x + text_w(it, F.big, num) + 14;
  if (sub && *sub) {
    it.print(lx, y + 12, F.lbl, INK, TextAlign::TOP_LEFT, label);
    it.print(lx, y + 44, F.sm, INK, TextAlign::TOP_LEFT, sub);
  } else {
    it.print(lx, y + 26, F.lbl, INK, TextAlign::TOP_LEFT, label);
  }
  return 72;
}
// One look for "nothing here": a title and a hint, centred under the strip
inline void empty_state(Display &it, const char *title, const char *hint = nullptr) {
  const int W = it.get_width(), H = it.get_height(), cy = STRIP + (H - STRIP) / 2;
  it.print(W / 2, hint ? cy - 24 : cy, F.mid, INK, TextAlign::CENTER, title);
  if (hint) print_fit(it, W / 2, cy + 28, W - 40, TextAlign::CENTER, INK, hint, {F.txt, F.sm});
}
// The black NEXT button of the wizards
inline Rect next_rect(int W, int H) {
  if (W > H) return {W - M - NEXT_W, STRIP + M, NEXT_W, H - STRIP - 2 * M};
  return {M, H - M - NEXT_H, W - 2 * M, NEXT_H};
}
inline void next_btn(Display &it, int W, int H) {
  Rect n = next_rect(W, H);
  it.filled_rectangle(n.x, n.y, n.w, n.h, INK);
  it.print(n.x + n.w / 2, n.y + n.h / 2, F.mid, PAPER, TextAlign::CENTER, "NEXT");
}

// ---------- agenda parsing ----------
struct Ev { bool today; std::string when, what; };
inline std::vector<Ev> agenda_items() {
  std::vector<Ev> v;
  if (S.agenda.empty() || S.agenda == "none" || S.agenda == "unknown") return v;
  for (auto &it : split(S.agenda, ';')) {
    auto f = split(it, '|');
    if (f.size() < 3) continue;
    v.push_back({f[0] == "T", f[1], f[2]});
  }
  return v;
}
// minutes since midnight for "HH:MM", -1 for all-day
inline int hhmm_min(const std::string &s) {
  int h, m;
  if (sscanf(s.c_str(), "%d:%d", &h, &m) == 2) return h * 60 + m;
  return -1;
}

// ---------- appliance progress line ----------
inline bool appliance_line(Display &it, int x, int y, int w) {
  const char *icon = nullptr;
  float prog = NAN;
  std::string fin;
  if (S.dish_state == "run" || S.dish_state == "delayedstart") { icon = I_DISH; prog = S.dish_prog; fin = S.dish_finish; }
  else if (S.wash_state == "run" || S.wash_state == "delayedstart") { icon = I_WASH; prog = S.wash_prog; fin = S.wash_finish; }
  if (!icon) return false;
  it.print(x, y, F.icon, INK, TextAlign::CENTER_LEFT, icon);
  std::string t = iso_to_local_hhmm(fin);
  int tw = 0;
  if (!t.empty()) {
    it.print(x + w, y, F.mid, INK, TextAlign::CENTER_RIGHT, t.c_str());
    int x1, y1, hh;
    it.get_text_bounds(x + w, y, t.c_str(), F.mid, TextAlign::CENTER_RIGHT, &x1, &y1, &tw, &hh);
  }
  bar(it, x + 52, y - 10, w - 52 - tw - 14, 20, std::isnan(prog) ? NAN : prog / 100.f);
  return true;
}

// ---------- pages ----------
// The dishwasher/washer bar on Today: drawn here, and a tap on it opens the dishwasher
inline Rect today_dish_rect(int W, int H) {
  if (W > H) return {452, H - 64, W - 452 - 18, 60};
  return {22, H - 140, W - 44, 60};
}
inline void page_today(Display &it, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  char clk[8], date[24], day[16];
  if (now.is_valid()) {
    snprintf(clk, sizeof(clk), "%d:%02d", now.hour, now.minute);
    now.strftime(date, sizeof(date), "%a %d %b");
    now.strftime(day, sizeof(day), "%A");
  } else {
    snprintf(clk, sizeof(clk), "--:--");
    snprintf(date, sizeof(date), "Waiting for time");
    day[0] = 0;
  }
  int dcur = -1, dn = 0;
  if (!S.do_mode && S.dots_drawn) glance_pos(P_TODAY, dcur, dn);
  strip(it, date, false, nullptr, dcur, dn);
  // WHY: the etched picture goes down first so every number and word is printed on top of it
  esphome::image::Image *pic = L ? PIC.land : PIC.port;
  if (pic) it.image(0, STRIP, pic, INK, PAPER);
  char buf[32];
  auto evs = agenda_items();
  int nowmin = now.is_valid() ? now.hour * 60 + now.minute : 0;

  if (L) {
    vline(it, 430, STRIP, H - STRIP, 4);
    it.print(18, STRIP + 4, F.clock, INK, TextAlign::TOP_LEFT, clk);
    it.print(22, 300, F.lbl, INK, TextAlign::TOP_LEFT, day);
    dotline(it, 22, 372, 390);
    it.print(22, 420, F.txt, INK, TextAlign::CENTER_LEFT, "Here");
    if (!std::isnan(S.room_t)) {
      snprintf(buf, sizeof(buf), "%.0f°", S.room_t);
      it.print(120, 420, F.big, INK, TextAlign::CENTER_LEFT, buf);
    }
    if (!std::isnan(S.room_h)) {
      snprintf(buf, sizeof(buf), "%.0f%%", S.room_h);
      it.print(250, 420, F.big, INK, TextAlign::CENTER_LEFT, buf);
    }
    // weather now
    int rx = 452;
    it.print(rx, STRIP + 62, F.iconbig, INK, TextAlign::CENTER_LEFT, wx_icon(S.wx_cond));
    if (!std::isnan(S.wx_t)) {
      snprintf(buf, sizeof(buf), "%.0f°", S.wx_t);
      it.print(rx + 104, STRIP + 62, F.big, INK, TextAlign::CENTER_LEFT, buf);
    }
    it.print(rx + 210, STRIP + 44, F.txt, INK, TextAlign::CENTER_LEFT, fit(wx_text(S.wx_cond), 10).c_str());
    auto days = split(S.forecast, ';');
    if (!days.empty()) {
      auto f = split(days[0], '|');
      if (f.size() >= 4) {
        snprintf(buf, sizeof(buf), "%s° / %s°", f[2].c_str(), f[3].c_str());
        it.print(rx + 210, STRIP + 80, F.txt, INK, TextAlign::CENTER_LEFT, buf);
      }
    }
    hline(it, 430, STRIP + 128, W - 430, 4);
    // next events
    int y = STRIP + 150;
    int shown = 0;
    if (evs.empty()) {
      it.print(rx, y + 10, F.txt, INK, TextAlign::TOP_LEFT, S.agenda == "none" ? "No calendar yet" : "Nothing planned");
    }
    for (auto &e : evs) {
      if (shown >= 2) break;
      int m = hhmm_min(e.when);
      if (e.today && m >= 0 && m + 30 < nowmin) continue;  // already over
      std::string when = e.today ? e.when : (std::string("Tmrw ") + (m >= 0 ? e.when : ""));
      if (!e.today && m < 0) when = "Tmrw";
      it.print(rx, y, F.mid, INK, TextAlign::TOP_LEFT, when.c_str());
      it.print(rx, y + 46, F.txt, INK, TextAlign::TOP_LEFT, clip(it, F.txt, e.what, W - rx - 18).c_str());
      y += 92;
      shown++;
    }
    Rect db = today_dish_rect(W, H);
    appliance_line(it, db.x, db.y + db.h / 2, db.w);
  } else {
    // portrait 480 x 800
    it.print(18, STRIP + 2, F.clock, INK, TextAlign::TOP_LEFT, clk);
    it.print(22, 290, F.lbl, INK, TextAlign::TOP_LEFT, day);
    hline(it, 0, 336, W, 4);
    it.print(18, 400, F.iconbig, INK, TextAlign::CENTER_LEFT, wx_icon(S.wx_cond));
    if (!std::isnan(S.wx_t)) {
      snprintf(buf, sizeof(buf), "%.0f°", S.wx_t);
      it.print(124, 400, F.big, INK, TextAlign::CENTER_LEFT, buf);
    }
    it.print(236, 384, F.txt, INK, TextAlign::CENTER_LEFT, fit(wx_text(S.wx_cond), 13).c_str());
    auto days = split(S.forecast, ';');
    if (!days.empty()) {
      auto f = split(days[0], '|');
      if (f.size() >= 4) {
        snprintf(buf, sizeof(buf), "%s° / %s°", f[2].c_str(), f[3].c_str());
        it.print(236, 420, F.txt, INK, TextAlign::CENTER_LEFT, buf);
      }
    }
    hline(it, 0, 462, W, 4);
    int y = 478, shown = 0;
    if (evs.empty())
      it.print(22, y + 8, F.txt, INK, TextAlign::TOP_LEFT, S.agenda == "none" ? "No calendar yet" : "Nothing planned");
    for (auto &e : evs) {
      if (shown >= 2) break;
      int m = hhmm_min(e.when);
      if (e.today && m >= 0 && m + 30 < nowmin) continue;
      std::string when = e.today ? e.when : (m >= 0 ? "Tmrw " + e.when : std::string("Tmrw"));
      it.print(22, y, F.mid, INK, TextAlign::TOP_LEFT, when.c_str());
      it.print(22, y + 46, F.txt, INK, TextAlign::TOP_LEFT, clip(it, F.txt, e.what, W - 44).c_str());
      y += 92;
      shown++;
    }
    Rect db = today_dish_rect(W, H);
    appliance_line(it, db.x, db.y + db.h / 2, db.w);
    dotline(it, 22, H - 76, W - 44);
    it.print(22, H - 38, F.txt, INK, TextAlign::CENTER_LEFT, "Here");
    if (!std::isnan(S.room_t)) {
      snprintf(buf, sizeof(buf), "%.0f°", S.room_t);
      it.print(150, H - 38, F.big, INK, TextAlign::CENTER_LEFT, buf);
    }
    if (!std::isnan(S.room_h)) {
      snprintf(buf, sizeof(buf), "%.0f%%", S.room_h);
      it.print(290, H - 38, F.big, INK, TextAlign::CENTER_LEFT, buf);
    }
  }
}

inline void page_calendar(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  page_strip(it, clk, "CALENDAR", P_CAL);
  auto evs = agenda_items();
  int nowmin = now.is_valid() ? now.hour * 60 + now.minute : 0;
  if (evs.empty()) {
    if (S.agenda == "none") empty_state(it, "No calendar yet", "Connect iCloud in Home Assistant");
    else empty_state(it, "Nothing planned");
    return;
  }
  int n_today = 0;
  for (auto &e : evs) n_today += e.today;
  // today leads with its count as the page's big number; tomorrow gets a plain heading on the same line
  auto draw_list = [&](int x, int y, int w, bool today, int max_rows) {
    if (today) hero(it, x, y - 4, std::to_string(n_today), n_today == 1 ? "EVENT" : "EVENTS", "today");
    else it.print(x, y + 30, F.lbl, INK, TextAlign::TOP_LEFT, "TOMORROW");
    y += 72;
    int rows = 0;
    for (auto &e : evs) {
      if (e.today != today) continue;
      if (rows >= max_rows) break;
      int m = hhmm_min(e.when);
      bool past = today && m >= 0 && m + 30 < nowmin;
      bool now_ = today && m >= 0 && m <= nowmin && nowmin < m + 60;
      if (now_) it.filled_rectangle(x - 6, y - 4, w + 12, 62, INK);
      Color c = now_ ? PAPER : INK;
      it.print(x, y + 27, F.mid, c, TextAlign::CENTER_LEFT, m >= 0 ? e.when.c_str() : "All day");
      std::string what = clip(it, F.txt, e.what, w - 128);
      it.print(x + 128, y + 27, F.txt, c, TextAlign::CENTER_LEFT, what.c_str());
      if (past) hline(it, x + 128, y + 27, text_w(it, F.txt, what), 3);
      if (!now_) dotline(it, x, y + 58, w);
      y += 66;
      rows++;
    }
    if (rows == 0) it.print(x, y + 10, F.txt, INK, TextAlign::TOP_LEFT, "Nothing");
  };
  if (L) {
    vline(it, W / 2, STRIP, H - STRIP, 4);
    draw_list(20, STRIP + 14, W / 2 - 40, true, 5);
    draw_list(W / 2 + 22, STRIP + 14, W / 2 - 40, false, 5);
  } else {
    draw_list(20, STRIP + 14, W - 40, true, 5);
    draw_list(20, STRIP + 14 + 72 + 5 * 66 + 20, W - 40, false, 3);
  }
}

inline void page_weather(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  page_strip(it, clk, "FORECAST", P_FC, data_age(S.wx_at, now, 3 * 3600));
  char buf[48];
  int top = STRIP + 10;
  it.print(20, top + 80, F.iconbig, INK, TextAlign::CENTER_LEFT, wx_icon(S.wx_cond));
  if (!std::isnan(S.wx_t)) {
    snprintf(buf, sizeof(buf), "%.0f°", S.wx_t);
    it.print(130, top + 80, F.huge, INK, TextAlign::CENTER_LEFT, buf);
  }
  int tx = L ? 360 : 24, ty = L ? top + 30 : top + 170;
  it.print(tx, ty, F.txtb, INK, TextAlign::TOP_LEFT, wx_text(S.wx_cond).c_str());
  if (!std::isnan(S.wx_h)) {
    snprintf(buf, sizeof(buf), "Humidity %.0f%%", S.wx_h);
    it.print(tx, ty + 40, F.txt, INK, TextAlign::TOP_LEFT, buf);
  }
  if (!std::isnan(S.wx_wind)) {
    snprintf(buf, sizeof(buf), "Wind %.0f km/h", S.wx_wind);
    it.print(tx, ty + 78, F.txt, INK, TextAlign::TOP_LEFT, buf);
  }
  int dy = L ? STRIP + 184 : STRIP + 300;
  hline(it, 0, dy, W, 4);
  auto days = split(S.forecast, ';');
  int n = std::min((int) days.size(), 5);
  if (n == 0) {
    it.print(W / 2, (dy + H) / 2, F.txt, INK, TextAlign::CENTER, "Forecast not loaded yet");
    return;
  }
  if (L) {
    int cw = W / 5;
    for (int i = 0; i < n; i++) {
      auto f = split(days[i], '|');
      if (f.size() < 4) continue;
      int cx = i * cw + cw / 2;
      if (i) for (int yy = dy + 20; yy < H - 16; yy += 6) it.filled_rectangle(i * cw, yy, 3, 3, INK);
      it.print(cx, dy + 34, F.lbl, INK, TextAlign::CENTER, f[0].c_str());
      it.print(cx, dy + 94, F.icon, INK, TextAlign::CENTER, wx_day_icon(f[1]));
      snprintf(buf, sizeof(buf), "%s°", f[2].c_str());
      it.print(cx, dy + 152, F.mid, INK, TextAlign::CENTER, buf);
      snprintf(buf, sizeof(buf), "%s°", f[3].c_str());
      it.print(cx, dy + 200, F.txt, INK, TextAlign::CENTER, buf);
    }
  } else {
    int rh = (H - dy - 10) / 5;
    for (int i = 0; i < n; i++) {
      auto f = split(days[i], '|');
      if (f.size() < 4) continue;
      int cy = dy + 10 + i * rh + rh / 2;
      if (i) dotline(it, 20, dy + 10 + i * rh, W - 40);
      it.print(24, cy, F.lbl, INK, TextAlign::CENTER_LEFT, f[0].c_str());
      it.print(150, cy, F.icon, INK, TextAlign::CENTER, wx_day_icon(f[1]));
      snprintf(buf, sizeof(buf), "%s°", f[2].c_str());
      it.print(330, cy, F.mid, INK, TextAlign::CENTER_RIGHT, buf);
      snprintf(buf, sizeof(buf), "%s°", f[3].c_str());
      it.print(W - 24, cy, F.txt, INK, TextAlign::CENTER_RIGHT, buf);
    }
  }
}

// Timer page tap targets are shared with touch handling
inline std::vector<std::pair<Rect, int>> timer_pads(int W, int H) {
  // values: >0 start minutes, -1 add minute, -2 pause, -3 stop
  std::vector<std::pair<Rect, int>> v;
  const int vals[6] = {1, 5, 10, -1, -2, -3};
  if (W > H) {
    int x0 = W - 316, y0 = STRIP + 16, pw = 143, ph = (H - y0 - 18 - 24) / 3;
    for (int i = 0; i < 6; i++) v.push_back({{x0 + (i % 2) * (pw + 12), y0 + (i / 2) * (ph + 12), pw, ph}, vals[i]});
  } else {
    int x0 = 18, y0 = H - 300, pw = (W - 36 - 24) / 3, ph = 132;
    for (int i = 0; i < 6; i++) v.push_back({{x0 + (i % 3) * (pw + 12), y0 + (i / 3) * (ph + 12), pw, ph}, vals[i]});
  }
  return v;
}
inline void page_timer(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  page_strip(it, clk, "TIMER", P_TIMER);
  char buf[16];
  uint32_t left = S.timer_left_s;
  snprintf(buf, sizeof(buf), "%u:%02u", left / 60, left % 60);
  if (!S.timer_run && !S.timer_paused) snprintf(buf, sizeof(buf), "0:00");
  float frac = S.timer_total_s ? 1.f - (float) left / (float) S.timer_total_s : 0.f;
  std::string sub;
  if (S.timer_run) sub = "of " + std::to_string(S.timer_total_s / 60) + " min · rings " + S.timer_ring_at;
  else if (S.timer_paused) sub = "Paused";
  else sub = "Tap a time to start";
  if (L) {
    it.print(24, STRIP + 4, F.clock, INK, TextAlign::TOP_LEFT, buf);
    bar(it, 24, STRIP + 262, W - 316 - 48, 30, frac);
    it.print(24, STRIP + 310, F.txt, INK, TextAlign::TOP_LEFT, sub.c_str());
  } else {
    it.print(W / 2, STRIP + 20, F.clock, INK, TextAlign::TOP_CENTER, buf);
    bar(it, 24, STRIP + 270, W - 48, 30, frac);
    it.print(W / 2, STRIP + 318, F.txt, INK, TextAlign::TOP_CENTER, sub.c_str());
  }
  const char *labels[6] = {"1 min", "5 min", "10 min", "+1", S.timer_paused ? "Go" : "Pause", "Stop"};
  auto pads = timer_pads(W, H);
  for (int i = 0; i < 6; i++) {
    auto &r = pads[i].first;
    pad(it, r.x, r.y, r.w, r.h, labels[i], i == 5);
  }
}

inline void page_wifi(Display &it, const char *clk, esphome::qr_code::QrCode *qr, const char *ssid) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  page_strip(it, clk, "WI-FI", P_WIFI);
  if (L) {
    it.qr_code(24, STRIP + 16, qr, INK, 11);
    it.print(430, STRIP + 70, F.big, INK, TextAlign::TOP_LEFT, "Scan to");
    it.print(430, STRIP + 136, F.big, INK, TextAlign::TOP_LEFT, "join");
    it.print(430, STRIP + 240, F.txt, INK, TextAlign::TOP_LEFT, "Network");
    it.print(430, STRIP + 276, F.mid, INK, TextAlign::TOP_LEFT, fit(ssid, 14).c_str());
  } else {
    it.qr_code(36, STRIP + 30, qr, INK, 13);
    it.print(W / 2, STRIP + 470, F.big, INK, TextAlign::TOP_CENTER, "Scan to join");
    it.print(W / 2, STRIP + 560, F.txt, INK, TextAlign::TOP_CENTER, "Network");
    it.print(W / 2, STRIP + 600, F.mid, INK, TextAlign::TOP_CENTER, fit(ssid, 16).c_str());
  }
}

inline std::vector<std::pair<Rect, int>> play_pads(int W, int H) {
  // 1 prev, 2 play/pause, 3 next, 4 vol-, 5 vol+
  std::vector<std::pair<Rect, int>> v;
  if (W > H) {
    int pw = (W - 36 - 48) / 5, y = H - 18 - 100;
    for (int i = 0; i < 5; i++) v.push_back({{18 + i * (pw + 12), y, pw, 100}, i + 1});
  } else {
    int pw = (W - 36 - 24) / 3;
    for (int i = 0; i < 3; i++) v.push_back({{18 + i * (pw + 12), H - 18 - 2 * 120 - 12, pw, 120}, i + 1});
    int pw2 = (W - 36 - 12) / 2;
    for (int i = 0; i < 2; i++) v.push_back({{18 + i * (pw2 + 12), H - 18 - 120, pw2, 120}, i + 4});
  }
  return v;
}
inline void page_playing(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  page_strip(it, clk, "PLAYING", P_PLAY);
  bool tv = !(S.tv_state.empty() || S.tv_state == "off" || S.tv_state == "unavailable" || S.tv_state == "unknown");
  std::string src = tv ? CFG.tv_label : CFG.soundbar_label;
  std::string name = !S.tv_title.empty() && S.tv_title != "unknown" ? S.tv_title : (!S.tv_app.empty() && S.tv_app != "unknown" ? S.tv_app : (tv ? "TV" : "Soundbar"));
  it.print(22, STRIP + 20, F.lbl, INK, TextAlign::TOP_LEFT, src.c_str());
  print_fit(it, 22, STRIP + 60, W - 44, TextAlign::TOP_LEFT, INK, name, {F.huge, F.big, F.mid});
  float vol = !std::isnan(S.sb_vol) && !tv ? S.sb_vol : S.tv_vol;
  if (!std::isnan(vol)) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", (int) (vol * 100));
    it.print(22, STRIP + 220, F.txt, INK, TextAlign::CENTER_LEFT, "Volume");
    int bw = std::min(300, W - 140 - 90);
    bar(it, 140, STRIP + 208, bw, 24, vol);
    it.print(140 + bw + 18, STRIP + 220, F.mid, INK, TextAlign::CENTER_LEFT, buf);
  }
  const char *icons[5] = {I_PREV, (S.tv_state == "playing") ? I_PAUSE : I_PLAY, I_NEXT, I_VOLDN, I_VOLUP};
  auto pads = play_pads(W, H);
  for (int i = 0; i < 5; i++) {
    auto &r = pads[i].first;
    pad(it, r.x, r.y, r.w, r.h, icons[i], false, nullptr, true);
  }
}

// ---------- alert card (covers the page until tapped) ----------
inline void alert_card(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  strip(it, clk, true, "ALERT");
  const char *ic = I_ALERT;
  if (S.alert_kind == "dishwasher") ic = I_DISH;
  else if (S.alert_kind == "washing") ic = I_WASH;
  else if (S.alert_kind == "timer") ic = I_TIMER;
  else if (S.alert_kind == "camera") ic = I_CAM;
  else if (S.alert_kind == "vacuum") ic = I_VACUUM;
  else if (S.alert_kind == "ok") ic = I_OK;
  // hazard stripes: diagonal bands
  int hx = 0, hy = STRIP, hw = L ? 190 : W, hh = L ? H - STRIP : 150;
  for (int y = hy; y < hy + hh; y++)
    for (int x = hx; x < hx + hw; x++)
      if (((x + y) / 26) % 2 == 0) it.draw_pixel_at(x, y, INK);
  int cx = L ? 190 + 30 : 30, cy = L ? STRIP + 40 : STRIP + 190;
  if (L) vline(it, 190, STRIP, H - STRIP, 4); else hline(it, 0, STRIP + 150, W, 4);
  it.print(cx, cy, F.iconbig, INK, TextAlign::TOP_LEFT, ic);
  // title may need two lines
  std::string t = S.alert_title;
  int maxch = L ? 13 : 11;
  std::string l1 = t, l2;
  if (t.size() > (size_t) maxch) {
    size_t sp = t.rfind(' ', maxch);
    if (sp == std::string::npos) sp = maxch;
    l1 = t.substr(0, sp);
    l2 = fit(t.substr(std::min(t.size(), sp + 1)), maxch);
  }
  int ty = cy + 110;
  it.print(cx, ty, F.big, INK, TextAlign::TOP_LEFT, l1.c_str());
  if (!l2.empty()) { ty += 66; it.print(cx, ty, F.big, INK, TextAlign::TOP_LEFT, l2.c_str()); }
  it.print(cx, ty + 78, F.txt, INK, TextAlign::TOP_LEFT, clip(it, F.txt, S.alert_detail, W - cx - 20).c_str());
  int bw = 260, bh = 64, bx = cx, by = L ? H - bh - 22 : H - bh - 40;
  it.filled_rectangle(bx, by, bw, bh, INK);
  it.print(bx + bw / 2, by + bh / 2, F.lbl, PAPER, TextAlign::CENTER, "TAP TO CLEAR");
}

// ---------- Do side: shared pieces ----------
inline void dashed(Display &it, int x, int y, int w, int h) {
  for (int i = 0; i < w; i += 8) { it.filled_rectangle(x + i, y, 4, 3, INK); it.filled_rectangle(x + i, y + h - 3, 4, 3, INK); }
  for (int i = 0; i < h; i += 8) { it.filled_rectangle(x, y + i, 3, 4, INK); it.filled_rectangle(x + w - 3, y + i, 3, 4, INK); }
}
// A big button: icon, label and an optional second line
inline void bigbtn(Display &it, const Rect &r, const char *icon, const char *label, const char *sub, bool dark) {
  if (dark) it.filled_rectangle(r.x, r.y, r.w, r.h, INK); else box(it, r.x, r.y, r.w, r.h, 4);
  Color c = dark ? PAPER : INK;
  int tx = r.x + 16;
  if (icon) { it.print(tx, r.y + r.h / 2, F.icon, c, TextAlign::CENTER_LEFT, icon); tx += 54; }
  if (sub && *sub) {
    it.print(tx, r.y + r.h / 2 - 2, F.lbl, c, TextAlign::BOTTOM_LEFT, label);
    it.print(tx, r.y + r.h / 2 + 4, F.txt, c, TextAlign::TOP_LEFT, sub);
  } else {
    it.print(tx, r.y + r.h / 2, F.lbl, c, TextAlign::CENTER_LEFT, label);
  }
}
inline void chip(Display &it, const Rect &r, const char *label, bool sel, bool off = false) {
  if (sel) it.filled_rectangle(r.x, r.y, r.w, r.h, INK);
  else if (off) dashed(it, r.x, r.y, r.w, r.h);
  else box(it, r.x, r.y, r.w, r.h, 3);
  it.print(r.x + r.w / 2, r.y + r.h / 2, F.lbl, sel ? PAPER : INK, TextAlign::CENTER, label);
}
// n equal cells in a cols x rows grid inside an area
inline std::vector<Rect> grid(int x, int y, int w, int h, int cols, int rows, int gap, int n) {
  std::vector<Rect> v;
  int cw = (w - gap * (cols - 1)) / cols, ch = (h - gap * (rows - 1)) / rows;
  for (int i = 0; i < n; i++) v.push_back({x + (i % cols) * (cw + gap), y + (i / cols) * (ch + gap), cw, ch});
  return v;
}
inline std::string ac_mode_name(const std::string &s) {
  for (auto &m : AC_MODES) if (s == m[0]) return m[1];
  if (s == "heat") return "Heat";
  return title_case(s);
}
inline std::string ac_fan_name(const std::string &s) {
  for (auto &m : AC_FANS) if (s == m[0]) return m[1];
  return title_case(s);
}
inline std::string temp_text(float t) {
  char b[12];
  if (std::isnan(t)) return "--°";
  if (std::fabs(t - std::round(t)) < 0.05f) snprintf(b, sizeof(b), "%.0f°", t); else snprintf(b, sizeof(b), "%.1f°", t);
  return b;
}
inline std::string dish_state_name() {
  const std::string &s = S.dish_state;
  if (s == "run") return "Running";
  if (s == "delayedstart") return "Waiting to start";
  if (s.empty() || s == "unknown" || s == "unavailable") return "Not connected";
  return title_case(s);
}
// "2026-11-18T19:31:47+00:00" -> "today 23:31" / "yesterday" / "3 days ago"
inline std::string ago_text(const std::string &iso, esphome::ESPTime now) {
  int64_t t = iso_to_epoch(iso);
  if (t < 0 || !now.is_valid()) return "";
  int mins, now_mins;
  int64_t day_t, day_now;
  local_time(t, mins, day_t);
  local_time((int64_t) now.timestamp, now_mins, day_now);
  char b[24];
  if (day_t == day_now) snprintf(b, sizeof(b), "today %02d:%02d", (int) (mins / 60), (int) (mins % 60));
  else if (day_t == day_now - 1) snprintf(b, sizeof(b), "yesterday");
  else snprintf(b, sizeof(b), "%d days ago", (int) (day_now - day_t));
  return b;
}
inline std::vector<std::string> reminder_items() {
  std::vector<std::string> v;
  if (S.reminders.empty() || S.reminders == "none" || S.reminders == "unknown" || S.reminders == "unavailable") return v;
  for (auto &r : split(S.reminders, '|')) if (!r.empty()) v.push_back(r);
  return v;
}

// ---------- Glance: Reminders page (the family's Apple Home list) ----------
// Rows under the big number; drawing and tapping both use these rectangles
static const int REM_ROW = 64, REM_TOP = STRIP + 8 + 72;
inline int rem_rows(int H) { return (H - REM_TOP - 8) / REM_ROW; }
inline std::vector<Rect> rem_rects(int W, int H, int n) {
  std::vector<Rect> v;
  for (int i = 0; i < std::min(n, rem_rows(H)); i++) v.push_back({18, REM_TOP + i * REM_ROW, W - 36, REM_ROW});
  return v;
}
// how many reminders fit as rows ("+3 more" takes the last row when the list is longer)
inline int rem_shown(int H, int n) { return n > rem_rows(H) ? rem_rows(H) - 1 : n; }
inline void page_reminders(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  page_strip(it, clk, "REMINDERS", P_REM);
  auto items = reminder_items();
  if (items.empty()) { empty_state(it, "Home list is clear", "Add to Home in Apple Reminders"); return; }
  int n = (int) items.size(), shown = rem_shown(H, n);
  hero(it, 18, STRIP + 8, std::to_string(n), "OPEN", "tap one when it is done");
  auto rs = rem_rects(W, H, shown + (n > shown ? 1 : 0));
  for (int i = 0; i < (int) rs.size(); i++) {
    const Rect &r = rs[i];
    int cy = r.y + r.h / 2;
    if (i < shown) {
      it.print(r.x, cy, F.icon, INK, TextAlign::CENTER_LEFT, I_CIRCLE);
      it.print(r.x + 54, cy, F.txt, INK, TextAlign::CENTER_LEFT, clip(it, F.txt, items[i], r.w - 54).c_str());
    } else {
      it.print(r.x + 54, cy, F.txt, INK, TextAlign::CENTER_LEFT, ("+" + std::to_string(n - shown) + " more on the phone").c_str());
    }
    if (i < (int) rs.size() - 1) dotline(it, r.x, r.y + r.h - 2, r.w);
  }
}

// ---------- Do: tiles ----------
struct TileDef { const char *name; int page; };  // page -1 = Home controls
static const TileDef TILES[8] = {{"Today", P_TODAY}, {"Calendar", P_CAL}, {"Weather", P_WX}, {"Reminders", P_REM},
                                 {"Timer", P_TIMER}, {"Home", -1}, {"Guest Wi-Fi", P_WIFI}, {"Playing", P_PLAY}};
inline std::vector<Rect> tile_rects(int W, int H) {
  bool L = W > H;
  return grid(16, STRIP + 16, W - 32, H - STRIP - 32, L ? 4 : 2, L ? 2 : 4, 12, 8);
}
inline void do_tiles(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "DO");
  auto rs = tile_rects(W, H);
  char sub[24];
  for (int i = 0; i < 8; i++) {
    const Rect &r = rs[i];
    const TileDef &t = TILES[i];
    bool home = t.page < 0, dim = t.page == P_PLAY && !playing();
    const char *ic = I_CLOCK;
    sub[0] = 0;
    switch (t.page) {
      case P_TODAY: if (now.is_valid()) now.strftime(sub, sizeof(sub), "%a %d"); break;
      case P_CAL: ic = I_CAL; { int n = 0; for (auto &e : agenda_items()) n += e.today; if (n) snprintf(sub, sizeof(sub), "%d today", n); } break;
      case P_WX: ic = wx_icon(S.wx_cond); if (!std::isnan(S.wx_t)) snprintf(sub, sizeof(sub), "%.0f°", S.wx_t); break;
      case P_REM: ic = I_LIST; { int n = (int) reminder_items().size(); if (n) snprintf(sub, sizeof(sub), "%d open", n); } break;
      case P_TIMER: ic = I_TIMER; if (S.timer_run || S.timer_paused) snprintf(sub, sizeof(sub), "%u:%02u", S.timer_left_s / 60, S.timer_left_s % 60); break;
      case P_WIFI: ic = I_QR; break;
      case P_PLAY: ic = I_TV; snprintf(sub, sizeof(sub), "%s", playing() ? "On" : "TV off"); break;
      default: ic = I_HOME; snprintf(sub, sizeof(sub), "%d AC on", ac_on_count()); break;
    }
    if (home) it.filled_rectangle(r.x, r.y, r.w, r.h, INK);
    else if (dim) dashed(it, r.x, r.y, r.w, r.h);
    else box(it, r.x, r.y, r.w, r.h, 4);
    Color c = home ? PAPER : INK;
    it.print(r.x + 12, r.y + 10, F.icon, c, TextAlign::TOP_LEFT, ic);
    if (sub[0]) it.print(r.x + r.w - 12, r.y + 14, F.txt, c, TextAlign::TOP_RIGHT, sub);
    it.print(r.x + 12, r.y + r.h - 10, F.lbl, c, TextAlign::BOTTOM_LEFT, t.name);
  }
}

// ---------- Do: Home controls ----------
// ids: 0-5 AC rooms, 10 all off, 11 door, 12 vacuum, 13 dishwasher
inline std::vector<std::pair<Rect, int>> home_rects(int W, int H) {
  std::vector<std::pair<Rect, int>> v;
  bool L = W > H;
  int x0 = 14, y0 = STRIP + 14, aw, ah, sx, sy, sw, sh;
  if (L) { aw = W - 28 - 236 - 12; ah = H - STRIP - 28; sx = x0 + aw + 12; sy = y0; sw = 236; sh = ah; }
  else { aw = W - 28; ah = H - STRIP - 28 - 220 - 12; sx = x0; sy = y0 + ah + 12; sw = aw; sh = 220; }
  auto ac = grid(x0, y0, aw, ah, L ? 3 : 2, L ? 2 : 3, 10, 6);
  for (int i = 0; i < CFG.ac_count; i++) v.push_back({ac[i], i});
  auto sd = grid(sx, sy, sw, sh, L ? 1 : 2, L ? 4 : 2, 10, 4);
  for (int i = 0; i < 4; i++) v.push_back({sd[i], 10 + i});
  return v;
}
inline void do_home(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "HOME");
  char b[32];
  for (auto &p : home_rects(W, H)) {
    const Rect &r = p.first;
    if (p.second < 6) {
      int i = p.second;
      bool on = is_on_ac(S.ac_state[i]);
      if (on) box(it, r.x, r.y, r.w, r.h, 6); else dashed(it, r.x, r.y, r.w, r.h);
      it.print(r.x + 12, r.y + 10, F.txt, INK, TextAlign::TOP_LEFT, CFG.ac_name[i].c_str());
      it.print(r.x + 12, r.y + r.h / 2 + 4, F.big, INK, TextAlign::CENTER_LEFT, on ? temp_text(S.ac_set[i]).c_str() : "Off");
      std::string sub = on ? ac_mode_name(S.ac_state[i]) + " · " + ac_fan_name(S.ac_fan[i])
                           : (std::isnan(S.ac_now[i]) ? std::string("") : "Room " + temp_text(S.ac_now[i]));
      if (S.ac_state[i] == "unavailable") sub = "Not connected";
      it.print(r.x + 12, r.y + r.h - 10, F.txt, INK, TextAlign::BOTTOM_LEFT, fit(sub, r.w > 200 ? 18 : 12).c_str());
      continue;
    }
    bool locked = S.lock_state == "locked";
    switch (p.second) {
      case 10: snprintf(b, sizeof(b), "%d on now", ac_on_count()); bigbtn(it, r, I_POWER, "All ACs off", b, false); break;
      case 11: bigbtn(it, r, locked ? I_LOCK : I_UNLOCK, "Door", locked ? "Locked" : (S.lock_state == "unlocked" ? "Unlocked" : "Unknown"), !locked && S.lock_state == "unlocked"); break;
      case 12: {
        std::string s = title_case(S.vacuum.empty() ? std::string("unknown") : S.vacuum);
        // the battery only matters when it is away from the dock
        if (S.vacuum != "docked" && !std::isnan(S.vac_batt)) { snprintf(b, sizeof(b), " %.0f%%", S.vac_batt); s += b; }
        bigbtn(it, r, I_VACUUM, "Vacuum", fit(s, 12).c_str(), false);
      } break;
      case 13: bigbtn(it, r, I_DISH, "Dishwasher", fit(dish_state_name(), 16).c_str(), false); break;
    }
  }
}

// ---------- Do: one AC room ----------
// ids: 1 colder, 2 warmer, 3 power, 10-13 mode, 20-23 fan
inline std::vector<std::pair<Rect, int>> room_rects(int W, int H) {
  std::vector<std::pair<Rect, int>> v;
  if (W > H) {
    v.push_back({{22, STRIP + 84, 92, 92}, 1});
    v.push_back({{310, STRIP + 84, 92, 92}, 2});
    v.push_back({{22, H - 16 - 74, 380, 74}, 3});
    auto m = grid(446, STRIP + 52, W - 18 - 446, 148, 2, 2, 8, 4);
    auto f = grid(446, STRIP + 250, W - 18 - 446, 148, 2, 2, 8, 4);
    for (int i = 0; i < 4; i++) { v.push_back({m[i], 10 + i}); v.push_back({f[i], 20 + i}); }
  } else {
    v.push_back({{24, STRIP + 64, 92, 92}, 1});
    v.push_back({{W - 24 - 92, STRIP + 64, 92, 92}, 2});
    v.push_back({{24, H - 16 - 74, W - 48, 74}, 3});
    auto m = grid(24, STRIP + 222, W - 48, 136, 2, 2, 8, 4);
    auto f = grid(24, STRIP + 412, W - 48, 136, 2, 2, 8, 4);
    for (int i = 0; i < 4; i++) { v.push_back({m[i], 10 + i}); v.push_back({f[i], 20 + i}); }
  }
  return v;
}
inline void do_room(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  int i = S.room;
  bool on = is_on_ac(S.ac_state[i]);
  std::string title = CFG.ac_name[i];
  for (auto &c : title) c = toupper(c);
  strip(it, clk, true, title.c_str());
  std::string info = "Room " + temp_text(S.ac_now[i]) + " · " + (on ? ac_mode_name(S.ac_state[i]) : std::string("Off"));
  it.print(L ? 22 : 24, STRIP + 16, F.txt, INK, TextAlign::TOP_LEFT, info.c_str());
  int mid_x = L ? 212 : W / 2, mid_y = L ? STRIP + 130 : STRIP + 110;
  it.print(mid_x, mid_y, F.huge, INK, TextAlign::CENTER, temp_text(S.ac_set[i]).c_str());
  if (L) {
    vline(it, 424, STRIP + 14, H - STRIP - 30, 4);
    it.print(446, STRIP + 14, F.lbl, INK, TextAlign::TOP_LEFT, "MODE");
    it.print(446, STRIP + 212, F.lbl, INK, TextAlign::TOP_LEFT, "FAN");
  } else {
    it.print(24, STRIP + 184, F.lbl, INK, TextAlign::TOP_LEFT, "MODE");
    it.print(24, STRIP + 374, F.lbl, INK, TextAlign::TOP_LEFT, "FAN");
  }
  for (auto &p : room_rects(W, H)) {
    const Rect &r = p.first;
    int id = p.second;
    if (id == 1 || id == 2) pad(it, r.x, r.y, r.w, r.h, id == 1 ? I_MINUS : I_PLUS, false, nullptr, true);
    else if (id == 3) {
      it.filled_rectangle(r.x, r.y, r.w, r.h, INK);
      it.print(r.x + r.w / 2, r.y + r.h / 2, F.lbl, PAPER, TextAlign::CENTER, on ? "TURN OFF" : "TURN ON");
    } else if (id >= 10 && id < 14) chip(it, r, AC_MODES[id - 10][1], on && S.ac_state[i] == AC_MODES[id - 10][0]);
    else if (id >= 20) chip(it, r, AC_FANS[id - 20][1], S.ac_fan[i] == AC_FANS[id - 20][0]);
  }
}

// ---------- Do: door confirm ----------
// ids: 1 cancel, 2 unlock
inline Rect door_card(int W, int H) {
  int w = W > H ? 560 : 440, h = W > H ? 320 : 380;
  return {(W - w) / 2, STRIP + (H - STRIP - h) / 2, w, h};
}
inline std::vector<std::pair<Rect, int>> door_rects(int W, int H) {
  Rect c = door_card(W, H);
  int bw = (c.w - 48 - 12) / 2;
  return {{{c.x + 24, c.y + c.h - 24 - 76, bw, 76}, 1}, {{c.x + 24 + bw + 12, c.y + c.h - 24 - 76, bw, 76}, 2}};
}
inline void do_door(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "DOOR");
  Rect c = door_card(W, H);
  it.filled_rectangle(c.x + 10, c.y + 10, c.w, c.h, INK);
  it.filled_rectangle(c.x, c.y, c.w, c.h, PAPER);
  box(it, c.x, c.y, c.w, c.h, 6);
  it.print(c.x + 24, c.y + 20, F.big, INK, TextAlign::TOP_LEFT, "Unlock the");
  it.print(c.x + 24, c.y + 84, F.big, INK, TextAlign::TOP_LEFT, "front door?");
  it.print(c.x + 24, c.y + 160, F.txt, INK, TextAlign::TOP_LEFT, "It stays open until locked again.");
  auto rs = door_rects(W, H);
  pad(it, rs[0].first.x, rs[0].first.y, rs[0].first.w, rs[0].first.h, "Cancel", false, F.lbl);
  pad(it, rs[1].first.x, rs[1].first.y, rs[1].first.w, rs[1].first.h, "Unlock", true, F.lbl);
}

// ---------- Do: vacuum and dishwasher (info on one side, 3 buttons on the other) ----------
inline std::vector<Rect> three_btns(int W, int H) {
  if (W > H) return grid(W / 2 + 10, STRIP + 14, W / 2 - 28, H - STRIP - 30, 1, 3, 10, 3);
  return grid(18, H - 16 - 3 * 96 - 20, W - 36, 3 * 96 + 20, 1, 3, 10, 3);
}
inline std::vector<Rect> vac_btns(int W, int H) {
  if (W > H) return grid(W / 2 + 10, STRIP + 14, W / 2 - 28, H - STRIP - 30, 1, 4, 8, 4);
  return grid(18, H - 16 - 4 * 86 - 24, W - 36, 4 * 86 + 24, 1, 4, 8, 4);
}
inline void do_vacuum(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "VACUUM");
  int x = 22, y = STRIP + 16;
  print_fit(it, x, y, W > H ? W / 2 - 40 : W - 44, TextAlign::TOP_LEFT, INK,
            title_case(S.vacuum.empty() ? std::string("Unknown") : S.vacuum), {F.big, F.mid});
  char b[40];
  if (!std::isnan(S.vac_batt)) {
    snprintf(b, sizeof(b), "Battery %.0f%%", S.vac_batt);
    it.print(x, y + 84, F.txt, INK, TextAlign::TOP_LEFT, b);
    bar(it, x, y + 122, (W > H ? W / 2 - 50 : W - 44), 22, S.vac_batt / 100.f);
  }
  dotline(it, x, y + 166, W > H ? W / 2 - 50 : W - 44);
  std::string last = ago_text(S.vac_last, now);
  if (!last.empty()) it.print(x, y + 184, F.txt, INK, TextAlign::TOP_LEFT, ("Last clean " + last).c_str());
  it.print(x, y + 222, F.txt, INK, TextAlign::TOP_LEFT, clip(it, F.txt, "Usual: " + CFG.vac_usual, W > H ? W / 2 - 50 : W - 44).c_str());
  bool busy = S.vacuum == "cleaning" || S.vacuum == "returning";
  auto bs = vac_btns(W, H);
  bigbtn(it, bs[0], I_PLAY, busy ? "Cleaning" : "Start usual", CFG.vac_usual.c_str(), true);
  bigbtn(it, bs[1], I_TUNE, "Choose", "mode, power, route", false);
  bigbtn(it, bs[2], I_HOME, "Send to dock", nullptr, false);
  bigbtn(it, bs[3], I_TUNE, "Saved shortcut", "from the vacuum app", false);
}
inline void do_dish(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "DISHWASHER");
  int x = 22, y = STRIP + 16;
  print_fit(it, x, y, W > H ? W / 2 - 40 : W - 44, TextAlign::TOP_LEFT, INK, dish_state_name(), {F.big, F.mid});
  bool running = S.dish_state == "run" || S.dish_state == "delayedstart";
  std::string door = S.dish_door == "open" ? "Door open" : S.dish_door == "closed" || S.dish_door == "locked" ? "Door closed" : "";
  it.print(x, y + 84, F.txt, INK, TextAlign::TOP_LEFT, door.c_str());
  if (running) {
    std::string fin = iso_to_local_hhmm(S.dish_finish);
    if (!fin.empty()) it.print(x, y + 122, F.txt, INK, TextAlign::TOP_LEFT, ("Done about " + fin).c_str());
    bar(it, x, y + 162, W > H ? W / 2 - 50 : W - 44, 22, std::isnan(S.dish_prog) ? NAN : S.dish_prog / 100.f);
  } else if (!S.dish_remote) {
    it.print(x, y + 122, F.txtb, INK, TextAlign::TOP_LEFT, "Press remote start");
    it.print(x, y + 158, F.txtb, INK, TextAlign::TOP_LEFT, "on the dishwasher first");
  } else {
    it.print(x, y + 122, F.txt, INK, TextAlign::TOP_LEFT, "Remote start is on");
  }
  auto bs = three_btns(W, H);
  bigbtn(it, bs[0], I_PLAY, "Start usual", (std::string(dish_prog_name(CFG.dish_usual)) + ", now").c_str(), true);
  bigbtn(it, bs[1], I_TUNE, "Choose", "program, options, time", false);
  bigbtn(it, bs[2], I_STOP, "Stop", nullptr, false);
}

// ---------- Do: dishwasher wizard ----------
inline std::vector<Rect> dw_prog_rects(int W, int H) { return grid(M, STRIP + M, W - 2 * M, H - STRIP - 2 * M, 3, 3, GAP, 9); }
inline std::vector<Rect> dw_opt_rects(int W, int H) {
  if (W > H) return grid(M, STRIP + M, W - 2 * M - NEXT_W - 12, H - STRIP - 2 * M, 1, 5, 8, 5);
  return grid(M, STRIP + M, W - 2 * M, H - STRIP - 2 * M - NEXT_H - 12, 1, 5, 8, 5);
}
inline std::vector<Rect> dw_when_rects(int W, int H) {
  int top = STRIP + 110;
  return grid(M, top, W - 2 * M, H - top - M, W > H ? 2 : 1, W > H ? 2 : 4, GAP, 4);
}
inline void do_dw1(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "PROGRAM", 0, 3);
  auto rs = dw_prog_rects(W, H);
  std::string cur = S.wiz_prog.empty() ? DISH_PROGS[0][0] : S.wiz_prog;
  for (int i = 0; i < 9; i++) chip(it, rs[i], DISH_PROGS[i][1], cur == DISH_PROGS[i][0]);
}
inline void do_dw2(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "OPTIONS", 1, 3);
  auto rs = dw_opt_rects(W, H);
  for (int i = 0; i < 5; i++) {
    const Rect &r = rs[i];
    bool on = S.dish_opt[i] == "on", off = S.dish_opt[i] != "on" && S.dish_opt[i] != "off";
    if (off) dashed(it, r.x, r.y, r.w, r.h); else box(it, r.x, r.y, r.w, r.h, 3);
    // the tick box on the left
    int s = r.h - 20;
    if (on) it.filled_rectangle(r.x + 10, r.y + 10, s, s, INK); else if (!off) box(it, r.x + 10, r.y + 10, s, s, 3);
    it.print(r.x + 22 + s, r.y + r.h / 2, F.lbl, INK, TextAlign::CENTER_LEFT, DISH_OPTS[i][1]);
    if (off && W > H) it.print(r.x + r.w - 12, r.y + r.h / 2, F.txt, INK, TextAlign::CENTER_RIGHT, "not for this program");
  }
  next_btn(it, W, H);
}
inline void do_dw3(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "START", 2, 3);
  std::string sum = dish_prog_name(S.wiz_prog.empty() ? DISH_PROGS[0][0] : S.wiz_prog);
  for (int i = 0; i < 5; i++) if (S.dish_opt[i] == "on") sum += std::string(" · ") + DISH_OPTS[i][1];
  print_fit(it, 22, STRIP + 16, W - 44, TextAlign::TOP_LEFT, INK, sum, {F.mid, F.txtb, F.txt});
  it.print(22, STRIP + 68, F.txt, INK, TextAlign::TOP_LEFT, S.dish_remote ? "Pick when it starts" : "Press remote start on the dishwasher first");
  auto rs = dw_when_rects(W, H);
  for (int i = 0; i < 4; i++) chip(it, rs[i], DISH_WHEN[i][1], i == 0);
}

// ---------- Glance pages added 2026-11-19: weather now, year, life in weeks, history, world clock, biorhythm, GitHub ----------
// Word-wrap into at most max_lines lines of about max_chars characters
inline std::vector<std::string> wrap_lines(const std::string &text, size_t max_chars, size_t max_lines) {
  std::vector<std::string> out;
  std::string line, word;
  auto flush_word = [&]() {
    if (word.empty()) return;
    if (line.empty()) line = word;
    else if (line.size() + 1 + word.size() <= max_chars) line += " " + word;
    else { out.push_back(line); line = word; }
    word.clear();
  };
  for (char c : text + " ") {
    if (c == ' ') flush_word(); else word += c;
  }
  if (!line.empty()) out.push_back(line);
  if (out.size() > max_lines) {
    out.resize(max_lines);
    std::string &l = out.back();
    if (l.size() + 3 > max_chars) l = l.substr(0, max_chars - 3);
    while (!l.empty() && (l.back() == ' ' || l.back() == ',' || l.back() == '.')) l.pop_back();
    l += "...";
  }
  return out;
}
// Days since 1970-01-01 for a calendar date, and back
inline int64_t civil_days(int y, int m, int d) { return days_from_civil(y, m, d); }
inline bool parse_ymd(const std::string &s, int &y, int &m, int &d) {
  return sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) == 3 && y > 1900 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}
// Western Easter Sunday (anonymous Gregorian algorithm)
inline void easter(int y, int &month, int &day) {
  int a = y % 19, b = y / 100, c = y % 100, d = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
  int h = (19 * a + b - d - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
  int m = (a + 11 * h + 22 * l) / 451;
  month = (h + l - 7 * m + 114) / 31;
  day = ((h + l - 7 * m + 114) % 31) + 1;
}
inline std::string moveable_feast(esphome::ESPTime now) {
  if (!now.is_valid()) return "";
  int em, ed;
  easter(now.year, em, ed);
  int64_t diff = civil_days(now.year, now.month, now.day_of_month) - civil_days(now.year, em, ed);
  switch (diff) {
    case -46: return "Ash Wednesday, the start of Lent";
    case -7: return "Palm Sunday";
    case -3: return "Maundy Thursday";
    case -2: return "Good Friday";
    case 0: return "Easter Sunday";
    case 39: return "Ascension Day";
    case 49: return "Pentecost";
    default: return "";
  }
}

// ---- Weather now: three numbers, then temperature / humidity / wind for the next 12 hours ----
struct Hour { int hh, t, h, w; };
inline std::vector<Hour> hourly() {
  std::vector<Hour> v;
  for (auto &p : split(S.hourly, ';')) {
    Hour x;
    if (sscanf(p.c_str(), "%d,%d,%d,%d", &x.hh, &x.t, &x.h, &x.w) == 4) v.push_back(x);
  }
  return v;
}
inline void page_weather_now(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  // Home Assistant sends weather at least hourly; 3 h without any means it is stuck
  page_strip(it, clk, "WEATHER", P_WX, data_age(S.wx_at, now, 3 * 3600));
  auto hs = hourly();
  int n = std::min((int) hs.size(), L ? 12 : 8);
  int tmin = 99, tmax = -99, hmin = 100, hmax = 0, wmin = 999, wmax = 0, wmax_hh = 0;
  for (int i = 0; i < n; i++) {
    const Hour &x = hs[i];
    tmin = std::min(tmin, x.t); tmax = std::max(tmax, x.t); hmin = std::min(hmin, x.h); hmax = std::max(hmax, x.h);
    wmin = std::min(wmin, x.w);
    if (x.w > wmax) { wmax = x.w; wmax_hh = x.hh; }
  }
  char b[48];
  // ---- the big part (about 70%): temperature, humidity and wind now ----
  const int body = H - STRIP, big_h = body * 70 / 100;
  const int hum_y = STRIP + big_h, wind_y = hum_y + body * 13 / 100, hours_y = H - 4;
  std::string t = temp_text(S.wx_t);
  if (L) {
    // three big numbers side by side: temperature (largest), humidity, wind
    it.print(10, STRIP - 10, F.clock, INK, TextAlign::TOP_LEFT, t.c_str());
    vline(it, 300, STRIP + 14, 150, 3);
    it.print(320, STRIP + 8, F.lbl, INK, TextAlign::TOP_LEFT, "HUMIDITY");
    if (std::isnan(S.wx_h)) snprintf(b, sizeof(b), "--"); else snprintf(b, sizeof(b), "%.0f%%", S.wx_h);
    it.print(316, STRIP + 30, F.huge, INK, TextAlign::TOP_LEFT, b);
    vline(it, 556, STRIP + 14, 150, 3);
    it.print(576, STRIP + 8, F.lbl, INK, TextAlign::TOP_LEFT, "WIND KM/H");
    if (std::isnan(S.wx_wind)) snprintf(b, sizeof(b), "--"); else snprintf(b, sizeof(b), "%.0f", S.wx_wind);
    it.print(572, STRIP + 30, F.huge, INK, TextAlign::TOP_LEFT, b);
    std::string cond = wx_text(S.wx_cond);
    if (n) { snprintf(b, sizeof(b), " · %d° to %d° next 12 h", tmin, tmax); cond += b; }
    it.print(320, STRIP + 160, F.sm, INK, TextAlign::TOP_LEFT, cond.c_str());
  } else {
    it.print(W / 2, STRIP + 6, F.clock, INK, TextAlign::TOP_CENTER, t.c_str());
    it.print(W / 2, STRIP + 212, F.txtb, INK, TextAlign::TOP_CENTER, wx_text(S.wx_cond).c_str());
    hline(it, 20, STRIP + 256, W - 40, 3);
    it.print(24, STRIP + 268, F.lbl, INK, TextAlign::TOP_LEFT, "HUMIDITY");
    it.print(W / 2 + 12, STRIP + 268, F.lbl, INK, TextAlign::TOP_LEFT, "WIND km/h");
    if (std::isnan(S.wx_h)) snprintf(b, sizeof(b), "--"); else snprintf(b, sizeof(b), "%.0f%%", S.wx_h);
    it.print(24, STRIP + 298, F.big, INK, TextAlign::TOP_LEFT, b);
    if (std::isnan(S.wx_wind)) snprintf(b, sizeof(b), "--"); else snprintf(b, sizeof(b), "%.0f", S.wx_wind);
    it.print(W / 2 + 12, STRIP + 298, F.big, INK, TextAlign::TOP_LEFT, b);
    vline(it, W / 2, STRIP + 266, 100, 3);
  }
  if (!n) { it.print(W / 2, hum_y + 20, F.txt, INK, TextAlign::CENTER, "Hourly forecast not loaded yet"); return; }
  // temperature for the next hours, the bottom of the big part
  int left = L ? 70 : 64, bw = (W - left - 12) / n;
  int t_top = L ? STRIP + 198 : STRIP + 400, t_base = hum_y - 6, span = t_base - t_top - 22;
  it.print(14, t_top + 4, F.sm, INK, TextAlign::TOP_LEFT, "TEMP");
  for (int i = 0; i < n; i++) {
    int cx = left + i * bw + bw / 2;
    int hgt = std::max(6, (hs[i].t - tmin + 2) * span / std::max(4, tmax - tmin + 2));
    it.filled_rectangle(cx - bw / 2 + 5, t_base - hgt, bw - 10, hgt, INK);
    snprintf(b, sizeof(b), "%d", hs[i].t);
    it.print(cx, t_base - hgt - 2, F.sm, INK, TextAlign::BOTTOM_CENTER, b);
  }
  // humidity row (~13%): a line through the values, numbers above each point
  for (int x = 14; x < W - 14; x += 6) it.filled_rectangle(x, hum_y, 3, 2, INK);
  int h_top = hum_y + 24, h_bot = wind_y - 6, hlo = std::max(0, hmin - 2), hspan = std::max(8, hmax - hlo + 2);
  it.print(14, hum_y + 4, F.sm, INK, TextAlign::TOP_LEFT, "HUMID");
  int px = 0, py = 0;
  for (int i = 0; i < n; i++) {
    int cx = left + i * bw + bw / 2, y = h_bot - (hs[i].h - hlo) * (h_bot - h_top) / hspan;
    it.filled_circle(cx, y, 4, INK);
    if (i) for (int k = -1; k <= 1; k++) it.line(px, py + k, cx, y + k, INK);
    snprintf(b, sizeof(b), "%d", hs[i].h);
    it.print(cx + (bw > 40 ? 0 : 0), y - 5, F.sm, INK, TextAlign::BOTTOM_CENTER, b);
    px = cx; py = y;
  }
  // wind row (~13%): short hatched bars
  for (int x = 14; x < W - 14; x += 6) it.filled_rectangle(x, wind_y, 3, 2, INK);
  int w_base = hours_y - 26, w_span = w_base - wind_y - 26;
  it.print(14, wind_y + 4, F.sm, INK, TextAlign::TOP_LEFT, "WIND");
  for (int i = 0; i < n; i++) {
    int cx = left + i * bw + bw / 2, hgt = std::max(4, hs[i].w * w_span / std::max(10, wmax));
    int bx = cx - bw / 2 + 8, bwid = bw - 16;
    box(it, bx, w_base - hgt, bwid, hgt, 2);
    for (int yy = w_base - hgt + 4; yy < w_base; yy += 4) it.filled_rectangle(bx + 2, yy, bwid - 4, 1, INK);
    snprintf(b, sizeof(b), "%d", hs[i].w);
    it.print(cx, w_base - hgt - 2, F.sm, INK, TextAlign::BOTTOM_CENTER, b);
  }
  for (int i = 0; i < n; i += 1) {
    int cx = left + i * bw + bw / 2;
    // the column for the hour we are in now gets a black "NOW" label
    if (now.is_valid() && hs[i].hh == now.hour) {
      it.filled_rectangle(cx - bw / 2 + 3, hours_y - 22, bw - 6, 22, INK);
      it.print(cx, hours_y - 1, F.sm, PAPER, TextAlign::BOTTOM_CENTER, bw >= 44 ? "NOW" : "now");
      continue;
    }
    snprintf(b, sizeof(b), "%02d", hs[i].hh);
    it.print(cx, hours_y, F.sm, INK, TextAlign::BOTTOM_CENTER, b);
  }
}

// ---- Year in progress ----
inline void page_year(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  char yr[8];
  snprintf(yr, sizeof(yr), "%d", now.is_valid() ? now.year : 0);
  page_strip(it, clk, yr, P_YEAR);
  if (!now.is_valid()) return;
  bool leap = (now.year % 4 == 0 && now.year % 100 != 0) || now.year % 400 == 0;
  int ylen = leap ? 366 : 365, doy = now.day_of_year;
  float frac = (doy - 1 + (now.hour * 60 + now.minute) / 1440.f) / ylen;
  char pct[8], l1[40], l2[40], l3[40];
  snprintf(pct, sizeof(pct), "%d%%", (int) (frac * 100));
  snprintf(l1, sizeof(l1), "Day %d of %d", doy, ylen);
  snprintf(l2, sizeof(l2), "%d days left in %d", ylen - doy, now.year);
  static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int mlen = mdays[now.month - 1] + (now.month == 2 && leap ? 1 : 0);
  float mfrac = (now.day_of_month - 1 + (now.hour * 60 + now.minute) / 1440.f) / mlen;
  int week = (doy - 1) / 7 + 1;
  char mname[16];
  now.strftime(mname, sizeof(mname), "%B");
  snprintf(l3, sizeof(l3), "Week %d · %s %d%%", week, mname, (int) (mfrac * 100));
  if (L) {
    it.print(18, STRIP + 4, F.clock, INK, TextAlign::TOP_LEFT, pct);
    it.print(400, STRIP + 40, F.txtb, INK, TextAlign::TOP_LEFT, l1);
    it.print(400, STRIP + 82, F.txt, INK, TextAlign::TOP_LEFT, l2);
    it.print(400, STRIP + 120, F.txt, INK, TextAlign::TOP_LEFT, l3);
  } else {
    it.print(W / 2, STRIP + 10, F.clock, INK, TextAlign::TOP_CENTER, pct);
    it.print(24, STRIP + 240, F.txtb, INK, TextAlign::TOP_LEFT, l1);
    it.print(24, STRIP + 280, F.txt, INK, TextAlign::TOP_LEFT, l2);
    it.print(24, STRIP + 318, F.txt, INK, TextAlign::TOP_LEFT, l3);
  }
  // the year as 365 thin bars: past days filled
  int bx = 22, bw = W - 44, by = L ? STRIP + 236 : STRIP + 390, bh = L ? 70 : 90;
  for (int d = 0; d < ylen; d++) {
    int x = bx + d * bw / ylen, x2 = bx + (d + 1) * bw / ylen;
    if (d < doy - 1) it.filled_rectangle(x, by, std::max(1, x2 - x - (L ? 0 : 0)), bh, INK);
    else if (d % 2 == 0) it.filled_rectangle(x, by + bh / 2 - 3, 1, 6, INK);
  }
  box(it, bx - 3, by - 3, bw + 6, bh + 6, 3);
  int acc = 0;
  const char *ml = "JFMAMJJASOND";
  for (int m = 0; m < 12; m++) {
    int len = mdays[m] + (m == 1 && leap ? 1 : 0);
    char s[2] = {ml[m], 0};
    it.print(bx + (acc + len / 2) * bw / ylen, by + bh + 8, F.sm, INK, TextAlign::TOP_CENTER, s);
    acc += len;
  }
  // today, this week, this month
  float wfrac = ((now.day_of_week + 5) % 7 + (now.hour * 60 + now.minute) / 1440.f) / 7.f;  // Monday first
  float dfrac = (now.hour * 60 + now.minute) / 1440.f;
  const char *nm[3] = {"Today", "This week", "This month"};
  float fr[3] = {dfrac, wfrac, mfrac};
  int y = by + bh + 44;
  for (int i = 0; i < 3; i++) {
    int x = L ? 22 + i * (W - 44) / 3 : 22, yy = L ? y : y + i * 62, w = L ? (W - 44) / 3 - 24 : W - 44;
    it.print(x, yy, F.sm, INK, TextAlign::TOP_LEFT, nm[i]);
    bar(it, x, yy + 26, w, 22, fr[i]);
  }
}

// ---- Life in weeks (needs the birth date from HA) ----
inline bool birth(int &y, int &m, int &d) { return parse_ymd(S.birth, y, m, d); }
inline void need_birth(Display &it) {
  empty_state(it, "Birth date needed", "Set 'Sticky birth date' in HA");
}
inline void page_life(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  int by, bm, bd;
  if (!now.is_valid() || !birth(by, bm, bd)) { page_strip(it, clk, "LIFE IN WEEKS", P_LIFE); need_birth(it); return; }
  int64_t days = civil_days(now.year, now.month, now.day_of_month) - civil_days(by, bm, bd);
  int week = (int) (days / 7), age = now.year - by - ((now.month < bm || (now.month == bm && now.day_of_month < bd)) ? 1 : 0);
  char lab[40];
  snprintf(lab, sizeof(lab), L ? "WEEK %d · AGE %d" : "WEEK %d", week + 1, age);
  page_strip(it, clk, lab, P_LIFE);
  // 90 years x 52 weeks. Landscape: a column per year; portrait: a row per year.
  int cols = L ? 90 : 52, rows = L ? 52 : 90;
  int x0 = L ? 40 : 34, y0 = STRIP + 10, gw = W - x0 - 10, gh = H - y0 - (L ? 26 : 10);
  float cw = (float) gw / cols, ch = (float) gh / rows;
  int s = std::max(2, (int) std::min(cw, ch) - 2);
  for (int yr = 0; yr < 90; yr++)
    for (int wk = 0; wk < 52; wk++) {
      int n = yr * 52 + wk;
      int cx = L ? yr : wk, cy = L ? wk : yr;
      int x = x0 + (int) (cx * cw), y = y0 + (int) (cy * ch);
      if (n < week) it.filled_rectangle(x, y, s, s, INK);
      else if (n == week) { box(it, x - 2, y - 2, s + 4, s + 4, 2); it.filled_rectangle(x, y, s, s, INK); }
      else it.filled_rectangle(x + s / 2, y + s / 2, 1, 1, INK);
    }
  for (int a = 0; a <= 80; a += 10) {
    char b[4];
    snprintf(b, sizeof(b), "%d", a);
    if (L) it.print(x0 + (int) (a * cw), H - 2, F.sm, INK, TextAlign::BOTTOM_LEFT, b);
    else it.print(x0 - 4, y0 + (int) (a * ch), F.sm, INK, TextAlign::TOP_RIGHT, b);
  }
}

// ---- This Day in History (built-in list from art/build_history.py; your topics first) ----
inline std::vector<std::pair<std::string, std::string>> history_today(esphome::ESPTime now) {
  std::vector<std::pair<std::string, std::string>> v;
  if (!now.is_valid()) return v;
  char key[8];
  snprintf(key, sizeof(key), "%02d%02d\t", now.month, now.day_of_month);
  const char *p = strstr(STICKY_HISTORY, key);
  if (p && (p == STICKY_HISTORY || p[-1] == '\n')) {
    const char *end = strchr(p, '\n');
    std::string line(p + 5, end ? end - p - 5 : strlen(p + 5));
    for (auto &f : split(line, '\t')) {
      size_t bar = f.find('|');
      if (bar != std::string::npos) v.push_back({f.substr(0, bar), f.substr(bar + 1)});
    }
  }
  std::string feast = moveable_feast(now);
  if (!feast.empty()) {
    v.insert(v.begin(), {"Today", feast});
    if (v.size() > 3) v.resize(3);
  }
  return v;
}
inline void page_history(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  char lab[24] = "HISTORY";
  if (now.is_valid()) { now.strftime(lab, sizeof(lab), "%d %B"); for (char *c = lab; *c; c++) *c = toupper(*c); }
  page_strip(it, clk, lab, P_HIST);
  auto items = history_today(now);
  if (items.empty()) { empty_state(it, "Nothing for today", "Run art/build_history.py to add events"); return; }
  int n = (int) items.size(), rh = (H - STRIP - 8) / n;
  for (int i = 0; i < n; i++) {
    int y = STRIP + 6 + i * rh;
    if (i) dotline(it, 18, y - 4, W - 36);
    if (L) {
      it.print(20, y + 10, items[i].first == "Today" ? F.mid : F.big, INK, TextAlign::TOP_LEFT, items[i].first.c_str());
      auto ls = wrap_lines(items[i].second, 38, 3);
      for (size_t k = 0; k < ls.size(); k++) it.print(200, y + 12 + k * 36, F.txt, INK, TextAlign::TOP_LEFT, ls[k].c_str());
    } else {
      it.print(20, y + 6, F.mid, INK, TextAlign::TOP_LEFT, items[i].first.c_str());
      auto ls = wrap_lines(items[i].second, 30, 4);
      for (size_t k = 0; k < ls.size(); k++) it.print(20, y + 60 + k * 36, F.txt, INK, TextAlign::TOP_LEFT, ls[k].c_str());
    }
  }
}

// ---- World clock (cities and their POSIX time zones come from the settings) ----
inline void page_world(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  page_strip(it, clk, "WORLD", P_WORLD);
  if (!now.is_valid() || CFG.city_count == 0) return;
  int64_t utc = (int64_t) now.timestamp;
  static const char *DAYS[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  int home_mins;
  int64_t home_day;
  local_time(utc, home_mins, home_day);
  int n = CFG.city_count, rh = (H - STRIP - 16) / MAX_CITY;
  // one row per city: local time, name, the day and how far ahead or behind home, sun by day or moon by night
  for (int c = 0; c < n; c++) {
    int mins;
    int64_t lday;
    local_time(utc, mins, lday, CFG.city_tz[c]);
    char t[8], d[48], off[24];
    snprintf(t, sizeof(t), "%02d:%02d", mins / 60, mins % 60);
    int diff = (int) ((lday - home_day) * 1440 + mins - home_mins), ad = std::abs(diff);
    if (diff == 0) snprintf(off, sizeof(off), "same as here");
    else if (ad % 60) snprintf(off, sizeof(off), "%d h %d min %s", ad / 60, ad % 60, diff > 0 ? "ahead" : "behind");
    else snprintf(off, sizeof(off), "%d h %s", ad / 60, diff > 0 ? "ahead" : "behind");
    snprintf(d, sizeof(d), "%s · %s", DAYS[((lday + 4) % 7 + 7) % 7], off);
    int y = STRIP + 8 + c * rh, cy = y + rh / 2;
    if (c) for (int x = 18; x < W - 18; x += 6) it.filled_rectangle(x, y, 3, 1, INK);
    it.print(18, cy, F.mid, INK, TextAlign::CENTER_LEFT, t);
    int nx = L ? 190 : 170;
    print_fit(it, nx, cy - 14, W - nx - 70, TextAlign::CENTER_LEFT, INK, CFG.city_name[c], {F.txtb, F.txt});
    print_fit(it, nx, cy + 16, W - nx - 70, TextAlign::CENTER_LEFT, INK, d, {F.txt, F.sm});
    bool day = mins >= 6 * 60 && mins < 18 * 60;
    it.print(W - 22, cy, F.icon, INK, TextAlign::CENTER_RIGHT, day ? "\U000F0599" : I_NIGHT);
  }
}

// ---- Biorhythm (physical 23 days, emotional 28, intellectual 33) ----
inline void page_bio(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  page_strip(it, clk, "BIORHYTHM", P_BIO);
  int by, bm, bd;
  if (!now.is_valid() || !birth(by, bm, bd)) { need_birth(it); return; }
  int64_t days = civil_days(now.year, now.month, now.day_of_month) - civil_days(by, bm, bd);
  int cx = 24, cw = L ? 600 : W - 48, cy = STRIP + (L ? 200 : 210), amp = L ? 150 : 150;
  for (int x = cx; x < cx + cw; x += 6) it.filled_rectangle(x, cy, 3, 2, INK);
  int tx = cx + cw / 2;
  it.filled_rectangle(tx - 1, cy - amp - 10, 3, 2 * amp + 20, INK);
  it.print(tx, cy - amp - 12, F.sm, INK, TextAlign::BOTTOM_CENTER, "TODAY");
  const int per[3] = {23, 28, 33};
  const char *nm[3] = {"Physical", "Emotional", "Intellectual"};
  for (int k = 0; k < 3; k++) {
    int py = 0;
    for (int i = 0; i <= cw; i++) {
      float d = days + (i * 30.f / cw) - 15.f;
      int y = cy - (int) (amp * std::sin(2 * M_PI * d / per[k]));
      // solid, dashed, dotted
      bool draw = k == 0 || (k == 1 && (i / 10) % 2 == 0) || (k == 2 && i % 6 < 2);
      if (i && draw) for (int t = -1; t <= 1; t++) it.line(cx + i - 1, py + t, cx + i, y + t, INK);
      py = y;
    }
  }
  for (int k = 0; k < 3; k++) {
    int v = (int) std::round(100 * std::sin(2 * M_PI * days / per[k]));
    char b[8];
    snprintf(b, sizeof(b), "%+d%%", v);
    int x = L ? 650 : 24 + k * (W - 48) / 3, y = L ? STRIP + 30 + k * 130 : H - 130;
    // the line style sample
    for (int i = 0; i < 50; i++) {
      bool draw = k == 0 || (k == 1 && (i / 10) % 2 == 0) || (k == 2 && i % 6 < 2);
      if (draw) it.filled_rectangle(x + i, y, 1, 4, INK);
    }
    it.print(x, y + 12, F.sm, INK, TextAlign::TOP_LEFT, nm[k]);
    it.print(x, y + 38, F.mid, INK, TextAlign::TOP_LEFT, b);
  }
}

// ---- GitHub contributions, last 53 weeks (fetched by the Sticky once a day) ----
inline void gh_cell(Display &it, int x, int y, int s, int lvl) {
  if (lvl <= 0) { it.rectangle(x, y, s, s, INK); return; }
  if (lvl >= 4) { it.filled_rectangle(x, y, s, s, INK); return; }
  it.rectangle(x, y, s, s, INK);
  int step = lvl == 1 ? 4 : lvl == 2 ? 3 : 2;
  for (int a = 1; a < s - 1; a += step)
    for (int b = 1 + ((a / step) % 2); b < s - 1; b += step) it.draw_pixel_at(x + a, y + b, INK);
}
inline void page_github(Display &it, const char *clk, esphome::ESPTime now) {
  const int W = it.get_width(), H = it.get_height();
  const bool L = W > H;
  // fetched once a day, so older than 26 h means the last fetches failed
  page_strip(it, clk, "GITHUB", P_GH, data_age(S.gh_at, now, 26 * 3600));
  if (S.gh_total < 0) {
    if (S.gh_error.empty()) empty_state(it, "Loading GitHub activity...");
    else empty_state(it, S.gh_error.c_str(), "Tries again every hour");
    return;
  }
  // streak and best day from the daily counts (newest last)
  int streak = 0, best = 0;
  for (int i = S.gh_days - 1; i >= 0; i--) { if (S.gh_count[i] > 0) streak++; else if (i < S.gh_days - 1) break; }
  for (int i = 0; i < S.gh_days; i++) best = std::max(best, (int) S.gh_count[i]);
  char b[48];
  hero(it, 20, STRIP + 8, std::to_string(S.gh_total), "CONTRIBUTIONS", "in the last year");
  snprintf(b, sizeof(b), "Streak %d · best day %d", streak, best);
  it.print(L ? W - 20 : 20, L ? STRIP + 30 : STRIP + 96, F.txtb, INK, L ? TextAlign::TOP_RIGHT : TextAlign::TOP_LEFT, b);
  // grid: weeks as columns, Sunday on top; last column is this week
  // landscape: 53 weeks; portrait: the last 26 weeks, bigger squares
  int weeks = L ? 53 : 26, s = L ? 12 : 15, g = 2;
  int gw = weeks * (s + g), x0 = (W - gw) / 2, y0 = L ? STRIP + 120 : STRIP + 160;
  int first_dow = S.gh_first_dow;  // weekday of the oldest day, 0 = Sunday
  int total_cols = (S.gh_days + first_dow + 6) / 7, skip = total_cols - weeks;
  for (int i = 0; i < S.gh_days; i++) {
    int idx = i + first_dow, col = idx / 7 - skip, row = idx % 7;
    if (col < 0) continue;
    gh_cell(it, x0 + col * (s + g), y0 + row * (s + g), s, S.gh_level[i]);
  }
  int ly = y0 + 7 * (s + g) + 20;
  it.print(L ? x0 : 20, ly, F.sm, INK, TextAlign::TOP_LEFT, "Less");
  for (int l = 0; l <= 4; l++) gh_cell(it, (L ? x0 : 20) + 54 + l * 20, ly + 4, 14, l);
  it.print((L ? x0 : 20) + 162, ly, F.sm, INK, TextAlign::TOP_LEFT, "More");
}
// Reads the JSON from github-contributions-api.jogruber.de/v4/<user>?y=last
inline bool parse_github(const std::string &body, esphome::ESPTime now) {
  const char *p = body.c_str();
  const char *tot = strstr(p, "\"lastYear\":");
  int total = tot ? atoi(tot + 11) : -1;
  std::vector<std::string> dates;
  std::vector<uint8_t> cnts, lvls;
  const char *c = p;
  while ((c = strstr(c, "\"date\":\""))) {
    const char *cnt = strstr(c, "\"count\":"), *lvl = strstr(c, "\"level\":");
    if (!cnt || !lvl) break;
    dates.emplace_back(c + 8, 10);
    cnts.push_back((uint8_t) std::min(255, atoi(cnt + 8)));
    lvls.push_back((uint8_t) std::min(4, atoi(lvl + 8)));
    c = lvl;
  }
  if (dates.size() < 300 || total < 0) return false;
  // keep the newest 371 days at most (53 weeks)
  size_t from = dates.size() > 371 ? dates.size() - 371 : 0;
  int n = (int) (dates.size() - from);
  for (int i = 0; i < n; i++) { S.gh_count[i] = cnts[from + i]; S.gh_level[i] = lvls[from + i]; }
  std::string first_date = dates[from];
  int y, m, d;
  S.gh_first_dow = parse_ymd(first_date, y, m, d) ? (int) ((civil_days(y, m, d) + 4) % 7) : 0;
  S.gh_days = n;
  S.gh_total = total;
  S.gh_error.clear();
  if (now.is_valid()) S.gh_doy = now.day_of_year;
  stamp(S.gh_at, now);
  return true;
}

// ---------- Do: vacuum wizard (mode -> suction and mop -> route, which starts it) ----------
inline std::vector<Rect> vw_rects(int W, int H, int n) {
  int top = STRIP + 70;
  return grid(M, top, W - 2 * M, H - top - M, W > H ? 2 : 1, W > H ? 2 : 4, GAP, n);
}
inline std::vector<Rect> vw2_rects(int W, int H, int which) {
  // which 0 = suction (4), 1 = mop wetness (3)
  int right = W > H ? W - M - NEXT_W - 12 : W - M, top = STRIP + 50;
  int area_h = (W > H ? H - M : H - M - NEXT_H - 12) - top;
  int half = (area_h - 40) / 2;
  if (which == 0) return grid(M, top, right - M, half, 4, 1, 8, 4);
  return grid(M, top + half + 40 + GAP, right - M, half - GAP, 3, 1, 8, 3);
}
inline void do_vw1(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "MODE", 0, 3);
  it.print(22, STRIP + 16, F.txt, INK, TextAlign::TOP_LEFT, "What should it do?");
  auto rs = vw_rects(W, H, 4);
  for (int k = 0; k < 4; k++) chip(it, rs[k], VAC_MODE[k][1], S.vwiz[0] == k);
}
inline void do_vw2(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "POWER", 1, 3);
  it.print(18, STRIP + 14, F.lbl, INK, TextAlign::TOP_LEFT, "SUCTION");
  auto su = vw2_rects(W, H, 0), mo = vw2_rects(W, H, 1);
  for (int k = 0; k < 4; k++) chip(it, su[k], VAC_SUCTION[k][1], S.vwiz[1] == k);
  it.print(18, mo[0].y - 40, F.lbl, INK, TextAlign::TOP_LEFT, "MOP WETNESS");
  bool mops = S.vwiz[0] != 0;  // plain sweeping does not use the mop
  for (int k = 0; k < 3; k++) chip(it, mo[k], VAC_MOP[k][1], mops && S.vwiz[2] == k, !mops);
  next_btn(it, W, H);
}
inline void do_vw3(Display &it, const char *clk) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, clk, true, "ROUTE", 2, 3);
  std::string sum = std::string(VAC_MODE[S.vwiz[0]][1]) + " · " + VAC_SUCTION[S.vwiz[1]][1];
  if (S.vwiz[0] != 0) sum += std::string(" · ") + VAC_MOP[S.vwiz[2]][1];
  print_fit(it, 22, STRIP + 14, W - 44, TextAlign::TOP_LEFT, INK, sum, {F.txtb, F.txt, F.sm});
  it.print(22, STRIP + 46, F.sm, INK, TextAlign::TOP_LEFT, "Tap a route to start cleaning");
  auto rs = vw_rects(W, H, 4);
  for (int k = 0; k < 4; k++) chip(it, rs[k], VAC_ROUTE[k][1], S.vwiz[3] == k);
}

inline void draw_page(Display &it, int page, const char *clk, esphome::ESPTime now, esphome::qr_code::QrCode *qr,
                      const char *guest_ssid) {
  switch (page) {
    case P_CAL: page_calendar(it, clk, now); break;
    case P_WX: page_weather_now(it, clk, now); break;
    case P_FC: page_weather(it, clk, now); break;
    case P_REM: page_reminders(it, clk); break;
    case P_YEAR: page_year(it, clk, now); break;
    case P_LIFE: page_life(it, clk, now); break;
    case P_HIST: page_history(it, clk, now); break;
    case P_WORLD: page_world(it, clk, now); break;
    case P_BIO: page_bio(it, clk, now); break;
    case P_GH: page_github(it, clk, now); break;
    case P_TIMER: page_timer(it, clk); break;
    case P_WIFI: page_wifi(it, clk, qr, guest_ssid); break;
    case P_PLAY: page_playing(it, clk); break;
    default: page_today(it, now); break;
  }
}
inline void draw_do(Display &it, const char *clk, esphome::ESPTime now, esphome::qr_code::QrCode *qr, const char *guest_ssid) {
  switch (S.do_screen) {
    case D_HOME: do_home(it, clk); break;
    case D_ROOM: do_room(it, clk); break;
    case D_DOOR: do_door(it, clk); break;
    case D_VAC: do_vacuum(it, clk, now); break;
    case D_DISH: do_dish(it, clk); break;
    case D_DW1: do_dw1(it, clk); break;
    case D_DW2: do_dw2(it, clk); break;
    case D_DW3: do_dw3(it, clk); break;
    case D_VW1: do_vw1(it, clk); break;
    case D_VW2: do_vw2(it, clk); break;
    case D_VW3: do_vw3(it, clk); break;
    case D_PAGE: draw_page(it, S.do_page, clk, now, qr, guest_ssid); break;
    default: do_tiles(it, clk, now); break;
  }
}

inline void asleep_bar(Display &it) {
  const int W = it.get_width(), H = it.get_height();
  it.filled_rectangle(0, 0, W, STRIP, INK);
  std::string t = "Asleep · updated " + S.asleep_at + " · press a button";
  if (W < H) t = "Asleep " + S.asleep_at + " · press a button";
  it.print(W / 2, STRIP / 2, F.txtb, PAPER, TextAlign::CENTER, t.c_str());
}

// ---------- touch setup (two dots) ----------
inline void touch_target(int step, int W, int H, int &x, int &y) {
  if (step == 1) { x = 90; y = STRIP + 90; } else { x = W - 90; y = H - 90; }
}
inline void page_touch_setup(Display &it) {
  const int W = it.get_width(), H = it.get_height();
  strip(it, "TOUCH SETUP", false, nullptr);
  int x, y;
  touch_target(S.touch_setup, W, H, x, y);
  it.filled_circle(x, y, 40, INK);
  it.filled_circle(x, y, 26, PAPER);
  it.filled_circle(x, y, 12, INK);
  it.print(W / 2, H / 2 - 30, F.big, INK, TextAlign::CENTER, "Tap the dot");
  it.print(W / 2, H / 2 + 34, F.txt, INK, TextAlign::CENTER, S.touch_setup == 1 ? "1 of 2" : "2 of 2");
}

// ---------- touch test (15 dots: corners, edges and middle; 5 x 3 or 3 x 5) ----------
static const int TT_PASS = 30;  // half the smallest tap target (64 px list rows), minus a little
static const int TT_N = 15;
inline void tt_grid(int W, int H, int &cols, int &rows) { cols = W > H ? 5 : 3; rows = W > H ? 3 : 5; }
inline int tt_row_y(int r, int rows, int H) { return STRIP + 40 + r * (H - 40 - (STRIP + 40)) / (rows - 1); }
inline void test_target(int i, int W, int H, int &x, int &y) {
  int cols, rows;
  tt_grid(W, H, cols, rows);
  x = 40 + (i % cols) * (W - 80) / (cols - 1);
  y = tt_row_y(i / cols, rows, H);
}
// middle of the gap below row r (text goes there so it never covers a dot)
inline int tt_gap_y(int r, int rows, int H) { return (tt_row_y(r, rows, H) + tt_row_y(r + 1, rows, H)) / 2; }
inline void page_touch_test(Display &it) {
  const int W = it.get_width(), H = it.get_height();
  int cols, rows;
  tt_grid(W, H, cols, rows);
  strip(it, "TOUCH TEST", false, nullptr);
  char b[64];
  if (S.touch_test <= TT_N) {
    int i = S.touch_test - 1, x, y;
    test_target(i, W, H, x, y);
    it.circle(x, y, 22, INK);
    it.line(x - 30, y, x + 30, y, INK);
    it.line(x, y - 30, x, y + 30, INK);
    it.filled_circle(x, y, 4, INK);
    int ty = tt_gap_y(i / cols <= 1 ? rows - 2 : 0, rows, H);
    snprintf(b, sizeof(b), "Tap the centre  %d of %d", S.touch_test, TT_N);
    print_fit(it, W / 2, ty, W - 40, TextAlign::CENTER, INK, b, {F.txtb, F.txt});
    if (S.tt_drawn != S.touch_test) S.tt_shown_at = esphome::millis();
    S.tt_drawn = S.touch_test;
    return;
  }
  int worst = 0, sum = 0;
  for (int i = 0; i < TT_N; i++) {
    int x, y;
    test_target(i, W, H, x, y);
    worst = std::max(worst, S.tt_miss[i]);
    sum += S.tt_miss[i];
    bool bad = S.tt_miss[i] > TT_PASS;
    if (bad) it.filled_circle(x, y, 22, INK); else it.circle(x, y, 22, INK);
    snprintf(b, sizeof(b), "%d", S.tt_miss[i]);
    it.print(x, y, F.txtb, bad ? PAPER : INK, TextAlign::CENTER, b);
  }
  snprintf(b, sizeof(b), "Average %d px, worst %d px", (sum + TT_N / 2) / TT_N, worst);
  print_fit(it, W / 2, tt_gap_y(0, rows, H), W - 40, TextAlign::CENTER, INK, b, {F.txtb, F.txt});
  print_fit(it, W / 2, tt_gap_y(1, rows, H), W - 40, TextAlign::CENTER, INK,
            worst <= TT_PASS ? "PASS: every tap close enough" : "Too far: black dots", {F.txt, F.sm});
  S.tt_drawn = S.touch_test;
}

// ---------- entry point ----------
inline void draw(Display &it, esphome::ESPTime now, esphome::qr_code::QrCode *qr, const char *guest_ssid) {
  INK = S.dark ? esphome::display::COLOR_ON : esphome::display::COLOR_OFF;
  PAPER = S.dark ? esphome::display::COLOR_OFF : esphome::display::COLOR_ON;
  it.fill(PAPER);
  char clk[8];
  if (now.is_valid()) snprintf(clk, sizeof(clk), "%d:%02d", now.hour, now.minute); else snprintf(clk, sizeof(clk), "--:--");
  S.dots_drawn = false;
  if (S.touch_setup) page_touch_setup(it);
  else if (S.touch_test) page_touch_test(it);
  else if (S.alert) alert_card(it, clk);
  else if (S.do_mode) draw_do(it, clk, now, qr, guest_ssid);
  else {
    // page dots: shown for DOTS_MS after the Glance page changes (not after a boot or wake)
    uint32_t ms = esphome::millis();
    if (S.dots_page < 0) S.dots_page = S.page;
    else if (S.dots_page != S.page) { S.dots_page = S.page; S.dots_ms = ms; }
    S.dots_drawn = S.dots_ms && ms - S.dots_ms < DOTS_MS;
    draw_page(it, S.page, clk, now, qr, guest_ssid);
  }
  if (S.asleep) asleep_bar(it);
}

// ---------- buttons and taps ----------
inline int shown_page() { return (S.do_mode && S.do_screen == D_PAGE) ? S.do_page : S.page; }
inline void open_do(int screen) { S.do_mode = true; S.do_screen = screen; }
inline void do_back() {
  switch (S.do_screen) {
    case D_ROOM: case D_DOOR: case D_VAC: case D_DISH: S.do_screen = D_HOME; break;
    case D_DW1: S.do_screen = D_DISH; break;
    case D_VW1: S.do_screen = D_VAC; break;
    case D_VW2: S.do_screen = D_VW1; break;
    case D_VW3: S.do_screen = D_VW2; break;
    case D_DW2: S.do_screen = D_DW1; break;
    case D_DW3: S.do_screen = D_DW2; break;
    default: S.do_screen = D_TILES; break;
  }
}
inline void open_tile(int i) {
  if (TILES[i].page < 0) S.do_screen = D_HOME;
  else { S.do_screen = D_PAGE; S.do_page = TILES[i].page; }
}
// Up/Down while on the Do side: step through the tile apps
inline void do_step(int dir) {
  int cur = 5;  // Home
  if (S.do_screen == D_PAGE) for (int i = 0; i < 8; i++) if (TILES[i].page == S.do_page) cur = i;
  if (S.do_screen == D_TILES) cur = dir > 0 ? -1 : 8;
  open_tile(((cur + dir) % 8 + 8) % 8);
}
// Short presses of the AI and Up/Down buttons
inline void press_ai() {
  // WHY: one press clears an alert; otherwise it flips between Glance (look) and Do (tiles)
  if (S.alert) S.alert = false;
  else if (S.do_mode) S.do_mode = false;
  else open_do(D_TILES);
}
inline void press_step(int dir) {
  if (S.alert) S.alert = false;
  else if (S.do_mode) do_step(dir);
  else S.page = next_page(S.page, dir);
}
inline std::string page_label() {
  if (S.alert) return "Alert";
  if (!S.do_mode) return PAGE_NAMES[S.page];
  if (S.do_screen == D_PAGE) return std::string("Do: ") + PAGE_NAMES[S.do_page];
  return std::string("Do: ") + DO_NAMES[S.do_screen];
}

// Result of a tap, carried out by sticky.yaml (it can call Home Assistant)
// kind: 0 none, 1 redraw, 3 media(value), 4 timer(value), 5 clear alert, 6 Home Assistant "Sticky: do" (what, target, val)
struct Tap { int kind = 0; int value = 0; std::string what, target, val; };
inline Tap ha(const char *what, const std::string &target = "", const std::string &val = "") {
  Tap t;
  t.kind = 6; t.what = what; t.target = target; t.val = val;
  return t;
}
// taps that work on a page wherever it is shown (Glance, or inside Do)
inline bool page_tap(int page, int x, int y, int W, int H, Tap &t) {
  if (page == P_TIMER)
    for (auto &p : timer_pads(W, H)) if (p.first.hit(x, y)) { t.kind = 4; t.value = p.second; return true; }
  if (page == P_PLAY)
    for (auto &p : play_pads(W, H)) if (p.first.hit(x, y)) { t.kind = 3; t.value = p.second; return true; }
  if (page == P_REM) {
    auto items = reminder_items();
    auto rs = rem_rects(W, H, rem_shown(H, (int) items.size()));
    int row = -1;
    for (int k = 0; k < (int) rs.size(); k++) if (rs[k].hit(x, y)) row = k;
    if (row >= 0) {
      t = ha("rem_done", "", items[row]);
      // WHY: take it off the screen now; Home Assistant confirms within a few seconds
      items.erase(items.begin() + row);
      std::string s;
      for (auto &r : items) s += (s.empty() ? "" : "|") + r;
      S.reminders = s.empty() ? "none" : s;
      return true;
    }
  }
  return false;
}
inline Tap on_tap_do(int x, int y, int W, int H) {
  Tap t;
  if (y < STRIP + 10) {
    if (S.do_screen != D_TILES && x < W / 2) { do_back(); t.kind = 1; }
    return t;
  }
  const int i = S.room;
  char b[16];
  switch (S.do_screen) {
    case D_TILES: {
      auto rs = tile_rects(W, H);
      for (int k = 0; k < 8; k++) if (rs[k].hit(x, y)) { open_tile(k); t.kind = 1; }
    } break;
    case D_HOME:
      for (auto &p : home_rects(W, H)) {
        if (!p.first.hit(x, y)) continue;
        int id = p.second;
        if (id < 6) { S.room = id; S.do_screen = D_ROOM; t.kind = 1; }
        else if (id == 10) t = ha("all_off");
        else if (id == 11) {
          if (S.lock_state == "locked") { S.do_screen = D_DOOR; t.kind = 1; }
          else t = ha("lock");
        } else if (id == 12) { S.do_screen = D_VAC; t.kind = 1; }
        else if (id == 13) { S.do_screen = D_DISH; t.kind = 1; }
        break;
      }
      break;
    case D_ROOM:
      for (auto &p : room_rects(W, H)) {
        if (!p.first.hit(x, y)) continue;
        int id = p.second;
        if (id == 1 || id == 2) {
          float cur = std::isnan(S.ac_set[i]) ? 24.f : S.ac_set[i];
          float nt = std::max(16.f, std::min(30.f, std::round(cur) + (id == 1 ? -1.f : 1.f)));
          S.ac_set[i] = nt;  // shown at once, sent to the AC by Home Assistant
          snprintf(b, sizeof(b), "%.1f", nt);
          t = ha("ac_temp", CFG.ac_id[i], b);
        } else if (id == 3) t = ha("ac_power", CFG.ac_id[i]);
        else if (id >= 10 && id < 14) { S.ac_state[i] = AC_MODES[id - 10][0]; t = ha("ac_mode", CFG.ac_id[i], AC_MODES[id - 10][0]); }
        else if (id >= 20) { S.ac_fan[i] = AC_FANS[id - 20][0]; t = ha("ac_fan", CFG.ac_id[i], AC_FANS[id - 20][0]); }
        break;
      }
      break;
    case D_DOOR: {
      auto rs = door_rects(W, H);
      if (rs[0].first.hit(x, y)) { S.do_screen = D_HOME; t.kind = 1; }
      else if (rs[1].first.hit(x, y)) { S.do_screen = D_HOME; t = ha("unlock"); }
    } break;
    case D_VAC: {
      auto bs = vac_btns(W, H);
      if (bs[0].hit(x, y)) t = ha("vac_start");
      else if (bs[1].hit(x, y)) { S.do_screen = D_VW1; t.kind = 1; }
      else if (bs[2].hit(x, y)) t = ha("vac_dock");
      else if (bs[3].hit(x, y)) t = ha("vac_custom");
    } break;
    case D_VW1: {
      auto rs = vw_rects(W, H, 4);
      for (int k = 0; k < 4; k++) if (rs[k].hit(x, y)) { S.vwiz[0] = k; S.do_screen = D_VW2; t.kind = 1; }
    } break;
    case D_VW2: {
      auto su = vw2_rects(W, H, 0), mo = vw2_rects(W, H, 1);
      for (int k = 0; k < 4; k++) if (su[k].hit(x, y)) { S.vwiz[1] = k; t.kind = 1; }
      for (int k = 0; k < 3; k++) if (mo[k].hit(x, y)) { S.vwiz[2] = k; t.kind = 1; }
      if (next_rect(W, H).hit(x, y)) { S.do_screen = D_VW3; t.kind = 1; }
    } break;
    case D_VW3: {
      auto rs = vw_rects(W, H, 4);
      for (int k = 0; k < 4; k++)
        if (rs[k].hit(x, y)) {
          S.vwiz[3] = k;
          S.do_screen = D_VAC;
          t = ha("vac_start_custom", "", std::string(VAC_MODE[S.vwiz[0]][0]) + "|" + VAC_SUCTION[S.vwiz[1]][0] + "|" +
                                          VAC_MOP[S.vwiz[2]][0] + "|" + VAC_ROUTE[k][0]);
        }
    } break;
    case D_DISH: {
      auto bs = three_btns(W, H);
      if (bs[0].hit(x, y)) t = ha("dish_start", "", CFG.dish_usual + "|now");
      else if (bs[1].hit(x, y)) {
        if (S.wiz_prog.empty()) S.wiz_prog = S.dish_selected.empty() ? DISH_PROGS[0][0] : S.dish_selected;
        S.do_screen = D_DW1; t.kind = 1;
      } else if (bs[2].hit(x, y)) t = ha("dish_stop");
    } break;
    case D_DW1: {
      auto rs = dw_prog_rects(W, H);
      for (int k = 0; k < 9; k++)
        if (rs[k].hit(x, y)) {
          S.wiz_prog = DISH_PROGS[k][0];
          S.do_screen = D_DW2;
          // WHY: the dishwasher only offers the options that fit the selected program, so select it first
          t = ha("dish_select", "", S.wiz_prog);
        }
    } break;
    case D_DW2: {
      auto rs = dw_opt_rects(W, H);
      for (int k = 0; k < 5; k++)
        if (rs[k].hit(x, y) && (S.dish_opt[k] == "on" || S.dish_opt[k] == "off")) {
          S.dish_opt[k] = S.dish_opt[k] == "on" ? "off" : "on";
          t = ha("dish_option", DISH_OPTS[k][0], S.dish_opt[k]);
        }
      if (next_rect(W, H).hit(x, y)) { S.do_screen = D_DW3; t.kind = 1; }
    } break;
    case D_DW3: {
      auto rs = dw_when_rects(W, H);
      for (int k = 0; k < 4; k++)
        if (rs[k].hit(x, y)) {
          S.do_screen = D_DISH;
          t = ha("dish_start", "", (S.wiz_prog.empty() ? std::string(DISH_PROGS[0][0]) : S.wiz_prog) + "|" + DISH_WHEN[k][0]);
        }
    } break;
    case D_PAGE: page_tap(S.do_page, x, y, W, H, t); break;
  }
  return t;
}
inline Tap on_tap(int x, int y, int W, int H) {
  Tap t;
  if (S.alert) { S.alert = false; t.kind = 5; return t; }
  if (S.do_mode) return on_tap_do(x, y, W, H);
  // Glance: the door and AC icons on the strip open Home controls
  if (y < STRIP + 10) {
    if (x > W / 2) { open_do(D_HOME); t.kind = 1; }
    return t;
  }
  if (page_tap(S.page, x, y, W, H, t)) return t;
  // the dishwasher bar on Today opens the dishwasher
  bool dish_bar = S.dish_state == "run" || S.dish_state == "delayedstart";
  if (S.page == P_TODAY && dish_bar && today_dish_rect(W, H).hit(x, y)) {
    open_do(D_DISH); t.kind = 1; return t;
  }
  // elsewhere: left third = previous page, right third = next page
  if (x < W / 3) { S.page = next_page(S.page, -1); t.kind = 1; }
  else if (x > 2 * W / 3) { S.page = next_page(S.page, +1); t.kind = 1; }
  return t;
}

// ---------- touch maths ----------
// The touch chip reports raw x 0..479, y 0..799. The screen memory is 800 x 480 whatever way up it is
// drawn. A touch "map" (0-7) says how raw points land in screen memory: 4 = axes swapped,
// 1 = memory x reversed, 2 = memory y reversed. Touch setup finds it with two taps.
static const int TOUCH_MAP_DEFAULT = 5;
inline void raw_to_mem(int map, int rx, int ry, int &mx, int &my) {
  if (map & 4) { mx = ry; my = rx; } else { mx = rx * 800 / 480; my = ry * 480 / 800; }
  mx = std::max(0, std::min(799, mx));
  my = std::max(0, std::min(479, my));
  if (map & 1) mx = 799 - mx;
  if (map & 2) my = 479 - my;
}
// Inverse of ESPHome epaper_spi's rotation for this model (its base transform is "mirror x")
inline void mem_to_screen(int rot, int mx, int my, int &x, int &y) {
  switch (rot) {
    case 90: x = my; y = mx; break;
    case 180: x = mx; y = 479 - my; break;
    case 270: x = 479 - my; y = 799 - mx; break;
    default: x = 799 - mx; y = my; break;
  }
}
inline void touch_to_screen(int map, int rot, int rx, int ry, int &x, int &y) {
  int mx, my;
  raw_to_mem(map, rx, ry, mx, my);
  mem_to_screen(rot, mx, my, x, y);
}
// Pick the map that puts both taps nearest their dots. Returns the map, err = worst miss in pixels.
inline int calibrate_touch(int rot, const int rx[2], const int ry[2], int W, int H, int &err) {
  int best = TOUCH_MAP_DEFAULT;
  err = 1 << 30;
  for (int m = 0; m < 8; m++) {
    int worst = 0;
    for (int i = 0; i < 2; i++) {
      int tx, ty, x, y;
      touch_target(i + 1, W, H, tx, ty);
      touch_to_screen(m, rot, rx[i], ry[i], x, y);
      worst = std::max(worst, std::abs(x - tx) + std::abs(y - ty));
    }
    if (worst < err) { err = worst; best = m; }
  }
  return best;
}

// ---------- which way up ----------
// imu: the motion sensor's angle (0/90/180/270). It turns the opposite way to the screen.
// flip: 0 none, 1 portrait, 2 landscape, 3 both (adds 180 degrees to fix an upside-down picture)
inline int rot_from_motion(int imu) { return (360 - (((imu % 360) + 360) % 360)) % 360; }
inline int apply_flip(int rot, int flip) {
  bool portrait = rot == 90 || rot == 270;
  if ((portrait && (flip & 1)) || (!portrait && (flip & 2))) rot = (rot + 180) % 360;
  return rot;
}

// ---------- keep the screen's data through deep sleep ----------
struct Saved {
  uint32_t magic;
  int16_t page, rot, base_rot;
  uint8_t alert, on_charger, watering;
  float nums[10];
  char text[1900];
  int16_t gh_total, gh_days, gh_first_dow, gh_doy;
  uint8_t gh_count[371], gh_level[371];
  int64_t wx_at, gh_at;
};
STICKY_RTC static Saved SAVED;
static const uint32_t SAVED_MAGIC = 0x5717C0DE;
inline std::string *saved_strings(int i) {
  std::string *v[] = {&S.lock_state, &S.ac_state[0], &S.ac_state[1], &S.ac_state[2], &S.ac_state[3],
                      &S.ac_state[4], &S.ac_state[5], &S.vacuum, &S.wx_cond, &S.forecast, &S.agenda,
                      &S.dish_state, &S.wash_state, &S.dish_finish, &S.wash_finish, &S.tv_state, &S.tv_app,
                      &S.tv_title, &S.sb_state, &S.alert_kind, &S.alert_title, &S.alert_detail, &S.reminders,
                      &S.dish_door, &S.dish_selected, &S.vac_last, &S.hourly, &S.birth};
  return i < (int) (sizeof(v) / sizeof(v[0])) ? v[i] : nullptr;
}
inline float *saved_floats(int i) {
  float *v[] = {&S.room_t, &S.room_h, &S.batt, &S.wx_t, &S.wx_h, &S.wx_wind, &S.dish_prog, &S.wash_prog, &S.tv_vol, &S.sb_vol};
  return v[i];
}
inline void save_state(int rot, int base_rot) {
  SAVED.magic = 0;
  SAVED.page = S.page;
  SAVED.rot = rot;
  SAVED.base_rot = base_rot;
  SAVED.alert = S.alert;
  SAVED.on_charger = S.on_charger;
  SAVED.watering = S.watering;
  for (int i = 0; i < 10; i++) SAVED.nums[i] = *saved_floats(i);
  SAVED.gh_total = S.gh_total; SAVED.gh_days = S.gh_days; SAVED.gh_first_dow = S.gh_first_dow; SAVED.gh_doy = S.gh_doy;
  SAVED.wx_at = S.wx_at; SAVED.gh_at = S.gh_at;
  memcpy(SAVED.gh_count, S.gh_count, sizeof(S.gh_count));
  memcpy(SAVED.gh_level, S.gh_level, sizeof(S.gh_level));
  size_t n = 0;
  for (int i = 0; saved_strings(i); i++) {
    const std::string &v = *saved_strings(i);
    size_t len = std::min(v.size(), sizeof(SAVED.text) - n - 1);
    memcpy(SAVED.text + n, v.data(), len);
    n += len;
    if (n < sizeof(SAVED.text) - 1) SAVED.text[n++] = '\x1f';
  }
  SAVED.text[n] = 0;
  SAVED.magic = SAVED_MAGIC;
}
// true when there was something to restore (a wake from deep sleep)
inline bool restore_state(int &rot, int &base_rot) {
  if (SAVED.magic != SAVED_MAGIC) return false;
  SAVED.magic = 0;
  S.page = std::max(0, std::min((int) P_COUNT - 1, (int) SAVED.page));
  rot = SAVED.rot;
  base_rot = SAVED.base_rot;
  S.alert = SAVED.alert;
  S.on_charger = SAVED.on_charger;
  S.watering = SAVED.watering;
  for (int i = 0; i < 10; i++) *saved_floats(i) = SAVED.nums[i];
  S.gh_total = SAVED.gh_total; S.gh_days = SAVED.gh_days; S.gh_first_dow = SAVED.gh_first_dow; S.gh_doy = SAVED.gh_doy;
  S.wx_at = SAVED.wx_at; S.gh_at = SAVED.gh_at;
  memcpy(S.gh_count, SAVED.gh_count, sizeof(S.gh_count));
  memcpy(S.gh_level, SAVED.gh_level, sizeof(S.gh_level));
  auto parts = split(std::string(SAVED.text), '\x1f');
  for (int i = 0; saved_strings(i) && i < (int) parts.size(); i++) *saved_strings(i) = parts[i];
  return true;
}

}  // namespace sticky
