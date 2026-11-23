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
   - **Grid power**: positive is power taken from the grid, negative is power fed in.
   - **Solar power**.
   - **Battery power (out)**, optionally **Battery power (in)**, and **Battery level**. With one signed sensor choose it
     only as *out*: positive is discharging. With two sensors each only counts up from 0.
   - **Device 1 to 8**: power sensors of single devices. They leave the home.
   - **Flip the sign of**: for a grid sensor that is positive while you feed in, or a single battery sensor that is
     positive while it charges.
4. Under **Colours** pick the colour of grid, solar and battery. *Device colours* takes colours in the order of the devices.
5. Under **Names** type your own names. Without one, a device shows the name of its sensor.

## Good to know

- Sensors in W, kW and MW all work; the tile converts them.
- A lane is idle (dim, no dots) while the power is below *Flow starts at* (10 W by default).
- The ring of the home shows where its power comes from, in the colours of grid, solar and battery.
- With more devices chosen than the tile has room for, it shows the ones that use the most power right now.
- The dots flow smoothly and faster with more power. They stop while the screen is dark, and *Moving dots* turns them off.
- The home is always worked out from its sources (grid + solar + battery).

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
