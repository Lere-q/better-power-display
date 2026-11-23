#include "better_power_display.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace esphome::better_power_display {

using tessera::Font;
namespace ui = tessera::ui;

namespace {

// ---- Nodes: the circles. Lanes: the lines between them. ----
enum Node : int { N_GRID = 0, N_SOLAR, N_BATT, N_HOME, N_DEV0 };
// The lanes of the sources among themselves and to the home; then one lane per device, from the device to the home.
enum Lane : int { L_GH = 0, L_SH, L_BH, L_SG, L_BG, L_SB, L_DEV0 };
constexpr int MAX_DEVICES = 8;
constexpr int NODES = N_DEV0 + MAX_DEVICES;
constexpr int LANES = L_DEV0 + MAX_DEVICES;
constexpr int MAX_PTS = 14;   // points of one lane, for the dots
constexpr int ARC_STEPS = 6;  // a corner is drawn as an arc; the dots follow it in 6 short steps
constexpr int DOTS = 3;       // dots on one lane at most

// Icons of Tessera's set (Material Design Icons codepoints).
constexpr uint32_t ICON_HOME = 0xF02DC, ICON_GRID = 0xF0D3E, ICON_SOLAR = 0xF0A72, ICON_PLUG = 0xF06A5;
constexpr uint32_t ICON_BATTERY_FULL = 0xF0079, ICON_BATTERY_10 = 0xF007A;  // battery-10 ... battery-90 follow

// ---- Colours: a palette made from the theme's own roles, so it follows the light and the dark look. ----
enum Swatch : int { GREEN, AZURE, BLUE, PURPLE, PINK, RED, ORANGE, AMBER, TEAL, GRAY, WHITE, SWATCHES };
const char *const SWATCH_NAME[SWATCHES] = {"green", "azure", "blue", "purple", "pink", "red",
                                           "orange", "amber", "teal", "gray", "white"};
// What a device gets when the person picked no colour for it.
const int DEVICE_AUTO[MAX_DEVICES] = {AMBER, PURPLE, TEAL, RED, GREEN, ORANGE, PINK, BLUE};

int swatch_of(const char *name, int fallback) {
  for (int i = 0; i < SWATCHES; ++i)
    if (name && strcmp(name, SWATCH_NAME[i]) == 0) return i;
  return fallback;
}

lv_color_t swatch_color(int s) {
  const lv_color_t green = ui::color(theme::MARK_GREEN), azure = ui::color(theme::MARK_BLUE);
  const lv_color_t purple = ui::color(theme::MARK_PURPLE), amber = ui::color(theme::MARK_AMBER);
  const lv_color_t red = ui::tone_color(tessera::Tone::ALERT, theme::INK);
  switch (s) {
    case GREEN: return green;
    case AZURE: return azure;
    case BLUE: return lv_color_mix(azure, purple, 150);
    case PURPLE: return purple;
    case PINK: return lv_color_mix(red, purple, 130);
    case RED: return red;
    case ORANGE: return lv_color_mix(amber, red, 170);
    case AMBER: return amber;
    case TEAL: return lv_color_mix(green, azure, 120);
    case GRAY: return ui::color(theme::MUTED);
    default: return ui::color(theme::INK);
  }
}

// ---- Small helpers ----
inline int lh(Font f) { return ui::line_height(f); }
inline void show(lv_obj_t *o, bool on) {
  if (!o) return;
  if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
inline void inert(lv_obj_t *o) {
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}
void set_bg(lv_obj_t *o, lv_color_t c) {
  if (o && !lv_color_eq(lv_obj_get_style_bg_color(o, LV_PART_MAIN), c)) lv_obj_set_style_bg_color(o, c, 0);
}
void set_border(lv_obj_t *o, lv_color_t c) {
  if (o && !lv_color_eq(lv_obj_get_style_border_color(o, LV_PART_MAIN), c)) lv_obj_set_style_border_color(o, c, 0);
}
void set_arc_color(lv_obj_t *o, lv_color_t c) {
  if (o && !lv_color_eq(lv_obj_get_style_arc_color(o, LV_PART_MAIN), c)) lv_obj_set_style_arc_color(o, c, LV_PART_MAIN);
}
void set_text_color(lv_obj_t *o, lv_color_t c) {
  if (o && !lv_color_eq(lv_obj_get_style_text_color(o, LV_PART_MAIN), c)) lv_obj_set_style_text_color(o, c, 0);
}
// An arc that is only its background track: clockwise from `from` degrees (0 = 3 o'clock) over `span` degrees.
void set_arc_span(lv_obj_t *a, float from, float span) {
  const int s = ((static_cast<int>(lroundf(from)) % 360) + 360) % 360;
  const int len = std::clamp(static_cast<int>(lroundf(span)), 1, 360);
  int e = (s + len) % 360;
  if (e == s) e = s + 360;
  lv_arc_set_bg_angles(a, s, e);
}
lv_obj_t *make_arc(lv_obj_t *parent, int size, int width) {
  lv_obj_t *a = lv_arc_create(parent);
  lv_obj_remove_style_all(a);
  inert(a);
  lv_obj_set_size(a, size, size);
  lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
  return a;
}
lv_obj_t *make_block(lv_obj_t *parent, int x, int y, int w, int h, int radius) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  inert(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, std::max(1, w), std::max(1, h));
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  return o;
}

// ---- Readings: what Home Assistant says about one chosen sensor ----
struct Reading {
  bool set = false;  // the entity came with the tile's data
  bool ok = false;   // and it has a number
  float w = 0;       // in watts (or the plain number for a battery level)
  char name[28] = "";
};

// The unit a power sensor reports: W, kW, MW (and mW). A sensor with no unit is taken as watts.
float unit_factor(const char *u) {
  if (!u || !*u) return 1.0f;
  if (u[0] == 'k' || u[0] == 'K') return 1000.0f;
  if (u[0] == 'M') return 1e6f;
  if (u[0] == 'G') return 1e9f;
  if (u[0] == 'm' && u[1] == 'W') return 0.001f;
  return 1.0f;
}

Reading read_entity(JsonObjectConst entities, const char *key, bool plain_number) {
  Reading r;
  JsonObjectConst e = entities[key].as<JsonObjectConst>();
  if (e.isNull()) return r;
  r.set = true;
  const char *state = e["state"] | "";
  char *end = nullptr;
  const float v = strtof(state, &end);
  if (end != state && std::isfinite(v)) {
    r.ok = true;
    r.w = plain_number ? v : v * unit_factor(e["attributes"]["unit_of_measurement"] | "");
  }
  const char *name = e["name"] | "";
  strncpy(r.name, name, sizeof(r.name) - 1);
  r.name[sizeof(r.name) - 1] = 0;
  return r;
}

bool chosen(JsonObjectConst options, const char *key) {
  const char *s = options[key] | "";
  return s[0] != 0;
}

void copy_text(char *to, size_t n, const char *from) {
  strncpy(to, from ? from : "", n - 1);
  to[n - 1] = 0;
}

// "238 W", "1.70 kW", "12.3 kW"
void format_power(char *out, size_t n, float w) {
  const float a = fabsf(w);
  if (a >= 1000.0f) {
    const float k = a / 1000.0f;
    snprintf(out, n, k < 10.0f ? "%.2f kW" : "%.1f kW", k);
  } else {
    snprintf(out, n, "%ld W", lroundf(a));
  }
}

uint32_t battery_icon(float level) {
  if (level >= 95.0f) return ICON_BATTERY_FULL;
  const int step = std::clamp(static_cast<int>(level / 10.0f), 1, 9);  // battery-10 ... battery-90
  return ICON_BATTERY_10 + static_cast<uint32_t>(step - 1);
}

}  // namespace

// ======================================================================================================
// The tile
// ======================================================================================================
class PowerTile : public tessera::Tile {
 public:
  explicit PowerTile(BetterPowerDisplay *plugin) : plugin_(plugin) {}
  ~PowerTile() override {
    if (clock_) lv_anim_delete(clock_, nullptr);
    plugin_->detach(this);
  }

  void create(const tessera::TileContext &c) override;
  void on_state(JsonObjectConst data) override;
  void on_theme() override { paint(); }

  // From the plugin: the screen goes dark or wakes. Dots stop while nobody looks.
  void set_paused(bool paused) {
    paused_ = paused;
    update_anim();
  }

 private:
  // One circle: a ring, and up to four labels in it or beside it.
  struct NodeUI {
    bool used = false;
    int cx = 0, cy = 0;
    lv_obj_t *ring = nullptr;  // a circle's border
    lv_obj_t *arc[3] = {};     // the home's ring: the shares of grid, solar, battery
    lv_obj_t *top = nullptr, *icon = nullptr, *value = nullptr, *name = nullptr;
    uint8_t swatch = AZURE;
  };
  // One lane between two circles, as pieces (blocks and an arc) and the path the dots follow.
  struct Lane {
    bool used = false;
    lv_obj_t *piece[3] = {};
    bool is_arc[3] = {};
    uint8_t pieces = 0;
    lv_obj_t *dot[DOTS] = {};
    uint8_t ndots = 1;
    float px[MAX_PTS] = {}, py[MAX_PTS] = {}, cum[MAX_PTS] = {};
    uint8_t np = 0;
    float total = 0;
    int8_t pos_node = -1, neg_node = -1;  // whose colour the lane has when it flows along its path, and against it
    int8_t dir = 0;                       // +1 along the path (from its first point), -1 against it, 0 nothing flows
    float phase = 0;                      // where the first dot is, 0 to 1
    float speed = 0;                      // pixels per millisecond
    bool dots_on = false;
    int dot_x[DOTS] = {}, dot_y[DOTS] = {};
  };

  void make_node(int n, int cx, int cy, int d, int bw, bool with_name, bool name_above, int name_width, uint32_t icon,
                 bool want_top);
  void add_rect(Lane &L, float x0, float y0, float x1, float y1);
  void add_arc(Lane &L, float cx, float cy, float r, int a0, int a1);
  void add_pt(Lane &L, float x, float y) {
    if (L.np < MAX_PTS) {
      L.px[L.np] = x;
      L.py[L.np] = y;
      ++L.np;
    }
  }
  void add_arc_pts(Lane &L, float cx, float cy, float r, float from_deg, float to_deg);
  void finish_path(Lane &L, bool reverse, int pos_node, int neg_node);
  void paint();
  void set_lane(int lane, float amount);
  void update_anim();
  void step();
  static void clock_cb(lv_anim_t *a, int32_t) { static_cast<PowerTile *>(lv_anim_get_user_data(a))->step(); }
  static void point_at(const Lane &L, float s, float &x, float &y);

  BetterPowerDisplay *plugin_;
  lv_obj_t *parent_ = nullptr;
  lv_obj_t *hint_ = nullptr;
  lv_obj_t *clock_ = nullptr;  // an invisible part that carries the one animation that moves the dots
  int W_ = 0, H_ = 0, D_ = 0, lw_ = 2, dot_ = 5;
  bool built_ = false;

  // Options
  bool has_[NODES] = {};
  bool batt_pair_ = false;  // the battery has a sensor for charging beside the one for discharging
  bool batt_power_ = false;
  bool invert_grid_ = false, invert_batt_ = false;
  float flow_min_ = 10;
  bool animate_ = true;
  char name_[3][28] = {};                 // names of grid, solar, battery that the person typed
  char dev_label_[MAX_DEVICES][28] = {};  // names of the devices that the person typed, in the order of the devices
  int dev_option_[MAX_DEVICES] = {};      // which option (device_1 .. device_8) each chosen device is
  int dev_swatch_[MAX_DEVICES] = {};      // the colour of each chosen device
  int ndev_ = 0;                          // devices chosen
  int nslots_ = 0;                        // devices the tile has room for

  NodeUI node_[NODES];
  Lane lane_[LANES];
  bool paused_ = false, anim_on_ = false, any_flow_ = false;
  uint32_t last_tick_ = 0;

  // The last data
  Reading rd_[NODES];  // grid, solar, battery (discharge), and the devices in their slots
  Reading charge_;     // battery charge power, when the battery has two sensors
  Reading level_;      // battery level
  int slot_dev_[MAX_DEVICES] = {};
  int nshown_ = 0;
  char dev_name_[MAX_DEVICES][28] = {};
  float share_[3] = {};  // part of the home's power that comes from grid, solar, battery
  bool arc_on_[3] = {};
  float arc_from_[3] = {}, arc_span_[3] = {};
  bool painted_ = false;
  bool prev_ok_[NODES + 2] = {};
  float prev_w_[NODES + 2] = {};
};

// ---------------------------------------------------------------------------------------------------
// Pieces of a lane
// ---------------------------------------------------------------------------------------------------
void PowerTile::add_rect(Lane &L, float x0, float y0, float x1, float y1) {
  if (L.pieces >= 3) return;
  const int ext = 1;  // a pixel of overlap, so a join never shows a seam
  const float ax = std::min(x0, x1), bx = std::max(x0, x1), ay = std::min(y0, y1), by = std::max(y0, y1);
  int x, y, w, h;
  if (fabsf(y1 - y0) < 0.5f) {  // horizontal
    x = static_cast<int>(lroundf(ax)) - ext;
    w = static_cast<int>(lroundf(bx - ax)) + 2 * ext;
    y = static_cast<int>(lroundf(ay)) - lw_ / 2;
    h = lw_;
  } else {  // vertical
    x = static_cast<int>(lroundf(ax)) - lw_ / 2;
    w = lw_;
    y = static_cast<int>(lroundf(ay)) - ext;
    h = static_cast<int>(lroundf(by - ay)) + 2 * ext;
  }
  L.piece[L.pieces] = make_block(parent_, x, y, w, h, 0);
  L.is_arc[L.pieces] = false;
  ++L.pieces;
}

// A quarter circle whose centre line has radius r around (cx, cy); a0 to a1 clockwise, 0 degrees at 3 o'clock.
void PowerTile::add_arc(Lane &L, float cx, float cy, float r, int a0, int a1) {
  if (L.pieces >= 3) return;
  const int size = static_cast<int>(lroundf(2 * r)) + lw_;
  lv_obj_t *a = make_arc(parent_, size, lw_);
  lv_obj_set_pos(a, static_cast<int>(lroundf(cx)) - size / 2, static_cast<int>(lroundf(cy)) - size / 2);
  set_arc_span(a, static_cast<float>(a0), static_cast<float>(a1 - a0 <= 0 ? a1 - a0 + 360 : a1 - a0));
  L.piece[L.pieces] = a;
  L.is_arc[L.pieces] = true;
  ++L.pieces;
}

void PowerTile::add_arc_pts(Lane &L, float cx, float cy, float r, float from_deg, float to_deg) {
  for (int i = 0; i <= ARC_STEPS; ++i) {
    const float deg = from_deg + (to_deg - from_deg) * static_cast<float>(i) / ARC_STEPS;
    const float rad = deg * 3.14159265f / 180.0f;
    add_pt(L, cx + r * cosf(rad), cy + r * sinf(rad));
  }
}

// The path is built from its first circle to its second, or the other way round and turned here.
void PowerTile::finish_path(Lane &L, bool reverse, int pos_node, int neg_node) {
  if (reverse) {
    for (int i = 0, j = L.np - 1; i < j; ++i, --j) {
      std::swap(L.px[i], L.px[j]);
      std::swap(L.py[i], L.py[j]);
    }
  }
  L.cum[0] = 0;
  for (int i = 1; i < L.np; ++i) {
    const float dx = L.px[i] - L.px[i - 1], dy = L.py[i] - L.py[i - 1];
    L.cum[i] = L.cum[i - 1] + sqrtf(dx * dx + dy * dy);
  }
  L.total = L.np ? L.cum[L.np - 1] : 0;
  L.used = L.np >= 2 && L.total > 1;
  L.pos_node = static_cast<int8_t>(pos_node);
  L.neg_node = static_cast<int8_t>(neg_node);
  L.ndots = static_cast<uint8_t>(std::clamp(static_cast<int>(lroundf(L.total / std::max(1.0f, D_ * 1.3f))), 1, DOTS));
}

void PowerTile::point_at(const Lane &L, float s, float &x, float &y) {
  x = L.px[0];
  y = L.py[0];
  for (int i = 1; i < L.np; ++i) {
    if (s <= L.cum[i] || i == L.np - 1) {
      const float seg = L.cum[i] - L.cum[i - 1];
      const float t = seg > 0 ? std::clamp((s - L.cum[i - 1]) / seg, 0.0f, 1.0f) : 0.0f;
      x = L.px[i - 1] + (L.px[i] - L.px[i - 1]) * t;
      y = L.py[i - 1] + (L.py[i] - L.py[i - 1]) * t;
      return;
    }
  }
}

// ---------------------------------------------------------------------------------------------------
// One circle and what is in it
// ---------------------------------------------------------------------------------------------------
void PowerTile::make_node(int n, int cx, int cy, int d, int bw, bool with_name, bool name_above, int name_width,
                          uint32_t icon, bool want_top) {
  NodeUI &N = node_[n];
  N.used = true;
  N.cx = cx;
  N.cy = cy;
  const int x = cx - d / 2, y = cy - d / 2;

  lv_obj_t *r = lv_obj_create(parent_);
  lv_obj_remove_style_all(r);
  inert(r);
  lv_obj_set_pos(r, x, y);
  lv_obj_set_size(r, d, d);
  lv_obj_set_style_radius(r, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(r, bw, 0);
  lv_obj_set_style_border_opa(r, LV_OPA_COVER, 0);
  N.ring = r;
  if (n == N_HOME) {  // over the ring: the part of the home's power that each source gives (hidden while one gives all)
    for (int i = 0; i < 3; ++i) {
      N.arc[i] = make_arc(parent_, d, bw);
      lv_obj_set_pos(N.arc[i], x, y);
      show(N.arc[i], false);
    }
  }

  // The words inside. The value is what matters, then the level above it (a battery), then the icon. The largest value
  // font that lets all of them fit; when none does, the icon goes first, then the level.
  const int inner = d - 2 * bw;
  const Font fonts[] = {Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE, Font::BODY};
  Font vf = Font::BODY, icf = Font::ICON;
  bool top_on = false, icon_on = false;
  bool found = false;
  for (int stage = 0; stage < 3 && !found; ++stage) {
    const bool top = want_top && stage < 2;
    const bool with_icon = stage != 1;
    for (Font f : fonts) {
      if (ui::text_width("1.70 kW", f) > inner * 84 / 100) continue;
      if (lh(f) > inner * 34 / 100) continue;
      for (int size = 0; size < (with_icon ? 2 : 1); ++size) {
        const Font ic = size == 0 ? Font::ICON : Font::ICON_SMALL;
        const int total = lh(f) + (with_icon ? lh(ic) : 0) + (top ? lh(Font::BODY) : 0);
        if (total > inner * 88 / 100) continue;
        vf = f;
        icf = ic;
        top_on = top;
        icon_on = with_icon;
        found = true;
        break;
      }
      if (found) break;
    }
  }
  int total = lh(vf) + (icon_on ? lh(icf) : 0) + (top_on ? lh(Font::BODY) : 0);

  auto line = [&](Font f, theme::Role role, int &y_cursor) {
    lv_obj_t *l = ui::label(parent_, f, role);
    lv_obj_set_width(l, inner);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, cx - inner / 2, y_cursor);
    y_cursor += lh(f);
    return l;
  };
  int yc = cy - total / 2;
  if (top_on) N.top = line(Font::BODY, theme::INK, yc);
  if (icon_on) {
    N.icon = line(icf, theme::INK, yc);
    ui::set_text(N.icon, ui::icon(icon));
  }
  N.value = line(vf, theme::INK, yc);

  if (with_name) {
    lv_obj_t *l = ui::label(parent_, Font::BODY, theme::MUTED);
    lv_obj_set_width(l, name_width);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    const int gap = std::max(2, ui::px(3));
    const int ny = name_above ? cy - d / 2 - lh(Font::BODY) - gap : cy + d / 2 + gap;
    lv_obj_set_pos(l, cx - name_width / 2, ny);
    N.name = l;
  }
}

// ---------------------------------------------------------------------------------------------------
// create(): the layout. It is worked out from the tile's size and from what is chosen, nothing else.
// ---------------------------------------------------------------------------------------------------
void PowerTile::create(const tessera::TileContext &c) {
  parent_ = c.parent;
  W_ = c.width;
  H_ = c.height;
  plugin_->attach(this);

  has_[N_GRID] = chosen(c.options, "grid");
  has_[N_SOLAR] = chosen(c.options, "solar");
  batt_power_ = chosen(c.options, "battery") || chosen(c.options, "battery_in");
  batt_pair_ = chosen(c.options, "battery") && chosen(c.options, "battery_in");
  has_[N_BATT] = batt_power_ || chosen(c.options, "battery_level");
  has_[N_HOME] = true;
  flow_min_ = c.options["flow_min"] | 10.0f;
  animate_ = c.options["animate"] | true;

  for (JsonVariantConst p : c.options["invert"].as<JsonArrayConst>()) {
    const char *v = p["value"] | "";
    if (strcmp(v, "grid") == 0) invert_grid_ = true;
    if (strcmp(v, "battery") == 0) invert_batt_ = true;
  }
  copy_text(name_[N_GRID], sizeof(name_[0]), c.options["name_grid"] | "");
  copy_text(name_[N_SOLAR], sizeof(name_[0]), c.options["name_solar"] | "");
  copy_text(name_[N_BATT], sizeof(name_[0]), c.options["name_battery"] | "");
  {
    int i = 0;
    for (JsonVariantConst t : c.options["device_names"].as<JsonArrayConst>()) {
      if (i >= MAX_DEVICES) break;
      copy_text(dev_label_[i++], sizeof(dev_label_[0]), t.as<const char *>());
    }
  }

  // Colours of the sources, then of each device: the person's pick, in order, or the next colour of the palette.
  node_[N_GRID].swatch = static_cast<uint8_t>(swatch_of(c.options["color_grid"] | "", BLUE));
  node_[N_SOLAR].swatch = static_cast<uint8_t>(swatch_of(c.options["color_solar"] | "", GREEN));
  node_[N_BATT].swatch = static_cast<uint8_t>(swatch_of(c.options["color_battery"] | "", ORANGE));
  node_[N_HOME].swatch = static_cast<uint8_t>(AZURE);
  int picks[MAX_DEVICES];
  int npicks = 0;
  for (JsonVariantConst p : c.options["device_colors"].as<JsonArrayConst>()) {
    if (npicks >= MAX_DEVICES) break;
    picks[npicks++] = swatch_of(p["value"] | "", -1);
  }
  ndev_ = 0;
  char key[12];
  for (int i = 0; i < MAX_DEVICES; ++i) {
    snprintf(key, sizeof(key), "device_%d", i + 1);
    if (!chosen(c.options, key)) continue;
    dev_option_[ndev_] = i;
    dev_swatch_[ndev_] = (ndev_ < npicks && picks[ndev_] >= 0) ? picks[ndev_] : DEVICE_AUTO[ndev_];
    ++ndev_;
  }

  const bool any_source = has_[N_GRID] || has_[N_SOLAR] || has_[N_BATT];
  if (!any_source && ndev_ == 0) {
    hint_ = ui::label(parent_, Font::BODY, theme::MUTED);
    lv_obj_set_width(hint_, W_);
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(hint_, 0, (H_ - lh(Font::BODY)) / 2);
    ui::set_text(hint_, plugin_->text("choose"));
    return;
  }

  lw_ = std::max(2, ui::px(2));
  dot_ = std::max(5, 2 * lw_ + 1);
  const int pad = std::max(2, ui::px(3));
  const bool has_mid = has_[N_SOLAR] || has_[N_BATT];
  const int hc = (has_[N_GRID] ? 1 : 0) + (has_mid ? 1 : 0);  // the column of the home

  // ---- How many devices fit, and how big the circles can be ----
  struct Geo {
    bool top, bottom, names;
    int rows, cols, cw, d, gap, top_pad, bot_pad;
  };
  auto geometry = [&](int nd, bool names) {
    Geo g{};
    g.names = names;
    g.top = has_[N_SOLAR] || nd >= 1;
    g.bottom = has_[N_BATT] || nd >= 2;
    g.rows = 1 + (g.top ? 1 : 0) + (g.bottom ? 1 : 0);
    g.cols = hc + std::max(1, (nd + 1) / 2);
    g.cw = W_ / g.cols;
    const int label = names ? lh(Font::BODY) : 0;
    auto gap_of = [&](int d) { return std::max(label + 2 * pad, d * 30 / 100); };
    auto need = [&](int d) {
      const int top = (g.top && names) ? label + pad : 0, bot = names ? label + pad : 0;
      return top + (g.rows - 1) * (d + gap_of(d)) + d + bot;
    };
    int d = std::min(g.cw * 72 / 100, H_);
    while (d > 4 && need(d) > H_) --d;
    g.d = d;
    g.gap = gap_of(d);
    g.top_pad = (g.top && names) ? label + pad : 0;
    g.bot_pad = names ? label + pad : 0;
    return g;
  };
  Geo g{};
  int nd = ndev_;
  for (;; --nd) {
    g = geometry(nd, true);
    if (g.d < ui::mm(9)) g = geometry(nd, false);
    if (g.d >= ui::mm(7) || nd == 0) break;
  }
  nslots_ = nd;
  D_ = g.d;
  const int D = D_;
  const int R = D / 2;
  const int bw = std::max(2, D / 16);
  const int need_h = g.top_pad + (g.rows - 1) * (D + g.gap) + D + g.bot_pad;
  const int y_first = (H_ - need_h) / 2 + g.top_pad;  // top of the first row of circles
  const int x_first = (W_ - g.cols * g.cw) / 2;
  auto cx_of = [&](int col) { return x_first + col * g.cw + g.cw / 2; };
  const int hr = g.top ? 1 : 0;  // the row of the home
  const int br = hr + 1;         // the row below it
  auto cy_of = [&](int row) { return y_first + row * (D + g.gap) + D / 2; };
  const int hx = cx_of(hc), hy = cy_of(hr);
  const int name_w = std::max(D, g.cw - pad);
  const int mid_col = has_[N_GRID] ? 1 : 0;
  const int gx = cx_of(0), mx = cx_of(mid_col);
  const int sy = cy_of(0), by = cy_of(br);

  // ---- The lanes first, so the circles lie over their ends ----
  // Beside each other at a circle (the lanes of one source leave it a little apart), and where they enter the home or
  // the grid (above and below the grid's own lane).
  const int o = std::max(2, R * 30 / 100);
  const int off = R * 35 / 100;
  const float edge_o = sqrtf(static_cast<float>(R * R - o * o));
  const float hxe = hx - sqrtf(static_cast<float>(R * R - off * off));   // the home's left edge at the offset
  const float gxe = gx + sqrtf(static_cast<float>(R * R - off * off));   // the grid's right edge at the offset
  const float ey_up = static_cast<float>(hy - off), ey_dn = static_cast<float>(hy + off);

  if (has_[N_GRID]) {  // grid to home
    Lane &L = lane_[L_GH];
    add_rect(L, gx + R, hy, hx - R, hy);
    add_pt(L, gx + R, hy);
    add_pt(L, hx - R, hy);
    finish_path(L, false, N_GRID, N_GRID);
  }
  if (has_[N_SOLAR]) {  // solar to home: down, then right
    const float x = static_cast<float>(mx + o);
    const float r = std::max(2.0f, std::min(ey_up - (sy + edge_o), hxe - x) * 0.7f);
    Lane &L = lane_[L_SH];
    add_rect(L, x, sy + edge_o, x, ey_up - r);
    add_arc(L, x + r, ey_up - r, r, 90, 180);
    add_rect(L, x + r, ey_up, hxe, ey_up);
    add_pt(L, x, sy + edge_o);
    add_pt(L, x, ey_up - r);
    add_arc_pts(L, x + r, ey_up - r, r, 180, 90);
    add_pt(L, hxe, ey_up);
    finish_path(L, false, N_SOLAR, N_SOLAR);
  }
  if (has_[N_BATT]) {  // battery to home: up, then right
    const float x = static_cast<float>(mx + o);
    const float r = std::max(2.0f, std::min((by - edge_o) - ey_dn, hxe - x) * 0.7f);
    Lane &L = lane_[L_BH];
    add_rect(L, x, by - edge_o, x, ey_dn + r);
    add_arc(L, x + r, ey_dn + r, r, 180, 270);
    add_rect(L, x + r, ey_dn, hxe, ey_dn);
    add_pt(L, x, by - edge_o);
    add_pt(L, x, ey_dn + r);
    add_arc_pts(L, x + r, ey_dn + r, r, 180, 270);
    add_pt(L, hxe, ey_dn);
    finish_path(L, false, N_BATT, N_BATT);
  }
  if (has_[N_SOLAR] && has_[N_GRID]) {  // solar to grid (feeding in): down, then left
    const float x = static_cast<float>(mx - o);
    const float r = std::max(2.0f, std::min(ey_up - (sy + edge_o), x - gxe) * 0.7f);
    Lane &L = lane_[L_SG];
    add_rect(L, x, sy + edge_o, x, ey_up - r);
    add_arc(L, x - r, ey_up - r, r, 0, 90);
    add_rect(L, x - r, ey_up, gxe, ey_up);
    add_pt(L, x, sy + edge_o);
    add_pt(L, x, ey_up - r);
    add_arc_pts(L, x - r, ey_up - r, r, 0, 90);
    add_pt(L, gxe, ey_up);
    finish_path(L, false, N_SOLAR, N_SOLAR);
  }
  if (has_[N_BATT] && has_[N_GRID]) {  // battery to grid, or grid to battery: up, then left
    const float x = static_cast<float>(mx - o);
    const float r = std::max(2.0f, std::min((by - edge_o) - ey_dn, x - gxe) * 0.7f);
    Lane &L = lane_[L_BG];
    add_rect(L, x, by - edge_o, x, ey_dn + r);
    add_arc(L, x - r, ey_dn + r, r, 270, 360);
    add_rect(L, x - r, ey_dn, gxe, ey_dn);
    add_pt(L, x, by - edge_o);
    add_pt(L, x, ey_dn + r);
    add_arc_pts(L, x - r, ey_dn + r, r, 360, 270);
    add_pt(L, gxe, ey_dn);
    finish_path(L, false, N_BATT, N_GRID);
  }
  if (has_[N_SOLAR] && has_[N_BATT]) {  // solar to battery: straight down
    Lane &L = lane_[L_SB];
    add_rect(L, mx, sy + R, mx, by - R);
    add_pt(L, mx, sy + R);
    add_pt(L, mx, by - R);
    finish_path(L, false, N_SOLAR, N_SOLAR);
  }
  for (int k = 0; k < nslots_; ++k) {
    const bool above = (k % 2) == 0;
    const int col = hc + k / 2;
    const float dx = static_cast<float>(cx_of(col)), dy = static_cast<float>(cy_of(above ? 0 : br));
    Lane &L = lane_[L_DEV0 + k];
    if (col == hc) {  // straight above or below the home
      if (above) {
        add_rect(L, hx, dy + R, hx, hy - R);
        add_pt(L, hx, dy + R);
        add_pt(L, hx, hy - R);
      } else {
        add_rect(L, hx, dy - R, hx, hy + R);
        add_pt(L, hx, dy - R);
        add_pt(L, hx, hy + R);
      }
      finish_path(L, false, N_DEV0 + k, N_DEV0 + k);
    } else {  // out of the home's side, then up or down to the circle
      const float hxr = static_cast<float>(hx + R);
      const float room_v = above ? (hy - (dy + R)) : ((dy - R) - hy);
      const float r = std::max(2.0f, std::min(dx - hxr, room_v) * 0.7f);
      if (above) {
        add_rect(L, hxr, hy, dx - r, hy);
        add_arc(L, dx - r, hy - r, r, 0, 90);
        add_rect(L, dx, dy + R, dx, hy - r);
        add_pt(L, hxr, hy);
        add_pt(L, dx - r, hy);
        add_arc_pts(L, dx - r, hy - r, r, 90, 0);
        add_pt(L, dx, dy + R);
      } else {
        add_rect(L, hxr, hy, dx - r, hy);
        add_arc(L, dx - r, hy + r, r, 270, 360);
        add_rect(L, dx, hy + r, dx, dy - R);
        add_pt(L, hxr, hy);
        add_pt(L, dx - r, hy);
        add_arc_pts(L, dx - r, hy + r, r, 270, 360);
        add_pt(L, dx, dy - R);
      }
      finish_path(L, true, N_DEV0 + k, N_DEV0 + k);  // built from the home out, turned so it runs from the circle
    }
  }

  // ---- The circles ----
  if (has_[N_GRID]) make_node(N_GRID, gx, hy, D, bw, g.names, false, name_w, ICON_GRID, false);
  if (has_[N_SOLAR]) make_node(N_SOLAR, mx, sy, D, bw, g.names, true, name_w, ICON_SOLAR, false);
  if (has_[N_BATT]) make_node(N_BATT, mx, by, D, bw, g.names, false, name_w, ICON_BATTERY_FULL, batt_power_);
  make_node(N_HOME, hx, hy, D, bw, false, false, name_w, ICON_HOME, false);
  for (int k = 0; k < nslots_; ++k) {
    const bool above = (k % 2) == 0;
    make_node(N_DEV0 + k, cx_of(hc + k / 2), cy_of(above ? 0 : br), D, bw, g.names, above, name_w, ICON_PLUG, false);
  }

  // ---- The dots, over everything, and the part that carries their animation ----
  for (int n = 0; n < LANES; ++n) {
    if (!lane_[n].used) continue;
    for (int i = 0; i < DOTS; ++i) {
      lv_obj_t *dot = make_block(parent_, 0, 0, dot_, dot_, LV_RADIUS_CIRCLE);
      show(dot, false);
      lane_[n].dot[i] = dot;
    }
  }
  clock_ = lv_obj_create(parent_);
  lv_obj_remove_style_all(clock_);
  lv_obj_set_size(clock_, 1, 1);
  lv_obj_add_flag(clock_, LV_OBJ_FLAG_HIDDEN);
  built_ = true;
  paint();
}

// ---------------------------------------------------------------------------------------------------
// on_state(): the new numbers
// ---------------------------------------------------------------------------------------------------
void PowerTile::on_state(JsonObjectConst data) {
  if (!built_) return;
  JsonObjectConst ents = data["entities"].as<JsonObjectConst>();
  rd_[N_GRID] = read_entity(ents, "grid", false);
  rd_[N_SOLAR] = read_entity(ents, "solar", false);
  rd_[N_BATT] = read_entity(ents, "battery", false);
  charge_ = read_entity(ents, "battery_in", false);
  level_ = read_entity(ents, "battery_level", true);

  // Devices, in the order they are chosen.
  Reading dev[MAX_DEVICES];
  char key[12];
  for (int i = 0; i < ndev_; ++i) {
    snprintf(key, sizeof(key), "device_%d", dev_option_[i] + 1);
    dev[i] = read_entity(ents, key, false);
  }
  // More devices than room: the ones that use the most power are shown. With room for all, they keep their places.
  int order[MAX_DEVICES];
  for (int i = 0; i < ndev_; ++i) order[i] = i;
  if (nslots_ < ndev_) {
    std::stable_sort(order, order + ndev_, [&](int a, int b) {
      return (dev[a].ok ? dev[a].w : -1.0f) > (dev[b].ok ? dev[b].w : -1.0f);
    });
  }
  nshown_ = std::min(nslots_, ndev_);
  for (int s = 0; s < nshown_; ++s) {
    const int d = order[s];
    slot_dev_[s] = d;
    rd_[N_DEV0 + s] = dev[d];
    const char *label = dev_label_[d][0] ? dev_label_[d] : dev[d].name;
    if (strncmp(dev_name_[s], label, sizeof(dev_name_[s]) - 1) != 0) painted_ = false;
    copy_text(dev_name_[s], sizeof(dev_name_[s]), label);
  }
  // Sensors report often, mostly the same number: draw again only when a number could change.
  bool changed = !painted_;
  for (int n = 0; n < NODES + 2; ++n) {
    const Reading &r = n < NODES ? rd_[n] : (n == NODES ? level_ : charge_);
    if (r.ok != prev_ok_[n] || fabsf(r.w - prev_w_[n]) >= 1.0f) changed = true;
    prev_ok_[n] = r.ok;
    prev_w_[n] = r.w;
  }
  if (!changed) return;
  painted_ = true;
  paint();
}

// ---------------------------------------------------------------------------------------------------
// paint(): the flows, texts, colours
// ---------------------------------------------------------------------------------------------------
void PowerTile::paint() {
  if (!built_) return;
  char buf[32];

  // What the sensors say. Grid: positive is power taken from the grid. Battery with one sensor: positive is power the
  // battery gives; with two, each is only its own direction.
  float grid = rd_[N_GRID].ok ? rd_[N_GRID].w : 0;
  if (invert_grid_) grid = -grid;
  const float solar = rd_[N_SOLAR].ok ? std::max(0.0f, rd_[N_SOLAR].w) : 0;
  float discharge = 0, charge = 0;
  if (batt_pair_) {
    discharge = rd_[N_BATT].ok ? std::max(0.0f, rd_[N_BATT].w) : 0;
    charge = charge_.ok ? std::max(0.0f, charge_.w) : 0;
  } else {
    float b = (rd_[N_BATT].ok ? rd_[N_BATT].w : 0) + (charge_.ok ? charge_.w : 0);
    if (invert_batt_) b = -b;
    discharge = std::max(b, 0.0f);
    charge = std::max(-b, 0.0f);
  }
  const float from_grid = std::max(grid, 0.0f), to_grid = std::max(-grid, 0.0f);

  // Where the power goes: solar first into the battery, then to the grid, the rest to the home; the battery gives first
  // to the grid (if that is what is fed in) and then to the home; the grid fills what the others do not.
  const float solar_to_batt = std::min(solar, charge);
  const float grid_to_batt = std::min(from_grid, charge - solar_to_batt);
  const float solar_left = solar - solar_to_batt;
  const float solar_to_grid = std::min(solar_left, to_grid);
  const float batt_to_grid = std::min(discharge, to_grid - solar_to_grid);
  const float solar_to_home = std::max(0.0f, solar_left - solar_to_grid);
  const float batt_to_home = std::max(0.0f, discharge - batt_to_grid);
  const float grid_to_home = std::max(0.0f, from_grid - grid_to_batt);
  const float home_w = solar_to_home + batt_to_home + grid_to_home;  // the home is always worked out from its sources

  const float sum = grid_to_home + solar_to_home + batt_to_home;
  share_[0] = sum > 0 ? grid_to_home / sum : 0;
  share_[1] = sum > 0 ? solar_to_home / sum : 0;
  share_[2] = sum > 0 ? batt_to_home / sum : 0;

  const lv_color_t card = ui::card_color();
  auto colour_of = [&](int n) {
    if (n >= N_DEV0) return swatch_color(dev_swatch_[slot_dev_[n - N_DEV0]]);
    return swatch_color(node_[n].swatch);
  };
  const lv_color_t idle = lv_color_mix(ui::color(theme::MUTED), card, 90);

  // ---- Lanes: each in the colour of where its power comes from, dimmed while nothing flows ----
  any_flow_ = false;
  set_lane(L_GH, grid_to_home);
  set_lane(L_SH, solar_to_home);
  set_lane(L_BH, batt_to_home);
  set_lane(L_SG, solar_to_grid);
  set_lane(L_BG, batt_to_grid - grid_to_batt);  // along the path: the battery gives to the grid; against it: the grid charges
  set_lane(L_SB, solar_to_batt);
  for (int s = 0; s < nslots_; ++s) set_lane(L_DEV0 + s, s < nshown_ && rd_[N_DEV0 + s].ok ? -rd_[N_DEV0 + s].w : 0);

  // ---- Circles ----
  auto node_value = [&](int n, bool ok, float shown) {
    NodeUI &N = node_[n];
    if (!N.used) return;
    if (ok) format_power(buf, sizeof(buf), shown);
    else snprintf(buf, sizeof(buf), "--");
    ui::set_text(N.value, buf);
    ui::set_color(N.value, theme::INK);
    const lv_color_t col = ok ? colour_of(n) : idle;
    set_border(N.ring, col);
    // The icon stays ink: a source colour that is light (solar's green, a device's amber) is too faint on a card in
    // the light look. The ring, the lanes and the dots carry the colour.
    set_text_color(N.icon, ui::color(theme::INK));
  };
  if (node_[N_GRID].used) {
    node_value(N_GRID, rd_[N_GRID].ok, grid);
    if (node_[N_GRID].name) ui::set_text(node_[N_GRID].name, name_[N_GRID][0] ? name_[N_GRID] : plugin_->text("grid"));
  }
  if (node_[N_SOLAR].used) {
    node_value(N_SOLAR, rd_[N_SOLAR].ok, solar);
    if (node_[N_SOLAR].name) ui::set_text(node_[N_SOLAR].name, name_[N_SOLAR][0] ? name_[N_SOLAR] : plugin_->text("solar"));
  }
  if (node_[N_BATT].used) {
    NodeUI &N = node_[N_BATT];
    if (batt_power_) {
      node_value(N_BATT, rd_[N_BATT].ok || charge_.ok, std::max(discharge, charge));
    } else {  // only a level: it is the value
      if (level_.ok) snprintf(buf, sizeof(buf), "%ld %%", lroundf(level_.w));
      else snprintf(buf, sizeof(buf), "--");
      ui::set_text(N.value, buf);
      ui::set_color(N.value, theme::INK);
      set_border(N.ring, level_.ok ? colour_of(N_BATT) : idle);
      set_text_color(N.icon, ui::color(theme::INK));
    }
    if (N.icon) ui::set_text(N.icon, ui::icon(level_.ok ? battery_icon(level_.w) : ICON_BATTERY_FULL));
    if (N.top) {
      if (level_.ok) snprintf(buf, sizeof(buf), "%ld %%", lroundf(level_.w));
      else buf[0] = 0;
      ui::set_text(N.top, buf);
      ui::set_color(N.top, theme::INK);
    }
    if (N.name) ui::set_text(N.name, name_[N_BATT][0] ? name_[N_BATT] : plugin_->text("battery"));
  }
  // The home: its ring shows where its power comes from, in the colours of the lanes.
  {
    NodeUI &N = node_[N_HOME];
    node_value(N_HOME, true, home_w);
    lv_color_t ring = swatch_color(N.swatch);  // its own colour while nothing flows into it
    float from = 270;
    for (int i = 0; i < 3; ++i) {
      const int src = i == 0 ? N_GRID : i == 1 ? N_SOLAR : N_BATT;
      lv_obj_t *a = N.arc[i];
      const float span = share_[i] * 360.0f;
      if (span < 1.0f || !node_[src].used) {
        if (arc_on_[i]) show(a, false);
        arc_on_[i] = false;
        continue;
      }
      if (span >= 359.0f) {  // one source gives all of it: the whole ring has its colour
        ring = swatch_color(node_[src].swatch);
        if (arc_on_[i]) show(a, false);
        arc_on_[i] = false;
        continue;
      }
      set_arc_color(a, swatch_color(node_[src].swatch));
      if (!arc_on_[i] || fabsf(arc_from_[i] - from) >= 1.0f || fabsf(arc_span_[i] - span) >= 1.0f) {
        set_arc_span(a, from, span);
        arc_from_[i] = from;
        arc_span_[i] = span;
      }
      if (!arc_on_[i]) show(a, true);
      arc_on_[i] = true;
      from += span;
    }
    set_border(N.ring, ring);
  }
  for (int s = 0; s < nshown_; ++s) {
    const int n = N_DEV0 + s;
    node_value(n, rd_[n].ok, rd_[n].w);
    if (node_[n].name) ui::set_text(node_[n].name, dev_name_[s]);
  }
  update_anim();
}

// One lane: `amount` is the power along its path (negative: against it). Its colour is the colour of the circle the power
// comes from; with nothing flowing, a dim version of its own.
void PowerTile::set_lane(int lane, float amount) {
  Lane &L = lane_[lane];
  if (!L.used) return;
  const lv_color_t card = ui::card_color();
  int8_t dir = 0;
  if (amount >= flow_min_) dir = 1;
  else if (amount <= -flow_min_) dir = -1;
  const int from_node = dir < 0 ? L.neg_node : L.pos_node;
  auto colour_of = [&](int n) {
    if (n >= N_DEV0) return swatch_color(dev_swatch_[slot_dev_[n - N_DEV0]]);
    return swatch_color(node_[n].swatch);
  };
  const lv_color_t own = colour_of(from_node);
  const lv_color_t col = dir ? own : lv_color_mix(own, card, 80);
  for (int i = 0; i < L.pieces; ++i) {
    if (L.is_arc[i]) set_arc_color(L.piece[i], col);
    else set_bg(L.piece[i], col);
  }
  for (int i = 0; i < DOTS; ++i) set_bg(L.dot[i], ui::color(theme::INK));
  if (dir == 0 || !animate_) {
    if (L.dots_on) {
      for (int i = 0; i < DOTS; ++i) show(L.dot[i], false);
      L.dots_on = false;
    }
  }
  L.dir = dir;
  if (dir != 0) {
    any_flow_ = true;
    // The more power, the faster: between a third and one and a half circles a second, along a log scale to 4 kW.
    const float t = std::clamp(logf(std::max(fabsf(amount), flow_min_) / std::max(flow_min_, 1.0f)) /
                                   logf(4000.0f / std::max(flow_min_, 1.0f)),
                               0.0f, 1.0f);
    L.speed = static_cast<float>(D_) * (0.3f + 1.2f * t) / 1000.0f;
  }
}

// ---------------------------------------------------------------------------------------------------
// The dots: one animation of LVGL for the whole tile, only while something flows and the screen is lit.
// ---------------------------------------------------------------------------------------------------
void PowerTile::update_anim() {
  const bool need = built_ && clock_ && animate_ && !paused_ && any_flow_;
  if (need && !anim_on_) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, clock_);  // LVGL drops the animation by itself when the part is deleted
    lv_anim_set_custom_exec_cb(&a, clock_cb);
    lv_anim_set_user_data(&a, this);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
    anim_on_ = true;
    last_tick_ = lv_tick_get();
  } else if (!need && anim_on_) {
    lv_anim_delete(clock_, nullptr);
    anim_on_ = false;
    for (int n = 0; n < LANES; ++n) {
      Lane &L = lane_[n];
      if (!L.dots_on) continue;
      for (int i = 0; i < DOTS; ++i) show(L.dot[i], false);
      L.dots_on = false;
    }
  }
}

// Every frame the animation runs (about 25 a second at most): each dot goes on along its lane at the speed of its power.
void PowerTile::step() {
  const uint32_t now = lv_tick_get();
  uint32_t dt = now - last_tick_;
  if (dt < 35) return;
  last_tick_ = now;
  if (dt > 250) dt = 250;
  for (int n = 0; n < LANES; ++n) {
    Lane &L = lane_[n];
    if (!L.used || L.dir == 0) continue;
    L.phase += L.speed * static_cast<float>(dt) / L.total;
    L.phase -= floorf(L.phase);
    for (int i = 0; i < L.ndots; ++i) {
      float t = L.phase + static_cast<float>(i) / static_cast<float>(L.ndots);
      t -= floorf(t);
      if (L.dir < 0) t = 1.0f - t;
      float x, y;
      point_at(L, t * L.total, x, y);
      const int px = static_cast<int>(lroundf(x)) - dot_ / 2, py = static_cast<int>(lroundf(y)) - dot_ / 2;
      if (!L.dots_on || px != L.dot_x[i] || py != L.dot_y[i]) {
        lv_obj_set_pos(L.dot[i], px, py);
        L.dot_x[i] = px;
        L.dot_y[i] = py;
      }
      if (!L.dots_on) show(L.dot[i], true);
    }
    L.dots_on = true;
  }
}

// ======================================================================================================
// The plugin
// ======================================================================================================
void BetterPowerDisplay::setup() {
  add_tile("flow", [this]() { return new PowerTile(this); });
}

void BetterPowerDisplay::attach(PowerTile *tile) {
  for (int i = 0; i < count_; ++i)
    if (tiles_[i] == tile) return;
  if (count_ < MAX_TILES) tiles_[count_++] = tile;
}

void BetterPowerDisplay::detach(PowerTile *tile) {
  for (int i = 0; i < count_; ++i) {
    if (tiles_[i] != tile) continue;
    tiles_[i] = tiles_[--count_];
    tiles_[count_] = nullptr;
    return;
  }
}

void BetterPowerDisplay::on_standby(bool dark) {
  for (int i = 0; i < count_; ++i) tiles_[i]->set_paused(dark);
}

}  // namespace esphome::better_power_display
