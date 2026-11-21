/* GPL-2.0-only. Local two-card lane (2026-11-21), header-only so ownership.c keeps no hash
 * dependency. Included by app.c and tests/test_two_card.c. */
#pragma once
#include "ownership.h"
#include "sha256.h"

/* Local two-card lane (2026-11-21): a card row that pins its firmware nvgop allows
   the guarded release of exactly that driver image: binding version, image size,
   Stop RVA inside .text, and the .text bytes at RVA 0x280. A row without a pin
   keeps the reviewed PR #82 behaviour. No file IO: safe right before the disconnect. */
static EFI_STATUS GuReleaseOwnerMatchesSpec(const GU_RELEASE_OWNER *Owner,const GU_CARD_SPEC *Spec) {
  if(!Spec || !Owner || !Owner->agent)return EFI_ACCESS_DENIED;
  if(!Spec->nvgop_text_sha256)return EFI_SUCCESS;
  if(!Spec->nvgop_version || !Spec->nvgop_image_size || !Spec->nvgop_stop || !Spec->nvgop_text_bytes)return EFI_ACCESS_DENIED;
  UINTN base=(UINTN)Owner->base;
  if(!base || Owner->version!=Spec->nvgop_version || Owner->size!=Spec->nvgop_image_size ||
     (UINTN)Owner->stop!=base+Spec->nvgop_stop || 0x280ULL+Spec->nvgop_text_bytes>Owner->size ||
     Spec->nvgop_stop<0x280U || Spec->nvgop_stop>=0x280U+Spec->nvgop_text_bytes)return EFI_ACCESS_DENIED;
  return GuHashMatches((const UINT8 *)base+0x280,Spec->nvgop_text_bytes,Spec->nvgop_text_sha256)?EFI_SUCCESS:EFI_CRC_ERROR;
}
