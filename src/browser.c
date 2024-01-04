#include "browser.h"
#include "lwip/tcp.h"

static struct tcp_pcb *pcb = NULL;

static char buf[1000] = {0};
static size_t len = 0;

static void recv(void *arg, struct tcp_pcb *tpcb,
                 struct pbuf *p, err_t err) {

}

void browser_init(void) {

}

void browser_update(void) {
    if (len > 0 && tcp_sndbuf(pcb) <= len) {
        err_t err = tcp_write(pcb, buf, len, 0);
        if (err != ERR_OK) {
            dbg_printf("Error %i in tcp write\n", err);
        }
        len = 0;
    }
}

void browser_navigate(const char *url) {
    if (pcb) {
        tcp_close(pcb);
        pcb = NULL;
    }

    struct ip_addr proxy = IPADDR4_INIT_BYTES(192,168,25,1);
    u16_t port = 3128;

    pcb = tcp_new();
    err_t err = tcp_connect(pcb, &proxy, port, NULL);
    if (err != ERR_OK) {
        dbg_printf("Error %i in tcp connect\n", err);
    }
    len = snprintf(buf, sizeof buf, "GET %s HTTP/1.0\r\n\r\n", url);
    tcp_recv(pcb, recv);
}
