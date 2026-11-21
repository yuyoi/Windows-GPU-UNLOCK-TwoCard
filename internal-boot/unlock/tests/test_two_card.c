/* GPL-2.0-only. Local two-card lane (2026-11-21): real GuDiscoverTargets and
 * GuReleaseOwnerMatchesSpec behind RAM-only EFI mocks. No GPU, firmware or boot access.
 * Build: -DGU_PROFILE_LOCAL -DGU_LOCAL_SECOND_CARD (console card = profile-local.h,
 * second card = profile-local-extra.h). */
#define main legacy_port_main
#include "test_port.c"
#undef main
#include "../release-pin.h"
#ifndef GU_LOCAL_TWO_CARD
#error test_two_card.c needs -DGU_LOCAL_SECOND_CARD
#endif

static unsigned Checks;
static void Check(int ok,const char *name) {Checks++;if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}}

typedef struct {EFI_PCI_IO_PROTOCOL io;UINT32 id,sub;UINTN seg,bus,dev,fun;BOOLEAN children;} CARD;
static CARD Cards[4];static UINTN CardCount;
static EFI_HANDLE CardHandle(UINTN i){return (EFI_HANDLE)(UINTN)(0x1000+i);}
static CARD *CardOf(EFI_PCI_IO_PROTOCOL *p){for(UINTN i=0;i<CardCount;i++)if(&Cards[i].io==p)return &Cards[i];return NULL;}
static EFI_TPL Tpl=TPL_APPLICATION;static unsigned InfoAtLowTpl;
static EFI_TPL EFIAPI TcRaise(EFI_TPL t){EFI_TPL o=Tpl;Tpl=t;return o;}
static VOID EFIAPI TcRestore(EFI_TPL t){Tpl=t;}
static EFI_STATUS EFIAPI TcFree(VOID *p){free(p);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI TcLocate(EFI_LOCATE_SEARCH_TYPE s,EFI_GUID *g,VOID *k,UINTN *n,EFI_HANDLE **out) {
  (void)k;EFI_GUID pg=EFI_PCI_IO_PROTOCOL_GUID;
  Check(s==ByProtocol && g && !memcmp(g,&pg,sizeof(pg)),"discovery enumerates PciIo only");
  *out=malloc((CardCount?CardCount:1)*sizeof(EFI_HANDLE));*n=CardCount;
  for(UINTN i=0;i<CardCount;i++)(*out)[i]=CardHandle(i);
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI TcHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  (void)g;UINTN i=(UINTN)h-0x1000;if(i>=CardCount)return EFI_UNSUPPORTED;*out=&Cards[i].io;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI TcRead(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 at,UINTN n,VOID *out) {
  CARD *c=CardOf(p);Check(c && w==EfiPciIoWidthUint32 && n==1 && (at==0 || at==0x2c),"identity reads only");
  *(UINT32 *)out=at?c->sub:c->id;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI TcLocation(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f) {
  CARD *c=CardOf(p);*s=c->seg;*b=c->bus;*d=c->dev;*f=c->fun;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI TcInfo(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **e,UINTN *n) {
  (void)g;if(Tpl<TPL_CALLBACK)InfoAtLowTpl++;
  UINTN i=(UINTN)h-0x1000;*n=0;*e=NULL;
  if(i<CardCount && Cards[i].children) {
    *e=calloc(1,sizeof(**e));(*e)[0].Attributes=EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER;(*e)[0].OpenCount=1;*n=1;
  }
  return EFI_SUCCESS;
}
static void AddCard(UINT32 id,UINT32 sub,UINTN seg,UINTN bus,BOOLEAN children) {
  CARD *c=&Cards[CardCount++];memset(c,0,sizeof(*c));
  c->io.Pci.Read=TcRead;c->io.GetLocation=TcLocation;
  c->id=id;c->sub=sub;c->seg=seg;c->bus=bus;c->children=children;
}
static void Machine(void) {
  CardCount=0;InfoAtLowTpl=0;Tpl=TPL_APPLICATION;
  memset(&bs,0,sizeof(bs));
  bs.LocateHandleBuffer=TcLocate;bs.HandleProtocol=TcHandle;bs.OpenProtocolInformation=TcInfo;
  bs.RaiseTPL=TcRaise;bs.RestoreTPL=TcRestore;bs.FreePool=TcFree;
  GuInit(&bs,NULL);
}
/* The real machine: ASUS console card first in PCI order is not assumed. */
static void RealPair(BOOLEAN hpFirst,BOOLEAN asusChildren) {
  Machine();
  if(hpFirst)AddCard(GU_EXTRA_BOARD_ID,GU_EXTRA_BOARD_SUBSYSTEM,GU_EXTRA_TARGET_SEGMENT,GU_EXTRA_TARGET_BUS,FALSE);
  AddCard(GU_BOARD_ID,GU_BOARD_SUBSYSTEM,GU_TARGET_SEGMENT,GU_TARGET_BUS,asusChildren);
  if(!hpFirst)AddCard(GU_EXTRA_BOARD_ID,GU_EXTRA_BOARD_SUBSYSTEM,GU_EXTRA_TARGET_SEGMENT,GU_EXTRA_TARGET_BUS,FALSE);
}

static UINT8 Image[0x2c000];
static void SpecOwner(GU_RELEASE_OWNER *o,const GU_CARD_SPEC *s) {
  memset(o,0,sizeof(*o));o->agent=(EFI_HANDLE)0x77;o->image=(EFI_HANDLE)0x77;o->base=Image;o->size=s->nvgop_image_size;
  o->version=s->nvgop_version;o->stop=(EFI_DRIVER_BINDING_STOP)(Image+s->nvgop_stop);
}

int main(int argc,char **argv) {
  /* Discovery: both rows pinned, any PCI order, console last. */
  for(int order=0;order<2;order++)for(int kids=0;kids<2;kids++) {
    RealPair(order,kids);
    Check(GuDiscoverTargets()==EFI_SUCCESS && GuTargetCount()==2,"both reviewed cards register");
    Check(!InfoAtLowTpl,"open-protocol information is read at raised TPL");
    const GU_CARD_SPEC *a=GuTargetSpec(0),*b=GuTargetSpec(1);
    Check(a && b && a!=b,"two distinct rows");
    Check((a->id==GU_EXTRA_BOARD_ID && a->bus==GU_EXTRA_TARGET_BUS) || !kids,"a console card with children is processed last");
    for(UINTN i=0;i<2;i++) {
      GuActivate(i);
      const GU_CARD_SPEC *s=GuTargetSpec(i);
      Check(Gu.board_id==s->id && Gu.board_subsystem==s->subsystem && Gu.bus==s->bus && Gu.segment==s->segment &&
            Gu.expected_chip==s->chip && Gu.expected_memory==s->memory && Gu.prom_sha256==s->prom_sha256,
            "activated context carries its own exact row");
      Check(GuTargetHandleOf(i)==Gu.handle && GuIsTargetHandle(Gu.handle) && GuIsVolatileHandle(Gu.handle),"target handle registered");
    }
    Check(GuTargetSpec(0)->nvgop_text_sha256 && GuTargetSpec(1)->nvgop_text_sha256,"both rows pin their nvgop");
  }
  /* Exactness: same ID with another subsystem or bus is never a target, and the run refuses. */
  RealPair(TRUE,TRUE);Cards[0].sub^=1;
  Check(GuDiscoverTargets()==EFI_UNSUPPORTED && GuTargetCount()==1,"second-card subsystem mismatch is not registered; run refused");
  RealPair(TRUE,TRUE);Cards[0].bus=GU_EXTRA_TARGET_BUS+1;
  Check(GuDiscoverTargets()==EFI_UNSUPPORTED,"second card on another bus refused");
  RealPair(TRUE,TRUE);Cards[0].dev=1;
  Check(GuDiscoverTargets()==EFI_UNSUPPORTED,"second card at a nonzero device refused");
  RealPair(TRUE,TRUE);Cards[0].seg=1;
  Check(GuDiscoverTargets()==EFI_UNSUPPORTED,"second card in another segment refused");
  RealPair(FALSE,TRUE);Cards[0].sub^=0x100;
  Check(GuDiscoverTargets()==EFI_UNSUPPORTED,"console subsystem mismatch refused");
  RealPair(FALSE,TRUE);Cards[0].bus=GU_EXTRA_TARGET_BUS;
  Check(GuDiscoverTargets()==EFI_UNSUPPORTED,"console card on the second card's bus refused");
  /* Missing card: not the captured machine. */
  Machine();AddCard(GU_BOARD_ID,GU_BOARD_SUBSYSTEM,GU_TARGET_SEGMENT,GU_TARGET_BUS,TRUE);
  Check(GuDiscoverTargets()==EFI_UNSUPPORTED,"console card alone refused");
  Machine();AddCard(GU_EXTRA_BOARD_ID,GU_EXTRA_BOARD_SUBSYSTEM,GU_EXTRA_TARGET_SEGMENT,GU_EXTRA_TARGET_BUS,FALSE);
  Check(GuDiscoverTargets()==EFI_UNSUPPORTED,"second card alone refused");
  Machine();
  Check(GuDiscoverTargets()==EFI_NOT_FOUND,"no PCI devices refused");
  /* Duplicates: one row cannot match two cards. */
  RealPair(TRUE,TRUE);AddCard(GU_EXTRA_BOARD_ID,GU_EXTRA_BOARD_SUBSYSTEM,GU_EXTRA_TARGET_SEGMENT,GU_EXTRA_TARGET_BUS,FALSE);
  Check(GuDiscoverTargets()==EFI_UNSUPPORTED && GuTargetCount()==0,"duplicate second card refused before any target exists");
  /* A third, unrelated NVIDIA card is ignored (never a target). */
  RealPair(TRUE,TRUE);AddCard(0x220410deU,0x145410deU,0,0x40,FALSE);
  Check(GuDiscoverTargets()==EFI_SUCCESS && GuTargetCount()==2,"unknown third card is not a target");

  /* Release fingerprint: exact second-card nvgop only. */
  if(argc<2){fprintf(stderr,"usage: test_two_card IMG24.BIN (the second card's nvgop dump)\n");return 2;}
  FILE *f=fopen(argv[1],"rb");Check(f!=NULL,"second-card nvgop image fixture opens");
  size_t got=fread(Image,1,sizeof(Image),f);fclose(f);
  RealPair(TRUE,FALSE);Check(GuDiscoverTargets()==EFI_SUCCESS,"discovery for release checks");
  UINTN hp=GuTargetSpec(0)->bus==GU_EXTRA_TARGET_BUS?0:1;
  const GU_CARD_SPEC *spec=GuTargetSpec(hp),*console=GuTargetSpec(hp^1);
  Check(got==spec->nvgop_image_size,"fixture is the full 6000b image");
  GU_RELEASE_OWNER o;SpecOwner(&o,spec);
  Check(GuReleaseOwnerMatchesSpec(&o,spec)==EFI_SUCCESS,"exact captured 6000b image passes");
  Check(GuReleaseOwnerMatchesSpec(&o,console)!=EFI_SUCCESS,"second card's driver fails the console row's 6000a pin");
  SpecOwner(&o,spec);o.version^=1;Check(GuReleaseOwnerMatchesSpec(&o,spec)==EFI_ACCESS_DENIED,"other binding version refused");
  SpecOwner(&o,spec);o.size-=0x40;Check(GuReleaseOwnerMatchesSpec(&o,spec)==EFI_ACCESS_DENIED,"other image size refused");
  SpecOwner(&o,spec);o.stop=(EFI_DRIVER_BINDING_STOP)(Image+spec->nvgop_stop+4);Check(GuReleaseOwnerMatchesSpec(&o,spec)==EFI_ACCESS_DENIED,"other Stop entry refused");
  SpecOwner(&o,spec);o.base=NULL;Check(GuReleaseOwnerMatchesSpec(&o,spec)==EFI_ACCESS_DENIED,"null image base refused");
  SpecOwner(&o,spec);o.agent=NULL;Check(GuReleaseOwnerMatchesSpec(&o,spec)==EFI_ACCESS_DENIED,"no agent refused");
  Check(GuReleaseOwnerMatchesSpec(&o,NULL)==EFI_ACCESS_DENIED,"no row refused");
  SpecOwner(&o,spec);Image[0x280+0x1000]^=1;
  Check(GuReleaseOwnerMatchesSpec(&o,spec)==EFI_CRC_ERROR,"one changed .text byte refused");
  Image[0x280+0x1000]^=1;Image[0x280+spec->nvgop_text_bytes]^=1;
  Check(GuReleaseOwnerMatchesSpec(&o,spec)==EFI_SUCCESS,"bytes outside the pinned .text do not matter");
  Image[0x280+spec->nvgop_text_bytes]^=1;
  GU_CARD_SPEC unpinned=*spec;unpinned.nvgop_text_sha256=NULL;
  Check(GuReleaseOwnerMatchesSpec(&o,&unpinned)==EFI_SUCCESS,"unpinned (named PR #82) rows keep their behaviour");
  GU_CARD_SPEC partial=*spec;partial.nvgop_text_bytes=0;
  Check(GuReleaseOwnerMatchesSpec(&o,&partial)==EFI_ACCESS_DENIED,"a partial pin refuses");
  printf("PASS two-card discovery and release fingerprint checks: %u; RAM-only, real 6000b image\n",Checks);
  return 0;
}
