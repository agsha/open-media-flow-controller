/* Test sending and receving over a pair of ethernet ports. */
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#define ETHERTYPE_LEN 2
#define ETHHDR_LEN 14
#define MAC_ADDR_LEN 6
#define BUFFER_LEN 4096

// Our internal functions
int get_time(char *buffer);
int ethx_function( void * ethx_sock_fd);
int ethy_function( void * ethy_sock_fd);
void fill_send_data(void);

int ethx_transmit_count=0;
int ethy_transmit_count=0;
int ethx_receive_count=0;
int ethy_receive_count=0;
int ethx_receive_not_ours_count=0;
int ethy_receive_not_ours_count=0;

unsigned char ethxMac[6];
unsigned char ethyMac[6];

struct ifreq ethx_ifr;
struct ifreq ethy_ifr;
char ethx_index , ethy_index;
char ethx_buffer[BUFFER_LEN];
char ethy_buffer[BUFFER_LEN];
struct sockaddr_ll ethx_sock_addr , ethy_sock_addr;
short int etherTypeT = 0x0008;
char ethx_devname[10];
char ethy_devname[10];
char ethx_name[10];
char ethy_name[10];

fd_set ethy_socks;
fd_set ethx_socks;
char send_data[11][1600];
FILE *ethx_fptr;
FILE *ethy_fptr;
FILE *fptr;

/*---------------------------------------------------------------------------*/
int get_time(char *buffer)
{
  struct timeval tv;
  time_t curtime;
  char tmp_buffer[30];
  gettimeofday(&tv, NULL);
  curtime=tv.tv_sec;
  strftime(tmp_buffer,30,"%a %d  %T.",localtime(&curtime));
  //sprintf(buffer,"%s%ld",tmp_buffer,tv.tv_usec);
  sprintf(buffer,"%s",tmp_buffer);
  return(1);
}

/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{

  char ethx_log[50];
  char ethy_log[50];
  char log_file[50];
  if( argc == 3)
  {
    strcpy(ethx_devname,argv[1]);
    strcpy(ethy_devname,argv[2]);
    strcpy(ethx_name,argv[1]);
    strcpy(ethy_name,argv[2]);
  }
  else if (argc == 5 )
  {
    strcpy(ethx_devname,argv[1]);
    strcpy(ethy_devname,argv[2]);
    strcpy(ethx_name,argv[3]);
    strcpy(ethy_name,argv[4]);
  }
  else
  {
    printf("Usage : <ethlooptest> <ethx> <ethy> [<namex> <namey>]\n");
    exit(0);
  }
  
  int ethx_sock_fd = 0, ethy_sock_fd=0;
  
  // create log files
  
  sprintf(ethx_log,"/tmp/%s_log",ethx_name);
  ethx_fptr=fopen(ethx_log,"w");
  
  sprintf(ethy_log,"/tmp/%s_log",ethy_name);
  ethy_fptr=fopen(ethy_log,"w");
  
  sprintf(log_file,"ethtest-%s-%s.log",ethx_name,ethy_name);
  fptr=fopen(log_file,"w");
  
  memset(&ethx_sock_addr, 0, sizeof(struct sockaddr_ll));
  memset(&ethy_sock_addr, 0, sizeof(struct sockaddr_ll));
  memset(ethx_buffer, 0, BUFFER_LEN);
  memset(ethy_buffer, 0, BUFFER_LEN);
  
  if((ethx_sock_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
  {
    fprintf(ethx_fptr,"Error : Socket creation failed:\n");
    exit(1);
  }
  
  if((ethy_sock_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
  {
    fprintf(ethy_fptr,"Error : Socket creation failed:\n");
    exit(1);
  }
  
  strncpy(ethx_ifr.ifr_name, ethx_devname , IFNAMSIZ);
  strncpy(ethy_ifr.ifr_name, ethy_devname , IFNAMSIZ);
  
  // retrieve ethx interface index
  
  if (ioctl(ethx_sock_fd, SIOCGIFINDEX, &ethx_ifr) == -1)
  {
    fprintf(ethx_fptr,"Error : ethx interface ifr index failed");
    exit(1);
  }
  ethx_index=ethx_ifr.ifr_ifindex;
  
  // retrieve ethy interface index
  
  if (ioctl(ethy_sock_fd, SIOCGIFINDEX, &ethy_ifr) == -1)
  {
    fprintf(ethy_fptr,"Error : ethy interface ifr index failed");
    exit(1);
  }
  ethy_index=ethy_ifr.ifr_ifindex;
  
  // bind the ethy(receive) socket to the interface
  
  ethy_sock_addr.sll_family   = PF_PACKET;
  ethy_sock_addr.sll_protocol = htons(ETH_P_ALL);
  ethy_sock_addr.sll_ifindex  = ethy_index;
  ethy_sock_addr.sll_halen = ETH_ALEN;
  ethy_sock_addr.sll_hatype = ARPHRD_ETHER;
  ethy_sock_addr.sll_pkttype = PACKET_OTHERHOST;
  
  if( bind(ethy_sock_fd, (struct sockaddr *) &ethy_sock_addr, sizeof(struct sockaddr_ll)) < 0 )
  {
    fprintf(ethy_fptr,"Error : Socket interface bind failed \n");
    exit(1);
  }
  
  ethx_sock_addr.sll_family = PF_PACKET;
  ethx_sock_addr.sll_protocol = htons(ETH_P_ALL);
  ethx_sock_addr.sll_halen = ETH_ALEN;
  ethx_sock_addr.sll_ifindex = ethx_index;
  ethx_sock_addr.sll_hatype = ARPHRD_ETHER;
  ethx_sock_addr.sll_pkttype = PACKET_OTHERHOST;
  
  if( bind(ethx_sock_fd, (struct sockaddr *) &ethx_sock_addr, sizeof(struct sockaddr_ll)) < 0 )
  {
    fprintf(ethy_fptr,"Error : Socket interface bind failed \n");
    fflush(ethy_fptr);
    exit(1);
  }

  // retrieve ethx MAC
  
  if (ioctl(ethx_sock_fd, SIOCGIFHWADDR, &ethx_ifr) == -1)
  {
    fprintf(ethx_fptr,"Error : retrieving Mac Id failed\n");
    exit(1);
  }
  int i;
  for(i=0;i<6;i++)
  {
    ethxMac[i]= (unsigned char)ethx_ifr.ifr_hwaddr.sa_data[i];
  }
  
  // retrieve ethy MAC
  
  if (ioctl(ethy_sock_fd, SIOCGIFHWADDR, &ethy_ifr) == -1)
  {
    fprintf(ethy_fptr,"Error : retrieving Mac Id failed\n");
    exit(1);
  }
  for(i=0;i<6;i++)
  {
    ethyMac[i]= (unsigned char)ethy_ifr.ifr_hwaddr.sa_data[i];
  }
  
  memcpy(&(ethx_sock_addr.sll_addr), ethyMac, MAC_ADDR_LEN);
  memcpy(&(ethy_sock_addr.sll_addr), ethxMac, MAC_ADDR_LEN);
  
  fill_send_data();
  // create thread functions
  pthread_t ethx_thread;
  pthread_t ethy_thread;
  int ret_val;
  
  ret_val=pthread_create(&ethy_thread,NULL,(void *) ethy_function,&ethy_sock_fd);
  if(ret_val)
  {
    fprintf(ethy_fptr,"Error : Unable to create the ethy send thread\n");
    exit(1);
  }
  
  ret_val=pthread_create(&ethx_thread,NULL,(void *) ethx_function,&ethx_sock_fd);
  if(ret_val)
  {
    fprintf(ethx_fptr,"Error : Unable to create the ethx send thread\n");
    exit(1);
  }
  
  ret_val = pthread_join(ethx_thread, NULL);
  ret_val = pthread_join(ethy_thread, NULL);
} // End main()
  
/*---------------------------------------------------------------------------*/
int ethx_function( void * ethx_sock_fd)
{
  int bytes_sent;
  int bytes_received;
  int sock_fd = * ( int * ) ethx_sock_fd;
  char *time_buffer=(char *)malloc(100);
  /* Ethernet Header Construction */
  memcpy(ethx_buffer, ethyMac, MAC_ADDR_LEN);
  memcpy((ethx_buffer+MAC_ADDR_LEN),ethxMac , MAC_ADDR_LEN);
  memcpy((ethx_buffer+(2*MAC_ADDR_LEN)), (void *)&(etherTypeT), sizeof(etherTypeT));
  
  int i;
  for(i=0;i<=10;i++)
  {
    memcpy((ethx_buffer+14),send_data[i],strlen(send_data[i]));
    bytes_sent = sendto(sock_fd, ethx_buffer, 14+strlen(send_data[i]), 0,
                 (struct sockaddr*)&ethx_sock_addr, sizeof(struct sockaddr_ll));
    if (bytes_sent == -1)
    {
      fprintf(ethx_fptr,"Error : Packet send failed!\n");
      exit(1);
      }
    else
    {
      get_time(time_buffer);
      ethx_transmit_count = ethx_transmit_count + 1;
      fprintf(ethx_fptr,"%s\t\t%d bytes of data is sent\n",time_buffer,bytes_sent);
      fflush(ethx_fptr);
  
    }
  sleep(1);
  }
  fprintf(ethx_fptr,"\nSummary : Total No of Packets sent from interface %s  : %d \n\n",
    ethx_name,ethx_transmit_count);
  
  memset(ethx_buffer, 0, BUFFER_LEN);
  FD_ZERO(&ethx_socks);
  FD_SET(sock_fd,&ethx_socks);
  
  struct timeval timeout;
  timeout.tv_sec = 20;
  timeout.tv_usec = 0;
  int readsocks;
  int total;
  int missed;
  while (1)
  {
    readsocks = select(sock_fd+1, &ethx_socks, (fd_set *) 0,(fd_set *) 0, &timeout);
    if ( readsocks > 0)
    {
      if (FD_ISSET(sock_fd,&ethx_socks))
      {
        bytes_received=recvfrom(sock_fd, ethx_buffer, BUFFER_LEN, 0, NULL, NULL);
        if(bytes_received > 0)
        {
          get_time(time_buffer);
          if( *(ethx_buffer+14) == 'z' )
          {
            ethx_receive_count=ethx_receive_count+1;
          }
          else
          {
            fprintf(ethx_fptr,"following data is not the one which we sent\n");
            ethx_receive_not_ours_count=ethx_receive_not_ours_count+1;
          }
          fprintf(ethx_fptr,"%s\t\t%d bytes of data is received\n",time_buffer,bytes_received);
          fflush(ethx_fptr);
        }
      }
    }
    else
    {
      total = ethx_receive_count + ethx_receive_not_ours_count;
      fprintf(ethx_fptr,"\nSummary : Total No of Packets received at interface %s  : %d \n\n",
        ethx_name, total);
      get_time(time_buffer);
      fprintf(fptr,"Ethlooptest : From %s to %s  : %s \n", ethy_name, ethx_name, time_buffer);
      fprintf(fptr,"No of Packets transmitted : %d \n",ethy_transmit_count);
      fprintf(fptr,"No of Packets received    : %d \n",ethx_receive_count);
      // It is ok to have missed a few packets.
      missed = ethy_transmit_count - ethx_receive_count ;
      if ( missed > 4 )
      {
        fprintf(fptr,"Result : %s FAIL\n\n", ethy_name);
      }
      else
      {
        fprintf(fptr,"Result : %s PASS\n\n", ethy_name);
      }
      pthread_exit(0);
    }
  }
} // END ethx_function()

/*---------------------------------------------------------------------------*/
int ethy_function( void * ethy_sock_fd)
{
  int bytes_received;
  int bytes_sent;
  int sock_fd = * ( int * ) ethy_sock_fd;
  char *time_buffer=(char *)malloc(100);
  
  FD_ZERO(&ethy_socks);
  FD_SET(sock_fd,&ethy_socks);
  
  struct timeval timeout;
  timeout.tv_sec = 20;
  timeout.tv_usec = 0;
  int readsocks;
  int total;
  int missed;
  while (1)
  {
    readsocks = select(sock_fd+1, &ethy_socks, (fd_set *) 0,(fd_set *) 0, &timeout);
    if ( readsocks > 0)
    {
      if (FD_ISSET(sock_fd,&ethy_socks))
      {
        bytes_received=recvfrom(sock_fd, ethy_buffer, BUFFER_LEN, 0, NULL, NULL);
        if(bytes_received > 0)
        {
          get_time(time_buffer);
          if( *(ethy_buffer+14) == 'z' )
          {
            ethy_receive_count=ethy_receive_count+1;
          }
          else
          {
            fprintf(ethy_fptr,"following data is not the one which we sent\n");
            ethy_receive_not_ours_count=ethy_receive_not_ours_count+1;
          }
          fprintf(ethy_fptr,"%s\t\t%d bytes of data is received\n",time_buffer,bytes_received);
          fflush(ethy_fptr);
        }
      }
    }
    else
    {
      total = ethy_receive_count + ethy_receive_not_ours_count;
      fprintf(ethy_fptr,"\nSummary : Total No of Packets received at interface %s  : %d \n\n",
        ethy_name, total);
      get_time(time_buffer);
      fprintf(fptr,"Ethlooptest : From %s to %s  : %s\n", ethx_name, ethy_name, time_buffer);
      fprintf(fptr,"No of Packets transmitted : %d \n",ethx_transmit_count);
      fprintf(fptr,"No of Packets received    : %d \n",ethy_receive_count);
      missed = ethx_transmit_count - ethy_receive_count ;
      if ( missed > 4 )
      {
        fprintf(fptr,"Result : %s FAIL\n\n", ethx_name);
      }
      else
      {
        fprintf(fptr,"Result : %s PASS\n\n", ethx_name);
      }
      break;
    }
  }
  
  memset(ethy_buffer, 0, BUFFER_LEN);
  memcpy(ethy_buffer, ethxMac, MAC_ADDR_LEN);
  memcpy((ethy_buffer+MAC_ADDR_LEN),ethyMac , MAC_ADDR_LEN);
  memcpy((ethy_buffer+(2*MAC_ADDR_LEN)), (void *)&(etherTypeT), sizeof(etherTypeT));
  
  int i;
  for(i=0;i<=10;i++)
  {
    memcpy((ethy_buffer+14),send_data[i],strlen(send_data[i]));
    bytes_sent = sendto(sock_fd, ethy_buffer, 14+strlen(send_data[i]), 0,
                 (struct sockaddr*)&ethy_sock_addr, sizeof(struct sockaddr_ll));
    if (bytes_sent == -1)
    {
      fprintf(ethy_fptr,"Error : Packet send failed!\n");
      exit(1);
    }
    else
    {
      get_time(time_buffer);
      ethy_transmit_count = ethy_transmit_count + 1;
      fprintf(ethy_fptr,"%s\t\t%d bytes of data is sent\n",time_buffer,bytes_sent);
      fflush(ethy_fptr);
    }
  sleep(1);
  }
  fprintf(ethy_fptr,"\nSummary : Total No of Packets sent from interface %s  : %d \n\n",
    ethy_name,ethy_transmit_count);
  pthread_exit(0);
} // End ethy_function()


void fill_send_data()
{
  strcpy(send_data[0],"z");
  strcpy(send_data[1],"zbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  strcpy(send_data[2],"zcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
  strcpy(send_data[3],"zdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
  strcpy(send_data[4],"zeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  strcpy(send_data[5],"zffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
  strcpy(send_data[6],"zgggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg");

  strcpy(send_data[7],"zhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");

  strcpy(send_data[8],"ziiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");

  strcpy(send_data[9],"zjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");

  strcpy(send_data[10],"zkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");

} // End fill_send_data()

