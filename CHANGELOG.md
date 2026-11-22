# Changelog

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
