# Working on the companion

Use Beads locally for durable task tracking when initialized. Keep the tracker in the ignored `.beads/` directory. The target board and verification procedure
are in [docs/HARDWARE.md](docs/HARDWARE.md). Consult that profile for changes to
screens, input, peripherals, storage, asset formats or network behavior.

- Design each play flow from the active character’s imported build, available
  resources and saved choices. Verify at least two contrasting profiles when
  changing selection, featured moves or session state.
- Review interactive screens inside the native 360 × 360 circle. Check the
  visible art, text, reachable touch areas and ring behavior in each state.
- Run `python3 tools/check_device_art.py` when changing device illustrations.
  Account for compressed flash size, decoded PSRAM and internal transfer buffers.
- Source hardware capabilities from the exact-board documentation. Record the
  board revision, measurements and test conditions when qualifying firmware.
  Label desktop results and pending physical checks explicitly.
- Keep personal portraits, character links and downloaded sheets out of public
  fixtures. Use original generic artwork and illustrative characters in docs.
- Keep device backups, Wi-Fi settings, private keys, profile caches and personal
  test captures under `.local/`. Publish built release assets separately. Keep
  the Beads database and its Dolt sync refs local to the development machine.
- Illustrate player instructions at the step where the player taps, spins or confirms. Use firmware captures for controls, an example phone page for setup, or a labeled diagram for a flow. Include descriptive alt text. Identify host renders and concept images wherever they could be mistaken for an on-device capture.

## Round-screen typography

- On portrait-led screens, curve the character name around the upper half of the portrait and the class names with their levels around the lower half. Give the name the largest type. On the sheet, place total level and proficiency on the next arc out above the name, and cache status between the classes and the bottom action. Keep roll totals and other numbers players need to read at a glance horizontal.
- Use the shared arc renderer in `firmware/Companion/rim_text.cpp`. Center both upper and lower captions and keep their glyphs upright for left-to-right reading. Leave the ends of each arc open. Omit decorative ticks, end strokes and label dividers drawn as lines.
- Fit the actual glyph widths along the available arc before reducing type size. Start character names at 36 px. Start class lists at 20 px, then try 18 and 14 px. Check a short build and a long multiclass build on the 360 × 360 display. Read every class level and verify clearance around the portrait and the adjacent captions.
- Place the connected SSID beside the Wi-Fi icon and reserve room for power and picker position. Shorten the SSID first. The top rim opens device status with the full name. Only actionable rim captions own a touch sector. See [the hardware profile](docs/HARDWARE.md) for dimensions and graphics costs.
- Keep character identity on the picker and sheet. Leave the upper selector free of names on Checks and Saves. On the character sheet, keep two landmarks visible per page. Support direct touch, horizontal paging and a portrait shortcut to Dice. On roll details use a short roll-kind caption and keep the selected skill, ability and total prominent. Review all six surrounding touch targets with the longest skill names and a conditional reminder visible on its own roll.
- Detail screens use the shared rounded return arc, inset 22 px from the glass with a 44 px touch band. Swipe right reaches the same parent. Spin effects use bounded local redraws on the rolling face, with the same dice rules at every speed.

## Rules from new releases

For a new supplement, identify the published class or feature on the imported sheet. Check its final rules and current official corrections. Exercise the normal filtered import and play transaction with an eligible build, a legacy or ineligible build, and an existing saved session when the storage format changes. Record the source and the automated or player-adjudicated behavior in [the rules reference](docs/RULES.md).

When that coverage changes, update the versioned book table and its manual links. Distinguish an imported spell or action, an engine-enforced rule, a reminder, and a decision the player makes at the table. Include a worked example of any new spending or recovery rule in the player guide.
