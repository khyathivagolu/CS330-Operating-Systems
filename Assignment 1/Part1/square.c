#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    long long int val = atoll(argv[argc-1]);
    int len=strlen(argv[argc-1])+1;

    val *= val;
    
    if(argc == 2){
    	printf("%lld", val);
        exit(0);
    }
    else{
        char str[len];
        sprintf(str, "%lld", val); 
        argv[argc-1] = str;
        str[len] = '\0';
        if(execv((argv+1)[0], (argv+1))){ 
            printf("UNABLE TO EXECUTE");
            exit(-1);
        }
    }
    return 0;
}
