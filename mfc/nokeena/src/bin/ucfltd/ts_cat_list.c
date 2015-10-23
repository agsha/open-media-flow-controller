#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <md_client.h>

#include "nkn_defs.h"
#include "nkn_debug.h"


typedef struct ts_cat_info_ {
    char code[3] ;
    int  int_code;
    char name[100];
} ts_category_info_t;


/*
 * Pre defined URL categories and their correponding alpha and
 * numberic codes.
 */
ts_category_info_t ts_category_list[] = 
{
  {"ac" ,100,"Art/Culture/Heritage"}, 
  {"al" ,101,"Alcohol"}, 
  {"an" ,102,"Anonymizers"}, 
  {"au" ,104,"Anonymizing Utilities"}, 
  {"bl" ,188,"Blogs/Wiki"}, 
  {"bu" ,105,"Business"}, 
  {"ch" ,106,"Chat"}, 
  {"ci" ,107,"Computing/Internet"}, 
  {"cm" ,108,"Public Information"}, 
  {"cs" ,109,"Criminal Activities"}, 
  {"cv" ,168,"Game/Cartoon Violence"}, 
  {"dc" ,189,"Digital Postcards"}, 
  {"dr" ,110,"Drugs"}, 
  {"eb" ,158,"Auctions/Classifieds"}, 
  {"ed" ,111,"Education/Reference"}, 
  {"et" ,112,"Entertainment"}, 
  {"ex" ,113,"Extreme"}, 
  {"fb" ,174,"Fashion/Beauty"}, 
  {"fi" ,114,"Finance/Banking"}, 
  {"fk" ,600,"For Kids"}, 
  {"gb" ,115,"Gambling"}, 
  {"gm" ,116,"Games"}, 
  {"gr" ,166,"Gambling Related"}, 
  {"gv" ,117,"Government/Military"}, 
  {"hi" ,601,"History"}, 
  {"hk" ,118,"Hacking/Computer Crime"}, 
  {"hl" ,119,"Health"}, 
  {"hm" ,120,"Humor/Comics"}, 
  {"hr" ,190,"Historical Revisionism"}, 
  {"hs" ,121,"Hate/Discrimination"}, 
  {"hw" ,175,"Software/Hardware"}, 
  {"ia" ,172,"Interactive Web Applications"}, 
  {"il" ,176,"Illegal Software"}, 
  {"im" ,122,"Instant Messaging"}, 
  {"in" ,123,"Stock Trading"}, 
  {"io" ,126,"Information Security"}, 
  {"ir" ,124,"Internet Radio/TV"}, 
  {"is" ,177,"Content Server"}, 
  {"it" ,178,"Internet Services"}, 
  {"js" ,125,"Job Search"}, 
  {"mb" ,159,"Forum/Bulletin Boards"}, 
  {"md" ,179,"Media Sharing"}, 
  {"mg" ,167,"Messaging"}, 
  {"mk" ,181,"Marketing/Merchandising"}, 
  {"mm" ,127,"Dating/Social Networking"}, 
  {"mn" ,180,"Incidental Nudity"}, 
  {"mo" ,128,"Mobile Phone"}, 
  {"mp" ,129,"Media Downloads"}, 
  {"mr" ,602,"Moderated"}, 
  {"ms" ,130,"Malicious Sites"}, 
  {"na" ,131,"Usenet News"}, 
  {"nd" ,132,"Nudity"}, 
  {"np" ,133,"Non-Profit/Advocacy/NGO"}, 
  {"ns" ,170,"Personal Network Storage"}, 
  {"nw" ,134,"General News"}, 
  {"os" ,136,"Online Shopping"}, 
  {"pa" ,137,"Provocative Attire"}, 
  {"pd" ,183,"Parked Domain"}, 
  {"ph" ,169,"Phishing"}, 
  {"pm" ,184,"Pharmacy"}, 
  {"pn" ,138,"P2P/File Sharing"}, 
  {"po" ,139,"Politics/Opinion"}, 
  {"pp" ,140,"Personal Pages"}, 
  {"pr" ,160,"Profanity"}, 
  {"ps" ,141,"Portal Sites"}, 
  {"ra" ,142,"Remote Access"}, 
  {"rb" ,185,"Restaurants"}, 
  {"re" ,186,"Real Estate"}, 
  {"rh" ,187,"Recreation/Hobbies"}, 
  {"rl" ,143,"Religion/Ideology"}, 
  {"rs" ,144,"Resource Sharing"}, 
  {"sc" ,161,"School Cheating Information"}, 
  {"se" ,145,"Search Engines"}, 
  {"sm" ,162,"Sexual Materials"}, 
  {"sp" ,146,"Sports"}, 
  {"st" ,147,"Streaming Media"}, 
  {"su" ,171,"Spam URLs"}, 
  {"sw" ,148,"Shareware/Freeware"}, 
  {"sx" ,149,"Pornography"}, 
  {"sy" ,150,"Spyware/Adware"}, 
  {"tb" ,151,"Tobacco"}, 
  {"tf" ,165,"Technical/Business Forums"}, 
  {"tg" ,163,"Gruesome Content"}, 
  {"ti" ,191,"Technical Information"}, 
  {"to" ,603,"Text/Spoken Only"}, 
  {"tr" ,152,"Travel"}, 
  {"u0" ,501,"User Defined Category 0"}, 
  {"u1" ,502,"User Defined Category 1"}, 
  {"u2" ,503,"User Defined Category 2"}, 
  {"u3" ,504,"User Defined Category 3"}, 
  {"u4" ,505,"User Defined Category 4"}, 
  {"u5" ,506,"User Defined Category 5"}, 
  {"u6" ,507,"User Defined Category 6"}, 
  {"u7" ,508,"User Defined Category 7"}, 
  {"u8" ,509,"User Defined Category 8"}, 
  {"u9" ,510,"User Defined Category 9"}, 
  {"vi" ,153,"Violence"}, 
  {"vs" ,164,"Visual Search Engine"}, 
  {"wa" ,154,"Web Ads"}, 
  {"we" ,155,"Weapons"}, 
  {"wm" ,156,"Web Mail"}, 
  {"wp" ,157,"Web Phone"}, 
} ;


extern const char * error_string (int error)     ;
int ts_category_find_int_code (char *in_code);
extern int parse_and_add_custom_site(char *buf) ;


/*
 *  Search for the integer code corresponding
 *  to a 2 letter alphabetical code for a category.
 */
int
ts_category_find_int_code (char *in_code)
{
     int i = 0;
     int cat_count = sizeof(ts_category_list)/sizeof(ts_category_info_t) ;

     
     for (i = 0; i < cat_count; i ++) {
         if(strcmp(ts_category_list[i].code, in_code) == 0) {
             DBG_LOG(MSG, MOD_UCFLT,"Internal category number:%d, code:%s, name:%s\n", 
                        ts_category_list[i].int_code,
                        ts_category_list[i].code,
                        ts_category_list[i].name) ;
             return ts_category_list[i].int_code ;
         }
     }

     return -1;
}


static char * mark_end_of_line(char * p);
static char * mark_end_of_line(char * p)
{
        while (1) {
                if (*p == '\r' || *p == '\n' || *p == '\0') {
                        *p = 0;
                        return p;
                }
                p++;
        }

        return NULL;
}


int ts_read_custom_cat_file(char * custom_sites_file);
int ts_read_custom_cat_file(char * custom_sites_file)
{
        FILE *fp = NULL;
        char *p = NULL;
        char buf[2048];
        int len, i;

        // Read the configuration
        fp = fopen(custom_sites_file, "r");
        DBG_LOG(MSG, MOD_UCFLT,"opening custom category list from file <%s>\n", 
                                custom_sites_file);
        if (!fp) {
                DBG_LOG(SEVERE, MOD_UCFLT,
                        "ERROR: failed to open custom category file <%s>\n", 
                        custom_sites_file);
                return 1;
        }

        while (!feof(fp)) {
                if (fgets(buf, sizeof(buf), fp) == NULL) break;

                p = buf;
                if (*p == '#') continue;
                mark_end_of_line(p);
                parse_and_add_custom_site(buf) ;

        }

        return 0 ;
}

