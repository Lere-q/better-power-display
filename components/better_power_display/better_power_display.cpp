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

// ---- Nodes: the circles. Links: the lines from a node to the home. Both use the same index. ----
enum Node : int { N_GRID = 0, N_SOLAR, N_BATT, N_HOME, N_DEV0 };
constexpr int MAX_DEVICES = 8;
constexpr int NODES = N_DEV0 + MAX_DEVICES;
constexpr int MAX_PTS = 14;         // points of one line, for the dots
constexpr int ARC_STEPS = 6;        // a corner is drawn as an arc; the dots follow it in 6 short steps
constexpr uint32_t LOOP_MS = 3000;  // a dot runs the whole line in this time
constexpr int DOTS = 1;             // dots on one line

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

// "238 W", "1.70 kW", "12.3 kW"
void format_power(char *out, size_t n, float w, float kw_from) {
  const float a = fabsf(w);
  if (a >= kw_from) {
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
  ~PowerTile() override { plugin_->detach(this); }

  void create(const tessera::TileContext &c) override;
  void on_state(JsonObjectConst data) override;
  void on_theme() override { paint(); }

  // From the plugin, every 250 ms: the dots move along their lines.
  void animate(uint32_t now_ms);

 private:
  // One circle: a ring, and up to five labels in it or beside it.
  struct NodeUI {
    bool used = false;
    int cx = 0, cy = 0;
    lv_obj_t *ring = nullptr;  // a circle's border (every node but the home)
    lv_obj_t *arc[3] = {};     // the home's ring: the shares of grid, solar, battery
    lv_obj_t *top = nullptr, *icon = nullptr, *value = nullptr, *sub = nullptr, *name = nullptr;
    bool top_on = false, sub_on = false;
    uint8_t swatch = AZURE;
  };
  // One line from a node to the home, as pieces (blocks and an arc) and the path the dots follow.
  struct Link {
    bool used = false;
    lv_obj_t *piece[3] = {};
    bool is_arc[3] = {};
    uint8_t pieces = 0;
    lv_obj_t *dot[DOTS] = {};
    float px[MAX_PTS] = {}, py[MAX_PTS] = {}, cum[MAX_PTS] = {};
    uint8_t np = 0;
    float total = 0;
    int8_t dir = 0;  // +1 towards the home, -1 away from it, 0 nothing flows
  };

  void make_node(int n, int cx, int cy, int d, int bw, bool with_name, bool name_above, int name_width, uint32_t icon,
                 bool want_top, bool want_sub, const char *name_text);
  void add_rect(Link &L, float x0, float y0, float x1, float y1);
  void add_arc(Link &L, float cx, float cy, float r, int a0, int a1);
  void add_pt(Link &L, float x, float y) {
    if (L.np < MAX_PTS) {
      L.px[L.np] = x;
      L.py[L.np] = y;
      ++L.np;
    }
  }
  void add_arc_pts(Link &L, float cx, float cy, float r, float from_deg, float to_deg);
  void finish_path(Link &L, bool reverse);
  void paint();
  static void point_at(const Link &L, float s, float &x, float &y);

  BetterPowerDisplay *plugin_;
  lv_obj_t *parent_ = nullptr;
  lv_obj_t *hint_ = nullptr;
  int W_ = 0, H_ = 0, D_ = 0, lw_ = 2, dot_ = 5;
  bool built_ = false;

  // Options
  bool has_[NODES] = {};
  bool invert_grid_ = false, invert_batt_ = false;
  float flow_min_ = 10, kw_from_ = 1000;
  int dev_option_[MAX_DEVICES] = {};  // which option (device_1 .. device_8) each chosen device is
  int dev_swatch_[MAX_DEVICES] = {};  // the colour of each chosen device
  int ndev_ = 0;                      // devices chosen
  int nslots_ = 0;                    // devices the tile has room for

  NodeUI node_[NODES];
  Link link_[NODES];
  bool dots_on_[NODES] = {};

  // The last data
  Reading rd_[NODES];               // grid, solar, battery, home, and the devices in their slots
  Reading level_;                   // battery level
  int slot_dev_[MAX_DEVICES] = {};  // which chosen device each slot shows
  int nshown_ = 0;
  char dev_name_[MAX_DEVICES][28] = {};
  float home_w_ = 0;
  float share_[3] = {};  // part of the home's power that comes from grid, solar, battery
  bool arc_on_[3] = {};
  float arc_from_[3] = {}, arc_span_[3] = {};
  bool animate_ = true;
  int last_x_[NODES] = {}, last_y_[NODES] = {};
  bool painted_ = false;
  bool prev_ok_[NODES + 1] = {};
  float prev_w_[NODES + 1] = {};
};

// ---------------------------------------------------------------------------------------------------
// Pieces of a line
// ---------------------------------------------------------------------------------------------------
void PowerTile::add_rect(Link &L, float x0, float y0, float x1, float y1) {
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
void PowerTile::add_arc(Link &L, float cx, float cy, float r, int a0, int a1) {
  if (L.pieces >= 3) return;
  const int size = static_cast<int>(lroundf(2 * r)) + lw_;
  lv_obj_t *a = make_arc(parent_, size, lw_);
  lv_obj_set_pos(a, static_cast<int>(lroundf(cx)) - size / 2, static_cast<int>(lroundf(cy)) - size / 2);
  set_arc_span(a, static_cast<float>(a0), static_cast<float>(a1 - a0 <= 0 ? a1 - a0 + 360 : a1 - a0));
  L.piece[L.pieces] = a;
  L.is_arc[L.pieces] = true;
  ++L.pieces;
}

void PowerTile::add_arc_pts(Link &L, float cx, float cy, float r, float from_deg, float to_deg) {
  for (int i = 0; i <= ARC_STEPS; ++i) {
    const float deg = from_deg + (to_deg - from_deg) * static_cast<float>(i) / ARC_STEPS;
    const float rad = deg * 3.14159265f / 180.0f;
    add_pt(L, cx + r * cosf(rad), cy + r * sinf(rad));
  }
}

// The path is built from the node to the home, or the other way round and turned here.
void PowerTile::finish_path(Link &L, bool reverse) {
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
}

void PowerTile::point_at(const Link &L, float s, float &x, float &y) {
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
                          uint32_t icon, bool want_top, bool want_sub, const char *name_text) {
  NodeUI &N = node_[n];
  N.used = true;
  N.cx = cx;
  N.cy = cy;
  const int x = cx - d / 2, y = cy - d / 2;

  {
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
  }
  if (n == N_HOME) {  // over the ring: the part of the home's power that each source gives (hidden while one gives all)
    for (int i = 0; i < 3; ++i) {
      N.arc[i] = make_arc(parent_, d, bw);
      lv_obj_set_pos(N.arc[i], x, y);
      show(N.arc[i], false);
    }
  }

  // The words inside: the largest value font the circle takes, then the icon, and the lines above and below if they fit.
  const int inner = d - 2 * bw;
  Font vf = Font::BODY;
  for (Font f : {Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE, Font::BODY}) {
    vf = f;
    if (ui::text_width("1.70 kW", f) <= inner * 82 / 100 && lh(f) <= inner * 32 / 100) break;
  }
  int total = lh(vf);
  Font icf = Font::ICON;
  bool icon_on = false;
  if (lh(Font::ICON) + total <= inner * 78 / 100) {
    icf = Font::ICON;
    icon_on = true;
  } else if (lh(Font::ICON_SMALL) + total <= inner * 78 / 100) {
    icf = Font::ICON_SMALL;
    icon_on = true;
  }
  if (icon_on) total += lh(icf);
  N.top_on = want_top && total + lh(Font::BODY) <= inner * 80 / 100;
  if (N.top_on) total += lh(Font::BODY);
  N.sub_on = want_sub && total + lh(Font::BODY) <= inner * 84 / 100;
  if (N.sub_on) total += lh(Font::BODY);

  auto line = [&](Font f, theme::Role role, int &y_cursor) {
    lv_obj_t *l = ui::label(parent_, f, role);
    lv_obj_set_width(l, inner);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, cx - inner / 2, y_cursor);
    y_cursor += lh(f);
    return l;
  };
  int yc = cy - total / 2;
  if (N.top_on) N.top = line(Font::BODY, theme::INK, yc);
  if (icon_on) {
    N.icon = line(icf, theme::INK, yc);
    ui::set_text(N.icon, ui::icon(icon));
  }
  N.value = line(vf, theme::INK, yc);
  if (N.sub_on) N.sub = line(Font::BODY, theme::MUTED, yc);

  if (with_name) {
    lv_obj_t *l = ui::label(parent_, Font::BODY, theme::MUTED);
    lv_obj_set_width(l, name_width);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    const int gap = std::max(2, ui::px(3));
    const int ny = name_above ? cy - d / 2 - lh(Font::BODY) - gap : cy + d / 2 + gap;
    lv_obj_set_pos(l, cx - name_width / 2, ny);
    if (name_text) ui::set_text(l, name_text);
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

  static const char *const CORE[] = {"grid", "solar", "battery"};
  for (int n = 0; n < 3; ++n) has_[n] = chosen(c.options, CORE[n]);
  has_[N_HOME] = chosen(c.options, "home");
  invert_grid_ = c.options["invert_grid"] | false;
  invert_batt_ = c.options["invert_battery"] | false;
  flow_min_ = c.options["flow_min"] | 10.0f;
  kw_from_ = c.options["kw_from"] | 1000.0f;
  const bool want_names = c.options["show_names"] | true;
  animate_ = c.options["animate"] | true;

  // Colours of the sources, then of each device: the person's pick, in order, or the next colour of the palette.
  node_[N_GRID].swatch = static_cast<uint8_t>(swatch_of(c.options["color_grid"] | "", BLUE));
  node_[N_SOLAR].swatch = static_cast<uint8_t>(swatch_of(c.options["color_solar"] | "", ORANGE));
  node_[N_BATT].swatch = static_cast<uint8_t>(swatch_of(c.options["color_battery"] | "", PINK));
  node_[N_HOME].swatch = static_cast<uint8_t>(swatch_of(c.options["color_home"] | "", AZURE));
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
    g = geometry(nd, want_names);
    if (want_names && g.d < ui::mm(9)) g = geometry(nd, false);
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

  // ---- The lines first, so the circles lie over their ends ----
  const int off = R * 35 / 100;  // the lines of the solar panels and the battery enter the home beside the grid's
  const float hxe = hx - sqrtf(static_cast<float>(R * R - off * off));
  const int mid_col = has_[N_GRID] ? 1 : 0;

  if (has_[N_GRID]) {
    const int gx = cx_of(0);
    Link &L = link_[N_GRID];
    add_rect(L, gx + R, hy, hx - R, hy);
    add_pt(L, gx + R, hy);
    add_pt(L, hx - R, hy);
    finish_path(L, false);
  }
  if (has_[N_SOLAR]) {  // from the top, down, and right into the home
    const float nx = cx_of(mid_col), ny = cy_of(0), ey = hy - off;
    const float r = std::max(2.0f, std::min(ey - (ny + R), hxe - nx) * 0.7f);
    Link &L = link_[N_SOLAR];
    add_rect(L, nx, ny + R, nx, ey - r);
    add_arc(L, nx + r, ey - r, r, 90, 180);
    add_rect(L, nx + r, ey, hxe, ey);
    add_pt(L, nx, ny + R);
    add_pt(L, nx, ey - r);
    add_arc_pts(L, nx + r, ey - r, r, 180, 90);
    add_pt(L, hxe, ey);
    finish_path(L, false);
  }
  if (has_[N_BATT]) {  // from below, up, and right into the home
    const float nx = cx_of(mid_col), ny = cy_of(br), ey = hy + off;
    const float r = std::max(2.0f, std::min((ny - R) - ey, hxe - nx) * 0.7f);
    Link &L = link_[N_BATT];
    add_rect(L, nx, ny - R, nx, ey + r);
    add_arc(L, nx + r, ey + r, r, 180, 270);
    add_rect(L, nx + r, ey, hxe, ey);
    add_pt(L, nx, ny - R);
    add_pt(L, nx, ey + r);
    add_arc_pts(L, nx + r, ey + r, r, 180, 270);
    add_pt(L, hxe, ey);
    finish_path(L, false);
  }
  for (int k = 0; k < nslots_; ++k) {
    const bool above = (k % 2) == 0;
    const int col = hc + k / 2;
    const float dx = cx_of(col), dy = cy_of(above ? 0 : br);
    Link &L = link_[N_DEV0 + k];
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
      finish_path(L, false);
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
      finish_path(L, true);  // built from the home out, turned so that it runs from the circle to the home
    }
  }

  // ---- The circles ----
  if (has_[N_GRID])
    make_node(N_GRID, cx_of(0), hy, D, bw, g.names, false, name_w, ICON_GRID, false, true, plugin_->text("grid"));
  if (has_[N_SOLAR])
    make_node(N_SOLAR, cx_of(mid_col), cy_of(0), D, bw, g.names, true, name_w, ICON_SOLAR, false, false,
              plugin_->text("solar"));
  if (has_[N_BATT])
    make_node(N_BATT, cx_of(mid_col), cy_of(br), D, bw, g.names, false, name_w, ICON_BATTERY_FULL, true, true,
              plugin_->text("battery"));
  make_node(N_HOME, hx, hy, D, bw, false, false, name_w, ICON_HOME, false, false, nullptr);
  for (int k = 0; k < nslots_; ++k) {
    const bool above = (k % 2) == 0;
    make_node(N_DEV0 + k, cx_of(hc + k / 2), cy_of(above ? 0 : br), D, bw, g.names, above, name_w, ICON_PLUG, false,
              false, nullptr);
  }

  // ---- The dots, over everything ----
  for (int n = 0; n < NODES; ++n) {
    if (!link_[n].used) continue;
    for (int i = 0; i < DOTS; ++i) {
      lv_obj_t *dot = make_block(parent_, 0, 0, dot_, dot_, LV_RADIUS_CIRCLE);
      show(dot, false);
      link_[n].dot[i] = dot;
    }
  }
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
  rd_[N_HOME] = read_entity(ents, "home", false);
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
    slot_dev_[s] = order[s];
    rd_[N_DEV0 + s] = dev[order[s]];
    if (strncmp(dev_name_[s], dev[order[s]].name, sizeof(dev_name_[s]) - 1) != 0) painted_ = false;
    strncpy(dev_name_[s], dev[order[s]].name, sizeof(dev_name_[s]) - 1);
    dev_name_[s][sizeof(dev_name_[s]) - 1] = 0;
  }
  // Sensors report often, mostly the same number: draw again only when a shown number or a colour could change.
  bool changed = !painted_;
  for (int n = 0; n <= NODES; ++n) {
    const Reading &r = n == NODES ? level_ : rd_[n];
    if (r.ok != prev_ok_[n] || fabsf(r.w - prev_w_[n]) >= 1.0f) changed = true;
    prev_ok_[n] = r.ok;
    prev_w_[n] = r.w;
  }
  if (!changed) return;
  painted_ = true;
  paint();
}

// ---------------------------------------------------------------------------------------------------
// paint(): texts, colours, the direction of every line
// ---------------------------------------------------------------------------------------------------
void PowerTile::paint() {
  if (!built_) return;
  const lv_color_t idle = lv_color_mix(ui::color(theme::MUTED), ui::card_color(), 70);
  char buf[32];

  // What flows where. Grid: positive is power taken from the grid. Battery: positive is power the battery gives.
  float grid = rd_[N_GRID].ok ? rd_[N_GRID].w : 0, batt = rd_[N_BATT].ok ? rd_[N_BATT].w : 0;
  const float solar = rd_[N_SOLAR].ok ? std::max(0.0f, rd_[N_SOLAR].w) : 0;
  if (invert_grid_) grid = -grid;
  if (invert_batt_) batt = -batt;

  home_w_ = (rd_[N_HOME].ok && has_[N_HOME]) ? rd_[N_HOME].w : std::max(0.0f, solar + grid + batt);
  // The home's ring shows where its power comes from.
  const float from_grid = std::max(grid, 0.0f), from_batt = std::max(batt, 0.0f);
  const float from_solar = std::max(solar - std::max(-grid, 0.0f) - std::max(-batt, 0.0f), 0.0f);
  const float sum = from_grid + from_solar + from_batt;
  share_[0] = sum > 0 ? from_grid / sum : 0;
  share_[1] = sum > 0 ? from_solar / sum : 0;
  share_[2] = sum > 0 ? from_batt / sum : 0;

  auto colour_of = [&](int n) {
    if (n >= N_DEV0) return swatch_color(dev_swatch_[slot_dev_[n - N_DEV0]]);
    return swatch_color(node_[n].swatch);
  };
  // `power` is positive when the power flows towards the home, unless `from_home`: then positive flows away from it.
  auto set_dir = [&](int n, float power, bool from_home) {
    Link &L = link_[n];
    if (!L.used) return;
    int8_t dir = 0;
    if (power >= flow_min_) dir = from_home ? -1 : 1;
    else if (power <= -flow_min_) dir = from_home ? 1 : -1;
    L.dir = dir;
    const lv_color_t col = dir ? colour_of(n) : idle;
    for (int i = 0; i < L.pieces; ++i) {
      if (L.is_arc[i]) set_arc_color(L.piece[i], col);
      else set_bg(L.piece[i], col);
    }
    for (int i = 0; i < DOTS; ++i) {
      set_bg(L.dot[i], colour_of(n));
      if (!dir || !animate_) show(L.dot[i], false);
    }
    if (!dir || !animate_) dots_on_[n] = false;
  };
  auto node_texts = [&](int n, bool ok, float shown) {
    NodeUI &N = node_[n];
    if (!N.used) return;
    if (ok) format_power(buf, sizeof(buf), shown, kw_from_);
    else snprintf(buf, sizeof(buf), "--");
    ui::set_text(N.value, buf);
    ui::set_color(N.value, theme::INK);
    const lv_color_t col = ok ? colour_of(n) : idle;
    if (N.ring) set_border(N.ring, col);
    // The icon stays ink: a source colour that is light (solar's orange, a device's amber) is too faint on a card in
    // the light look. The ring, the lines and the dots carry the colour.
    set_text_color(N.icon, ui::color(theme::INK));
  };

  if (node_[N_GRID].used) {
    node_texts(N_GRID, rd_[N_GRID].ok, grid);
    set_dir(N_GRID, grid, false);
    if (node_[N_GRID].sub) {
      const bool imp = grid >= flow_min_, exp = grid <= -flow_min_;
      ui::set_text(node_[N_GRID].sub, !rd_[N_GRID].ok ? "" : imp ? plugin_->text("import") : exp ? plugin_->text("export") : "");
    }
  }
  if (node_[N_SOLAR].used) {
    node_texts(N_SOLAR, rd_[N_SOLAR].ok, solar);
    set_dir(N_SOLAR, solar, false);
  }
  if (node_[N_BATT].used) {
    node_texts(N_BATT, rd_[N_BATT].ok, batt);
    set_dir(N_BATT, batt, false);
    NodeUI &N = node_[N_BATT];
    if (N.icon) ui::set_text(N.icon, ui::icon(level_.ok ? battery_icon(level_.w) : ICON_BATTERY_FULL));
    if (N.top) {
      if (level_.ok) snprintf(buf, sizeof(buf), "%ld %%", lroundf(level_.w));
      else buf[0] = 0;
      ui::set_text(N.top, buf);
      ui::set_color(N.top, theme::INK);
    }
    if (N.sub) {
      const bool give = batt >= flow_min_, take = batt <= -flow_min_;
      ui::set_text(N.sub, !rd_[N_BATT].ok ? "" : give ? plugin_->text("discharging") : take ? plugin_->text("charging") : "");
    }
  }
  // The home: a ring of its own colour, and over it the part of each source.
  {
    NodeUI &N = node_[N_HOME];
    node_texts(N_HOME, true, home_w_);
    lv_color_t ring = swatch_color(N.swatch);  // the home's own colour while nothing flows into it
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
    const Reading &r = rd_[n];
    node_texts(n, r.ok, r.w);
    set_dir(n, r.ok ? r.w : 0, true);
    if (node_[n].name) ui::set_text(node_[n].name, dev_name_[s]);
  }
}

// ---------------------------------------------------------------------------------------------------
// animate(): called every 250 ms by the plugin. Each dot is one position on its line.
// ---------------------------------------------------------------------------------------------------
void PowerTile::animate(uint32_t now_ms) {
  if (!built_ || !animate_) return;
  const float phase = static_cast<float>(now_ms % LOOP_MS) / static_cast<float>(LOOP_MS);
  for (int n = 0; n < NODES; ++n) {
    Link &L = link_[n];
    if (!L.used) continue;
    if (L.dir == 0) {
      if (dots_on_[n]) {
        for (int i = 0; i < DOTS; ++i) show(L.dot[i], false);
        dots_on_[n] = false;
      }
      continue;
    }
    float t = phase;
    if (L.dir < 0) t = 1.0f - t;
    float x, y;
    point_at(L, t * L.total, x, y);
    const int px = static_cast<int>(lroundf(x)) - dot_ / 2, py = static_cast<int>(lroundf(y)) - dot_ / 2;
    if (!dots_on_[n] || px != last_x_[n] || py != last_y_[n]) {
      lv_obj_set_pos(L.dot[0], px, py);
      last_x_[n] = px;
      last_y_[n] = py;
    }
    if (!dots_on_[n]) {
      show(L.dot[0], true);
      dots_on_[n] = true;
    }
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

void BetterPowerDisplay::on_interval(uint32_t now_ms) {
  if (dark_) return;
  for (int i = 0; i < count_; ++i) tiles_[i]->animate(now_ms);
}

}  // namespace esphome::better_power_display
