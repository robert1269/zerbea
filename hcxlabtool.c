#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <ifaddrs.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <linux/if_ether.h>
#include <linux/filter.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/if.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <inttypes.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/filter.h>

#include "include/hcxlaboratory.h"
#include "include/wireless-lite.h"
#include "include/rpigpio.h"
#include "include/wireless-lite.h"
#include "include/ieee80211.c"
#include "include/pcap.c"

/*===========================================================================*/
/* global var */

static struct timeval tv;
static struct timeval tvold;
static struct timeval tvlast;

static uint64_t timestamp;
static bool wantstopflag;
static int gpiostatusled;
static int gpiobutton;
static int interfacecount;
static interfacelist_t *interfacelist;
static struct sock_fprog bpf;
static int errorcount;

static enhanced_packet_block_t *epbhdr;
static int packetlen;
static uint8_t *packetptr;
static uint8_t *packetoutptr;
static mac_t *macfrx;
static uint8_t *ieee82011ptr;
static uint32_t ieee82011len;
static uint8_t *payloadptr;
static uint32_t payloadlen;
static uint8_t *llcptr;
static llc_t *llc;

static uint16_t mydeauthenticationsequence;

static const uint8_t hdradiotap[] =
{
0x00, 0x00, /* radiotap version and padding */
0x0e, 0x00, /* radiotap header length */
0x06, 0x8c, 0x00, 0x00, /* bitmap */
0x02, /* flags */
0x0c, /* rate */
0x14, /* tx power */
0x01, /* antenna */
0x18, 0x00 /* tx flags */
};
#define HDRRT_SIZE sizeof(hdradiotap)

static int channelscanlist[256];

static uint8_t epb[PCAPNG_MAXSNAPLEN *2];
static uint8_t epbown[PCAPNG_MAXSNAPLEN *2];

/*===========================================================================*/
/*===========================================================================*/
static inline void debugmac2(uint8_t *mac1, uint8_t *mac2)
{
static uint32_t p;

for(p = 0; p < 6; p++) printf("%02x", mac1[p]);
printf(" ");
for(p = 0; p < 6; p++) printf("%02x", mac2[p]);
printf("\n");
return;
}
/*===========================================================================*/
static inline void debugmac(uint8_t *xxmac)
{
static uint32_t p;

for(p = 0; p < 6; p++) printf("%02x", xxmac[p]);
printf("\n");
return;
}
/*===========================================================================*/
static inline void debugframe4(int ifnr, char *message)
{
static uint32_t p;

printf("%s %s %d\n", (interfacelist +ifnr)->ifname, message,  payloadlen);
for(p = 0; p < 6; p++) printf("%02x", macfrx->addr1[p]);
printf(" ");
for(p = 0; p < 6; p++) printf("%02x", macfrx->addr2[p]);
printf(" ");
for(p = 0; p < 6; p++) printf("%02x", macfrx->addr3[p]);
printf(" ");
for(p = 0; p < 6; p++) printf("%02x", macfrx->addr4[p]);
printf("\n");
for(p = 0; p < payloadlen; p++) printf("%02x", payloadptr[p]);
printf("\n\n");
return;
}
/*===========================================================================*/
static inline void debugframe3(int ifnr, char *message)
{
static uint32_t p;

printf("%s %s %d\n", (interfacelist +ifnr)->ifname, message,  payloadlen);
for(p = 0; p < 6; p++) printf("%02x", macfrx->addr1[p]);
printf(" ");
for(p = 0; p < 6; p++) printf("%02x", macfrx->addr2[p]);
printf(" ");
for(p = 0; p < 6; p++) printf("%02x", macfrx->addr3[p]);
printf("\n");
for(p = 0; p < payloadlen; p++) printf("%02x", payloadptr[p]);
printf("\n\n");
return;
}
/*===========================================================================*/
static inline void debugframe2(int ifnr, char *message)
{
static uint32_t p;

printf("%s %s %d\n", (interfacelist +ifnr)->ifname, message,  payloadlen);
for(p = 0; p < 6; p++) printf("%02x", macfrx->addr1[p]);
printf(" ");
for(p = 0; p < 6; p++) printf("%02x", macfrx->addr2[p]);
printf("\n");
for(p = 0; p < payloadlen; p++) printf("%02x", payloadptr[p]);
printf("\n\n");
return;
}
/*===========================================================================*/
static inline void debugframe1(int ifnr, char *message)
{
static uint32_t p;

printf("%s %s %d\n", (interfacelist +ifnr)->ifname, message,  payloadlen);
for(p = 0; p < 6; p++) printf("%02x", macfrx->addr1[p]);
printf("\n");
for(p = 0; p < payloadlen; p++) printf("%02x", payloadptr[p]);
printf("\n\n");
return;
}
/*===========================================================================*/
/*===========================================================================*/
static inline void globalclose()
{
static int ifnr;

if(interfacelist != NULL)
	{
	for(ifnr = 0; ifnr < interfacecount; ifnr++)
		{
		if((interfacelist +ifnr)->fd > 0)
			{
			if(bpf.filter != NULL) setsockopt((interfacelist +ifnr)->fd, SOL_SOCKET, SO_DETACH_FILTER, &bpf, sizeof(bpf));
			close((interfacelist +ifnr)->fd);
			}
		if((interfacelist +ifnr)->fdpcapng > 0) close((interfacelist +ifnr)->fdpcapng);
		if((interfacelist +ifnr)->aplist != NULL) free((interfacelist +ifnr)->aplist);
		if((interfacelist +ifnr)->rgaplist != NULL) free((interfacelist +ifnr)->rgaplist);
		if((interfacelist +ifnr)->eapolm1list != NULL) free((interfacelist +ifnr)->eapolm1list);
		if((interfacelist +ifnr)->eapolm2list != NULL) free((interfacelist +ifnr)->eapolm2list);
		if((interfacelist +ifnr)->eapolm3list != NULL) free((interfacelist +ifnr)->eapolm3list);
		if((interfacelist +ifnr)->owndlist != NULL) free((interfacelist +ifnr)->owndlist);
		}
	free(interfacelist);
	}
if(bpf.filter != NULL) free(bpf.filter);
return;
}
/*===========================================================================*/
/*===========================================================================*/
static void send_deauthentication(int ifnr, uint8_t *client, uint8_t *ap, uint8_t reason)
{
static mac_t *macftx;

packetoutptr = epbown +EPB_SIZE;
memset(packetoutptr, 0, HDRRT_SIZE +MAC_SIZE_NORM +2 +1);
memcpy(packetoutptr, &hdradiotap, HDRRT_SIZE);
macftx = (mac_t*)(packetoutptr +HDRRT_SIZE);
macftx->type = IEEE80211_FTYPE_MGMT;
macftx->subtype = IEEE80211_STYPE_DEAUTH;
memcpy(macftx->addr1, client, 6);
memcpy(macftx->addr2, ap, 6);
memcpy(macftx->addr3, ap, 6);
macftx->duration = 0x013a;
macftx->sequence = mydeauthenticationsequence++ << 4;
if(mydeauthenticationsequence >= 4096) mydeauthenticationsequence = 1;
packetoutptr[HDRRT_SIZE +MAC_SIZE_NORM] = reason;
if(write((interfacelist +ifnr)->fd, packetoutptr, HDRRT_SIZE +MAC_SIZE_NORM +2) == -1) errorcount++;
return;
}
/*===========================================================================*/
static inline void send_ack(int ifnr)
{
static mac_t *macftx;

packetoutptr = epbown +EPB_SIZE;
memset(packetoutptr, 0, HDRRT_SIZE +MAC_SIZE_ACK+1);
memcpy(packetoutptr, &hdradiotap, HDRRT_SIZE);
macftx = (mac_t*)(packetoutptr +HDRRT_SIZE);
macftx->type = IEEE80211_FTYPE_CTL;
macftx->subtype = IEEE80211_STYPE_ACK;
memcpy(macftx->addr1, macfrx->addr2, 6);
if(write((interfacelist +ifnr)->fd, packetoutptr, HDRRT_SIZE +MAC_SIZE_ACK) == -1) errorcount++;
return;
}
/*===========================================================================*/
static inline void writeepb(int fd)
{
static int epblen;
static int written;
static uint16_t padding;
static total_length_t *totallenght;

epbhdr = (enhanced_packet_block_t*)epb;
epblen = EPB_SIZE;
epbhdr->block_type = EPBID;
epbhdr->interface_id = 0;
epbhdr->cap_len = packetlen;
epbhdr->org_len = packetlen;
epbhdr->timestamp_high = timestamp >> 32;
epbhdr->timestamp_low = (uint32_t)timestamp &0xffffffff;
padding = (4 -(epbhdr->cap_len %4)) %4;
epblen += packetlen;
memset(&epb[epblen], 0, padding);
epblen += padding;
epblen += addoption(epb +epblen, SHB_EOC, 0, NULL);
totallenght = (total_length_t*)(epb +epblen);
epblen += TOTAL_SIZE;
epbhdr->total_length = epblen;
totallenght->total_length = epblen;
written = write(fd, &epb, epblen);
if(written != epblen) errorcount++;
return;	
}
/*===========================================================================*/
/*===========================================================================*/
static inline aplist_t* getaptags(aplist_t* aplist, uint8_t *macap)
{
static aplist_t *zeiger;

for(zeiger = aplist; zeiger < aplist +APLIST_MAX; zeiger++)
	{
	if(zeiger->timestamp == 0) return NULL;
	if(memcmp(zeiger->macap, macap, 6) == 0) return zeiger;
	}
return NULL;
}
/*===========================================================================*/
static inline uint8_t geteapolownd(int ifnr, uint8_t *macclient, uint8_t *macap)
{
static owndlist_t *zeiger; 

for(zeiger = (interfacelist +ifnr)->owndlist; zeiger < (interfacelist +ifnr)->owndlist +OWNDLIST_MAX; zeiger++)
	{
	if(zeiger->timestamp == 0) return zeiger->eapolstatus;
	if(memcmp(zeiger->macap, macap, 6) != 0) continue;
	if(memcmp(zeiger->macclient, macclient, 6) != 0) continue;
	return zeiger->eapolstatus;
	}
return zeiger->eapolstatus;
}
/*===========================================================================*/
static inline void addeapolownd(int ifnr, uint8_t *macclient, uint8_t *macap, uint8_t ownstatus)
{
static owndlist_t *zeiger; 

for(zeiger = (interfacelist +ifnr)->owndlist; zeiger < (interfacelist +ifnr)->owndlist +OWNDLIST_MAX; zeiger++)
	{
	if(zeiger->timestamp == 0) break;
	if(memcmp(zeiger->macap, macap, 6) != 0) continue;
	if(memcmp(zeiger->macclient, macclient, 6) != 0) continue;
	zeiger->timestamp = timestamp;
	zeiger->eapolstatus |= ownstatus;
	return;
	}
memset(zeiger, 0, OWNDLIST_SIZE);
zeiger->timestamp = timestamp;
zeiger->eapolstatus = ownstatus;
memcpy(zeiger->macap, macap, 6);
memcpy(zeiger->macclient, macclient, 6);
qsort((interfacelist +ifnr)->owndlist, zeiger -(interfacelist +ifnr)->owndlist +1, OWNDLIST_SIZE, sort_owndlist_by_time);
return;
}
/*===========================================================================*/
static inline void gettagwpa(int wpalen, uint8_t *ieptr, aplist_t *zeiger)
{
static int c;
static wpaie_t *wpaptr;
static int wpatype;
static suite_t *gsuiteptr;
static suitecount_t *csuitecountptr;
static suite_t *csuiteptr;
static int csuitecount;
static suitecount_t *asuitecountptr;
static suite_t *asuiteptr;
static int asuitecount;

wpaptr = (wpaie_t*)ieptr;
wpalen -= WPAIE_SIZE;
ieptr += WPAIE_SIZE;
if(memcmp(wpaptr->oui, &ouimscorp, 3) != 0) return;
if(wpaptr->ouitype != 1) return;
wpatype = wpaptr->type;
if(wpatype != VT_WPA_IE) return;
zeiger->kdversion |= KV_WPAIE;
gsuiteptr = (suite_t*)ieptr;
if(memcmp(gsuiteptr->oui, &ouimscorp, 3) == 0)
	{
	if(gsuiteptr->type == CS_WEP40) zeiger->groupcipher |= TCS_WEP40;
	if(gsuiteptr->type == CS_TKIP) zeiger->groupcipher |= TCS_TKIP;
	if(gsuiteptr->type == CS_WRAP) zeiger->groupcipher |= TCS_WRAP;
	if(gsuiteptr->type == CS_CCMP) zeiger->groupcipher |= TCS_CCMP;
	if(gsuiteptr->type == CS_WEP104) zeiger->groupcipher |= TCS_WEP104;
	if(gsuiteptr->type == CS_BIP) zeiger->groupcipher |= TCS_BIP;
	if(gsuiteptr->type == CS_NOT_ALLOWED) zeiger->groupcipher |= TCS_NOT_ALLOWED;
	}
wpalen -= SUITE_SIZE;
ieptr += SUITE_SIZE;
csuitecountptr = (suitecount_t*)ieptr;
wpalen -= SUITECOUNT_SIZE;
ieptr += SUITECOUNT_SIZE;
csuitecount = csuitecountptr->count;
for(c = 0; c < csuitecount; c++)
	{
	csuiteptr = (suite_t*)ieptr;
	if(memcmp(csuiteptr->oui, &ouimscorp, 3) == 0)
		{
		if(csuiteptr->type == CS_WEP40) zeiger->cipher |= TCS_WEP40;
		if(csuiteptr->type == CS_TKIP) zeiger->cipher |= TCS_TKIP;
		if(csuiteptr->type == CS_WRAP) zeiger->cipher |= TCS_WRAP;
		if(csuiteptr->type == CS_CCMP) zeiger->cipher |= TCS_CCMP;
		if(csuiteptr->type == CS_WEP104) zeiger->cipher |= TCS_WEP104;
		if(csuiteptr->type == CS_BIP) zeiger->cipher |= TCS_BIP;
		if(csuiteptr->type == CS_NOT_ALLOWED) zeiger->cipher |= TCS_NOT_ALLOWED;
		}
	wpalen -= SUITE_SIZE;
	ieptr += SUITE_SIZE;
	if(wpalen <= 0) return;
	}
asuitecountptr = (suitecount_t*)ieptr;
wpalen -= SUITECOUNT_SIZE;
ieptr += SUITECOUNT_SIZE;
asuitecount = asuitecountptr->count;
for(c = 0; c < asuitecount; c++)
	{
	asuiteptr = (suite_t*)ieptr;
	if(memcmp(asuiteptr->oui, &ouimscorp, 3) == 0)
		{
		if(asuiteptr->type == AK_PMKSA) zeiger->akm |= TAK_PMKSA;
		if(asuiteptr->type == AK_PSK) zeiger->akm |= TAK_PSK;
		if(asuiteptr->type == AK_FT) zeiger->akm |= TAK_FT;
		if(asuiteptr->type == AK_FT_PSK) zeiger->akm |= TAK_FT_PSK;
		if(asuiteptr->type == AK_PMKSA256) zeiger->akm |= TAK_PMKSA256;
		if(asuiteptr->type == AK_PSKSHA256) zeiger->akm |= TAK_PSKSHA256;
		if(asuiteptr->type == AK_TDLS) zeiger->akm |= TAK_TDLS;
		if(asuiteptr->type == AK_SAE_SHA256) zeiger->akm |= TAK_SAE_SHA256;
		if(asuiteptr->type == AK_FT_SAE) zeiger->akm |= TAK_FT_SAE;
		}
	wpalen -= SUITE_SIZE;
	ieptr += SUITE_SIZE;
	if(wpalen <= 0) return;
	}
return;
}
/*===========================================================================*/
static inline void gettagvendor(int vendorlen, uint8_t *ieptr, aplist_t *zeiger)
{
static wpaie_t *wpaptr;

wpaptr = (wpaie_t*)ieptr;
if(memcmp(wpaptr->oui, &ouimscorp, 3) != 0) return;
if((wpaptr->ouitype == VT_WPA_IE) && (vendorlen >= WPAIE_LEN_MIN)) gettagwpa(vendorlen, ieptr, zeiger);
return;
}
/*===========================================================================*/
static inline void gettagrsn(int rsnlen, uint8_t *ieptr, aplist_t *zeiger)
{
static int c;
static rsnie_t *rsnptr;
static int rsnver;
static suite_t *gsuiteptr;
static suitecount_t *csuitecountptr;
static suite_t *csuiteptr;
static int csuitecount;
static suitecount_t *asuitecountptr;
static suite_t *asuiteptr;
static int asuitecount;
static rsnpmkidlist_t *rsnpmkidlistptr;
static int rsnpmkidcount;

rsnptr = (rsnie_t*)ieptr;
rsnver = rsnptr->version;
if(rsnver != 1) return;
zeiger->kdversion |= KV_RSNIE;
rsnlen -= RSNIE_SIZE;
ieptr += RSNIE_SIZE;
gsuiteptr = (suite_t*)ieptr;
if(memcmp(gsuiteptr->oui, &suiteoui, 3) == 0)
	{
	if(gsuiteptr->type == CS_WEP40) zeiger->groupcipher |= TCS_WEP40;
	if(gsuiteptr->type == CS_TKIP) zeiger->groupcipher |= TCS_TKIP;
	if(gsuiteptr->type == CS_WRAP) zeiger->groupcipher |= TCS_WRAP;
	if(gsuiteptr->type == CS_CCMP) zeiger->groupcipher |= TCS_CCMP;
	if(gsuiteptr->type == CS_WEP104) zeiger->groupcipher |= TCS_WEP104;
	if(gsuiteptr->type == CS_BIP) zeiger->groupcipher |= TCS_BIP;
	if(gsuiteptr->type == CS_NOT_ALLOWED) zeiger->groupcipher |= TCS_NOT_ALLOWED;
	}
rsnlen -= SUITE_SIZE;
ieptr += SUITE_SIZE;
csuitecountptr = (suitecount_t*)ieptr;
rsnlen -= SUITECOUNT_SIZE;
ieptr += SUITECOUNT_SIZE;
csuitecount = csuitecountptr->count;
for(c = 0; c < csuitecount; c++)
	{
	csuiteptr = (suite_t*)ieptr;
	if(memcmp(csuiteptr->oui, &suiteoui, 3) == 0)
		{
		if(csuiteptr->type == CS_WEP40) zeiger->cipher |= TCS_WEP40;
		if(csuiteptr->type == CS_TKIP) zeiger->cipher |= TCS_TKIP;
		if(csuiteptr->type == CS_WRAP) zeiger->cipher |= TCS_WRAP;
		if(csuiteptr->type == CS_CCMP) zeiger->cipher |= TCS_CCMP;
		if(csuiteptr->type == CS_WEP104) zeiger->cipher |= TCS_WEP104;
		if(csuiteptr->type == CS_BIP) zeiger->cipher |= TCS_BIP;
		if(csuiteptr->type == CS_NOT_ALLOWED) zeiger->cipher |= TCS_NOT_ALLOWED;
		}
	rsnlen -= SUITE_SIZE;
	ieptr += SUITE_SIZE;
	if(rsnlen <= 0) return;
	}
asuitecountptr = (suitecount_t*)ieptr;
rsnlen -= SUITECOUNT_SIZE;
ieptr += SUITECOUNT_SIZE;
asuitecount = asuitecountptr->count;
for(c = 0; c < asuitecount; c++)
	{
	asuiteptr = (suite_t*)ieptr;
	if(memcmp(asuiteptr->oui, &suiteoui, 3) == 0)
		{
		if(asuiteptr->type == AK_PMKSA) zeiger->akm |= TAK_PMKSA;
		if(asuiteptr->type == AK_PSK) zeiger->akm |= TAK_PSK;
		if(asuiteptr->type == AK_FT) zeiger->akm |= TAK_FT;
		if(asuiteptr->type == AK_FT_PSK) zeiger->akm |= TAK_FT_PSK;
		if(asuiteptr->type == AK_PMKSA256) zeiger->akm |= TAK_PMKSA256;
		if(asuiteptr->type == AK_PSKSHA256) zeiger->akm |= TAK_PSKSHA256;
		if(asuiteptr->type == AK_TDLS) zeiger->akm |= TAK_TDLS;
		if(asuiteptr->type == AK_SAE_SHA256) zeiger->akm |= TAK_SAE_SHA256;
		if(asuiteptr->type == AK_FT_SAE) zeiger->akm |= TAK_FT_SAE;
		}
	rsnlen -= SUITE_SIZE;
	ieptr += SUITE_SIZE;
	if(rsnlen <= 0) return;
	}
rsnlen -= RSNCAPABILITIES_SIZE;
ieptr += RSNCAPABILITIES_SIZE;
if(rsnlen <= 0) return;
rsnpmkidlistptr = (rsnpmkidlist_t*)ieptr;
rsnpmkidcount = rsnpmkidlistptr->count;
if(rsnpmkidcount == 0) return;
rsnlen -= RSNPMKIDLIST_SIZE;
ieptr += RSNPMKIDLIST_SIZE;
if(rsnlen < 16) return;
if(((zeiger->akm &TAK_PSK) == TAK_PSK) || ((zeiger->akm &TAK_PSKSHA256) == TAK_PSKSHA256))
	{
	if(memcmp(&zeroed32, ieptr, 16) == 0) return;
	}
return;
}
/*===========================================================================*/
static inline void gettags(int infolen, uint8_t *infoptr, aplist_t *zeiger)
{
static ietag_t *tagptr;

while(0 < infolen)
	{
	if(infolen == 4) return;
	tagptr = (ietag_t*)infoptr;
	if(tagptr->len == 0)
		{
		infoptr += tagptr->len +IETAG_SIZE;
		infolen -= tagptr->len +IETAG_SIZE;
		continue;
		}
	if(tagptr->len > infolen) return;
	if(tagptr->id == TAG_SSID)
		{
		if((tagptr->len > 0) && (tagptr->len <= ESSID_LEN_MAX) && (tagptr->data[0] != 0))
			{
			memcpy(zeiger->essid, &tagptr->data[0], tagptr->len);
			zeiger->essidlen = tagptr->len;
			}
		}
	else if(tagptr->id == TAG_CHAN)
		{
		if(tagptr->len == 1) zeiger->channel = tagptr->data[0];
		}
	else if(tagptr->id == TAG_RSN)
		{
		if(tagptr->len >= RSNIE_LEN_MIN) gettagrsn(tagptr->len, tagptr->data, zeiger);
		}
	else if(tagptr->id == TAG_VENDOR)
		{
		if(tagptr->len >= VENDORIE_SIZE) gettagvendor(tagptr->len, tagptr->data, zeiger);
		}
	infoptr += tagptr->len +IETAG_SIZE;
	infolen -= tagptr->len +IETAG_SIZE;
	}
return;
}
/*===========================================================================*/
static inline uint8_t gettagessid(int infolen, uint8_t *infoptr, uint8_t **essidstr)
{
static ietag_t *tagptr;

while(0 < infolen)
	{
	if(infolen == 4) return 0;
	tagptr = (ietag_t*)infoptr;
	if(tagptr->len == 0)
		{
		infoptr += tagptr->len +IETAG_SIZE;
		infolen -= tagptr->len +IETAG_SIZE;
		continue;
		}
	if(tagptr->len > infolen) return 0;
	if(tagptr->id == TAG_SSID)
		{
		if((tagptr->len > 0) && (tagptr->len <= ESSID_LEN_MAX) && (&tagptr->data[0] != 0))
			{
			*essidstr = &tagptr->data[0];
			return tagptr->len;
			}
		return 0;
		}
	infoptr += tagptr->len +IETAG_SIZE;
	infolen -= tagptr->len +IETAG_SIZE;
	}
return 0;
}
/*===========================================================================*/
static inline void process80211exteap(int ifnr, int authlen)
{
static uint8_t *eapauthptr;
static exteap_t *exteap;
static uint16_t exteaplen;

eapauthptr = payloadptr +LLC_SIZE +EAPAUTH_SIZE;
exteap = (exteap_t*)eapauthptr;
exteaplen = ntohs(exteap->len);
if(exteaplen > authlen) return;
writeepb((interfacelist +ifnr)->fdpcapng);
if(exteap->type == EAP_TYPE_ID)
	{
	if(exteap->code == EAP_CODE_REQ)
		{
		}
	else if(exteap->code == EAP_CODE_RESP)
		{
		}
	}
return;
}
/*===========================================================================*/
static inline void process80211eapol_m4(int ifnr, uint8_t *wpakptr)
{
static wpakey_t *wpak;
static eapollist_t *zeigerm3;

wpak = (wpakey_t*)wpakptr;
if(memcmp(wpak->nonce, &zeroed32, 32) == 0)
	{
	if((geteapolownd(ifnr, macfrx->addr2, macfrx->addr1) &0xf) == 0)
		{
		send_deauthentication(ifnr, macfrx->addr2, macfrx->addr1, WLAN_REASON_DISASSOC_AP_BUSY);
		send_deauthentication(ifnr, macfrx->addr2, macfrx->addr1, WLAN_REASON_DISASSOC_AP_BUSY);
		return;
		}
	}
writeepb((interfacelist +ifnr)->fdpcapng);
for(zeigerm3 = (interfacelist +ifnr)->eapolm3list; zeigerm3 < (interfacelist +ifnr)->eapolm3list +EAPOLLIST_MAX; zeigerm3++)
	{
	if(timestamp - zeigerm3->timestamp > EAPOLM3M4TIMEOUT) return;
	if((be64toh(wpak->replaycount)) != zeigerm3->rc) continue;
	if(memcmp(macfrx->addr1, zeigerm3->macap, 6) != 0) continue;
	if(memcmp(macfrx->addr2, zeigerm3->macclient, 6) != 0) continue;
	addeapolownd(ifnr, macfrx->addr2, macfrx->addr1, OWND_EAPOLM3M4);
	return;
	}
if((geteapolownd(ifnr, macfrx->addr2, macfrx->addr1) &0xf) == 0)
	{
	send_deauthentication(ifnr, macfrx->addr2, macfrx->addr1, WLAN_REASON_DISASSOC_AP_BUSY);
	send_deauthentication(ifnr, macfrx->addr2, macfrx->addr1, WLAN_REASON_DISASSOC_AP_BUSY);
	}
return;
}
/*===========================================================================*/
static inline void process80211eapol_m3(int ifnr, uint8_t *wpakptr)
{
static wpakey_t *wpak;
static eapollist_t *zeiger, *zeigerm2;

writeepb((interfacelist +ifnr)->fdpcapng);
wpak = (wpakey_t*)wpakptr;
zeiger = (interfacelist +ifnr)->eapolm3list +EAPOLLIST_MAX;
zeiger->timestamp = timestamp;
memcpy(zeiger->macap, macfrx->addr2, 6);
memcpy(zeiger->macclient, macfrx->addr1, 6);
zeiger->rc = be64toh(wpak->replaycount);
qsort((interfacelist +ifnr)->eapolm3list, EAPOLLIST_MAX +1, EAPOLLIST_SIZE, sort_eapollist_by_time);
for(zeigerm2 = (interfacelist +ifnr)->eapolm2list; zeigerm2 < (interfacelist +ifnr)->eapolm2list +EAPOLLIST_MAX; zeigerm2++)
	{
	if((interfacelist +ifnr)->eapolm3list->timestamp - zeigerm2->timestamp > EAPOLM2M3TIMEOUT) return;
	if((interfacelist +ifnr)->eapolm3list->rc != (zeigerm2->rc +1)) continue;
	if(memcmp((interfacelist +ifnr)->eapolm3list->macap, zeigerm2->macap, 6) != 0) continue;
	if(memcmp((interfacelist +ifnr)->eapolm3list->macclient, zeigerm2->macclient, 6) != 0) continue;
	addeapolownd(ifnr, macfrx->addr1, macfrx->addr2, OWND_EAPOLM2M3);
	}
return;
}
/*===========================================================================*/
static inline void process80211eapol_m2(int ifnr, uint8_t *wpakptr)
{
static wpakey_t *wpak;
static eapollist_t *zeiger, *zeigerm1;

writeepb((interfacelist +ifnr)->fdpcapng);
wpak = (wpakey_t*)wpakptr;
zeiger = (interfacelist +ifnr)->eapolm2list +EAPOLLIST_MAX;
zeiger->timestamp = timestamp;
memcpy(zeiger->macap, macfrx->addr1, 6);
memcpy(zeiger->macclient, macfrx->addr2, 6);
zeiger->rc = be64toh(wpak->replaycount);
qsort((interfacelist +ifnr)->eapolm2list, EAPOLLIST_MAX +1, EAPOLLIST_SIZE, sort_eapollist_by_time);
for(zeigerm1 = (interfacelist +ifnr)->eapolm1list; zeigerm1 < (interfacelist +ifnr)->eapolm1list +EAPOLLIST_MAX; zeigerm1++)
	{
	if((interfacelist +ifnr)->eapolm2list->timestamp - zeigerm1->timestamp > EAPOLM1M2TIMEOUT) return;
	if((interfacelist +ifnr)->eapolm2list->rc != zeigerm1->rc) continue;
	if(memcmp((interfacelist +ifnr)->eapolm2list->macap, zeigerm1->macap, 6) != 0) continue;
	if(memcmp((interfacelist +ifnr)->eapolm2list->macclient, zeigerm1->macclient, 6) != 0) continue;
	addeapolownd(ifnr, macfrx->addr2, macfrx->addr1, OWND_EAPOLM1M2);
	}
return;
}
/*===========================================================================*/
static inline void process80211eapol_m1(int ifnr, uint16_t authlen, uint8_t *wpakptr)
{
static wpakey_t *wpak;
static int infolen;
static pmkid_t *pmkid;
static eapollist_t *zeiger;

writeepb((interfacelist +ifnr)->fdpcapng);
wpak = (wpakey_t*)wpakptr;
zeiger = (interfacelist +ifnr)->eapolm1list +EAPOLLIST_MAX;
zeiger->timestamp = timestamp;
memcpy(zeiger->macap, macfrx->addr2, 6);
memcpy(zeiger->macclient, macfrx->addr1, 6);
zeiger->rc = be64toh(wpak->replaycount);
qsort((interfacelist +ifnr)->eapolm1list, EAPOLLIST_MAX +1, EAPOLLIST_SIZE, sort_eapollist_by_time);
infolen = ntohs(wpak->wpadatalen);
if(infolen < (int)PMKID_SIZE) return;
if(authlen >= WPAKEY_SIZE +PMKID_SIZE)
	{
	pmkid = (pmkid_t*)(wpakptr +WPAKEY_SIZE);
	if(pmkid->id != TAG_VENDOR)
		{
		addeapolownd(ifnr, macfrx->addr1, macfrx->addr2, OWND_NO_PMKID);
		return;
		}
	if((pmkid->len != 0x14) && (pmkid->type != 0x04))
		{
		addeapolownd(ifnr, macfrx->addr1, macfrx->addr2, OWND_NO_PMKID);
		return;
		}
	if(memcmp(pmkid->pmkid, &zeroed32, 16) != 0)
		{
		addeapolownd(ifnr, macfrx->addr1, macfrx->addr2, OWND_PMKID_AP);
		return;
		}
	}
addeapolownd(ifnr, macfrx->addr1, macfrx->addr2, OWND_NO_PMKID);
return;
}
/*===========================================================================*/
static inline void process80211eapol(int ifnr, uint16_t authlen)
{
static uint8_t *wpakptr;
static wpakey_t *wpak;
static uint16_t keyinfo;

wpakptr = payloadptr +LLC_SIZE +EAPAUTH_SIZE;
wpak = (wpakey_t*)wpakptr;
keyinfo = (getkeyinfo(ntohs(wpak->keyinfo)));
if(keyinfo == 1) process80211eapol_m1(ifnr, authlen, wpakptr);
else if(keyinfo == 2) process80211eapol_m2(ifnr, wpakptr);
else if(keyinfo == 3) process80211eapol_m3(ifnr, wpakptr);
else if(keyinfo == 4) process80211eapol_m4(ifnr, wpakptr);
return;
}
/*===========================================================================*/
static inline void process80211eap(int ifnr)
{
static uint8_t *eapauthptr;
static eapauth_t *eapauth;
static uint16_t eapauthlen;
static uint16_t authlen;

eapauthptr = payloadptr +LLC_SIZE;
eapauthlen = payloadlen -LLC_SIZE;
eapauth = (eapauth_t*)eapauthptr;
authlen = ntohs(eapauth->len);
if(authlen > (eapauthlen -4)) return;
if(eapauth->type == EAPOL_KEY)
	{
	if(authlen >= WPAKEY_SIZE) process80211eapol(ifnr, authlen);
	}
else if(eapauth->type == EAP_PACKET) process80211exteap(ifnr, authlen);
return;
}
/*===========================================================================*/
/*===========================================================================*/
static inline void process80211blockack(int ifnr)
{
static aplist_t *zeiger;

//debug
return;

zeiger = getaptags((interfacelist +ifnr)->aplist, macfrx->addr2);
if(zeiger != NULL)
	{
	if(((zeiger->akm &TAK_PSK) == TAK_PSK) || ((zeiger->akm &TAK_PSKSHA256) == TAK_PSKSHA256))
		{
		if(((zeiger->kdversion &KV_RSNIE) == KV_RSNIE) || ((zeiger->kdversion &KV_WPAIE) == KV_WPAIE))
			{
			if((geteapolownd(ifnr, macfrx->addr1, macfrx->addr2) &0xf) == 0) send_deauthentication(ifnr, macfrx->addr1, macfrx->addr2, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
			}
		}
	}
else
	{
	zeiger = getaptags((interfacelist +ifnr)->aplist, macfrx->addr1);
	if(zeiger != NULL)
		{
		if(((zeiger->akm &TAK_PSK) == TAK_PSK) || ((zeiger->akm &TAK_PSKSHA256) == TAK_PSKSHA256))
			{
			if(((zeiger->kdversion &KV_RSNIE) == KV_RSNIE) || ((zeiger->kdversion &KV_WPAIE) == KV_WPAIE))
				{
				if((geteapolownd(ifnr, macfrx->addr2, macfrx->addr1) &0xf) == 0) send_deauthentication(ifnr, macfrx->addr2, macfrx->addr1, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
				}
			}
		}
	}
return;
}
/*===========================================================================*/
static inline void process80211blockack_req(int ifnr)
{
static aplist_t *zeiger;

//debug
return;
zeiger = getaptags((interfacelist +ifnr)->aplist, macfrx->addr2);
if(zeiger != NULL)
	{
	if(((zeiger->akm &TAK_PSK) == TAK_PSK) || ((zeiger->akm &TAK_PSKSHA256) == TAK_PSKSHA256))
		{
		if(((zeiger->kdversion &KV_RSNIE) == KV_RSNIE) || ((zeiger->kdversion &KV_WPAIE) == KV_WPAIE))
			{
			if((geteapolownd(ifnr, macfrx->addr1, macfrx->addr2) &0xf) == 0) send_deauthentication(ifnr, macfrx->addr1, macfrx->addr2, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
			}
		}
	}
else
	{
	zeiger = getaptags((interfacelist +ifnr)->aplist, macfrx->addr1);
	if(zeiger != NULL)
		{
		if(((zeiger->akm &TAK_PSK) == TAK_PSK) || ((zeiger->akm &TAK_PSKSHA256) == TAK_PSKSHA256))
			{
			if(((zeiger->kdversion &KV_RSNIE) == KV_RSNIE) || ((zeiger->kdversion &KV_WPAIE) == KV_WPAIE))
				{
				if((geteapolownd(ifnr, macfrx->addr2, macfrx->addr1) &0xf) == 0) send_deauthentication(ifnr, macfrx->addr2, macfrx->addr1, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
				}
			}
		}
	}
return;
}
/*===========================================================================*/
static inline void process80211qosnull(int ifnr)
{
static aplist_t *zeiger;

zeiger = getaptags((interfacelist +ifnr)->aplist, macfrx->addr1);
if(zeiger != NULL)
	{
	if(((zeiger->akm &TAK_PSK) == TAK_PSK) || ((zeiger->akm &TAK_PSKSHA256) == TAK_PSKSHA256))
		{
		if(((zeiger->kdversion &KV_RSNIE) == KV_RSNIE) || ((zeiger->kdversion &KV_WPAIE) == KV_WPAIE))
			{
			debugmac2(macfrx->addr2, macfrx->addr1);
			if((geteapolownd(ifnr, macfrx->addr2, macfrx->addr1) &0xf) == 0) send_deauthentication(ifnr, macfrx->addr2, macfrx->addr1, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
			}
		}
	}
return;
}
/*===========================================================================*/
static inline void process80211null(int ifnr)
{
static aplist_t *zeiger;

//debug
return;
zeiger = getaptags((interfacelist +ifnr)->aplist, macfrx->addr1);
if(zeiger != NULL)
	{
	if(((zeiger->akm &TAK_PSK) == TAK_PSK) || ((zeiger->akm &TAK_PSKSHA256) == TAK_PSKSHA256))
		{
		if(((zeiger->kdversion &KV_RSNIE) == KV_RSNIE) || ((zeiger->kdversion &KV_WPAIE) == KV_WPAIE))
			{
			debugmac2(macfrx->addr2, macfrx->addr1);
			if((geteapolownd(ifnr, macfrx->addr2, macfrx->addr1) &0xf) == 0) send_deauthentication(ifnr, macfrx->addr2, macfrx->addr1, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
			}
		}
	}
return;
}
/*===========================================================================*/
static inline void process80211powersave_poll(int ifnr)
{

debugframe2(ifnr, "power save");


writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
static inline void process80211action(int ifnr)
{

printf("debug ACTION\n");

writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
/*===========================================================================*/
static inline void process80211reassociation_resp(int ifnr)
{


writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
static inline void process80211association_resp(int ifnr)
{


writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
static inline void process80211reassociation_req(int ifnr)
{
//static uint8_t *clientinfoptr;
static uint16_t clientinfolen;

//clientinfoptr = payloadptr +CAPABILITIESSTA_SIZE;
clientinfolen = payloadlen -CAPABILITIESSTA_SIZE;
if(clientinfolen < IETAG_SIZE) return;


writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
static inline void process80211association_req(int ifnr)
{
//static uint8_t *clientinfoptr;
static uint16_t clientinfolen;

//clientinfoptr = payloadptr +CAPABILITIESSTA_SIZE;
clientinfolen = payloadlen -CAPABILITIESSTA_SIZE;
if(clientinfolen < IETAG_SIZE) return;


writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
static inline void process80211authentication_resp(int ifnr)
{

writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
static inline void process80211authentication_req(int ifnr)
{

writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
static inline void process80211authentication(int ifnr)
{
static authf_t *auth;

auth = (authf_t*)payloadptr;
if(payloadlen < AUTHENTICATIONFRAME_SIZE) return;
if((auth->sequence %2) == 1) process80211authentication_req(ifnr);
if((auth->sequence %2) == 1) process80211authentication_req(ifnr);
else  process80211authentication_resp(ifnr);
return;
}
/*===========================================================================*/
static inline void process80211probe_req(int ifnr)
{
static rgaplist_t *zeiger;
static uint8_t essidlen;
static uint8_t *essidptr;

if(payloadlen < IETAG_SIZE) return;
essidlen = gettagessid(payloadlen, payloadptr, &essidptr);
if(essidlen == 0) return;
for(zeiger = (interfacelist +ifnr)->rgaplist; zeiger < (interfacelist +ifnr)->rgaplist +RGAPLIST_MAX; zeiger++)
	{
	if(zeiger->timestamp == 0) break;
	if(zeiger->essidlen != essidlen) continue;
	if(memcmp(zeiger->essid, essidptr, essidlen) != 0) continue;
	zeiger->timestamp = timestamp;
	return;
	}
memset(zeiger, 0, RGAPLIST_SIZE);
zeiger->timestamp = timestamp;
zeiger->essidlen = essidlen;
memcpy(zeiger->essid, essidptr, essidlen);
zeiger->macrgap[5] = (interfacelist +ifnr)->nicrgap & 0xff;
zeiger->macrgap[4] = ((interfacelist +ifnr)->nicrgap >> 8) & 0xff;
zeiger->macrgap[3] = ((interfacelist +ifnr)->nicrgap >> 16) & 0xff;
zeiger->macrgap[2] = (interfacelist +ifnr)->ouirgap & 0xff;
zeiger->macrgap[1] = ((interfacelist +ifnr)->ouirgap >> 8) & 0xff;
zeiger->macrgap[0] = ((interfacelist +ifnr)->ouirgap >> 16) & 0xff;
(interfacelist +ifnr)->nicrgap += 1;
qsort((interfacelist +ifnr)->rgaplist, zeiger -(interfacelist +ifnr)->rgaplist +1, RGAPLIST_SIZE, sort_rgaplist_by_time);
writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
static inline void process80211probe_resp(int ifnr)
{
static int apinfolen;
static uint8_t *apinfoptr;
static aplist_t *zeiger;

if(payloadlen < CAPABILITIESAP_SIZE +IETAG_SIZE) return;
apinfoptr = payloadptr +CAPABILITIESAP_SIZE;
apinfolen = payloadlen -CAPABILITIESAP_SIZE;
for(zeiger = (interfacelist +ifnr)->aplist; zeiger < (interfacelist +ifnr)->aplist +APLIST_MAX; zeiger++)
	{
	if(zeiger->timestamp == 0) break;
	if(memcmp(zeiger->macap, macfrx->addr2, 6) != 0) continue;
	gettags(apinfolen, apinfoptr, (interfacelist +ifnr)->aplist);
	if((interfacelist +ifnr)->channel != zeiger->channel) return;
	zeiger->timestamp = timestamp;
	zeiger->count += 1;
	gettags(apinfolen, apinfoptr, (interfacelist +ifnr)->aplist);
	if((zeiger->status & STATUS_PRESP) != STATUS_PRESP) writeepb((interfacelist +ifnr)->fdpcapng);
	zeiger->status |= STATUS_PRESP;
	return;
	}
memset(zeiger, 0, APLIST_SIZE);
gettags(apinfolen, apinfoptr, zeiger);
if((interfacelist +ifnr)->channel != zeiger->channel) return;
zeiger->timestamp = timestamp;
zeiger->count += 1;
zeiger->status = STATUS_PRESP;
memcpy(zeiger->macap, macfrx->addr2, 6);
gettags(apinfolen, apinfoptr, (interfacelist +ifnr)->aplist);
qsort((interfacelist +ifnr)->aplist, zeiger -(interfacelist +ifnr)->aplist +1, APLIST_SIZE, sort_aplist_by_time);
writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
static inline void process80211beacon(int ifnr)
{
static int apinfolen;
static uint8_t *apinfoptr;
static aplist_t *zeiger;

if(payloadlen < CAPABILITIESAP_SIZE +IETAG_SIZE) return;
apinfoptr = payloadptr +CAPABILITIESAP_SIZE;
apinfolen = payloadlen -CAPABILITIESAP_SIZE;

for(zeiger = (interfacelist +ifnr)->aplist; zeiger < (interfacelist +ifnr)->aplist +APLIST_MAX; zeiger++)
	{
	if(zeiger->timestamp == 0) break;
	if(memcmp(zeiger->macap, macfrx->addr2, 6) != 0) continue;
	zeiger->timestamp = timestamp;
	zeiger->count += 1;
	if((zeiger->status & STATUS_BEACON) != STATUS_BEACON) writeepb((interfacelist +ifnr)->fdpcapng);
	zeiger->status |= STATUS_BEACON;
	return;
	}
//debug
//send_deauthentication(ifnr, macfrx->addr1, macfrx->addr2, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
//send_deauthentication(ifnr, macfrx->addr1, macfrx->addr2, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
memset(zeiger, 0, APLIST_SIZE);
gettags(apinfolen, apinfoptr, zeiger);
if((interfacelist +ifnr)->channel != zeiger->channel) return;
zeiger->timestamp = timestamp;
zeiger->count += 1;
zeiger->status = STATUS_BEACON;
memcpy(zeiger->macap, macfrx->addr2, 6);
qsort((interfacelist +ifnr)->aplist, zeiger -(interfacelist +ifnr)->aplist +1, APLIST_SIZE, sort_aplist_by_time);
writeepb((interfacelist +ifnr)->fdpcapng);
return;
}
/*===========================================================================*/
static inline void process_packet(int ifnr, int sd)
{
static int rthl;
static rth_t *rth;
static uint32_t rthp;

packetlen = recvfrom(sd, epb +EPB_SIZE, PCAPNG_MAXSNAPLEN, 0, NULL, NULL);
timestamp = ((uint64_t)tv.tv_sec *1000000) + tv.tv_usec;
if(packetlen < 0)
	{
	errorcount++;
	return;
	}
if(packetlen == 0)
	{
	errorcount++;
	return;
	}
if(packetlen < (int)RTH_SIZE)
	{
	errorcount++;
	return;
	}
packetptr = &epb[EPB_SIZE];
rth = (rth_t*)packetptr;
if(rth->it_version != 0)
	{
	errorcount++;
	return;
	}
if(rth->it_pad != 0)
	{
	errorcount++;
	return;
	}
if(rth->it_present == 0)
	{
	errorcount++;
	return;
	}
rthl = le16toh(rth->it_len);
if(rthl > packetlen)
	{
	errorcount++;
	return;
	}
rthp = le32toh(rth->it_present);
if((rthp & IEEE80211_RADIOTAP_TX_FLAGS) == IEEE80211_RADIOTAP_TX_FLAGS) return;
ieee82011ptr = packetptr +rthl;
ieee82011len = packetlen -rthl;
if(ieee82011len < MAC_SIZE_ACK) return;
tvlast.tv_sec = tv.tv_sec;
macfrx = (mac_t*)ieee82011ptr;
if((macfrx->from_ds == 1) && (macfrx->to_ds == 1))
	{
	payloadptr = ieee82011ptr +MAC_SIZE_LONG;
	payloadlen = ieee82011len -MAC_SIZE_LONG;
	}
else
	{
	payloadptr = ieee82011ptr +MAC_SIZE_NORM;
	payloadlen = ieee82011len -MAC_SIZE_NORM;
	}
if(macfrx->type == IEEE80211_FTYPE_MGMT)
	{
	if(macfrx->subtype == IEEE80211_STYPE_BEACON) process80211beacon(ifnr);
	else if(macfrx->subtype == IEEE80211_STYPE_PROBE_RESP) process80211probe_resp(ifnr);
	else if(macfrx->subtype == IEEE80211_STYPE_PROBE_REQ) process80211probe_req(ifnr);
	else if(macfrx->subtype == IEEE80211_STYPE_AUTH) process80211authentication(ifnr);
	else if(macfrx->subtype == IEEE80211_STYPE_ASSOC_REQ) process80211association_req(ifnr);
	else if(macfrx->subtype == IEEE80211_STYPE_REASSOC_REQ) process80211reassociation_req(ifnr);
	else if(macfrx->subtype == IEEE80211_STYPE_ASSOC_RESP) process80211association_resp(ifnr);
	else if(macfrx->subtype == IEEE80211_STYPE_REASSOC_RESP) process80211reassociation_resp(ifnr);
	else if(macfrx->subtype == IEEE80211_STYPE_ACTION) process80211action(ifnr);
	}
else if(macfrx->type == IEEE80211_FTYPE_CTL)
	{
	if(macfrx->subtype == IEEE80211_STYPE_BACK) process80211blockack(ifnr);
	else if(macfrx->subtype == IEEE80211_STYPE_BACK_REQ) process80211blockack_req(ifnr);
	else if(macfrx->subtype == IEEE80211_STYPE_PSPOLL) process80211powersave_poll(ifnr);
	}
else if(macfrx->type == IEEE80211_FTYPE_DATA)
	{
	if((macfrx->subtype &IEEE80211_STYPE_QOS_DATA) == IEEE80211_STYPE_QOS_DATA)
		{
		payloadptr += QOS_SIZE;
		payloadlen -= QOS_SIZE;
		}
	if((macfrx->subtype &IEEE80211_STYPE_NULLFUNC) == IEEE80211_STYPE_NULLFUNC) process80211null(ifnr);
	else if((macfrx->subtype &IEEE80211_STYPE_QOS_NULLFUNC) == IEEE80211_STYPE_QOS_NULLFUNC) process80211qosnull(ifnr);
	if(payloadlen < LLC_SIZE) return;
	llcptr = payloadptr;
	llc = (llc_t*)llcptr;
	if(((ntohs(llc->type)) == LLC_TYPE_AUTH) && (llc->dsap == LLC_SNAP) && (llc->ssap == LLC_SNAP)) process80211eap(ifnr);
#ifdef DUMPALL
	else writeepb((interfacelist +ifnr)->fdpcapng);
#endif
	}
return;
}
/*===========================================================================*/
static inline bool set_channel(int ifnr)
{
static struct iwreq pwrq;

memset(&pwrq, 0, sizeof(pwrq));
memcpy(pwrq.ifr_name, (interfacelist +ifnr)->ifname, IFNAMSIZ);
pwrq.u.freq.flags = IW_FREQ_FIXED;
pwrq.u.freq.m = (interfacelist +ifnr)->channel;
pwrq.u.freq.e = 0;
if(ioctl((interfacelist +ifnr)->fd, SIOCSIWFREQ, &pwrq) < 0) return false;
return true;
}
/*===========================================================================*/
static inline void fdloop()
{
static int ifnr;
static int csc;
static int sd;
static int fdnum;
static fd_set readfds;
static struct timespec tsfd;
static struct timespec sleepled;

fprintf(stdout, "entering main loop...\n");
sleepled.tv_sec = 0;
sleepled.tv_nsec = GPIO_LED_DELAY;
tsfd.tv_sec = FDTIMER;
tsfd.tv_nsec = 0;
for(ifnr = 0; ifnr < interfacecount; ifnr ++)
	{
	if(set_channel(ifnr) == false) return;
	}
csc = 0;
while(wantstopflag == false)
	{
	if(gpiobutton > 0)
		{
		if(GET_GPIO(gpiobutton) > 0) wantstopflag = true;
		}
	if(errorcount > ERROR_MAX) wantstopflag = true;
	gettimeofday(&tv, NULL);
	if((tv.tv_sec -tvold.tv_sec) >= 10)
		{
		tvold.tv_sec = tv.tv_sec;
		if(gpiostatusled > 0)
			{
			GPIO_SET = 1 << gpiostatusled;
			nanosleep(&sleepled, NULL);
			GPIO_CLR = 1 << gpiostatusled;
			if((tv.tv_sec - tvlast.tv_sec) > WATCHDOG)
				{
				nanosleep(&sleepled, NULL);
				GPIO_SET = 1 << gpiostatusled;
				nanosleep(&sleepled, NULL);
				GPIO_CLR = 1 << gpiostatusled;
				}
			}
		(interfacelist +0)->channel = channelscanlist[csc];
		set_channel(0);
		csc++;
		if(channelscanlist[csc] == 0) csc = 0;
		}
	FD_ZERO(&readfds);
	for(ifnr = 0; ifnr < interfacecount; ifnr++)
		{
		sd = (interfacelist +ifnr)->fd;
		FD_SET(sd, &readfds);
		}
	fdnum = pselect(sd +1, &readfds, NULL, NULL, &tsfd, NULL);
	if(fdnum < 0)
		{
		errorcount++;
		continue;
		}
	for(ifnr = 0; ifnr < interfacecount; ifnr++)
		{
		sd = (interfacelist +ifnr)->fd;
		if(FD_ISSET(sd, &readfds)) process_packet(ifnr, sd);
		}
	}
return;
}
/*===========================================================================*/
/*===========================================================================*/
static inline void emptyloop()
{
static struct timespec sleepled;

sleepled.tv_sec = 5;
sleepled.tv_nsec = 0;
while(wantstopflag == false)
	{
	if(gpiobutton > 0)
		{
		if(GET_GPIO(gpiobutton) > 0) break;
		}
	if(gpiostatusled > 0)
		{
		GPIO_SET = 1 << gpiostatusled;
		nanosleep(&sleepled, NULL);
		GPIO_CLR = 1 << gpiostatusled;
		nanosleep(&sleepled, NULL);
		GPIO_SET = 1 << gpiostatusled;
		nanosleep(&sleepled, NULL);
		GPIO_CLR = 1 << gpiostatusled;
		}
	nanosleep(&sleepled, NULL);
	}
return;
}
/*===========================================================================*/
/*===========================================================================*/
static inline void programmende(int signum)
{
if((signum == SIGINT) || (signum == SIGTERM) || (signum == SIGKILL)) wantstopflag = true;
return;
}
/*===========================================================================*/
static inline size_t chop(char *buffer, size_t len)
{
static char *ptr;

ptr = buffer +len -1;
while(len)
	{
	if (*ptr != '\n') break;
	*ptr-- = 0;
	len--;
	}
while(len)
	{
	if (*ptr != '\r') break;
	*ptr-- = 0;
	len--;
	}
return len;
}
/*---------------------------------------------------------------------------*/
static inline int fgetline(FILE *inputstream, size_t size, char *buffer)
{
static size_t len;
static char *buffptr;

if(feof(inputstream)) return -1;
buffptr = fgets (buffer, size, inputstream);
if(buffptr == NULL) return -1;
len = strlen(buffptr);
len = chop(buffptr, len);
return len;
}
/*===========================================================================*/
/*===========================================================================*/
static inline void getscanlist(int ifnr)
{
static int c;
static int csc;
static struct iwreq pwrq;

csc = 0;
for(c = 1; c < 256; c++)
	{
	memset(&pwrq, 0, sizeof(pwrq));
	memcpy(&pwrq.ifr_name, (interfacelist +ifnr)->ifname, IFNAMSIZ);
	pwrq.u.freq.flags = IW_FREQ_FIXED;
	pwrq.u.freq.m = c;
	pwrq.u.freq.e = 0;
	if(ioctl((interfacelist +ifnr)->fd , SIOCSIWFREQ, &pwrq) < 0) continue;
	if((interfacecount > 1) && (c == 1)) continue;
	if((interfacecount > 2) && (c == 6)) continue;
	if((interfacecount > 3) && (c == 11)) continue;
	if(c == 14) continue;
	channelscanlist[csc] = c;
	csc++;
	}
channelscanlist[csc] = 0;
return;
}
/*===========================================================================*/
static inline int opensocket()
{
static int ifnr;
static int csc;
static int fd_info;
static struct ifaddrs *ifaddr = NULL;
static struct ifaddrs *ifa = NULL;
static struct iwreq iwrinfo, iwr;
static struct iw_param param;
static struct ifreq ifr;
static struct sockaddr_ll ll;
static struct packet_mreq mr;
static struct ethtool_perm_addr *epmaddr;

ifnr = 0;
if(getifaddrs(&ifaddr) == -1) return 0;
for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
	if(ifnr == INTERFACE_MAX) break;
	if((ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_PACKET))
		{
		if((fd_info = socket(AF_INET, SOCK_STREAM, 0)) != -1)
			{
			memcpy((interfacelist +ifnr)->ifname, ifa->ifa_name, IFNAMSIZ);
			memset(&iwrinfo, 0, sizeof(iwr));
			memcpy(&iwrinfo.ifr_name, ifa->ifa_name, IFNAMSIZ);
			if(ioctl(fd_info, SIOCGIWNAME, &iwrinfo) != -1)
				{
				if(((interfacelist +ifnr)->fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) return 0;
					{
					if(bpf.len > 0) if(setsockopt((interfacelist +ifnr)->fd, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)) < 0) return 0;
					memset(&ifr, 0, sizeof(ifr));
					memcpy(&ifr.ifr_name, ifa->ifa_name, IFNAMSIZ);
					if(ioctl((interfacelist +ifnr)->fd, SIOCSIFFLAGS, &ifr) < 0) return 0;

					memset(&iwr, 0, sizeof(iwr));
					iwr.u.mode = IW_MODE_MONITOR;
					memcpy(&iwr.ifr_name, ifa->ifa_name, IFNAMSIZ);
					if(ioctl((interfacelist +ifnr)->fd, SIOCSIWMODE, &iwr) < 0) return 0;

					memset(&iwr, 0, sizeof(iwr));
					memcpy(&iwr.ifr_name, ifa->ifa_name, IFNAMSIZ);
					memset(&param,0 , sizeof(param));
					iwr.u.data.pointer = &param;
					ioctl((interfacelist +ifnr)->fd, SIOCSIWPOWER, &iwr);

					memset(&ifr, 0, sizeof(ifr));
					memcpy(&ifr.ifr_name, ifa->ifa_name, IFNAMSIZ);
					ifr.ifr_flags = IFF_UP | IFF_BROADCAST | IFF_RUNNING;
					if(ioctl((interfacelist +ifnr)->fd, SIOCSIFFLAGS, &ifr) < 0) return 0;

					memset(&ifr, 0, sizeof(ifr));
					memcpy(&ifr.ifr_name, ifa->ifa_name, IFNAMSIZ);
					ifr.ifr_flags = 0;
					if(ioctl((interfacelist +ifnr)->fd, SIOCGIFINDEX, &ifr) < 0) return 0;
					memset(&ll, 0, sizeof(ll));
					ll.sll_family = PF_PACKET;
					ll.sll_ifindex = ifr.ifr_ifindex;
					ll.sll_protocol = htons(ETH_P_ALL);
					ll.sll_halen = ETH_ALEN;
					ll.sll_pkttype = PACKET_OTHERHOST | PACKET_OUTGOING;
					if(bind((interfacelist +ifnr)->fd, (struct sockaddr*) &ll, sizeof(ll)) < 0) return 0;

					memset(&mr, 0, sizeof(mr));
					mr.mr_ifindex = ifr.ifr_ifindex;
					mr.mr_type = PACKET_MR_PROMISC;
					if(setsockopt((interfacelist +ifnr)->fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) < 0) return 0;

					epmaddr = (struct ethtool_perm_addr*)calloc(1, sizeof(struct ethtool_perm_addr) +6);
					if(!epmaddr) return 0;
					memset(&ifr, 0, sizeof(ifr));
					memcpy(&ifr.ifr_name, ifa->ifa_name, IFNAMSIZ);
					epmaddr->cmd = ETHTOOL_GPERMADDR;
					epmaddr->size = 6;
					ifr.ifr_data = (char*)epmaddr;
					if(ioctl((interfacelist +ifnr)->fd, SIOCETHTOOL, &ifr) < 0) return 0;
					if(epmaddr->size != 6) return 0;
					memcpy((interfacelist +ifnr)->ifmac, epmaddr->data, 6);
					free(epmaddr);
					}
				ifnr++;
				}
			close(fd_info);
			}
		}
	}
freeifaddrs(ifaddr);
interfacecount = ifnr;
if(interfacecount > 0) getscanlist(0);
(interfacelist +0)->channel = channelscanlist[0];
(interfacelist +1)->channel = 1;
(interfacelist +2)->channel = 6;
(interfacelist +3)->channel = 11;
csc = 0;
for(ifnr = 0; ifnr < interfacecount; ifnr++)
	{
	if(ifnr == 0)
		{
		fprintf(stdout, "%d %s scanlist:", ifnr,(interfacelist +ifnr)->ifname);
		while(channelscanlist[csc] != 0)
			{
			fprintf(stdout, "%d ", channelscanlist[csc]);
			csc++;
			}
		fprintf(stdout, "\n");
		}
	if(ifnr == 1) fprintf(stdout, "%d %s channel:%d\n", ifnr, (interfacelist +ifnr)->ifname, (interfacelist +ifnr)->channel);
	if(ifnr == 2) fprintf(stdout, "%d %s channel:%d\n", ifnr,(interfacelist +ifnr)->ifname, (interfacelist +ifnr)->channel);
	if(ifnr == 3) fprintf(stdout, "%d %s channel:%d\n", ifnr,(interfacelist +ifnr)->ifname, (interfacelist +ifnr)->channel);
	}
return ifnr;
}
/*===========================================================================*/
static inline void readbpfc(char *bpfname)
{
static int len;
static uint16_t c;
static struct sock_filter *zeiger;
static FILE *fh_filter;
static char linein[128];

if((fh_filter = fopen(bpfname, "r")) == NULL)
	{
	fprintf(stderr, "failed to open Berkeley Packet Filter list %s\n", bpfname);
	return;
	}
if((len = fgetline(fh_filter, 128, linein)) == -1)
	{
	fclose(fh_filter);
	fprintf(stderr, "failed to read Berkeley Packet Filter array size\n");
	return;
	}
sscanf(linein, "%"SCNu16, &bpf.len);
if(bpf.len == 0)
	{
	fclose(fh_filter);
	fprintf(stderr, "failed to read Berkeley Packet Filter array size\n");
	return;
	}
bpf.filter = (struct sock_filter*)calloc(bpf.len, sizeof(struct sock_filter));
c = 0;
zeiger = bpf.filter;
while(c < bpf.len)
	{
	if((len = fgetline(fh_filter, 128, linein)) == -1)
		{
		bpf.len = 0;
		break;
		}
	sscanf(linein, "%" SCNu16 "%" SCNu8 "%" SCNu8 "%" SCNu32, &zeiger->code, &zeiger->jt,  &zeiger->jf,  &zeiger->k);
	zeiger++;
	c++;
	}
if(bpf.len != c) fprintf(stderr, "failed to read Berkeley Packet Filter\n");
fclose(fh_filter);
return;
}
/*===========================================================================*/
static inline bool initgpio(int gpioperi)
{
static int fd_mem;

fd_mem = open("/dev/mem", O_RDWR|O_SYNC);
if(fd_mem < 0)
	{
	fprintf(stderr, "failed to get device memory\n");
	return false;
	}
gpio_map = mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd_mem, GPIO_BASE +gpioperi);
close(fd_mem);
if(gpio_map == MAP_FAILED)
	{
	fprintf(stderr, "failed to map GPIO memory\n");
	return false;
	}
gpio = (volatile unsigned *)gpio_map;
return true;
}
/*===========================================================================*/
static inline int getrpirev()
{
static FILE *fh_rpi;
static int len;
static int rpi = 0;
static int rev = 0;
static int gpioperibase = 0;
static char *revptr = NULL;
static const char *revstr = "Revision";
static const char *hwstr = "Hardware";
static const char *snstr = "Serial";
static char linein[128];

fh_rpi = fopen("/proc/cpuinfo", "r");
if(fh_rpi == NULL)
	{
	perror("failed to retrieve cpuinfo");
	return gpioperibase;
	}
while(1)
	{
	if((len = fgetline(fh_rpi, 128, linein)) == -1) break;
	if(len < 15) continue;
	if(memcmp(&linein, hwstr, 8) == 0)
		{
		rpi |= 1;
		continue;
		}
	if(memcmp(&linein, revstr, 8) == 0)
		{
		rpirevision = strtol(&linein[len -6], &revptr, 16);
		if((revptr - linein) == len)
			{
			rev = (rpirevision >> 4) &0xff;
			if(rev <= 3)
				{
				gpioperibase = GPIO_PERI_BASE_OLD;
				rpi |= 2;
				continue;
				}
			if(rev == 0x09)
				{
				gpioperibase = GPIO_PERI_BASE_OLD;
				rpi |= 2;
				continue;
				}
			if(rev == 0x0c)
				{
				gpioperibase = GPIO_PERI_BASE_OLD;
				rpi |= 2;
				continue;
				}
			if((rev == 0x04) || (rev == 0x08) || (rev == 0x0d) || (rev == 0x0e) || (rev == 0x11) || (rev == 0x13))
				{
				gpioperibase = GPIO_PERI_BASE_NEW;
				rpi |= 2;
				continue;
				}
			continue;
			}
		rpirevision = strtol(&linein[len -4], &revptr, 16);
		if((revptr - linein) == len)
			{
			if((rpirevision < 0x02) || (rpirevision > 0x15)) continue;
			if((rpirevision == 0x11) || (rpirevision == 0x14)) continue;
			gpioperibase = GPIO_PERI_BASE_OLD;
			rpi |= 2;
			}
		continue;
		}
	if(memcmp(&linein, snstr, 6) == 0)
		{
		rpi |= 4;
		continue;
		}
	}
fclose(fh_rpi);
if(rpi < 0x7) return 0;
return gpioperibase;
}
/*===========================================================================*/
static inline bool openpcapng()
{
static int ifnr;
static char timestring[16];
static char filename[PATH_MAX];

for(ifnr = 0; ifnr < interfacecount; ifnr++)
	{
	strftime(timestring, PATH_MAX, "%Y%m%d%H%M%S", localtime(&tv.tv_sec));
	snprintf(filename, PATH_MAX, "%s-%s", timestring, (interfacelist +ifnr)->ifname);
	(interfacelist +ifnr)->fdpcapng = hcxcreatepcapngdump(filename, (interfacelist +ifnr)->ifmac, (interfacelist +ifnr)->ifname, (interfacelist +ifnr)->macrgap, (interfacelist +ifnr)->rc, (interfacelist +ifnr)->anonce, (interfacelist +ifnr)->macrgclient, (interfacelist +ifnr)->snonce, 8, "12345678");
	if((interfacelist +ifnr)->fdpcapng == -1) return false;
	}
return true;
}
/*===========================================================================*/
static inline bool globalinit()
{
static int c;
static int ncc;
static int ifnr;
static int gpiobasemem = 0;
static uint32_t ouirgclient;
static uint32_t nicrgclient;
static uint8_t macrgclient[6];

gettimeofday(&tv, NULL);
timestamp = ((uint64_t)tv.tv_sec *1000000) +tv.tv_usec;
tvold.tv_sec = tv.tv_sec;
tvold.tv_usec = tv.tv_usec;
tvlast.tv_sec = tv.tv_sec;
tvlast.tv_sec = tv.tv_sec;
srand(time(NULL));
if((gpiobutton > 0) || (gpiostatusled > 0))
	{
	if(gpiobutton == gpiostatusled)
		{
		fprintf(stderr, "same value for wpi_button and wpi_statusled is not allowed\n");
		return false;
		}
	gpiobasemem = getrpirev();
	if(gpiobasemem == 0)
		{
		fprintf(stderr, "failed to locate GPIO\n");
		return false;
		}
	if(initgpio(gpiobasemem) == false)
		{
		fprintf(stderr, "failed to init GPIO\n");
		return false;
		}
	if(gpiostatusled > 0)
		{
		INP_GPIO(gpiostatusled);
		OUT_GPIO(gpiostatusled);
		}
	if(gpiobutton > 0)
		{
		INP_GPIO(gpiobutton);
		}
	}

wantstopflag = false;
interfacecount = INTERFACE_MAX;
errorcount = 0;
memset(&bpf, 0, sizeof(bpf));
mydeauthenticationsequence = 1;

if((interfacelist = (interfacelist_t*)calloc((INTERFACE_MAX), INTERFACELIST_SIZE)) == NULL) return false;

ncc = 0x200000;
ouirgclient = (myvendorclient[rand() %((MYVENDORCLIENT_SIZE /sizeof(int)))]) &0xffffff;
nicrgclient = rand() & 0xffffff;
macrgclient[5] = nicrgclient & 0xff;
macrgclient[4] = (nicrgclient >> 8) & 0xff;
macrgclient[3] = (nicrgclient >> 16) & 0xff;
macrgclient[2] = ouirgclient & 0xff;
macrgclient[1] = (ouirgclient >> 8) & 0xff;
macrgclient[0] = (ouirgclient >> 16) & 0xff;
for(ifnr = 0; ifnr < interfacecount; ifnr++)
	{
	(interfacelist +ifnr)->ouirgap = (myvendorap[rand() %((MYVENDORAP_SIZE /sizeof(int)))]) &0xfcffff;
	(interfacelist +ifnr)->nicrgap = (rand() & 0x0fffff) +ncc;
	ncc += 0x200000;
	(interfacelist +ifnr)->macrgap[5] = (interfacelist +ifnr)->nicrgap & 0xff;
	(interfacelist +ifnr)->macrgap[4] = ((interfacelist +ifnr)->nicrgap >> 8) & 0xff;
	(interfacelist +ifnr)->macrgap[3] = ((interfacelist +ifnr)->nicrgap >> 16) & 0xff;
	(interfacelist +ifnr)->macrgap[2] = (interfacelist +ifnr)->ouirgap & 0xff;
	(interfacelist +ifnr)->macrgap[1] = ((interfacelist +ifnr)->ouirgap >> 8) & 0xff;
	(interfacelist +ifnr)->macrgap[0] = ((interfacelist +ifnr)->ouirgap >> 16) & 0xff;
	(interfacelist +ifnr)->ouirgclient = ouirgclient;
	(interfacelist +ifnr)->nicrgclient = nicrgclient;
	memcpy((interfacelist +ifnr)->macrgclient, &macrgclient, 6);
	for(c = 0; c < 32; c++)
		{
		(interfacelist +ifnr)->anonce[c] = rand() %0xff;
		(interfacelist +ifnr)->snonce[c] = rand() %0xff;
		}
	(interfacelist +ifnr)->rc = (rand()%0xfff) +0xf000;
	if(((interfacelist +ifnr)->aplist = (aplist_t*)calloc((APLIST_MAX +1), APLIST_SIZE)) == NULL) return false;
	if(((interfacelist +ifnr)->rgaplist = (rgaplist_t*)calloc((RGAPLIST_MAX +1), RGAPLIST_SIZE)) == NULL) return false;
	if(((interfacelist +ifnr)->eapolm1list = (eapollist_t*)calloc((EAPOLLIST_MAX +1), EAPOLLIST_SIZE)) == NULL) return false;
	if(((interfacelist +ifnr)->eapolm2list = (eapollist_t*)calloc((EAPOLLIST_MAX +1), EAPOLLIST_SIZE)) == NULL) return false;
	if(((interfacelist +ifnr)->eapolm3list = (eapollist_t*)calloc((EAPOLLIST_MAX +1), EAPOLLIST_SIZE)) == NULL) return false;
	if(((interfacelist +ifnr)->owndlist = (owndlist_t*)calloc((OWNDLIST_MAX +1), OWNDLIST_SIZE)) == NULL) return false;
	}
signal(SIGINT, programmende);
return true;
}
/*===========================================================================*/
__attribute__ ((noreturn))
static inline void version(char *eigenname)
{
printf("%s %s (C) %s ZeroBeat\n", eigenname, VERSIONTAG, VERSIONYEAR);
exit(EXIT_SUCCESS);
}
/*---------------------------------------------------------------------------*/
__attribute__ ((noreturn))
static inline void usage(char *eigenname)
{
printf("%s %s  (C) %s ZeroBeat\n"
	"usage  : %s <options>\n"
	"\n"
	"short options:\n"
	"-h             : show this help\n"
	"-v             : show version\n"
	"\n"
	"long options:\n"
	"--gpio_button=<digit>     : Raspberry Pi GPIO pin number of button (2...27)\n"
	"                            default = GPIO not in use\n"
	"--gpio_statusled=<digit>  : Raspberry Pi GPIO number of status LED (2...27)\n"
	"                            default = GPIO not in use\n"
	"--bpfc=<file>             : input kernel space Berkeley Packet Filter (BPF) code\n"
	"                            affected: incoming and outgoing traffic - that include rca scan\n"
	"                            steps to create a BPF (it only has to be done once):\n"
	"                             set hcxdumptool monitormode\n"
	"                             $ hcxumptool -m <interface>\n"
	"                             create BPF to protect a MAC\n"
	"                             $ tcpdump -i <interface> not wlan addr1 11:22:33:44:55:66 and not wlan addr2 11:22:33:44:55:66 -ddd > protect.bpf\n"
	"                             recommended to protect own devices\n"
	"                             or create BPF to attack a MAC\n"
	"                             $ tcpdump -i <interface> wlan addr1 11:22:33:44:55:66 or wlan addr2 11:22:33:44:55:66 -ddd > attack.bpf\n"
	"                             not recommended, because important pre-authentication frames will be lost due to MAC randomization of the CLIENTs\n"
	"                             use the BPF code\n"
	"                             $ hcxumptool --bpfc=attack.bpf ...\n"
	"                             notice: this is a protect/attack, a capture and a display filter\n"
	"                             see man pcap-filter for a list of all filter options\n"
	"--help                    : show this help\n"
	"--version                 : show version\n",
	eigenname, VERSIONTAG, VERSIONYEAR, eigenname);
exit(EXIT_SUCCESS);
}
/*---------------------------------------------------------------------------*/
__attribute__ ((noreturn))
static inline void usageerror(char *eigenname)
{
printf("%s %s (C) %s by ZeroBeat\n"
	"usage: %s -h for help\n", eigenname, VERSIONTAG, VERSIONYEAR, eigenname);
exit(EXIT_FAILURE);
}
/*===========================================================================*/
int main(int argc, char *argv[])
{
static int auswahl;
static int index;
static char *bpfcname;

static const char *short_options = "hv";
static const struct option long_options[] =
{
	{"gpio_button",			required_argument,	NULL,	HCX_GPIO_BUTTON},
	{"gpio_statusled",		required_argument,	NULL,	HCX_GPIO_STATUSLED},
	{"bpfc",			required_argument,	NULL,	HCX_BPFC},
	{"version",			no_argument,		NULL,	HCX_VERSION},
	{"help",			no_argument,		NULL,	HCX_HELP},
	{NULL,				0,			NULL,	0}
};

auswahl = -1;
index = 0;
optind = 1;
optopt = 0;
gpiobutton = 0;
gpiostatusled = 0;
bpfcname = NULL;

while((auswahl = getopt_long(argc, argv, short_options, long_options, &index)) != -1)
	{
	switch (auswahl)
		{
		case HCX_GPIO_BUTTON:
		gpiobutton = strtol(optarg, NULL, 10);
		if((gpiobutton < 2) || (gpiobutton > 27))
			{
			fprintf(stderr, "invalid GPIO option\n");
			exit(EXIT_FAILURE);
			}
		if(gpiostatusled == gpiobutton)
			{
			fprintf(stderr, "invalid GPIO option\n");
			exit(EXIT_FAILURE);
			}
		break;

		case HCX_GPIO_STATUSLED:
		gpiostatusled = strtol(optarg, NULL, 10);
		if((gpiostatusled < 2) || (gpiostatusled > 27))
			{
			fprintf(stderr, "invalid GPIO option\n");
			exit(EXIT_FAILURE);
			}
		if(gpiostatusled == gpiobutton)
			{
			fprintf(stderr, "invalid GPIO option\n");
			exit(EXIT_FAILURE);
			}
		break;

		case HCX_BPFC:
		bpfcname = optarg;
		break;

		case HCX_HELP:
		usage(basename(argv[0]));
		break;

		case HCX_VERSION:
		version(basename(argv[0]));
		break;

		case '?':
		usageerror(basename(argv[0]));
		break;
		}
	}
setbuf(stdout, NULL);
if(argc < 2)
	{
	fprintf(stderr, "no option selected\n");
	exit(EXIT_FAILURE);
	}

if(getuid() != 0)
	{
	fprintf(stderr, "this program requires root privileges\n");
	exit(EXIT_FAILURE);
	}

globalinit();
if(bpfcname != NULL) readbpfc(bpfcname);
interfacecount = opensocket();
if((bpfcname != NULL) && (bpf.len == 0)) fprintf(stderr, "BPF code not loaded\n");
if(interfacecount > 0)
	{
	if(openpcapng() == true) fdloop();
	}
else emptyloop();
globalclose();

if(gpiobutton != 0)
	{
	if(system("poweroff") != 0) fprintf(stderr, "can't power off\n");
	}
return EXIT_SUCCESS;
}
/*===========================================================================*/
