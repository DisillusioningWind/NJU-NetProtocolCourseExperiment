#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

int topology_getNodeIDfromip(struct in_addr *addr)
{
    uint32_t addr_net = ntohl(addr->s_addr);
    printf("addr小端:%x\n", addr_net);
    int last = addr_net & 0xff;
    return last;
}

int topology_getIDIPfromname(char *hostname, in_addr_t *addr, int *id)
{
    struct hostent *host = gethostbyname(hostname);
    if (host == NULL)
        return -1;
    uint32_t addr_net = ntohl(*((uint32_t *)host->h_addr_list[0]));
    printf("addr小端:%x\n", addr_net);
    *addr = *((in_addr_t *)host->h_addr_list[0]);
    *id = addr_net & 0xff;
    return 0;
}

int main()
{
    printf("%d\n", 1 != htonl(1));
    char hostname[100];
    char hostipstr[100];
    gethostname(hostname, sizeof(hostname));
    printf("%s\n", hostname);
    struct hostent *host = gethostbyname(hostname);
    if (host == NULL) return -1;

    inet_ntop(host->h_addrtype, host->h_addr_list[0], hostipstr, sizeof(hostipstr));
    struct in_addr ip_addr;
    if (inet_aton(hostipstr, &ip_addr) == 0)
    {
        printf("Invalid IP address!\n");
        exit(1);
    }
    in_addr_t addr;
    int id;

    printf("addr大端:%x\n", ip_addr.s_addr);
    id = topology_getNodeIDfromip(&ip_addr);
    printf("id:%d\n", id);

    topology_getIDIPfromname(hostname, &addr, &id);
    printf("addr大端:%x\n", addr);
    printf("id:%d\n", id);
    return 0;
}