#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::better_power_display {

class PowerTile;

// The plugin: an ESPHome component and a tessera::Plugin. It registers the tile in setup() and, every 250 ms, moves
// the dots along the lines of every power-flow tile that is alive (a tile's own on_tick is only once a second).
class BetterPowerDisplay : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void on_interval(uint32_t now_ms) override;
  void on_standby(bool dark) override { dark_ = dark; }

  // A tile tells the plugin when it exists, so the plugin can animate it.
  void attach(PowerTile *tile);
  void detach(PowerTile *tile);

 protected:
  static constexpr int MAX_TILES = 8;
  PowerTile *tiles_[MAX_TILES]{};
  uint8_t count_{0};
  bool dark_{false};
};

}  // namespace esphome::better_power_display
