# Two cards in one boot (local USB route)

Fork branch `two-card-unlock`, built on upstream PR #82 (adaptive multi-card unlock) and
the GA106 2503 evidence of PR #116. One exact machine, hardware-tested once; treat it as
research, not a supported profile.

## Result

| | Machine |
|---|---|
| Board | MSI B350 Tomahawk (AMI), Ryzen 7 5700X, no iGPU, Windows 10 Pro |
| Console card | ASUS RTX 3060 12 GB, `10de:2504` / `1043:881D`, VBIOS 94.06.25.00.b6, top x16, bus 0x29, nvgop-ga1xx 0x6000a |
| Second card | HP RTX 3060 12 GB, `10de:2503` / `103c:8903`, VBIOS 94.06.14.40.c0, chipset x4 slot, bus 0x25, nvgop-ga1xx 0x6000b, no display |

One cold boot from USB (Secure Boot off): both transactions `result=0 containment=0
quiesced=1 fault=0 retained=0 maps=0 FLR=1`, both cards `SS0=0x88888888 SS1=0x8`,
`FINAL status=0 attempted=1 driver_release=1 clean=1`, wrapper `verify-complete`, Windows
started with both cards on the NVIDIA driver.

8192x8192 dense GEMM, PyTorch, FP16 / BF16 / TF32 TFLOPS:

| Card | Before | After |
|---|---|---|
| 2503 (bus 0x25) | 26.8 / 26.8 / 13.3 | 48.0 / 49.4 / 24.0 |
| 2504 (bus 0x29, display) | 23.5 / 22.4 / 12.1 | 48.0 / 48.6 / 23.8 |

Not established: repeated cold boots, restart, sleep, other boards, other card pairs.

## What differs from PR #82

PR #82 was written for two cards that share one firmware GOP image on an aggregate AMI
console. Here the two cards carry different option-ROM GOP images (0x6000a and 0x6000b)
and the console is the non-aggregate, no-EDID-driver headless type. Changes:

- **Exact discovery.** A card registers only on an exact row: PCI ID, subsystem and (when the
  row pins it) segment/bus at device 0 function 0. One card per row, every row present;
  a missing or duplicate card refuses the whole run. (PR #82 review finding: a same-ID card
  with another subsystem was registered.)
- **Per-card driver pin.** The child releases a non-console card's firmware driver only when it
  is that row's exact nvgop: binding version, image size, Stop RVA and the `.text` SHA-256
  (`release-pin.h`).
- **Wrapper tolerance for non-aggregate consoles.** After the child ran, exactly one baseline
  edge may be missing: the PciIo BY_DRIVER open of that pinned nvgop image, whose option ROM
  is the exact second card (location, ID, subsystem re-read). No cache; every other edge
  change still blocks the Windows handoff.
- **Final gate restored.** Right before each transaction no open may remain on the GPU handle
  except the child's own exclusive claim (the single-card invariant PR #82 dropped). Only
  opens on the GPU handle count: the PCI bus driver names every GPU as a child controller.
- **Single-card builds unchanged.** `GuInit` preloads the compile-time card; automatic
  single-card builds never release a driver in the child; no AllHandles scan; exact BAR0 pin
  and 64-bit BAR refusal kept; original log text kept. The existing 22 local host jobs pass
  unchanged.
- **Two-card read-only probe.** `build-probe.ps1 -TargetBus N -OtherNvidia` captures one card
  while other NVIDIA display controllers are logged by location and ID only (no config dump,
  MMIO or PROM read). The generic envelope admits `10de:2503`.

## Using it on another machine

There is no generator for the second card yet. The route used here:

1. Generate the console card's local profile as usual (`new-machine.ps1`), with that card alone
   or with `-TargetBus`/`-OtherNvidia`.
2. Probe the second card: `new-machine.ps1 ... -TargetBus <bus> -OtherNvidia`, two agreeing
   cold captures. Read its nvgop version, image size, Stop RVA and `.text` hash from the
   captured driver image; run `fwsec-locate` on its PROM.
3. Write `internal-boot/unlock/profile-local-extra.h` by hand (see the file in this branch).
   Only build with it when the second card's GOP is a hardware-proven image.
4. `research/profile/test-profile.ps1` runs two extra jobs when that header exists (discovery and
   pin checks; wrapper release replay with the real console graph and the second card's image).
5. `research/headless/build-usb.ps1 -HardwareProfile local -SecondCard`, then `stage-usb.ps1`.

Recovery for the USB route is pulling the stick. A failure after a driver release halts with a
black screen: hold power.
