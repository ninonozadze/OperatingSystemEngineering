#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

struct queued_packet {
  uint32 source_ip;
  uint16 source_port;
  char *packet_data;
  int length_of_packet;
  struct queued_packet *next;
};

struct bound_port {
  struct queued_packet *head;
  struct queued_packet *tail;
  uint16 port_number;
  int is_bound;
  int current_number_of_packets_in_queue;
  struct spinlock lock;
};

#define MAXIMUM_NUMBER_OF_PORTS_THAT_CAN_BE_BOUND 64
#define MAXIMUM_QUEUED_PACKETS_PER_PORT 16

static struct bound_port ports[MAXIMUM_NUMBER_OF_PORTS_THAT_CAN_BE_BOUND];

void
netinit(void)
{
  initlock(&netlock, "netlock");
  for(int i = 0; i < MAXIMUM_NUMBER_OF_PORTS_THAT_CAN_BE_BOUND; i++) {
    ports[i].is_bound = 0;
    initlock(&ports[i].lock, "portlock");
  }
}

static uint16 min(uint16 a, uint16 b) {
  return a < b ? a : b;
}

static struct bound_port*
find_bound_port(uint16 port)
{
  for(int i = 0; i < MAXIMUM_NUMBER_OF_PORTS_THAT_CAN_BE_BOUND; i++) {
    if(ports[i].is_bound && ports[i].port_number == port)
      return &ports[i];
    if(!ports[i].is_bound) {
      return &ports[i];
    }
  }
  return 0;
}

//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  //
  // Your code here.
  //


  int argint_res;
  argint(0, &argint_res);
  if(argint_res < 0)
    return -1;

  int port;
  port = argint_res;

  acquire(&netlock);
  struct bound_port *bp = find_bound_port(port);
  if(bp == 0) {
    release(&netlock);
    return -1;
  }

  if(bp->is_bound) {
    release(&netlock);
    return -1;
  }

  bp->head = 0;
  bp->tail = 0;
  bp->is_bound = 1;
  bp->current_number_of_packets_in_queue = 0;
  bp->port_number = port;
  release(&netlock);

  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  //
  // Your code here.
  //

  int argint_res;
  argint(0, &argint_res);
  if(argint_res < 0)
    return -1;

  int dport;
  dport = argint_res;

  uint64 src_addr;

  argaddr(1, &src_addr);
  if(src_addr == 0)
    return -1;

  uint64 sport_addr;

  argaddr(2, &sport_addr);
  if(sport_addr == 0)
    return -1;

  uint64 buf;

  argaddr(3, &buf);
  if(buf == 0)
    return -1;

  int maxlen;

  argint(4, &argint_res);
  if(argint_res < 0)
    return -1;
  maxlen = argint_res;

  acquire(&netlock);
  struct bound_port *bp = find_bound_port(dport);
  if(bp == 0 || !bp->is_bound) {
    release(&netlock);
    return -1;
  }
  release(&netlock);

  acquire(&bp->lock);
  while(bp->current_number_of_packets_in_queue == 0) {
    sleep(&bp->head, &bp->lock);
  }

  struct queued_packet *pkt = bp->head;
  bp->head = pkt->next;
  if(bp->head == 0)
    bp->tail = 0;
  bp->current_number_of_packets_in_queue--;

  struct proc *p = myproc();
  if(copyout(p->pagetable, src_addr, (char *)&pkt->source_ip, sizeof(uint32)) < 0 ||
     copyout(p->pagetable, sport_addr, (char *)&pkt->source_port, sizeof(uint16)) < 0 ||
     copyout(p->pagetable, buf, pkt->packet_data, min(maxlen, pkt->length_of_packet)) < 0) {
    kfree(pkt->packet_data);
    kfree(pkt);
    release(&bp->lock);
    return -1;
     }

  int copied = min(maxlen, pkt->length_of_packet);
  kfree(pkt->packet_data);
  kfree(pkt);
  release(&bp->lock);

  return copied;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  //
  // Your code here.
  //

  struct eth *eth = (struct eth *)buf;
  struct ip *ip = (struct ip *)(eth + 1);

  if(ip->ip_p != IPPROTO_UDP) {
    kfree(buf);
    return;
  }

  struct udp *udp = (struct udp *)(ip + 1);
  uint16 dport = ntohs(udp->dport);

  acquire(&netlock);
  struct bound_port *bp = find_bound_port(dport);
  if(bp == 0 || !bp->is_bound) {
    release(&netlock);
    kfree(buf);
    return;
  }
  release(&netlock);

  acquire(&bp->lock);
  if(bp->current_number_of_packets_in_queue >= MAXIMUM_QUEUED_PACKETS_PER_PORT) {
    release(&bp->lock);
    kfree(buf);
    return;
  }

  struct queued_packet *pkt = kalloc();
  if(pkt == 0) {
    release(&bp->lock);
    kfree(buf);
    return;
  }

  int payload_len = ntohs(udp->ulen) - sizeof(struct udp);
  char *payload = kalloc();
  if(payload == 0) {
    kfree(pkt);
    release(&bp->lock);
    kfree(buf);
    return;
  }

  memmove(payload, (char *)(udp + 1), payload_len);
  pkt->source_ip = ntohl(ip->ip_src);
  pkt->source_port = ntohs(udp->sport);
  pkt->packet_data = payload;
  pkt->length_of_packet = payload_len;
  pkt->next = 0;

  if(bp->tail == 0) {
    bp->head = pkt;
  } else {
    bp->tail->next = pkt;
  }
  bp->tail = pkt;
  bp->current_number_of_packets_in_queue++;

  wakeup(&bp->head);
  release(&bp->lock);

  kfree(buf);
  
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
