<p align="center"><img src="vkQuake2.png"></p>

# vkQuake2 for PS Vita

This is Quake II running on the PS Vita with the Vulkan renderer from
[vkQuake2](https://github.com/kondrak/vkQuake2). Not a software renderer,
not OpenGL, but the actual Vulkan code. The Vita has no Vulkan
driver, so this port runs on [VkVita](https://github.com/DrDecki/vkvita), a
Vulkan layer Claude wrote on top of the Vita's GXM graphics API. Even the
SPIR-V shaders of vkQuake2 are used. The Vita GPU can't run SPIR-V, so VkVita
translates them to Cg and compiles them. The compiled shaders ship with the
VPK, so you don't need the shader compiler on your Vita.

What works:

- the whole single player campaign
- the mission packs The Reckoning and Ground Zero, plus Zaero and Capture the Flag
- deathmatch over Wi-Fi, against other Vitas or PCs
- sound effects and the ogg soundtrack
- 4x MSAA
- 60 FPS almost everywhere

## What you need

- a PS Vita with HENkaku or Enso and VitaShell
- the game files of Quake II. They are not included, you need your own copy
  (Steam, GOG or the original CD, version 3.20)

## Installation

1. Download `vkquake2.vpk` from the
   [releases](https://github.com/DrDecki/vkquake2-vita/releases) and install
   it with VitaShell.

2. Create the folder `ux0:data/vkquake2/baseq2` on your Vita.

3. Copy these files from the `baseq2` folder of your Quake II install into it:

   ```
   ux0:data/vkquake2/baseq2/pak0.pak
   ux0:data/vkquake2/baseq2/pak1.pak
   ux0:data/vkquake2/baseq2/pak2.pak
   ux0:data/vkquake2/baseq2/players/
   ```

   `pak0.pak` is all you need for the single player game. `pak1.pak` has the
   deathmatch maps, and the `players` folder has the player models. Without
   it other players show up as white diamonds.

4. Optional: for music, copy the `.ogg` tracks (`track02.ogg` to
   `track21.ogg`) from the `music` folder of the GOG or Steam release into
   `ux0:data/vkquake2/baseq2/music/`. Tracks 12 to 21 are the music of the
   mission packs.

5. Start the game.

## Mission packs

The Reckoning and Ground Zero need their own game files, they come with the
GOG and Steam release. Copy the folders next to `baseq2`:

```
ux0:data/vkquake2/xatrix/pak0.pak     The Reckoning
ux0:data/vkquake2/rogue/pak0.pak      Ground Zero
```

Zaero goes into `zaero` and Capture the Flag into `ctf` the same way.

Then pick Game, change game in the menu. The game restarts with the mission
pack loaded. The mission packs play their music from the GOG and Steam tracks
in `baseq2/music`, nothing extra to copy.

## Controls

| Button | Action |
|---|---|
| Left stick | Move |
| Right stick | Look |
| R | Fire |
| L or Cross | Jump |
| Circle | Crouch |
| Square / Triangle | Previous / next weapon |
| D-pad | Inventory |
| Select | Help computer |
| Start | Menu |

In menus the d-pad moves, Cross selects and Circle goes back. You can rebind
everything in the controls menu. The buttons show up as AUX keys there. The
look speed is `in_lookspeed` and `in_lookinvert 1` inverts the right stick.

## Multiplayer

Connect the Vita to Wi-Fi before you start the game.

**Hosting on the Vita:** Multiplayer, Start Network Server, pick a map and
start. Other players connect to the Vita's IP address. The game prints it in
the console on startup, and you can also find it in the Vita's network
settings.

**Joining from the Vita:** Multiplayer, Join Network Server. Servers on your
local network show up in the list. To join one directly, add its IP address
to the address book, for example `192.168.1.20` or `192.168.1.20:27910`. Host
names are not resolved, so use the IP.

**Playing against a PC:** any Quake II client that speaks the original
network protocol works. I tested it with
[Yamagi Quake II](https://www.yamagi.org/quake2/). Put the same `pak` files
and the `players` folder into its `baseq2` folder and type
`connect <ip of the vita>` in its console.

## Settings

Antialiasing can be changed under Options, Video. 4x is the default and
costs no more than 2x on the Vita. If you played an older version, your
saved settings keep 2x until you switch it there.

## If something goes wrong

Everything gets logged to `ux0:data/vkquake2/`:

- `vkquake2.log` is the game console
- `vkvita.log` is the renderer

If the game closes on its own, the reason is usually at the end of
`vkquake2.log`. Please attach both files when you open an issue.

## Building it yourself

You need [VitaSDK](https://vitasdk.org) and a checkout of
[VkVita](https://github.com/DrDecki/vkvita).

```sh
cmake -S vita -B build-vita -DVKVITA_DIR=/path/to/vkvita
cmake --build build-vita -j
```

That gives you `build-vita/vkquake2.vpk` (title ID `VKQUAKE02`). All the Vita
code is in the `vita` folder. The changes to the rest of the code are small
and marked with `__vita__`. `vita/host/build_host.sh` builds the whole game
for Linux against the VkVita test mock, which is handy for catching crashes
before you copy anything to the Vita.

The compiled shaders in `vita/shaders` go into the VPK. If a change to
VkVita alters the generated shaders, delete that folder, run the game once
with `libshacccg.suprx` installed and copy the new files from
`ux0:data/vkquake2/shaders` back into it.

## Credits

- id Software for Quake II and its source code
- Krzysztof Kondrak for vkQuake2. The original README is in
  [README-vkQuake2.md](README-vkQuake2.md)
- Felipe Izzo for vitaQuakeII, which parts of the system code are based on
- [stb_vorbis](https://github.com/nothings/stb) by Sean Barrett for the music

## License

GPL 2.0 or later, like Quake II and vkQuake2. The game data is not covered
by this and is not part of the project.
