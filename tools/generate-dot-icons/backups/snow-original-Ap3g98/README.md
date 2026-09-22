# Original snowflake backup

Snapshot taken before trying the solid six-arm reference shape. It retains the
original Lucide snowflake, the solid sun disk, and all six weather icon categories.
The lock page's 5px dot diameter is unchanged and lives outside these assets.

`weather.json` is the original manifest. `WeatherDotIcons.h` is the exact generated
header. `generate-dot-icons.py` is a provenance snapshot of the generator, not an
entry point to run from this backup directory (its repository-relative root differs).

To restore, copy this `weather.json` to `tools/generate-dot-icons/weather.json`
and run the main generator at `tools/generate-dot-icons/generate-dot-icons.py`.
The saved header can be used to compare the regenerated output byte for byte.
