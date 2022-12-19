#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include<unistd.h>
#include<signal.h>
#include<fcntl.h>
#include<assert.h>
#include<wait.h>
#include <dirent.h>
#include <sys/stat.h>

int main(int argc, char** argv)
{
	
	// Creation of tar file
	if(!strcmp(argv[1],"-c")){					
		
		if(chdir(argv[2]) < 0){
			write(2, "Failed to complete creation operation\n", 39); // error in changing directory
		}
	
		DIR *dir = opendir(".");
		if (dir == NULL) {
    	    perror("Failed to complete creation operation\n");  //error in opening current directory
    	    exit(1);
    	}
    	int fd = open(argv[3], O_RDWR | O_CREAT, 0644);
    	
    	if(fd < 0){
			write(2, "Failed to complete creation operation\n", 39);
			exit(1);
		}
    	
    	struct dirent *dp;
    	
    	while(dp = readdir(dir)){
    	
			if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..") || !strcmp(dp->d_name, argv[3])) 
				continue;
				
			//printf("cont");
			char str[20]; 
			strcpy(str, dp->d_name);
			
			int fds = open(str, O_RDONLY); // for each folder
			
			if(fds < 0){
				perror("Failed to complete creation operation\n");
				exit(1);
			}
			
			write(fd, str, strlen(str));
			write(fd," ", 1);
			
			struct stat record;
    		fstat(fds, &record);
    		
    		long int rsize = record.st_size;

			char str2[16];
			
			sprintf(str2, "%ld", rsize);
			
			write(fd, str2, strlen(str2));
			write(fd, "\n", 1);
			
			if( fds < 0 ){
				write(2, "Failed to complete creation operation\n", 39);
				exit(1);
			}
			
			char c;
			while(read(fds, &c, 1) == 1){
				write(fd, &c, 1);
			}
			
			write(fd, "\n", 1);
			close(fds);
		}
	close(fd);
	closedir(dir);
	}
	
	// Extraction of all archived files from a tar file
	if(!strcmp(argv[1],"-d")){					
		
		int fdt = open(argv[2], O_RDONLY);   // points to tar file
		if(fdt < 0){
			perror("Failed to complete extraction operation\n");
			exit(1);
		}
		//printf("check");
    	char tardumppath[strlen(argv[2])+1];
    	memset(tardumppath, '\0', strlen(argv[2])+1);
		strncpy(tardumppath, argv[2], strlen(argv[2])-4);
		strcat(tardumppath, "Dump\0");
		//printf("%s", tardumppath);
		
		if(mkdir(tardumppath, 0777) < 0){
			perror("Failed to complete extraction operation\n");
			exit(1);
		}
		
		DIR* dir = opendir(".");
		if (dir == NULL) {
    	    perror("Failed to complete extraction operation\n");  //error in opening current directory
    	    exit(1);
    	}
		if(chdir(tardumppath) < 0){
			perror("Failed to complete extraction operation\n"); // error in changing directory
			exit(1);
		}
		//printf("noo");
	  	char name[20];
	  	char size[20];
	  	char c;
		read(fdt, &c, 1);
		
		while(c != ' ' && c != '\n'){
			//printf("noo");
			int i = 0;
			while(c != ' '){			// reading file name
				name[i] = c;
				i++;
				read(fdt, &c, 1);
			}
			
			name[i] = '\0';
			
			//printf("%s",name);
			int j = 0;
			
			read(fdt, &c, 1);
			while(c != '\n'){			// reading file size
				size[j] = c;
				j++;
				read(fdt, &c, 1);
			}
			size[j] = '\0';
			
			int fd = open(name, O_RDWR | O_CREAT, 0644);     //points to file created in dump folder
			if(fd < 0){
				perror("Failed to complete extraction operation\n");
  		 	    exit(1);
			}
			//printf("check");
			int filesize = atoi(size);
			char* datastr[filesize];
			
			//datastr[0] = '\0';
			read(fdt, datastr, filesize);
			write(fd, datastr, filesize);
			//datastr[filesize] = '\0';
			
			close(fd);
			read(fdt, &c, 1);  // \n
			read(fdt, &c, 1);  // first char of next file 
		}
		
	  	close(fdt);
		closedir(dir);
		
	}
	
	// Extraction of single file from a tar file
	if(!strcmp(argv[1],"-e")){					
		
		char reqfile[20];
		strcpy(reqfile, argv[3]);
		//printf("check");
		int fdt = open(argv[2], O_RDONLY);   // points to tar file
		if(fdt < 0){
			perror("Failed to complete extraction operation\n");
			exit(1);
		}
		//printf("check");
    	char tardumppath[strlen(argv[2])+1];
    	strncpy(tardumppath, argv[2], strlen(argv[2])+1);
    	
    	int l = strlen(argv[2]);
		while(tardumppath[l] != '/'){
			tardumppath[l] = '\0';
			l--;
		}
		strcat(tardumppath, "IndividualDump");
		//printf("%s", tardumppath);
		
		if(mkdir(tardumppath, 0777) < 0){
			perror("Failed to complete extraction operation\n");
			exit(1);
		}
		
		DIR* dir = opendir(".");
		if (dir == NULL) {
    	    perror("Failed to complete extraction operation\n");  //error in opening current directory
    	    exit(1);
    	}
		if(chdir(tardumppath) < 0){
			perror("Failed to complete extraction operation\n"); // error in changing directory
			exit(1);
		}
		int flag=0;
		//printf("noo");
	  	char name[20];
	  	char size[20];
	  	char c;
		read(fdt, &c, 1);
		
		while(c != ' ' && c != '\n'){
			//printf("noo");
			int i = 0;
			while(c != ' '){			// reading file name
				name[i] = c;
				i++;
				read(fdt, &c, 1);
			}
			
			name[i] = '\0';
			
			if(!strcmp(name, reqfile)){
            	flag=1;
        	}
			
			//printf("%s",name);
			int j = 0;
			
			read(fdt, &c, 1);
			while(c != '\n'){			// reading file size
				size[j] = c;
				j++;
				read(fdt, &c, 1);
			}
			size[j] = '\0';
			
			if(flag){
				int fd = open(name, O_RDWR | O_CREAT, 0644);     //points to file created in dump folder
				if(fd < 0){
					perror("Failed to complete extraction operation\n");
	  		 	    exit(1);
				}
				//printf("check");
				int filesize = atoi(size);
				char* datastr[filesize];
				
				//datastr[0] = '\0';
				read(fdt, datastr, filesize);
				write(fd, datastr, filesize);
				//datastr[filesize] = '\0';
				close(fd);
				exit(1);
			}

			read(fdt, &c, 1);  // \n
			read(fdt, &c, 1);  // first char of next file 
		}
		
		if(!flag) 
			printf("No such file is present in tar file.\n");
	  	close(fdt);
		closedir(dir);
		
	}
	
	// Listing the contents of tar file
	if(!strcmp(argv[1],"-l")){					
		
		int fdt = open(argv[2], O_RDONLY);   // points to tar file
		if(fdt < 0){
			perror("Failed to complete extraction operation\n");
			exit(1);
		}
		//printf("check");
    	
	
}
