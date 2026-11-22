# Better power display

[![check](https://github.com/Lere-q/better-power-display/actions/workflows/check.yml/badge.svg)](https://github.com/Lere-q/better-power-display/actions/workflows/check.yml)
[![build](https://github.com/Lere-q/better-power-display/actions/workflows/build.yml/badge.svg)](https://github.com/Lere-q/better-power-display/actions/workflows/build.yml)

A live power-flow tile for Tessera, drawn like [power-flow-card-plus](https://github.com/flixlix/power-flow-card-plus):
grid, solar, battery and home as circles, your devices around the home, and lines with dots that move in the direction
of the power.

It uses the **live power** (W or kW) of your sensors. It does not read the Energy dashboard and does not turn energy
(Wh) into power, so the numbers are as current as the sensors.

## Install in Tessera

In Tessera open **Plugins**, choose **Add with a link** and paste:

```
https://github.com/Lere-q/better-power-display
```

Add the plugin to a screen and let Tessera build it. The plugin needs a Tessera firmware with plugin API 0.26 or newer.
Updates are published as GitHub releases, so a new version shows up on the screen's Plugins tab and is one tap away.

## Set up

1. Add the plugin to a screen in Tessera. The screen builds once.
2. In Layout, place the **Power flow** tile from the library's Plugins group. A larger tile gives larger circles and room
   for more devices.
3. In the inspector, choose your sensors. Everything is optional; only what you choose is drawn:
   - **Grid power**: positive is power taken from the grid. If your sensor is the other way round, turn on *Grid sensor:
     positive is export*.
   - **Solar power**.
   - **Battery power** and **Battery level**: positive is power the battery gives. If your sensor is positive while it
     charges, turn on *Battery sensor: positive is charging*.
   - **Home power** (optional): without it the home's power is grid + solar + battery.
   - **Device 1 to 8**: power sensors of single devices. They leave the home.
4. Under **Colours** pick the colour of each source. *Device colours* takes colours in the order of the devices.

## Good to know

- Sensors in W, kW and MW all work; the tile converts them.
- A line is idle (grey, no dots) while the power is below *Flow starts at* (10 W by default).
- The ring of the home shows where its power comes from, in the colours of grid, solar and battery.
- With more devices chosen than the tile has room for, it shows the ones that use the most power right now.
- The dots move four times a second; *Moving dots* under **More** turns them off if the screen feels slow.

## Privacy

Nothing leaves your home: the plugin reads the sensors you choose through Home Assistant and uses no internet service.

## Development

The rules and the tools live in Tessera's [plugins repository](https://github.com/MaxGramser/tessera-plugins); clone it
beside this one.

- `python3 ../tessera-plugins/tools/check.py .` checks the manifest, the texts and the code rules (seconds).
- `python3 ../tessera-plugins/tools/preview.py . --strict` builds the real firmware on every kind of screen and audits
  what it draws (the first build takes minutes).

Every push runs the check and the build on GitHub. A push to `main` that raises `version` in `tessera-plugin.yaml`
publishes a GitHub release with the lines of that version from `CHANGELOG.md`, and Tessera offers it to the screens as
an update.

