#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::better_power_display {

class PowerTile;

// The plugin: an ESPHome component and a tessera::Plugin. It registers the tile in setup() and tells the tiles that
// are alive when the screen goes dark, so that their dots stop moving while nobody looks.
class BetterPowerDisplay : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void on_standby(bool dark) override;

  void attach(PowerTile *tile);
  void detach(PowerTile *tile);

 protected:
  static constexpr int MAX_TILES = 8;
  PowerTile *tiles_[MAX_TILES]{};
  uint8_t count_{0};
};

}  // namespace esphome::better_power_display
