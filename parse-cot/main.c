//
//  main.c
//  parse-cot
//
//  Created by david on 4/7/17.
//  Copyright © 2017 combobulated. All rights reserved.
//

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <curl/curl.h>

#define NG_URL "http://www.cftc.gov/dea/futures/nat_gas_sf.htm"
#define PET_URL "http://www.cftc.gov/dea/futures/petroleum_sf.htm"
#define OTHER_URL "http://www.cftc.gov/dea/futures/other_sf.htm"
#define NODE_BEGIN "Disaggregated Commitments of Traders"
#define NODE_END "----------"
#define DOT_INT INT_MIN
#define DOT_FLOAT NAN

#define FAIL(x) { fprintf(stderr,"error: " x "\n"); exit(1); }

char gLine[1024];
char *my_readline(FILE *fd, int times)
{
    while( times-- ) {
        char *res = fgets(gLine,1024,fd);
        if ( res == NULL ) FAIL("my_readline");
    }
    return gLine;
}

char *get_code(char *line, char **code)
{
    int idx = 0;
    unsigned long len = strlen(line);
    while ( idx < len ) {
        if ( line[idx] == '#' ) {
            idx++;
            *code = line + idx;
            
            while (line[idx] != ' ' && line[idx] != '\t')
                idx++;
            
            line[idx] = '\0';
            idx++;
            break;
        }
        idx++;
    }
    
    return line + idx;
}

int get_ints(const char *line, int **numbers, int dots)
{
    int idx = 0;
    unsigned long len = strlen(line);
    int numsSize = 11;
    int nNums = 0;
    int *nums = malloc(numsSize*sizeof(int));
    while ( idx < len ) {
        if ( ( line[idx] <= '9' && line[idx] >= '0' ) || line[idx] == '-' || (dots && line[idx] == '.') ) {
            
            int aNum;
            if ( (dots && line[idx] == '.' ) ) {
                aNum = DOT_INT;
                goto next;
            }
            
            char chomped[128];
            bzero(chomped, 128);
            short negative = (line[idx] == '-');
            if ( negative ) idx++;
            if ( nNums >= numsSize ) {
                numsSize *= 2;
                nums = realloc(nums,numsSize*sizeof(int));
            }
            
            int base = idx;
            int commas = 0;
            while ( (line[idx] <= '9' && line[idx] >= '0') || line[idx] == ',' ) {
                if ( line[idx] != ',' )
                    chomped[idx - base - commas] = line[idx];
                else
                    commas++;
                idx++;
            }
            
            aNum = atoi(chomped);
            if ( negative ) aNum *= -1;
        next:
            nums[nNums] = aNum;
            nNums++;
        }
        idx++;
    }
    
    *numbers = nums;
    
    return nNums;
}

int get_floats(const char *line, float **numbers, int dots)
{
    int idx = 0;
    unsigned long len = strlen(line);
    int numsSize = 11;
    int nNums = 0;
    float *nums = malloc(numsSize*sizeof(float));
    while ( idx < len ) {
        if ( ( line[idx] <= '9' && line[idx] >= '0' ) || line[idx] == '-' || (dots && line[idx] == '.')  ) {
            
            float aNum;
            if ( (dots && line[idx] == '.') ) {
                aNum = DOT_FLOAT;
                goto next;
            }
            
            char chomped[128];
            bzero(chomped, 128);
            short negative = (line[idx] == '-');
            if ( negative ) idx++;
            if ( nNums >= numsSize ) {
                numsSize *= 2;
                nums = realloc(nums,numsSize*sizeof(float));
            }
            
            int base = idx;
            while ( (line[idx] <= '9' && line[idx] >= '0') || line[idx] == '.' ) {
                chomped[idx - base] = line[idx];
                idx++;
            }
            
            aNum = atof(chomped);
            if ( negative ) aNum *= -1;
        next:
            nums[nNums] = aNum;
            nNums++;
        }
        
        idx++;
    }
    
    *numbers = nums;
    
    return nNums;
}

typedef struct p_c_or_n {
    int pLong;
    int pShort;
    int sLong;
    int sShort;
    int sSpreading;
    int mLong;
    int mShort;
    int mSpreading;
    int oLong;
    int oShort;
    int oSpreading;
} p_c_or_n;

typedef struct poic {
    float pLong;
    float pShort;
    float sLong;
    float sShort;
    float sSpreading;
    float mLong;
    float mShort;
    float mSpreading;
    float oLong;
    float oShort;
    float oSpreading;
} poic;

typedef struct node {
    p_c_or_n p;
    p_c_or_n c;
    poic poic;
    p_c_or_n n;
} node;

int parse(FILE *fd, node **outNodes)
{
    char *line = NULL;
    
    while(1) {
        char *aLine = my_readline(fd, 1);
        char *txtBegin = strstr(aLine, NODE_BEGIN);
        if ( txtBegin ) {
            line = txtBegin;
            break;
        }
    }
    
    if ( ! line ) FAIL("couldn't find txt");
    
    int nodesSize = 52, nNodes = 0; // as of 4/2017
    int *nums,nNums;
    float *floats;
    node *nodes = calloc(nodesSize,sizeof(node));
    for (int i = 0; ; i++ ) {
        //line = my_readline(fd,1);
        int prefLen = strlen(NODE_BEGIN);
        //short inset = ( i == 0 ) ? 1 : 0; // xxx
        if ( strncmp( NODE_BEGIN, line, prefLen ) != 0 )
            break;
        line = my_readline(fd, 7);
        printf("parsing '%s'",line);
        
        line = my_readline(fd, 1);
        char *code;
        int openInterest;
        line = get_code(line, &code);
        nNums = get_ints(line, &nums, 0);
        if ( nNums != 1 ) FAIL("Code");
        openInterest = nums[0];
        printf("\tcftc id: %s, open interest: %d\n",code,openInterest);
        
        line = my_readline(fd, 2);
        nNums = get_ints(line, &nums, 0);
        if ( nNums != 11 ) FAIL("Positions");
        printf("positions: ");
        for(int j = 0; j < nNums; j++) {
            if(nums[j]==DOT_INT)
                printf(".%s",(j == nNums - 1)?"\n":" ");
            else
                printf("%d%s",nums[j],(j == nNums - 1)?"\n":" ");
            ((int *)&nodes[nNodes].p)[j] = nums[j];
        }
        free(nums);
        
        line = my_readline(fd, 3);
        nNums = get_ints(line, &nums, 1);
        if ( nNums != 11 ) FAIL("Changes");
        printf("changes: ");
        for(int j = 0; j < nNums; j++) {
            if(nums[j]==DOT_INT)
                printf(".%s",(j == nNums - 1)?"\n":" ");
            else
                printf("%d%s",nums[j],(j == nNums - 1)?"\n":" ");
            ((int *)&nodes[nNodes].c)[j] = nums[j];
        }
        free(nums);
        
        line = my_readline(fd, 3);
        nNums = get_floats(line, &floats, 1);
        if ( nNums != 11 ) FAIL("NG Perc");
        printf("perc: ");
        for(int j = 0; j < nNums; j++) {
            if(nums[j]==DOT_FLOAT)
                printf(".%s",(j == nNums - 1)?"\n":" ");
            else
                printf("%0.1f%s",floats[j],(j == nNums - 1)?"\n":" ");
            ((float *)&nodes[nNodes].poic)[j] = floats[j];
        }
        free(floats);
        
        line = my_readline(fd, 3);
        nNums = get_ints(line, &nums, 1);
        if ( nNums != 11 ) FAIL("Traders");
        printf("traders: ");
        for(int j = 0; j < nNums; j++) {
            if(nums[j]==DOT_INT)
                printf(".%s",(j == nNums - 1)?"\n":" ");
            else
                printf("%d%s",nums[j],(j == nNums - 1)?"\n":" ");
            ((int *)&nodes[nNodes].n)[j] = nums[j];
        }
        free(nums);
        
        line = my_readline(fd, 1);
        prefLen = strlen(NODE_END);
        if ( strncmp(line,NODE_END,prefLen) != 0 ) FAIL("footer");
        
        nNodes++;
        
        line = my_readline(fd, 1);
    }
    
    *outNodes = nodes;
    
    return nNodes;
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

int main(int argc, const char * argv[]) {
    
    if ( argc < 2 ) {
        fprintf(stderr,"usage: parse-cot <n|c|g>\n");
        return 1;
    }
    
    CURL *curl;
    FILE *fp;
    CURLcode res;
    char *url;
    if ( argv[1][0] == 'n' )
        url = NG_URL;
    else if ( argv[1][0] == 'c' )
        url = PET_URL;
    else if ( argv[1][0] == 'g' )
        url = OTHER_URL;
    else
        FAIL("unknown commodity type");
    char *template = strdup("/tmp/parse-cot-XXXXXXXXXXX");
    int tempFd = mkstemp(template);
    if ( tempFd == -1 ) FAIL("mkstemp");
    curl = curl_easy_init();
    if (curl) {
        fp = fdopen(tempFd,"wb");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        /* always cleanup */
        curl_easy_cleanup(curl);
        fclose(fp);
        close(tempFd);
    } else FAIL("curl");

    fp = fopen(template, "r");
    if ( fp == NULL ) {
        fprintf(stderr,"fopen(%s) failed: %s\n",template,strerror(errno));
        return 1;
    }
    
    node *nodes;
    int nNodes = parse(fp,&nodes);
    printf("parsed %d nodes\n",nNodes);
    
    fclose(fp);
    unlink(template);
    
    return 0;
}
