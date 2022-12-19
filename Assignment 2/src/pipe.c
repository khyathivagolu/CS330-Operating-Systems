#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>

// Per process info for the pipe.
struct pipe_info_per_process {

    // TODO:: Add members as per your need...
    int pid;
    int valid;
    int readopen;
    int writeopen;

};

// Global information for the pipe.
struct pipe_info_global {

    char *pipe_buff;    // Pipe buffer: DO NOT MODIFY THIS.
	int R;
	int W;
	int ref_count;
	int datasize;		// Number of bytes in the pipe with data
	
    // TODO:: Add members as per your need...

};

// Pipe information structure.
// NOTE: DO NOT MODIFY THIS STRUCTURE.
struct pipe_info {

    struct pipe_info_per_process pipe_per_proc [MAX_PIPE_PROC];
    struct pipe_info_global pipe_global;

};


// Function to allocate space for the pipe and initialize its members.
struct pipe_info* alloc_pipe_info () {
	
    // Allocate space for pipe structure and pipe buffer.
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);

    // Assign pipe buffer.
    pipe->pipe_global.pipe_buff = buffer;
    
	pipe->pipe_per_proc[0].valid = 1;
	pipe->pipe_per_proc[0].readopen = 1;
	pipe->pipe_per_proc[0].writeopen = 1;
	
	for (int i=1; i<MAX_PIPE_PROC; i++)
		pipe->pipe_per_proc[i].valid = 0;
    
    pipe->pipe_global.R = 0;
	pipe->pipe_global.W = 0;
	pipe->pipe_global.ref_count = 1;
	pipe->pipe_global.datasize = 0;

    /**
     *  TODO:: Initializing pipe fields
     *  
     *  Initialize per process fields for this pipe.
     *  Initialize global fields for this pipe.
     *
     */
    // Return the pipe.
    return pipe;

}

// Function to free pipe buffer and pipe info object.
// NOTE: DO NOT MODIFY THIS FUNCTION.
void free_pipe (struct file *filep) {

    os_page_free(OS_DS_REG, filep->pipe->pipe_global.pipe_buff);
    os_page_free(OS_DS_REG, filep->pipe);

}

// Fork handler for the pipe.
int do_pipe_fork (struct exec_context *child, struct file *filep) {

    /**
     *  TODO:: Implementation for fork handler
     *
     *  You may need to update some per process or global info for the pipe.
     *  This handler will be called twice since pipe has 2 file objects.
     *  Also consider the limit on no of processes a pipe can have.
     *  Return 0 on success.
     *  Incase of any error return -EOTHERS.
     *
     */
     
    int pid = child->pid;
    int ppid = child->ppid;
    struct pipe_info *pipe = filep->pipe;
    int flag=0;
    int ropen, wopen;
    
    for(int i=0; i<MAX_PIPE_PROC; i++){
    	if(pipe->pipe_per_proc[i].valid == 1 && pipe->pipe_per_proc[i].pid == ppid){
    		ropen = pipe->pipe_per_proc[i].readopen;
    		wopen = pipe->pipe_per_proc[i].writeopen;
    	} 	
    }
    
    for(int i=0; i<MAX_PIPE_PROC; i++){
    	if(pipe->pipe_per_proc[i].valid == 1 && pipe->pipe_per_proc[i].pid == pid){
    		flag = 1;
    		if(filep->mode == O_READ){
    			pipe->pipe_per_proc[i].readopen = ropen;
    		}
    		else if(filep->mode == O_WRITE){
    			pipe->pipe_per_proc[i].writeopen = wopen;
    		}
    	} 	
    }
    int flag2 = 0;
    if(!flag){
    	pipe->pipe_global.ref_count += 1;
    	for(int i=0; i<MAX_PIPE_PROC; i++){
			if(pipe->pipe_per_proc[i].valid == 0){
				flag2 = 1;
				pipe->pipe_per_proc[i].valid = 1;
				pipe->pipe_per_proc[i].pid = pid;
				if(filep->mode == O_READ){
					pipe->pipe_per_proc[i].readopen = ropen;
				}
				else if(filep->mode == O_WRITE){
					pipe->pipe_per_proc[i].writeopen = wopen;
				}	
			}	
		}
		if(!flag2) return -EOTHERS;
    }
	
    // Return successfully.
    return 0;

}

// Function to close the pipe ends and free the pipe when necessary.
long pipe_close (struct file *filep) {

    /**
     *  TODO:: Implementation of Pipe Close
     *
     *  Close the read or write end of the pipe depending upon the file
     *      object's mode.
     *  You may need to update some per process or global info for the pipe.
     *  Use free_pipe() function to free pipe buffer and pipe object,
     *      whenever applicable.
     *  After successful close, it return 0.
     *  Incase of any error return -EOTHERS.
     *
     */
	
	struct pipe_info *pipe = filep->pipe;
    struct exec_context *current = get_current_ctx();
    int pid = current->pid;
    int findex=-1;
    for(int i=0; i<MAX_PIPE_PROC; i++){
    	if(pipe->pipe_per_proc[i].valid == 1 && pipe->pipe_per_proc[i].pid == pid){
    		findex = i;
    	} 	
    }
    if(findex==-1) return -EOTHERS;
    
    if(filep->mode == O_READ){
    	pipe->pipe_per_proc[findex].readopen = 0;
    }
    else if(filep->mode == O_WRITE){
    	pipe->pipe_per_proc[findex].writeopen = 0;
    }
    
    if(pipe->pipe_per_proc[findex].readopen == 0 && pipe->pipe_per_proc[findex].writeopen == 0){
    	pipe->pipe_per_proc[findex].valid = 0;
    	pipe->pipe_global.ref_count -= 1;
    }
    
    if(pipe->pipe_global.ref_count == 0) free_pipe(filep);
    
    int ret_value;

    // Close the file and return.
    ret_value = file_close (filep);         // DO NOT MODIFY THIS LINE.

    // And return.
    return ret_value;

}

// Check whether passed buffer is valid memory location for read or write.
int is_valid_mem_range (unsigned long buff, u32 count, int access_bit) {

    /**
     *  TODO:: Implementation for buffer memory range checking
     *
     *  Check whether passed memory range is suitable for read or write.
     *  If access_bit == 1, then it is asking to check read permission.
     *  If access_bit == 2, then it is asking to check write permission.
     *  If range is valid then return 1.
     *  Incase range is not valid or have some permission issue return -EBADMEM.
     *
     */
    int ret_value = -EBADMEM;

    struct exec_context *current = get_current_ctx();
    struct mm_segment *mms = current->mms; 
    struct vm_area *vma = current->vm_area;
    
    while(vma != NULL){
        if((buff >= vma->vm_start) && (buff+count <= vma->vm_end) && (vma->access_flags & access_bit == access_bit)){
        	ret_value = 1;
        }
        vma = vma->vm_next;
    }
    
    for(int i=0; i<MAX_MM_SEGS; i++){
    	if((buff >= mms[i].start) && (buff+(unsigned long)count <= mms[i].end) && (mms[i].access_flags & access_bit == access_bit)){
    		ret_value = 1;
    	} 
    }
    // Return the finding.
    return ret_value;

}

// Function to read given no of bytes from the pipe.
int pipe_read (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of Pipe Read
     *
     *  Read the data from pipe buffer and write to the provided buffer.
     *  If count is greater than the present data size in the pipe then just read
     *       that much data.
     *  Validate file object's access right.
     *  On successful read, return no of bytes read.
     *  Incase of Error return valid error code.
     *       -EACCES: In case access is not valid.
     *       -EINVAL: If read end is already closed.
     *       -EOTHERS: For any other errors.
     *
     */
	
	if(is_valid_mem_range((unsigned long)buff, count, 1) == -EBADMEM) return -EACCES;
	
    int bytes_read = 0;
    
    struct pipe_info *pipe = filep->pipe;
    struct exec_context *current = get_current_ctx();
    int pid = current->pid;
    int findex=-1;
        
    for(int i=0; i<MAX_PIPE_PROC; i++){
    	if(pipe->pipe_per_proc[i].valid == 1 && pipe->pipe_per_proc[i].pid == pid){
    		findex = i;
    		break;
    	} 	
    }
    if(findex==-1) return -EACCES;
    if(pipe->pipe_per_proc[findex].readopen == 0) return -EINVAL;
    	
	int R = pipe->pipe_global.R;
	int W = pipe->pipe_global.W;
	int datasize = pipe->pipe_global.datasize;
	char* pipe_buff = pipe->pipe_global.pipe_buff;
	
	//if(datasize == MAX_PIPE_SIZE) return -EINVAL;
	if (R<=W && datasize==0){
		int s = W - R;
		if(count<=s){
			memcpy(buff, pipe_buff+R, count);
			R+=count;
			bytes_read = count;
		}
		else{
			memcpy(buff, pipe_buff+R, s);
			R=W;
			bytes_read = s;
		}
	}
	else{					
		int s1 = MAX_PIPE_SIZE - R;
		int s2 = W;
		if(count<=s1){
			memcpy(buff, pipe_buff+R, count);
			R+=count;
			bytes_read = count;
		}
		else{
			memcpy(buff, pipe_buff+R, s1);
			if(count-s1<=s2){
				memcpy(buff+s1, pipe_buff, count-s1);
				R=count-s1;
				bytes_read = count;
			}
			else{
				memcpy(buff+s1, pipe_buff, s2);
				R=s2;
				bytes_read = s1+s2;
			}
		}	
	}
	datasize -= bytes_read;
	pipe->pipe_global.R = R;
	pipe->pipe_global.datasize = datasize;
    // Return no of bytes read.
    return bytes_read;

}

// Function to write given no of bytes to the pipe.
int pipe_write (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of Pipe Write
     *
     *  Write the data from the provided buffer to the pipe buffer.
     *  If count is greater than available space in the pipe then just write data
     *       that fits in that space.
     *  Validate file object's access right.
     *  On successful write, return no of written bytes.
     *  Incase of Error return valid error code.
     *       -EACCES: In case access is not valid.
     *       -EINVAL: If write end is already closed.
     *       -EOTHERS: For any other errors.
     *
     */

	if(is_valid_mem_range((unsigned long)buff, count, 2) == -EBADMEM) return -EACCES;
    int bytes_written = 0;
    
    struct pipe_info *pipe = filep->pipe;
    struct exec_context *current = get_current_ctx();
    int pid = current->pid;
    int findex=-1;
        
    for(int i=0; i<MAX_PIPE_PROC; i++){
    	if(pipe->pipe_per_proc[i].valid == 1 && pipe->pipe_per_proc[i].pid == pid){
    		findex = i;
    		break;
    	} 	
    }
    if(findex==-1) return -EACCES;
    if(pipe->pipe_per_proc[findex].writeopen == 0) return -EINVAL;
    	
	int R = pipe->pipe_global.R;
	int W = pipe->pipe_global.W;
	int datasize = pipe->pipe_global.datasize;
	char* pipe_buff = pipe->pipe_global.pipe_buff;

	if (R<=W && datasize==0){
		int s1 = MAX_PIPE_SIZE - W;
		int s2 = R;
		if(count<=s1){
			memcpy(pipe_buff+W, buff, count);
			W += count;
			bytes_written = count;
		}
		else{
			memcpy(pipe_buff+W, buff, s1);
			if(count-s1<=s2){
				memcpy(pipe_buff, buff+s1, count-s1);
				W=count-s1;
				bytes_written = count;
			}
			else{
				memcpy(pipe_buff, buff+s1, s2);
				W=s2;
				bytes_written = s1+s2;
			}
		}

	}
	else{					
		int s = R - W;
		if(count<=s){
			memcpy(pipe_buff+W, buff, count);
			W+=count;
			bytes_written = count;
		}
		else{
			memcpy(pipe_buff+W, buff, s);
			W+=s;
			bytes_written = s;
		}	
	}
	datasize += bytes_written;
	pipe->pipe_global.W = W;
	pipe->pipe_global.datasize = datasize;
    // Return no of bytes written.
    return bytes_written;
	
}

// Function to create pipe.
int create_pipe (struct exec_context *current, int *fd) {

    /**
     *  TODO:: Implementation of Pipe Create
     *
     *  Find two free file descriptors.
     *  Create two file objects for both ends by invoking the alloc_file() function. 
     *  Create pipe_info object by invoking the alloc_pipe_info() function and
     *       fill per process and global info fields.
     *  Fill the fields for those file objects like type, fops, etc.
     *  Fill the valid file descriptor in *fd param.
     *  On success, return 0.
     *  Incase of Error return valid Error code.
     *       -ENOMEM: If memory is not enough.
     *       -EOTHERS: Some other errors.
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
     struct pipe_info *pipe = alloc_pipe_info();
     
     pipe->pipe_per_proc[0].pid = current->pid;
     
     fo1->type = PIPE;
     fo1->mode = O_READ;
     fo1->pipe = pipe;
     fo1->ref_count = 1;
     fo1->fops->read = pipe_read;
     fo1->fops->close = pipe_close;
     
     fo2->type = PIPE;
     fo2->mode = O_WRITE;
     fo2->pipe = pipe;
     fo2->ref_count = 1;
     fo2->fops->write = pipe_write;
     fo2->fops->close = pipe_close;
     
     current->files[fd1] = fo1;
     current->files[fd2] = fo2;
     fd[0] = fd1;
     fd[1] = fd2; 

    // Simple return.
    return 0;

}
