//
//  main.c
//  parse-cot
//
//  Created by david on 4/7/17.
//  Copyright © 2017 combobulated. All rights reserved.
//

// BUGS: 'code' can be alphanumeric, see how it's defined and expand parser

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#define URL "http://www.cftc.gov/dea/futures/nat_gas_sf.htm"
#define NG_0 "Disaggregated Commitments"
#define NG_D "----------"
#define DOT_INT INT_MIN
#define DOT_FLOAT NAN

#define FAIL(x) { fprintf(stderr,"error: " x "\n"); exit(1); }

char gLine[1024];
const char *my_readline(FILE *fd, int times)
{
    while( times-- ) {
        char *res = fgets(gLine,1024,fd);
        if ( res == NULL ) FAIL("my_readline");
    }
    return gLine;
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

typedef struct ng_p_c_or_n {
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
} ng_p_c_or_n;

typedef struct ng_poic {
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
} ng_poic;

typedef struct ng_node {
    ng_p_c_or_n p;
    ng_p_c_or_n c;
    ng_poic poic;
    ng_p_c_or_n n;
} ng_node;

int parse_ng(FILE *fd, ng_node **outNodes)
{
    const char *line;
    int nodesSize = 52, nNodes = 0; // as of 4/2017
    int *nums,nNums;
    float *floats;
    ng_node *nodes = calloc(nodesSize,sizeof(ng_node));
    for (int i = 0; ; i++ ) {
        line = my_readline(fd,1);
        int prefLen = strlen(NG_0);
        short inset = ( i == 0 ) ? 1 : 0; // xxx
        if ( strncmp( NG_0, line + inset, prefLen ) != 0 )
            break;
        line = my_readline(fd, 7);
        printf("parsing '%s'",line);
        
        line = my_readline(fd, 1);
        nNums = get_ints(line, &nums, 0);
        if ( nNums != 2 ) FAIL("NG Code");
        printf("\tcftc id: %d, open interest: %d\n",nums[0],nums[1]);
        
        line = my_readline(fd, 2);
        nNums = get_ints(line, &nums, 0);
        if ( nNums != 11 ) FAIL("NG Positions");
        printf("positions: ");
        for(int j = 0; j < nNums; j++) {
            printf("%d%s",nums[j],(j == nNums - 1)?"\n":" ");
            ((int *)&nodes[nNodes].p)[j] = nums[j];
        }
        free(nums);
        
        line = my_readline(fd, 3);
        nNums = get_ints(line, &nums, 1);
        if ( nNums != 11 ) FAIL("NG Changes");
        printf("changes: ");
        for(int j = 0; j < nNums; j++) {
            printf("%d%s",nums[j],(j == nNums - 1)?"\n":" ");
            ((int *)&nodes[nNodes].c)[j] = nums[j];
        }
        free(nums);
        
        line = my_readline(fd, 3);
        nNums = get_floats(line, &floats, 1);
        if ( nNums != 11 ) FAIL("NG Perc");
        printf("perc: ");
        for(int j = 0; j < nNums; j++) {
            printf("%0.2f%s",floats[j],(j == nNums - 1)?"\n":" ");
            ((float *)&nodes[nNodes].poic)[j] = floats[j];
        }
        free(floats);
        
        line = my_readline(fd, 3);
        nNums = get_ints(line, &nums, 1);
        if ( nNums != 11 ) FAIL("NG Traders");
        printf("traders: ");
        for(int j = 0; j < nNums; j++) {
            printf("%d%s",nums[j],(j == nNums - 1)?"\n":" ");
            ((int *)&nodes[nNodes].n)[j] = nums[j];
        }
        free(nums);
        
        line = my_readline(fd, 1);
        prefLen = strlen(NG_D);
        if ( strncmp(line,NG_D,prefLen) != 0 ) FAIL("NG footer");
        
        nNodes++;
    }
    
    *outNodes = nodes;
    
    return nNodes;
}

int main(int argc, const char * argv[]) {
    // insert code here...
    
    if ( argc < 3 ) {
        fprintf(stderr,"usage: parse-cot <file> <n|c|g>\n");
        return 1;
    }
    
    const char *file = argv[1];
    const char *com = argv[2];
    
    FILE *fd = fopen(file, "r");
    if ( fd == NULL ) {
        fprintf(stderr,"fopen(%s) failed: %s\n",file,strerror(errno));
        return 1;
    }
    
    if ( strcmp(com,"n") == 0 ) {
        ng_node *nodes;
        int nNodes = parse_ng(fd,&nodes);
        printf("parsed %d ng nodes\n",nNodes);
    }
    
    fclose(fd);
    
    return 0;
}
