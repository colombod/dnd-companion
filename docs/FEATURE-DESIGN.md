# Feature design notes

These proposals describe design goals. For features you can use on a released dial, see the [supported feature list](FEATURES.md). [Screen design previews](SCREEN-FLOWS.md)

## The active character comes first

Forge Artificers can spend Flash of Genius on a failed check or save. Cartographers can choose how to cast *Faerie Fire* and recover feature uses on a rest. [Rules coverage](RULES.md#published-supplements-and-class-versions)

From the top status arc, players can check for a firmware release, review its version and size, and install it over Wi-Fi. [Update guide](FIRMWARE-UPDATES.md)

The dial and phone setup share a saved language choice: English, Italian or Spanish. Imported sheet text keeps its source language.

Open on the last player's character, with their portrait, classes, a featured
move and their remaining resources. The player can feature a spell, skill or
save by holding it. Store its source ID with that character. If a refresh
removes the move, flag it for review. Browsing recently used moves does not
silently change the player's choice.

Build the spellbook, available slot pools, skill and save bonuses, companion
and recharge rules from the imported character. Keep build data, player
preferences and resources spent at the table distinct. Switching characters
switches all three together. For example, the same Intelligence modifier can
produce different Arcana and History bonuses depending on training or a sheet
override. Read the named skill as well as the underlying ability.

## Touch and dial together

Touch makes direct choices. The dial makes browsing, scrolling and numeric
changes fast. Use the touchscreen for confirming and going back. Always
highlight the field or list the dial controls.

| Interaction | Intended behavior |
| --- | --- |
| Tap a character portrait | Open that character's cached sheet immediately. |
| Turn in the character picker | Browse saved characters; tap to open. |
| Swipe right on a detail face | Return to its parent. The destination is also printed at six o’clock. A swipe beginning on a die, spell or resource cancels its tap or hold. |
| Swipe vertically within a list | Scroll spells, skills, inventory or details. The dial can scroll the same list. |
| Tap a stat, skill or save | Open its roll with the current bonus and applicable options. |
| Tap an action or spell | Open a compact detail view with a visible Use or Cast action. |
| Open Actions | See defenses and equipped weapons first. Roll weapon attacks and damage, inspect AC, track temporary HP and resolve a melee hit against Armor of Agathys. |
| Choose a bonded weapon | Show the choice when an active Pact Weapon feature has conflicting marks. Check the saved item and its roll modifiers against the accepted character profile. |
| Read a spell’s slots | See remaining and total regular and Pact slots, with filled and hollow dots for small pools. |
| Tap Distant or Twinned in the cast preview | Preview a learned option’s effect and remaining Sorcery Points. Tap again to clear it or tap Cast to spend the resources. |
| Tap a use-based resource tile | Spend one use and offer Undo. |
| Hold a use-based resource tile | Restore one use, with feedback and Undo. |
| Tap a numeric resource | Select it; turn to adjust its value. |
| Turn on the Combat page | Preview damage or healing to HP, unless another value is selected. Pause briefly to commit or tap Apply. |
| Pull down from the top edge | Open the session log. Also available from the menu. |
| Tap a roll result | Expand the dice, bonuses and effects behind the total. |

Pair gestures with visible controls. Once a drag becomes a swipe or scroll,
cancel the tile tap or hold. Reserve the log gesture for a swipe that begins at
the top edge. Clear the dial selection on page changes and use the first touch
or turn only to wake a sleeping screen. Use large targets, readable text,
optional left-handed layouts, and adjustable dial direction and sensitivity.
Use a phone or computer for longer setup and editing.

Three common flows should work without a settings detour:

| At the table | Flow |
| --- | --- |
| “Give me a Wisdom save.” | Tap the save and its roll mode, then spin the ring or enter physical dice. |
| “You take 12 damage.” | Turn left to preview 12, check any damage type or defense, then apply. Show how much went to temporary HP and offer Undo. |
| “I cast a spell using Pact Magic.” | Touch the grimoire, tap the spell, select an eligible Pact slot, then confirm its cost and effect. |

## Saved characters and direct online refresh

**Device limitation.** Players supply each character’s individual sheet or share
link. Import and refresh require the linked character to be available to the
device while signed out. An account-level My Characters page or a sign-in-only
sheet cannot be imported. See the [player manual](MANUAL.md).

- Configure a library of D&D Beyond character URLs using a local setup page on
  a phone or computer. Accept normal and shared links and normalize duplicates
  by character ID. Support at least 12 saved characters, with campaign labels,
  portraits, favorites and a recently used view.
- Choose a picker-wide appearance: background and icon together, icon only, or background only. Each linked character owns one replaceable phone-cropped 360 × 360 background and a separate centered 128 × 128 profile icon.
- Switch characters on the device using touch or the dial. Keep each character's
  play state, appearance, favorites and companions separate.
- Fetch the selected character directly over Wi-Fi. Provide Refresh, optional
  refresh-on-open and a throttled Refresh All operation. A computer is not needed
  for routine refresh once the device is configured.
- Open cached data first. Show last successful refresh, offline/stale status,
  progress and useful errors without blocking play or discarding the cache.
- Keep a refresh tied to the character it requested: a late response must not
  replace the character the user has since switched to.
- Separate the imported character definition, the last source snapshot and
  local session changes. Preserve locally edited HP, slots, resources and effects
  by default. Show conflicts and let the user explicitly adopt source values.
- Preview meaningful build changes such as a new level, spell list, equipment,
  resource maximum or derived bonus. Preserve manual corrections and custom
  resources, and flag them for review if their source changes.
- Persist safely across restarts and firmware updates, with versioned settings,
  backup/export, validated imports and recovery from an interrupted save.

Wi-Fi supplies internet access. Bluetooth LE can provide a later paired setup
and configuration channel; it is not itself a connection to D&D Beyond. The
device should also offer a temporary local Wi-Fi setup network. Only enable
configuration access deliberately; credentials stay on the device.

## Import and configuration

| Input | Result |
| --- | --- |
| D&D Beyond link | Read character data, normalize it and present an import review. |
| Exported D&D Beyond PDF | Extract form fields first, then available text; map to the same settings format. |
| Existing companion configuration | Validate and restore a character or library backup. |
| Manual setup | Create or correct a character, homebrew option or resource without an online source. |

Both online and PDF paths produce the same versioned character model. Keep
source identifiers, rules versions and provenance alongside normalized values.
Treat missing information as missing, not as zero or an invented default. Show
uncertain values for review, especially calculated bonuses, spell preparation,
resource recharge rules and companion abilities. Image-only PDFs need an explicit
OCR-assisted review path; the PDF parser runs on the setup device, not the ESP32.

The unofficial D&D Beyond character service is an optional adapter, not a hard
dependency. Support public/user-accessible data without requesting account
cookies or passwords. A shared URL is not assumed to unlock private API access.
If access or the response format changes, retain the cache and offer PDF or
manual import. Uploading characters or session changes back to D&D Beyond is a
separate, unverified integration, not part of the initial refresh promise.

## Character at a glance

- Portrait, name, campaign, species, background, classes and levels.
- Current/max HP and temp HP, AC, initiative, speed, proficiency bonus, spell
  attack bonus, spell save DC and passive senses.
- A glanceable HP ring with a numeric equivalent. While turning the dial, show
  the pending change and let the player cancel it. After a plain heal, offer to
  treat it as temporary HP using the selected edition's rules.
- All six ability scores and modifiers, skill bonuses, saving throws, tools,
  proficiencies, expertise, senses, languages and defenses.
- Tap a number to see its calculation and roll where applicable. Distinguish
  imported, derived, overridden and unresolved values.
- Fast favorites for the player's most-used skills, saves, attacks and spells.
  Include checks and saves omitted by the competitor's counter-only workflow.

## Checks, saving throws and dice

- On an armed roll screen, tap Normal, Advantage or Disadvantage and spin the
  ring in either direction. Offer a touch Roll control. A roll settles once.
  Residual encoder movement leaves the result visible. Wheel speed affects
  neither the random draw nor its odds.
- Open **Dice tray** by tapping the character portrait or the center die on Checks or Saves. Seven die shapes follow the upper arc. Tap to add, hold to remove one every 0.5 seconds, or switch to Remove and tap. Tap the center or spin once. A numbered d20 tumbles in the direction and at a speed set by the ring, and the result shows the total and each face. Swipe vertically for more faces. Tap the total to edit. Each character keeps their last rolled tray. Pinned formulas and a flat tray bonus are planned.

- Saves presents all six abilities on one face. Checks presents the eighteen
  named skills in three groups of six, followed by the six plain abilities. Tap
  a name, swipe vertically to change groups, or turn to browse. Show the
  selected roll name, ability, bonus and training in the result.
- Ability checks, skills, tools, initiative and saving throws using the correct
  ability, proficiency, expertise and applicable feature modifiers.
- Normal, advantage and disadvantage rolls, situational bonuses and additional
  dice such as eligible spell or class effects. Show why each modifier applies.
- Optional DC comparison, with a clear result and complete roll breakdown.
  Do not treat every natural 20 or 1 as an automatic success or failure.
- Physical-dice mode: enter the real dice results with touch/dial and let the
  companion handle the calculation. Keep physical and device-generated rolls
  identifiable in the log.
- Roll d4, d6, d8, d10, d12, d20 and d100. Add dice at the player's pace. The pool
  remains editable until the player rolls. Use a brief result reveal with an instant option.
  A richer tumble is an optional effect subject to frame-time and memory checks on the unit.
  Highlight natural 20s and 1s when relevant to the roll. Keep at least eight
  named saved rolls per character, with at least three dice types plus a bonus.

## Actions, attacks, damage and healing

- Browse available actions, bonus actions, reactions, attacks and limited-use
  features. Separate always-available choices from choices blocked by resources
  or known effects; explain the reason.
- Show action cost, range/reach, targets, attack bonus or save DC, damage/healing
  components, damage types, prerequisites and available uses.
- Resolve weapon, unarmed and spell attacks using the correct ability and
  equipment. Distinguish attack rolls, save-based actions and automatic effects.
- Apply critical-hit rules only to eligible damage components. Support multiple
  attacks, multiple damage types, versatile weapons and extra damage when their
  conditions are met. Keep an explicit manual path for unsupported features.
- Calculate healing and damage with relevant resistances, immunities,
  vulnerabilities and reductions. Respect temp HP, HP limits and zero-HP rules.
  Confirm target context when it is not known rather than guessing an outcome.
- Spend slots, charges, ammunition and limited uses exactly once. Make the
  outcome and costs visible, with a reversible transaction in the session log.

## Spellcasting that helps during play

- Each detent moves to another spell. A vertical swipe or touch arrow does the
  same. Preview the casting source, attack bonus or save ability and DC,
  automatic effects, damage type, dice per hit and number of attacks or darts.
  Touch opens the spell. Turning while browsing cannot cast it.
- Resolve the spell version and the casting source before adding class features.
  Derive attack bonus and DC for that spell. Mark unsupported or overridden
  bonuses for review. A caster who uses a different pool to pay retains the
  original spell's casting ability.
- Eldritch Blast scales from one to two, three and four beams at total character
  levels 1, 5, 11 and 17. Each beam can have its own target and attack outcome.
  Double eligible damage dice on a critical and apply a selected Agonizing Blast
  bonus once per hit. [Spell](https://www.dndbeyond.com/spells/2619161-eldritch-blast)
  · [Invocation](https://www.dndbeyond.com/classes/2190885-warlock)
- Magic Missile delivers three darts at slot level 1 and one more per higher
  slot level. Allocate the darts, show 1d4 + 1 Force per dart and a subtotal for
  each target. The preview uses a separate damage die per dart as its table
  convention. Preserve the table's chosen convention and any applicable
  defenses. [Spell](https://www.dndbeyond.com/spells/2619022-magic-missile)
- Thunderwave shows **CON save** and the caster's **spell save DC**. Targets
  use their Constitution saving throws against that DC. Roll one area damage
  total, show full damage and push on failure, or half rounded down on success.
  Each higher slot adds 1d8 to its base 2d8 Thunder damage.
  [Spell](https://www.dndbeyond.com/spells/2619184-thunderwave)

- Browse known and prepared spells, cantrips, rituals and spells granted by
  items or features. Filter by level, casting time, concentration, favorites and
  currently usable options.
- Show casting time, range, components, duration, concentration, attack/save,
  damage/healing and an available description or concise reference.
- Cast flow: choose the spell, an eligible casting source, slot/pool and level;
  show the resulting effect and cost; then cast and record one transaction.
- Keep ordinary spell slots, Pact Magic, free casts, item charges and other
  casting resources distinct. Offer only options actually permitted by the
  selected spell and character features.
- Handle upcasting, character-level cantrip scaling, multiple beams/targets,
  ritual and other supported casting variations. Expose unsupported choices for
  a manual ruling instead of silently applying incomplete automation.
- Track concentration and prompt for relevant saves after damage. Starting a
  new concentration effect clearly replaces the old one after the user commits.

## Resources, rests and progression

- Class and level setup supplies hit dice, spellcasting progression and class
  resources, including supported subclass options and manual/homebrew setup.
- Multiclass profiles support at least three classes and 20 total levels where
  the selected rules allow, with separate Pact Magic and ordinary slot pools.
- Track at least 24 configurable resources per character as uses or numbers,
  including feats, item charges, ammunition and subclass features. Give each an
  explicit reset rule, including short rest, long rest or manual recovery.
- Preview and apply short/long rests, mixed-class hit-die spending and healing,
  eligible feature recovery and edition-specific hit-dice recovery.
- Support resource conversions, including Sorcery Points and eligible slots,
  with caps, costs, expiry and recovery kept distinct. A rest must not refill
  every counter indiscriminately.
- Guided level changes show HP gain, resource maxima and affected choices.
  Allow rolled HP or an applicable average. Imported level-ups enter review;
  unselected feats, spells and other build choices are not chosen automatically.

## Conditions, effects and zero HP

- Conditions, exhaustion, inspiration and temporary effects, including their
  source, targets, duration and known mechanical consequences.
- Round-based and timed effects use the appropriate clock. The user advances
  turns/rounds explicitly when the companion does not know the table's order.
- Zero-HP/death-save flow supports rolled and entered results, damage while
  down, stabilization and healing, with the appropriate rules profile.
- Surface critical states and applicable rules; keep DM adjudication and manual
  correction available. Make all state changes reversible and explainable.

## Inventory, features and companions

- Inventory, currency, equipment, attunement, ammunition and consumables, with
  derived-stat changes shown when equipment changes.
- Class, subclass, species, background and feat features: useful references,
  selected options and available uses, with manual additions/corrections.
- Companions, familiars and summons have their own HP, defenses, actions and
  resources, linked to the owning character. Show command/action costs and
  effects that apply to a companion rather than mixing them into the player's HP.
- Keep user artwork and licensed character material local. Public examples and
  tests use synthetic or appropriately licensed data.

## Useful, fun and easy

| Feature | How it helps at the table |
| --- | --- |
| My Turn view | Bring together favorite actions, bonus actions, reactions, movement and active-effect reminders. This is a local turn aid, not an assumption about the DM's initiative order. |
| What can I do now? | Surface usable favorites and relevant reactions using known resources and effects, with a reason for suggestions. |
| Quick reaction access | Reach a reaction from the current screen and show its trigger and cost. The user confirms that the trigger occurred. |
| Cast assistant | Compare valid slot choices and show upcast results before spending, particularly for multiclass casters. |
| Explain this roll | Expand a total into dice, bonuses and feature effects so the player can answer the DM quickly. |
| Physical-dice mode | Keep the pleasure of rolling real dice while reducing mental arithmetic. |
| Quiet companion reminders | Optional, brief reminders for concentration, recurring effects, a companion's command or resource recovery. |
| Portraits and companion identity | Character artwork, readable themes and a companion view give the device personality without obscuring stats. |
| Optional feedback | Haptics and short roll animations for results and important state changes, with quiet/reduced-motion modes. |
| Session resume | Restore the character, screen and local play state without starting setup again. |
| Session recap | A compact log of rolls, damage, healing, casts, resources and rests. Offer immediate Undo and allow undoing the most recent change from the log after the prompt expires. |

The main play loop should take a few deliberate interactions, not a trip through
settings. Keep core mechanics deterministic and available offline. Personality
and suggestions must not invent rules, require a cloud model or interrupt a
player's turn.

## Proposed game profiles and rules design

The dial reads supported 2014 and 2024 D&D rules from each character’s sheet. The table below describes **proposed capabilities**. Per-class Hit Dice recovery remains a design target for D&D. The additional game profiles are proposals. [Current coverage by book](RULES.md#rulebooks-and-expansions)

| Game profile | Expected behavior |
| --- | --- |
| D&D 5e 2014 and 2024 | Use the selected profile for class progression, rests, hit dice, effects and death saves. |
| D&D 3.5 | Track spells per day by class, negative HP and stabilization, and flag massive damage. |
| Pathfinder 1e | Track class resources and spells per day, stabilization and the Constitution-based death threshold. |
| Call of Cthulhu 7e | Track Sanity, Luck, Magic Points and major wounds. Resolve percentile skill checks and their degrees of success. |
| Daggerheart | Track Hope, Stress and Armor Slots, roll Duality Dice, and resolve downtime choices and death moves. |
| Other | Provide configurable HP, stats, counters and dice for games without a dedicated profile. |

The current design scope is player play. Full Pathfinder 2e rules, DM encounter
tools and synchronized party play can be evaluated separately.

Each automated outcome must retain its inputs, applicable rules and modifiers,
resource costs and result. Validate feature-specific eligibility: for example,
advantage on concentration saves is not advantage on every Constitution save.
Unknown mechanics stay visible as unresolved and offer a manual path. A sheet's
raw base HP or ability score must not be presented as its resolved total.

Use regression cases for multiclass slot pools, effects on checks/saves,
critical damage, temp HP, concentration, death saves, resource conversions,
edition-specific rests, switching characters during a refresh, and conflicting
source/local changes. On-device acceptance includes usable touch/dial navigation,
responsive input during refresh and offline play after a restart.

## Hardware and technical direction

- Target the Waveshare ESP32-S3-Knob-Touch-LCD-1.8: 360x360 round capacitive
  touchscreen, rotary encoder, Wi-Fi, Bluetooth and local persistent storage.
- The firmware owns the character library, cache, play state, rules engine and
  touch/dial UI. A local setup interface handles longer configuration and PDF
  parsing. Use one shared versioned settings model for every import path.
- Stream/filter remote data and bound caches rather than loading unrestricted
  full responses into display memory. Do network work outside the UI loop, with
  timeouts, retries/backoff, validated HTTPS and atomic cache replacement.
- Validate the actual panel revision, display/touch drivers, encoder behavior,
  storage, memory headroom and power behavior on hardware before claiming device
  support. Firmware refresh and character-data refresh are separate operations.

## References

- [Exact-board constraints and validation](HARDWARE.md) govern graphics,
  navigation, memory, radios, peripherals and on-unit acceptance.

- [Soul Dial D&D companion](https://souldial.net/pages/dnd) for the reference
  interactions and supported game modes.
- [Waveshare hardware documentation](https://www.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8).
- [D&D Beyond PDF export](https://dndbeyond-support.wizards.com/hc/en-us/articles/7747238449556-Export-Sheet).
- [D&D Beyond API status discussion](https://www.dndbeyond.com/forums/d-d-beyond-general/general-discussion/192944-api-questions).
- [D&D SRD](https://www.dndbeyond.com/srd) for applicable open rules.
