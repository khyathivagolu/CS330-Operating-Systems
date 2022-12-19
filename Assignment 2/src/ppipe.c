#include<ppipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>


// Per process information for the ppipe.
struct ppipe_info_per_process {

    // TODO:: Add members as per your need...
	int pid;
    int valid;
    int readopen;
    int writeopen;
    int Rp;
};

// Global information for the ppipe.
struct ppipe_info_global {

    char *ppipe_buff;       // Persistent pipe buffer: DO NOT MODIFY THIS.
	int R;
	int flushR;
	int W;
	int ref_count;
	int datasize;		// Number of bytes in the pipe with data
	//int read_count;
    // TODO:: Add members as per your need...

};

// Persistent pipe structure.
// NOTE: DO NOT MODIFY THIS STRUCTURE.
struct ppipe_info {

    struct ppipe_info_per_process ppipe_per_proc [MAX_PPIPE_PROC];
    struct ppipe_info_global ppipe_global;

};


// Function to allocate space for the ppipe and initialize its members.
struct ppipe_info* alloc_ppipe_info() {

    // Allocate space for ppipe structure and ppipe buffer.
    struct ppipe_info *ppipe = (struct ppipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);

    // Assign ppipe buffer.
    ppipe->ppipe_global.ppipe_buff = buffer;

	ppipe->ppipe_per_proc[0].valid = 1;
	ppipe->ppipe_per_proc[0].readopen = 1;
	ppipe->ppipe_per_proc[0].writeopen = 1;
	ppipe->ppipe_per_proc[0].Rp = 0;
	
	for (int i=1; i<MAX_PPIPE_PROC; i++)
		ppipe->ppipe_per_proc[i].valid = 0;
    
    ppipe->ppipe_global.R = 0;
	ppipe->ppipe_global.W = 0;
	ppipe->ppipe_global.ref_count = 1;
	ppipe->ppipe_global.datasize = 0;

    /**
     *  TODO:: Initializing pipe fields
     *
     *  Initialize per process fields for this ppipe.
     *  Initialize global fields for this ppipe.
     *
     */ 

    // Return the ppipe.
    return ppipe;

}

// Function to free ppipe buffer and ppipe info object.
// NOTE: DO NOT MODIFY THIS FUNCTION.
void free_ppipe (struct file *filep) {

    os_page_free(OS_DS_REG, filep->ppipe->ppipe_global.ppipe_buff);
    os_page_free(OS_DS_REG, filep->ppipe);

} 

// Fork handler for ppipe.
int do_ppipe_fork (struct exec_context *child, struct file *filep) {
    
    /**
     *  TODO:: Implementation for fork handler
     *
     *  You may need to update some per process or global info for the ppipe.
     *  This handler will be called twice since ppipe has 2 file objects.
     *  Also consider the limit on no of processes a ppipe can have.
     *  Return 0 on success.
     *  Incase of any error return -EOTHERS.
     *
     */
	int pid = child->pid;
    int ppid = child->ppid;
    struct ppipe_info *ppipe = filep->ppipe;
    int flag=0;
    int ropen, wopen, rp;
    
    for(int i=0; i<MAX_PPIPE_PROC; i++){
    	if(ppipe->ppipe_per_proc[i].valid == 1 && ppipe->ppipe_per_proc[i].pid == ppid){
    		ropen = ppipe->ppipe_per_proc[i].readopen;
    		wopen = ppipe->ppipe_per_proc[i].writeopen;
    		rp = ppipe->ppipe_per_proc[i].Rp;
    	} 	
    }
    
    for(int i=0; i<MAX_PPIPE_PROC; i++){
    	if(ppipe->ppipe_per_proc[i].valid == 1 && ppipe->ppipe_per_proc[i].pid == pid){
    		flag = 1;
    		if(filep->mode == O_READ){
    			ppipe->ppipe_per_proc[i].readopen = ropen;
    			ppipe->ppipe_per_proc[i].Rp = rp;
    		}
    		else if(filep->mode == O_WRITE){
    			ppipe->ppipe_per_proc[i].writeopen = wopen;
    		}
    	} 	
    }
    int flag2 = 0;
    if(!flag){
    	ppipe->ppipe_global.ref_count += 1;
    	for(int i=0; i<MAX_PPIPE_PROC; i++){
			if(ppipe->ppipe_per_proc[i].valid == 0){
				flag2 = 1;
				ppipe->ppipe_per_proc[i].valid = 1;
				ppipe->ppipe_per_proc[i].pid = pid;
				if(filep->mode == O_READ){
					ppipe->ppipe_per_proc[i].readopen = ropen;
				}
				else if(filep->mode == O_WRITE){
					ppipe->ppipe_per_proc[i].writeopen = wopen;
				}	
			}	
		}
		if(!flag2) return -EOTHERS;
    }
    
    int minR = MAX_PPIPE_SIZE;
	for(int i=0; i<MAX_PPIPE_PROC; i++){
    	if(ppipe->ppipe_per_proc[i].valid == 1 && ppipe->ppipe_per_proc[i].readopen == 1){
    		if(ppipe->ppipe_per_proc[i].Rp<minR){
    			minR = ppipe->ppipe_per_proc[i].Rp;
    		}
    	} 	
    }
    ppipe->ppipe_global.flushR = minR;
    
    // Return successfully.
    return 0;

}


// Function to close the ppipe ends and free the ppipe when necessary.
long ppipe_close (struct file *filep) {

    /**
     *  TODO:: Implementation of Pipe Close
     *
     *  Close the read or write end of the ppipe depending upon the file
     *      object's mode.
     *  You may need to update some per process or global info for the ppipe.
     *  Use free_pipe() function to free ppipe buffer and ppipe object,
     *      whenever applicable.
     *  After successful close, it return 0.
     *  Incase of any error return -EOTHERS.
     *                                                                          
     */
    struct ppipe_info *ppipe = filep->ppipe;
    struct exec_context *current = get_current_ctx();
    int pid = current->pid;
    int findex=-1;
    for(int i=0; i<MAX_PPIPE_PROC; i++){
    	if(ppipe->ppipe_per_proc[i].valid == 1 && ppipe->ppipe_per_proc[i].pid == pid){
    		findex = i;
    	} 	
    }
    if(findex==-1) return -EOTHERS;
    
    if(filep->mode == O_READ){
    	ppipe->ppipe_per_proc[findex].readopen = 0;
    }
    else if(filep->mode == O_WRITE){
    	ppipe->ppipe_per_proc[findex].writeopen = 0;
    }
    
    if(ppipe->ppipe_per_proc[findex].readopen == 0 && ppipe->ppipe_per_proc[findex].writeopen == 0){
    	ppipe->ppipe_per_proc[findex].valid = 0;
    	ppipe->ppipe_global.ref_count -= 1;
    }
    
    if(ppipe->ppipe_global.ref_count == 0) free_ppipe(filep);
    
    int minR = MAX_PPIPE_SIZE;
	for(int i=0; i<MAX_PPIPE_PROC; i++){
    	if(ppipe->ppipe_per_proc[i].valid == 1 && ppipe->ppipe_per_proc[i].readopen == 1){
    		if(ppipe->ppipe_per_proc[i].Rp<minR){
    			minR = ppipe->ppipe_per_proc[i].Rp;
    		}
    	} 	
    }
    ppipe->ppipe_global.flushR = minR;

    int ret_value;

    // Close the file.
    ret_value = file_close (filep);         // DO NOT MODIFY THIS LINE.

    // And return.
    return ret_value;

}

// Function to perform flush operation on ppipe.
int do_flush_ppipe (struct file *filep) {

    /**
     *  TODO:: Implementation of Flush system call
     *
     *  Reclaim the region of the persistent pipe which has been read by 
     *      all the processes.
     *  Return no of reclaimed bytes.
     *  In case of any error return -EOTHERS.
     *
     */

    int reclaimed_bytes = 0;
    if (filep->type!=PPIPE) return -EOTHERS;
    struct ppipe_info *ppipe = filep->ppipe;
    int R = ppipe->ppipe_global.R;
    int flushR = ppipe->ppipe_global.flushR;
    reclaimed_bytes = flushR - R;
    ppipe->ppipe_global.R = flushR;
    // Return reclaimed bytes.
    return reclaimed_bytes;

}

// Read handler for the ppipe.
int ppipe_read (struct file *filep, char *buff, u32 count) {
    
    /**
     *  TODO:: Implementation of PPipe Read
     *
     *  Read the data from ppipe buffer and write to the provided buffer.
     *  If count is greater than the present data size in the ppipe then just read
     *      that much data.
     *  Validate file object's access right.
     *  On successful read, return no of bytes read.
     *  Incase of Error return valid error code.
     *      -EACCES: In case access is not valid.
     *      -EINVAL: If read end is already closed.
     *      -EOTHERS: For any other errors.
     *
     */

    int bytes_read = 0;
	struct ppipe_info *ppipe = filep->ppipe;
    struct exec_context *current = get_current_ctx();
    int pid = current->pid;
    int findex=-1;
        
    for(int i=0; i<MAX_PPIPE_PROC; i++){
    	if(ppipe->ppipe_per_proc[i].valid == 1 && ppipe->ppipe_per_proc[i].pid == pid){
    		findex = i;
    		break;
    	} 	
    }
    if(findex==-1) return -EACCES;
    if(ppipe->ppipe_per_proc[findex].writeopen == 0) return -EINVAL;
    	
	int R = ppipe->ppipe_global.R;
	int W = ppipe->ppipe_global.W;
	int datasize = ppipe->ppipe_global.datasize;
	char* ppipe_buff = ppipe->ppipe_global.ppipe_buff;
	int Rp = ppipe->ppipe_per_proc[findex].Rp;
	
	if (Rp<=W && datasize==0){
		int s = W - Rp;
		if(count<=s){
			memcpy(buff, ppipe_buff+Rp, count);
			Rp+=count;
			bytes_read = count;
		}
		else{
			memcpy(buff, ppipe_buff+Rp, s);
			Rp=W;
			bytes_read = s;
		}
	}
	else{					
		int s1 = MAX_PPIPE_SIZE - Rp;
		int s2 = W;
		if(count<=s1){
			memcpy(buff, ppipe_buff+Rp, count);
			Rp+=count;
			bytes_read = count;
		}
		else{
			memcpy(buff, ppipe_buff+Rp, s1);
			if(count-s1<=s2){
				memcpy(buff+s1, ppipe_buff, count-s1);
				Rp=count-s1;
				bytes_read = count;
			}
			else{
				memcpy(buff+s1, ppipe_buff, s2);
				Rp=s2;
				bytes_read = s1+s2;
			}
		}	
	}

	ppipe->ppipe_per_proc[findex].Rp = Rp;
	int minR = MAX_PPIPE_SIZE;
	for(int i=0; i<MAX_PPIPE_PROC; i++){
    	if(ppipe->ppipe_per_proc[i].valid == 1 && ppipe->ppipe_per_proc[i].readopen == 1){
    		if(ppipe->ppipe_per_proc[i].Rp<minR){
    			minR = ppipe->ppipe_per_proc[i].Rp;
    		}
    	} 	
    }
    ppipe->ppipe_global.flushR = minR;
	
    // Return no of bytes read.
    return bytes_read;
	
}

// Write handler for ppipe.
int ppipe_write (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of PPipe Write
     *
     *  Write the data from the provided buffer to the ppipe buffer.
     *  If count is greater than available space in the ppipe then just write
     *      data that fits in that space.
     *  Validate file object's access right.
     *  On successful write, return no of written bytes.
     *  Incase of Error return valid error code.
     *      -EACCES: In case access is not valid.
     *      -EINVAL: If write end is already closed.
     *      -EOTHERS: For any other errors.
     *
     */

    int bytes_written = 0;
    
    struct ppipe_info *ppipe = filep->ppipe;
    struct exec_context *current = get_current_ctx();
    int pid = current->pid;
    int findex=-1;
        
    for(int i=0; i<MAX_PPIPE_PROC; i++){
    	if(ppipe->ppipe_per_proc[i].valid == 1 && ppipe->ppipe_per_proc[i].pid == pid){
    		findex = i;
    		break;
    	} 	
    }
    if(findex==-1) return -EACCES;
    if(ppipe->ppipe_per_proc[findex].writeopen == 0) return -EINVAL;
    	
	int R = ppipe->ppipe_global.R;
	int W = ppipe->ppipe_global.W;
	int datasize = ppipe->ppipe_global.datasize;
	char* ppipe_buff = ppipe->ppipe_global.ppipe_buff;

	if(datasize == MAX_PIPE_SIZE) return -EINVAL;
	if (R<=W && datasize==0){
		int s1 = MAX_PPIPE_SIZE - W;
		int s2 = R;
		if(count<=s1){
			memcpy(ppipe_buff+W, buff, count);
			W += count;
			bytes_written = count;
		}
		else{
			memcpy(ppipe_buff+W, buff, s1);
			if(count-s1<=s2){
				memcpy(ppipe_buff, buff+s1, count-s1);
				W=count-s1;
				bytes_written = count;
			}
			else{
				memcpy(ppipe_buff, buff+s1, s2);
				W=s2;
				bytes_written = s1+s2;
			}
		}

	}
	else{					
		int s = R - W;
		if(count<=s){
			memcpy(ppipe_buff+W, buff, count);
			W+=count;
			bytes_written = count;
		}
		else{
			memcpy(ppipe_buff+W, buff, s);
			W+=s;
			bytes_written = s;
		}	
	}
	datasize += bytes_written;
	ppipe->ppipe_global.W = W;
	ppipe->ppipe_global.datasize = datasize;

    // Return no of bytes written.
    return bytes_written;

}

// Function to create persistent pipe.
int create_persistent_pipe (struct exec_context *current, int *fd) {

    /**
     *  TODO:: Implementation of PPipe Create
     *
     *  Find two free file descriptors.
     *  Create two file objects for both ends by invoking the alloc_file() function.
     *  Create ppipe_info object by invoking the alloc_ppipe_info() function and
     *      fill per process and global info fields.
     *  Fill the fields for those file objects like type, fops, etc.
     *  Fill the valid file descriptor in *fd param.
     *  On success, return 0.
     *  Incase of Error return valid Error code.
     *      -ENOMEM: If memory is not enough.
     *      -EOTHERS: Some other errors.
     *
     */
	 int fd1, fd2;
     int i=0;
     while(current->files[i]!=NULL){
     	i++;
     }
     fd1=i;
     i++;
     while(current->files[i]!=NULL){
     	i++;
     }
     fd2=i;
     if(i>=MAX_OPEN_FILES) return -ENOMEM;
     
     struct file *fo1 = alloc_file();
     struct file *fo2 = alloc_file();
     struct ppipe_info *ppipe = alloc_ppipe_info();
     
     ppipe->ppipe_per_proc[0].pid = current->pid;
     
     fo1->type = PPIPE;
     fo1->mode = O_READ;
     fo1->ppipe = ppipe;
     fo1->ref_count = 1;
     fo1->fops->read = ppipe_read;
     fo1->fops->close = ppipe_close;
     
     fo2->type = PPIPE;
     fo2->mode = O_WRITE;
     fo2->ppipe = ppipe;
     fo2->ref_count = 1;
     fo2->fops->write = ppipe_write;
     fo2->fops->close = ppipe_close;
     
     current->files[fd1] = fo1;
     current->files[fd2] = fo2;
     fd[0] = fd1;
     fd[1] = fd2; 
     
    // Simple return.
    return 0;

}
