# AGENTS.md

Portal game code reimplemented on Source SDK 2013 (no Valve Portal source used).
Most of `src/` is unmodified upstream SDK — the actual project code is:

- `src/game/shared/portal/`, `src/game/server/portal/`, `src/game/client/portal/` (game DLLs)
- `src/game/server/server_portal.vpc`, `src/game/client/client_portal.vpc`, `src/launcher_main/launcher_main_portal.*`
- `src/vpc_scripts/projects.vgc` registers those projects behind the `$PORTAL` condition
- `#ifdef PORTAL` hooks are scattered through stock SDK files (`physics.cpp`, `viewrender.cpp`, `basecombatcharacter.cpp`, `hl2_player.cpp`, `view_beams.cpp`, `hud_crosshair.cpp`, `baseentity_shared.cpp`, etc.) — expect edits in both project dirs and stock SDK files

Primary docs (Japanese): `README.md` (setup/build/run), `docs/architecture.md` (design, data flow), `docs/status.md` (what is implemented vs. stubbed). Keep new docs consistent with these.

## Build (Linux)

```bash
cd src && ./buildportal            # release; ./buildportal debug for debug
```

- Requires **x86_64 Linux + podman**; `buildportal` runs VPC/ninja inside a Steam Runtime Sniper container (`src/sdk_container`). The bundled `devtools/bin/vpc` is an x86_64-only binary — it cannot run on aarch64 builds of this repo
- No cmake/make/autotools — it's VPC + ninja. VPC generates ninja files under `src/_vpc_/ninja/` only when missing; delete the `sdk_portal_<mode>.ninja` file there to force VPC regeneration (e.g. after changing `.vpc`/`.vgc` files)
- `compile_commands.json` (clangd) is generated once by `buildportal` together with the ninja files, with a few unsupported flags patched out via `sed` — it is not regenerated automatically
- Windows: run `src/createportalprojects.bat`, build the generated `src/portal.sln` in VS 2022

There is **no test suite and no CI** — verification is: does it compile, then does the game run.

Outputs: client/server DLLs in `game/portal/bin/<platform>/`, launcher `game/portal_linux64` (Windows: `game/portal_win64.exe`).

## Run

- Requires Steam installs of **Source SDK 2013 Multiplayer (AppID 243750)** and **Portal (AppID 400)** — the 243750 launcher is what actually starts the engine, so both are mandatory
- Portal's content must be linked/copied to `PortalData/` at the repo root (gitignored, never commit it): `ln -s "$HOME/.steam/steam/steamapps/common/Portal/portal" PortalData`
- Run from the `game/` directory: `./portal_linux64` (the launcher infers mod name from its filename and passes `-game <repo>/game/portal`, per `game/portal/gameinfo.txt`)
- Load a map via in-game console, e.g. `map testchmb_a_00`

## Known constraints / gotchas

- The dev environment used for much of this work was aarch64 without podman, so parts of the codebase have never been compiled or run (see `docs/status.md` §1). Don't assume the code builds just because it's committed
- Player portal traversal is intentionally unimplemented (a prior partial impl was removed because it could wall-clip); `CPortalGameMovement` is stock HL2 movement. See `docs/status.md` §3.3 before "restoring" it
- Portals are not BSP holes — traced/rendered/teleported in software; props cross via end-of-frame teleport (`PortalSimulator.cpp`), not continuous physics
- Trace results that cross a portal return hit positions already in the *linked portal's* space (fraction is mapped back to the original ray)
- Callers must handle coordinates correctly across portal spaces; `IsActivatedAndLinked()` guards against uninitialized corners
- Only one blue/orange portal pair exists; placement has no surface-material restriction yet
- Repo/SDK docs majority is Japanese; README-src.md is the upstream SDK README
- Commit hygiene: `.gitattributes` normalizes many extensions to `text` (LF); never commit `PortalData/` or build outputs (`game/portal/bin/*`, `game/portal_linux64`, `src/_vpc_/`, `compile_commands.json`)
