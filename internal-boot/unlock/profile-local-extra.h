/* GPL-2.0-only. LOCAL two-card extension, hand-written 2026-11-21 (not generated).
 * Included by machine-profile.h after profile-local.h only with -DGU_LOCAL_SECOND_CARD.
 * Builds without that define stay the single-card local profile.
 *
 * Second card on this machine: HP RTX 3060 12 GB (GA106, 10de:2503, subsystem 103c:8903,
 * VBIOS 94.06.14.40.c0) at 0:25:00.0, BAR0 0xf9000000, no display children
 * (monitor on the console card). Values come from two agreeing read-only probe
 * captures (probe 5474e122, select-bus 0x25):
 *   Desktop/gpu_unlock_backup/probe_twocard_2026-11-21/DIAG00.TXT, DIAG01.TXT, PROM.BIN, IMG24.BIN
 * Firmware nvgop on this card: nvgop-ga1xx 0x6000b, image 0x2bec0, Stop RVA 0x698,
 * .text 0x244ba bytes at RVA 0x280, sha256 61246a83... This is byte-identical to the
 * GOP of upstream PR #116 (ASRock B450M + RTX 3060 2503), which completed a clean
 * hardware unlock. The console card stays the generated profile-local.h card. */
#pragma once
#define GU_LOCAL_TWO_CARD 1
#define GU_EXTRA_BOARD_ID 0x250310deU
#define GU_EXTRA_BOARD_SUBSYSTEM 0x8903103cU
#define GU_EXTRA_TARGET_SEGMENT 0x0U
#define GU_EXTRA_TARGET_BUS 0x25U
#define GU_EXTRA_CHIP_ID 0xb76000a1U
#define GU_EXTRA_DEVICE_MEMORY_SIZE 0x3000U
#define GU_EXTRA_PROM_SHA256 "99885e570f70ef47eb819b558c902b9a453030bf3ab31900691a0e08758150d2"
#define GU_EXTRA_NAME "LOCAL-250310de-8903103c-99885e57"
#define GU_EXTRA_NVGOP_VERSION 0x6000bU
#define GU_EXTRA_NVGOP_IMAGE_SIZE 0x2bec0U
#define GU_EXTRA_NVGOP_STOP 0x698U
#define GU_EXTRA_NVGOP_TEXT_BYTES 0x244baU
#define GU_EXTRA_NVGOP_TEXT_SHA256 "61246a83c52e4ca5d93d2aa878c1844f61a7bfe5f26d70a701706fb20f348921"
