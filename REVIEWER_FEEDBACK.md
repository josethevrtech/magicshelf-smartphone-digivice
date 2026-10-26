# Reviewer update: Smartphone Digivice 0.2.8

On restores visibility. When the Steam Bar dock is unavailable at startup, the
creation opens in room space after one second of tracked operation. Hold a
controller grip while pointing at the housing or reaching near it to pick it up,
and release to place it. Trigger pickup remains available. Tracking loss releases
the grab. The creation's center button opens its own device settings; MagicShelf
manages installation, power, updates and removal.

The pinned MagicShelf SDK now uses trilinear mipmaps when shrinking the game
screen, while retaining nearest magnification for enlarged pixel art. Game assets,
creation identity, permissions and save locations are unchanged. This targets
small/angled pixel shimmer; perceived quality still needs a headset comparison.

The native geometry, configuration, dock, keyboard and capture-recovery checks
pass. The development package was built against the Frame's own libraries using
game/model inputs from the checksum-verified public release. Physical grip and
frametop behavior remain headset tests. Frametop hand tracking is not included.

Fallback room positions use separate shelf slots rather than stacking creations
at one location. Up to four creations share a row, with additional rows below.
