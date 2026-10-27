# vkQuake2 for PS Vita

This is the Vulkan renderer of vkQuake2 running on a PS Vita. There is no
OpenGL in here: the game talks Vulkan, and [VkVita](https://github.com/DrDecki/vkvita)
turns that into GXM calls for the Vita GPU. The shaders are the original
SPIR-V ones, translated when the game starts.

Everything Vita specific lives in this folder. The rest of the tree is
vkQuake2 with a handful of small changes, all marked with `__vita__`.

## Game data

You need the data of the full game, the 3.20 version. Copy `pak0.pak` from
the `baseq2` folder of your Quake II install to

```
ux0:data/vkquake2/baseq2/pak0.pak
```

For the music copy the `.ogg` tracks from the `music` folder of the GOG or
Steam release to

```
ux0:data/vkquake2/baseq2/music/
```

The game runs fine without them, there is just no music then. `cd_volume`
sets the volume, and `miniaudio play 5` or `miniaudio stop` work in the
console like on the PC.

Logs end up in `ux0:data/vkquake2/`: `vkquake2.log` is the game console and
`vkvita.log` is the renderer. The first start takes a while because every
shader gets compiled once, later starts load them from
`ux0:data/vkquake2/shaders/`.

## Controls

| Button | Action |
|---|---|
| Left stick | Move |
| Right stick | Look |
| R | Fire |
| L, Cross | Jump |
| Circle | Crouch |
| Square, Triangle | Previous and next weapon |
| D-pad | Inventory |
| Select | Help computer |
| Start | Menu |

In the menus the d-pad moves, Cross selects and Circle or Start goes back.
Everything can be rebound in the menu, the buttons show up as AUX keys there.

## Multisampling

The edges are smoothed with 2x MSAA by default. 4x also runs at 60 FPS in
most places but leaves the GPU less room in big fights. Both can be set under
Options, Video, multisampling.

## Building

You need [VitaSDK](https://vitasdk.org) and a checkout of VkVita.

```sh
cmake -S vita -B build-vita -DVKVITA_DIR=/path/to/vkvita
cmake --build build-vita -j
```

The VPK ends up in `build-vita/vkquake2.vpk` with the title ID `VKQUAKE02`.

`vita/host/build_host.sh` builds the whole game for a PC against the VkVita
mock, which is how crashes get caught before they reach a Vita.

## What does not work yet

- Network play, only the local server is there
- `vk_showtris`, GXM has no line strips

## License

GPL 2.0 or later, like Quake II and vkQuake2.
