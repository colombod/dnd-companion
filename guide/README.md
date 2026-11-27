# Player guides on GitHub Pages

On the dial, tap the top status arc and open **Firmware** to see the installed version. Each published firmware has its own illustrated manual at `guide/vX.Y.Z/` with installation at `install.html`, released features at `features.html`, and book coverage at `guide/vX.Y.Z/rules.html`. The home page highlights the highest stable version. Earlier guides sit in a collapsed archive and each archived page points to its current counterpart. Put the permanent matching guide URL in each GitHub Release's notes.

## Preview on a computer

Install Python 3 and Pandoc 3. Run these commands from the repository root and open [the local preview](http://localhost:8771/).

```sh
python3 tools/build_player_guide.py --preview
python3 -m http.server 8771 --bind 127.0.0.1 --directory .build/player-guide-preview
```

Preview pages use the working copy and visibly identify the development build. The **Check USB** link opens the orientation checker and qualified one-time 0.1.4 USB installer bundled with the guide. The build verifies the pinned Espressif library on download. The English manual covers the dial's English, Italian and Spanish language settings.

## Publish a qualified release's guide

1. Set **Settings → Pages → Source → GitHub Actions**. Put the guide workflow on the default branch before cutting the release. The `github-pages` environment can keep its default-branch deployment restriction. [GitHub Pages publishing sources](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site)
2. Replace the single plain-text paragraph under `## What’s new in X.Y.Z` in `docs/FEATURES.md` with what changed since the preceding stable firmware. Keep `## What works today` limited to shipped player actions and put proposals in `docs/FEATURE-DESIGN.md`. The build checks the release heading against the published version and reuses its paragraph on the home page. Include the workflow, manual, `docs/INSTALL.md`, marked player sections in `docs/FEATURES.md` and `docs/RULES.md`, screenshots and `firmware/VERSION` in the release commit. Tag that commit `vX.Y.Z`. Keep published tags fixed so links continue to describe the firmware players installed.
3. After qualification, publish the stable GitHub Release against that tag in `colombod/dnd-companion`. Publication starts **Publish player guides**, which requests a build and deployment from the default branch. To retry, use **Actions → Publish player guides → Run workflow** on that branch. Only stable releases enter the guide index. The firmware distribution feed has its own publication step. [GitHub release events](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#release)
4. Open the URL returned by the Pages deployment, choose the released version, and follow a setup link and a spell link. Check that the previous version still opens at its original URL.

The job enumerates published stable firmware releases in the source repository, reads each matching tag, and builds the manual and its referenced guide images from that tag. A mismatch with `firmware/VERSION`, a missing asset or an empty release list stops the build. Only the generated site directory is uploaded.

### Publish a correction to installation instructions

Include the corrected installation page, preparation script and images in a PR. Give each correction its own annotated tag, such as `guide-v0.1.5-usb.1`, and set the matching firmware entry in `INSTALL_REVISIONS` to its full `refs/tags/...` name. After merging, tag the reviewed revision and push the new tag before dispatching Pages. Keep published correction tags fixed. The installation page and its referenced assets are read from that tag. For a correction to the manual, feature list or rules, use a separate annotated tag in `PLAY_REVISIONS` (for example `guide-v0.1.7-play.1`). Those pages and their images come from the pinned correction. Other releases continue to use their own tagged text. Publish the reviewed correction tag before dispatching Pages.

For 0.1.5 the distribution release supplies `first-usb-0.1.5.zip`. Pages verifies its SHA-256 and serves it beside the versioned installation page. To qualify a local build before uploading the ZIP, place the tested archive at `.build/first-usb-0.1.5.zip` and build with `--published`. After merging and uploading the release asset, run **Publish player guides** on `main`. From the live home page choose **Install or update**, download the ZIP, and compare all four flash images and `flash_args` against the hardware qualification kit. Follow **Reproduce the installer files** to verify the hosted preparation script independently.

After the clean history rewrite and first release, protect `main` with an active branch rule requiring a pull request for every change, including administrator changes. Block force pushes and deletion. Subsequent releases use tags on merged commits.
