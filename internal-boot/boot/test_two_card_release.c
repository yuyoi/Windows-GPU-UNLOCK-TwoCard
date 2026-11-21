/* GPL-2.0-only. Local two-card lane (2026-11-21): the wrapper's real release replay of the
 * console card (this machine's captured graph) plus a second card whose fingerprinted nvgop
 * BY_DRIVER edge the child releases. RAM-only mocks; no GPU, firmware or boot access.
 * Build like the no-EDID release replay, plus -DGU_LOCAL_SECOND_CARD.
 * Args: console nvgop image, console PROM, console full PCI config, second-card nvgop image. */
#define GU_REPLAY_MAIN HeadlessReplayMain
#include "test_headless_full.c"
#ifndef GU_LOCAL_TWO_CARD
#error test_two_card_release.c needs -DGU_LOCAL_SECOND_CARD
#endif
#if !GU_NO_EDID_DRIVER
#error This replay follows the no-EDID-driver console profile of this machine
#endif

#define HP_PCI ((EFI_HANDLE)(UINTN)0x7001)
#define HP_AGENT ((EFI_HANDLE)(UINTN)0x7002)
static UINT8 HpImage[GU_EXTRA_NVGOP_IMAGE_SIZE];
static EFI_DRIVER_BINDING_PROTOCOL HpBinding;
static EFI_LOADED_IMAGE_PROTOCOL HpLoaded;
static EFI_PCI_IO_PROTOCOL HpPci;
static UINT8 HpPath[4+sizeof(L"nvgop-ga1xx")+4];
static UINTN HpBus=GU_EXTRA_TARGET_BUS,HpDev;static UINT32 HpId=GU_EXTRA_BOARD_ID,HpSub=GU_EXTRA_BOARD_SUBSYSTEM;
static EFI_LOCATE_HANDLE_BUFFER RealLocate;static EFI_HANDLE_PROTOCOL RealHandle;
static EFI_STATUS EFIAPI HpRead(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 at,UINTN n,VOID *out) {
  Check(p==&HpPci && w==EfiPciIoWidthUint32 && n==1 && (at==0 || at==0x2c),"second-card identity reads only");
  *(UINT32 *)out=at?HpSub:HpId;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI HpLocation(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f) {
  (void)p;*s=GU_EXTRA_TARGET_SEGMENT;*b=HpBus;*d=HpDev;*f=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI HpLocate(EFI_LOCATE_SEARCH_TYPE search,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **out) {
  EFI_STATUS s=RealLocate(search,g,key,n,out);
  if(s==EFI_SUCCESS && search==AllHandles) {
    EFI_HANDLE *more=malloc((*n+2)*sizeof(**out));memcpy(more,*out,*n*sizeof(**out));free(*out);
    more[(*n)++]=HP_PCI;more[(*n)++]=HP_AGENT;*out=more;
  }
  return s;
}
static EFI_STATUS EFIAPI HpHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  *out=NULL;
  if(h==HP_AGENT && GuGuidEqual(g,&BindingGuid)){*out=&HpBinding;return EFI_SUCCESS;}
  if(h==HP_AGENT && GuGuidEqual(g,&ImageGuid)){*out=&HpLoaded;return EFI_SUCCESS;}
  if(h==HP_PCI && GuGuidEqual(g,&PciGuid)){*out=&HpPci;return EFI_SUCCESS;}
  if(h==HP_PCI || h==HP_AGENT)return EFI_UNSUPPORTED;
  return RealHandle(h,g,out);
}
static const EDGE HpEdge={HP_PCI,HP_AGENT,HP_PCI,EFI_PCI_IO_PROTOCOL_GUID,EFI_OPEN_PROTOCOL_BY_DRIVER,1};
static VOID HpMachine(VOID) {
  HpBus=GU_EXTRA_TARGET_BUS;HpDev=0;HpId=GU_EXTRA_BOARD_ID;HpSub=GU_EXTRA_BOARD_SUBSYSTEM;
  HpBinding=(EFI_DRIVER_BINDING_PROTOCOL){0};HpBinding.DriverBindingHandle=HP_AGENT;HpBinding.ImageHandle=HP_AGENT;
  HpBinding.Version=GU_EXTRA_NVGOP_VERSION;HpBinding.Stop=(EFI_DRIVER_BINDING_STOP)(HpImage+GU_EXTRA_NVGOP_STOP);
  const CHAR16 name[]=L"nvgop-ga1xx";
  memset(HpPath,0,sizeof(HpPath));HpPath[0]=MEDIA_DEVICE_PATH;HpPath[1]=MEDIA_FILEPATH_DP;HpPath[2]=(UINT8)(4+sizeof(name));
  memcpy(HpPath+4,name,sizeof(name));HpPath[4+sizeof(name)]=END_DEVICE_PATH_TYPE;HpPath[5+sizeof(name)]=END_ENTIRE_DEVICE_PATH_SUBTYPE;HpPath[6+sizeof(name)]=4;
  HpLoaded=(EFI_LOADED_IMAGE_PROTOCOL){0};HpLoaded.ImageBase=HpImage;HpLoaded.ImageSize=GU_EXTRA_NVGOP_IMAGE_SIZE;
  HpLoaded.ImageCodeType=EfiBootServicesCode;HpLoaded.FilePath=(EFI_DEVICE_PATH_PROTOCOL *)HpPath;HpLoaded.DeviceHandle=HP_PCI;
  HpPci=(EFI_PCI_IO_PROTOCOL){0};HpPci.Pci.Read=HpRead;HpPci.GetLocation=HpLocation;
}
/* A fresh console release with the second card's driver edge in both the prepared and the
 * live graph, then the child's release (edge removed) before the pre-Windows verification. */
static EFI_STATUS Released(int mutate) {
  HpMachine();
  Check(RunFull(0)==EFI_SUCCESS,"console card release replay succeeds");
  EFI_BOOT_SERVICES *b=LastTable->BootServices;
  RealLocate=b->LocateHandleBuffer;RealHandle=b->HandleProtocol;b->LocateHandleBuffer=HpLocate;b->HandleProtocol=HpHandle;
  GRAPH *prepared=(GRAPH *)PreparedGraph;
  prepared->Edges[prepared->Count++]=HpEdge;Live.Edges[Live.Count++]=HpEdge;
  EFI_STATUS before=GuVerifyReleased(LastTable,FALSE);
  Check(before==EFI_SUCCESS,"second card bound: exact graph still verifies");
  Live.Count--; /* the child released the second card's nvgop */
  switch(mutate) {
    case 1:HpImage[0x280+0x2000]^=1;break;                    /* .text drift */
    case 2:HpBinding.Version^=1;break;                        /* other binding version */
    case 3:HpBus++;break;                                     /* driver from a card on another bus */
    case 4:HpSub^=1;break;                                    /* other subsystem */
    case 5:HpId=GU_BOARD_ID;break;                            /* console-card identity */
    case 6:HpLoaded.DeviceHandle=PreparedPlan.RtxPci;break;   /* the console card's own ROM */
    case 7:HpPath[4]^=1;break;                                /* other image name */
    case 8:HpBinding.Stop=(EFI_DRIVER_BINDING_STOP)(HpImage+GU_EXTRA_NVGOP_STOP+8);break;
    case 9:HpLoaded.ImageSize-=0x40;break;
    case 10:HpDev=1;break;
  }
  EFI_STATUS s=GuVerifyReleased(LastTable,FALSE);
  if(mutate==1)HpImage[0x280+0x2000]^=1;
  b->LocateHandleBuffer=RealLocate;b->HandleProtocol=RealHandle;
  return s;
}
/* Same, but the removed edge is not the fingerprinted driver's own PciIo BY_DRIVER open. */
static EFI_STATUS ReleasedOther(EDGE e) {
  HpMachine();
  Check(RunFull(0)==EFI_SUCCESS,"console card release replay succeeds");
  EFI_BOOT_SERVICES *b=LastTable->BootServices;
  RealLocate=b->LocateHandleBuffer;RealHandle=b->HandleProtocol;b->LocateHandleBuffer=HpLocate;b->HandleProtocol=HpHandle;
  GRAPH *prepared=(GRAPH *)PreparedGraph;
  prepared->Edges[prepared->Count++]=e;
  EFI_STATUS s=GuVerifyReleased(LastTable,FALSE); /* never added to Live: it vanished */
  b->LocateHandleBuffer=RealLocate;b->HandleProtocol=RealHandle;
  return s;
}
int main(int argc,char **argv) {
  Check(argc==5,"console nvgop image, PROM, full PCI config and second-card nvgop image required");
  LoadNv(argv[1]);
  FILE *f=fopen(argv[2],"rb");Check(f && fread(PromMemory,1,sizeof(PromMemory),f)==sizeof(PromMemory),"read pinned PROM");fclose(f);
  f=fopen(argv[3],"rb");Check(f && fread(InitialFullConfig,1,sizeof(InitialFullConfig),f)==sizeof(InitialFullConfig),"read exact full config");fclose(f);
  f=fopen(argv[4],"rb");Check(f && fread(HpImage,1,sizeof(HpImage),f)==sizeof(HpImage),"read the second card's nvgop image");fclose(f);
  Check(GuHashMatches(HpImage+0x280,GU_EXTRA_NVGOP_TEXT_BYTES,GU_EXTRA_NVGOP_TEXT_SHA256),"second-card image matches its pin");
  Check(Released(0)==EFI_SUCCESS,"the second card's fingerprinted nvgop release is tolerated");
  for(int m=1;m<=10;m++)Check(Released(m)!=EFI_SUCCESS,"any drift in the released driver or its card blocks Windows handoff");
  EDGE path=HpEdge;path.Protocol=PathGuid;
  Check(ReleasedOther(path)!=EFI_SUCCESS,"another protocol's edge from the same driver may not vanish");
  EDGE other=HpEdge;other.Handle=other.Controller=(EFI_HANDLE)(UINTN)1;
  Check(ReleasedOther(other)!=EFI_SUCCESS,"the same driver's open on another handle may not vanish");
  EDGE child=HpEdge;child.Attributes=EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER;
  Check(ReleasedOther(child)!=EFI_SUCCESS,"a child edge may not vanish");
  EDGE two=HpEdge;two.Count=2;
  Check(ReleasedOther(two)!=EFI_SUCCESS,"a multi-open edge may not vanish");
  /* The console card's own nvgop agent (6000a) never qualifies as a released second-card driver. */
  EDGE console=HpEdge;console.Agent=FX_NvAgent;
  Check(ReleasedOther(console)!=EFI_SUCCESS,"the console card's driver on the second card's handle is never waived");
  printf("PASS two-card wrapper release checks: %d; real console graph + real 6000b image, RAM-only\n",Checks);
  return 0;
}
