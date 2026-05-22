/*
 * UAE - The Un*x Amiga Emulator
 *
 * Unix uaenet packet backend
 *
 * Copyright 2026 WinUAE contributors
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include <algorithm>
#include <cstring>

#include <ifaddrs.h>
#include <net/if.h>
#include <pcap/pcap.h>
#include <pcap/dlt.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <net/if_dl.h>
#endif
#if defined(__linux__)
#include <netpacket/packet.h>
#endif

#include "options.h"
#include "sana2.h"
#include "threaddep/thread.h"
#include "uaenet.h"
#include "uae.h"

int log_ethernet;

static struct netdriverdata tds[MAX_TOTAL_NET_DEVICES];
static int enumerated;
static int ethernet_paused;

struct uaenetdataunix
{
	void *user;
	struct netdriverdata *tc;
	uae_u8 *readbuffer;
	uae_u8 *writebuffer;
	int mtu;

	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_t *fp;
	uaenet_gotfunc *gotfunc;
	uaenet_getfunc *getfunc;

	volatile int threadactiver;
	uae_thread_id tidr;
	uae_sem_t sync_semr;
	volatile int threadactivew;
	uae_thread_id tidw;
	uae_sem_t sync_semw;
	uae_sem_t change_sem;
	uae_sem_t write_sem;
};

int uaenet_getdatalenght(void)
{
	return sizeof(struct uaenetdataunix);
}

static void uaenet_initdata(struct uaenetdataunix *sd, void *user)
{
	memset(sd, 0, sizeof(*sd));
	sd->user = user;
}

static void uaenet_trap_threadr(void *arg)
{
	struct uaenetdataunix *sd = (struct uaenetdataunix*)arg;

	uae_set_thread_priority(NULL, 1);
	sd->threadactiver = 1;
	uae_sem_post(&sd->sync_semr);

	while (sd->threadactiver == 1) {
		struct pcap_pkthdr *header = NULL;
		const u_char *pkt_data = NULL;
		const int r = pcap_next_ex(sd->fp, &header, &pkt_data);
		if (r == 1 && header && pkt_data && !ethernet_paused) {
			const int len = std::min<int>((int)header->caplen, (int)header->len);
			if (len > 0) {
				uae_sem_wait(&sd->change_sem);
				sd->gotfunc((struct s2devstruct*)sd->user, pkt_data, len);
				uae_sem_post(&sd->change_sem);
			}
		} else if (r == 0) {
			continue;
		} else if (r == PCAP_ERROR_BREAK && sd->threadactiver != 1) {
			break;
		} else if (r < 0) {
			write_log(_T("uaenet: pcap_next_ex failed, err=%d\n"), r);
			break;
		}
	}

	sd->threadactiver = 0;
	uae_sem_post(&sd->sync_semr);
}

static void uaenet_trap_threadw(void *arg)
{
	struct uaenetdataunix *sd = (struct uaenetdataunix*)arg;

	uae_set_thread_priority(NULL, 1);
	sd->threadactivew = 1;
	uae_sem_post(&sd->sync_semw);

	while (sd->threadactivew == 1) {
		int towrite = sd->mtu;
		bool wrote = false;

		uae_sem_wait(&sd->change_sem);
		if (sd->getfunc((struct s2devstruct*)sd->user, sd->writebuffer, &towrite)) {
			if (log_ethernet & 1) {
				TCHAR out[1600 * 2], *p = out;
				for (int i = 0; i < towrite && i < 1600; i++) {
					_stprintf(p, _T("%02x"), sd->writebuffer[i]);
					p += 2;
					*p = 0;
				}
				write_log(_T("OUT %4d: %s\n"), towrite, out);
			}
			if (pcap_sendpacket(sd->fp, sd->writebuffer, towrite) < 0) {
				TCHAR *err = au(pcap_geterr(sd->fp));
				write_log(_T("uaenet: pcap_sendpacket failed: %s\n"), err);
				xfree(err);
			}
			wrote = true;
		}
		uae_sem_post(&sd->change_sem);

		if (!wrote) {
			uae_sem_trywait_delay(&sd->write_sem, 100);
		}
	}

	sd->threadactivew = 0;
	uae_sem_post(&sd->sync_semw);
}

void uaenet_trigger(void *vsd)
{
	struct uaenetdataunix *sd = (struct uaenetdataunix*)vsd;
	if (sd) {
		uae_sem_post(&sd->write_sem);
	}
}

static const uae_u8 uaemac[] = { 0xaa, 0x82, 0x8a, 0x00, 0x00, 0x00 };

static bool get_interface_mac(const char *name, uae_u8 *mac)
{
	struct ifaddrs *ifaddr = NULL;
	bool found = false;

	if (getifaddrs(&ifaddr) != 0) {
		return false;
	}

	for (const struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_name || !ifa->ifa_addr || strcmp(ifa->ifa_name, name) != 0) {
			continue;
		}
#if defined(__linux__)
		if (ifa->ifa_addr->sa_family == AF_PACKET) {
			const struct sockaddr_ll *sll = (const struct sockaddr_ll*)ifa->ifa_addr;
			if (sll->sll_halen >= 6) {
				memcpy(mac, sll->sll_addr, 6);
				found = true;
				break;
			}
		}
#elif defined(AF_LINK)
		if (ifa->ifa_addr->sa_family == AF_LINK) {
			const struct sockaddr_dl *sdl = (const struct sockaddr_dl*)ifa->ifa_addr;
			if (sdl->sdl_alen >= 6) {
				memcpy(mac, LLADDR(sdl), 6);
				found = true;
				break;
			}
		}
#endif
	}

	freeifaddrs(ifaddr);
	return found;
}

int uaenet_open(void *vsd, struct netdriverdata *tc, void *user, uaenet_gotfunc *gotfunc, uaenet_getfunc *getfunc, int promiscuous, const uae_u8 *mac)
{
	struct uaenetdataunix *sd = (struct uaenetdataunix*)vsd;
	char *name;

	uaenet_initdata(sd, user);
	name = ua(tc->name);
	if (mac) {
		memcpy(tc->mac, mac, 6);
	}
	if (memcmp(tc->mac, tc->originalmac, 6) != 0) {
		promiscuous = 1;
	}

	sd->fp = pcap_open_live(name, 65536, promiscuous ? 1 : 0, 100, sd->errbuf);
	xfree(name);
	if (!sd->fp) {
		TCHAR *err = au(sd->errbuf);
		write_log(_T("uaenet: '%s' failed to open: %s\n"), tc->name, err);
		xfree(err);
		return 0;
	}

	if (pcap_datalink(sd->fp) != DLT_EN10MB) {
		write_log(_T("uaenet: '%s' is not an Ethernet adapter\n"), tc->name);
		uaenet_close(sd);
		return 0;
	}

	sd->tc = tc;
	sd->user = user;
	sd->mtu = tc->mtu > 0 ? tc->mtu : 1522;
	sd->readbuffer = xmalloc(uae_u8, sd->mtu);
	sd->writebuffer = xmalloc(uae_u8, sd->mtu);
	sd->gotfunc = gotfunc;
	sd->getfunc = getfunc;

	uae_sem_init(&sd->change_sem, 0, 1);
	uae_sem_init(&sd->write_sem, 0, 0);
	uae_sem_init(&sd->sync_semr, 0, 0);
	if (!uae_start_thread(_T("uaenet_unixr"), uaenet_trap_threadr, sd, &sd->tidr)) {
		goto end;
	}
	uae_sem_wait(&sd->sync_semr);

	uae_sem_init(&sd->sync_semw, 0, 0);
	if (!uae_start_thread(_T("uaenet_unixw"), uaenet_trap_threadw, sd, &sd->tidw)) {
		goto end;
	}
	uae_sem_wait(&sd->sync_semw);

	write_log(_T("uaenet_unix initialized\n"));
	return 1;

end:
	uaenet_close(sd);
	return 0;
}

void uaenet_close(void *vsd)
{
	struct uaenetdataunix *sd = (struct uaenetdataunix*)vsd;
	if (!sd) {
		return;
	}

	if (sd->threadactiver) {
		sd->threadactiver = -1;
		if (sd->fp) {
			pcap_breakloop(sd->fp);
		}
		uae_wait_thread(sd->tidr);
		write_log(_T("uaenet_unix read thread stopped\n"));
	}
	if (sd->threadactivew) {
		sd->threadactivew = -1;
		uae_sem_post(&sd->write_sem);
		uae_wait_thread(sd->tidw);
		write_log(_T("uaenet_unix write thread stopped\n"));
	}

	uae_sem_destroy(&sd->sync_semr);
	uae_sem_destroy(&sd->sync_semw);
	uae_sem_destroy(&sd->change_sem);
	uae_sem_destroy(&sd->write_sem);

	xfree(sd->readbuffer);
	xfree(sd->writebuffer);
	if (sd->fp) {
		pcap_close(sd->fp);
	}
	uaenet_initdata(sd, sd->user);
	write_log(_T("uaenet_unix closed\n"));
}

void uaenet_enumerate_free(void)
{
	for (int i = 0; i < MAX_TOTAL_NET_DEVICES; i++) {
		if (tds[i].name) {
			xfree((void*)tds[i].name);
		}
		if (tds[i].desc) {
			xfree((void*)tds[i].desc);
		}
		tds[i].active = 0;
		tds[i].name = NULL;
		tds[i].desc = NULL;
	}
	enumerated = 0;
}

static struct netdriverdata *enumit(const TCHAR *name)
{
	if (!name) {
		return tds;
	}
	for (int i = 0; i < MAX_TOTAL_NET_DEVICES; i++) {
		TCHAR mac[20];
		struct netdriverdata *tc = &tds[i];
		_stprintf(mac, _T("%02X:%02X:%02X:%02X:%02X:%02X"),
			tc->mac[0], tc->mac[1], tc->mac[2], tc->mac[3], tc->mac[4], tc->mac[5]);
		if (tc->active && (!_tcsicmp(name, tc->name) || !_tcsicmp(name, mac))) {
			return tc;
		}
	}
	return NULL;
}

struct netdriverdata *uaenet_enumerate(const TCHAR *name)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_if_t *alldevs = NULL;
	int cnt = 0;

	if (enumerated) {
		return enumit(name);
	}

	if (pcap_findalldevs(&alldevs, errbuf) < 0) {
		TCHAR *err = au(errbuf);
		write_log(_T("uaenet: failed to get interfaces: %s\n"), err);
		xfree(err);
		return NULL;
	}

	if (pcap_lib_version()) {
		TCHAR *version = au(pcap_lib_version());
		write_log(_T("uaenet: %s\n"), version);
		xfree(version);
	} else {
		write_log(_T("uaenet: libpcap\n"));
	}
	write_log(_T("uaenet: detecting interfaces\n"));

	for (pcap_if_t *d = alldevs; d && cnt < MAX_TOTAL_NET_DEVICES; d = d->next) {
		struct netdriverdata *tc = &tds[cnt];
		pcap_t *fp;
		char openerr[PCAP_ERRBUF_SIZE];
		const char *desc = d->description ? d->description : d->name;
		TCHAR *tname = au(d->name);
		TCHAR *tdesc = au(desc);

		write_log(_T("%s\n- %s\n"), tname, tdesc);

		fp = pcap_open_live(d->name, 65536, 0, 0, openerr);
		if (!fp) {
			TCHAR *err = au(openerr);
			write_log(_T("- pcap_open_live() failed: %s\n"), err);
			xfree(err);
			xfree(tname);
			xfree(tdesc);
			continue;
		}
		const int datalink = pcap_datalink(fp);
		pcap_close(fp);
		if (datalink != DLT_EN10MB) {
			write_log(_T("- not an Ethernet adapter (%d)\n"), datalink);
			xfree(tname);
			xfree(tdesc);
			continue;
		}

		memset(tc, 0, sizeof(*tc));
		memcpy(tc->mac, uaemac, 6);
		if (get_interface_mac(d->name, tc->mac)) {
			memcpy(tc->originalmac, tc->mac, 6);
		} else {
			memcpy(tc->originalmac, uaemac, 6);
		}

		write_log(_T("- MAC %02X:%02X:%02X:%02X:%02X:%02X -> %02X:%02X:%02X:%02X:%02X:%02X\n"),
			tc->mac[0], tc->mac[1], tc->mac[2], tc->mac[3], tc->mac[4], tc->mac[5],
			uaemac[0], uaemac[1], uaemac[2], tc->mac[3], tc->mac[4], tc->mac[5]);
		memcpy(tc->mac, uaemac, 3);
		tc->type = UAENET_PCAP;
		tc->active = 1;
		tc->mtu = 1522;
		tc->name = tname;
		tc->desc = tdesc;
		cnt++;
	}

	write_log(_T("uaenet: end of detection, %d devices found.\n"), cnt);
	pcap_freealldevs(alldevs);
	enumerated = 1;
	return enumit(name);
}

void uaenet_close_driver(struct netdriverdata *tc)
{
	if (!tc) {
		return;
	}
	for (int i = 0; i < MAX_TOTAL_NET_DEVICES; i++) {
		tds[i].active = 0;
	}
}

void ethernet_pause(int pause)
{
	ethernet_paused = pause;
}

void ethernet_reset(void)
{
	ethernet_paused = 0;
}
