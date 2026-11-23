# Changelog

## 0.2.0 - 2026-11-22
- A lane for every flow: solar to home, grid to home, battery to home, and now also solar to grid (feeding in), battery to grid, grid to battery and solar to battery. Every lane has the colour of where its power comes from, and a dim version of it while nothing flows.
- The ring of the home is split by what each source gives it, in the colours of the lanes.
- The dots flow smoothly now and go faster with more power. They stop while the screen is dark. *Moving dots* turns them off.
- Battery: a second sensor for charging beside the one for discharging (*Battery power (in)*), and the battery level is shown in the circle.
- Names of grid, solar, battery and every device can be typed in. Without one, a device shows the name of its sensor.
- The home is always worked out from its sources; its sensor option is gone. The words under the grid and battery values are gone, and so are the options *Show names* and *Show kW from* (kW from 1000 W).
- *Flip the sign of* is one option for grid and battery. The home has no colour of its own to pick.

## 0.1.1 - 2026-11-22
- Fixed: the home's ring was not drawn. It is now a full circle in the home's colour, or in the colour of the one source that gives all of its power, with arcs over it when several sources share.
- Fixed: names under and over the circles were dropped too early on large tiles. They now stay unless the circles get very small.
- Lighter on the screen: one moving dot per line, drawn only when it has moved, and the tile draws again only when a number changed. New setting *Moving dots* turns the animation off.
- Circles are a little larger because the rows sit closer together.

## 0.1.0 - 2026-11-22
- First version: a live power-flow tile in watts. Grid, solar, battery (with its level), home and up to eight devices as circles joined by lines, with dots that move in the direction of the flow.
- Every source is a power sensor you choose in the tile's settings; nothing is worked out from energy (Wh).
- The layout fits the size of the tile: circles scale, and when there is not room for all devices the ones that use the most power are shown.
- Colours of the sources and the devices can be chosen.
