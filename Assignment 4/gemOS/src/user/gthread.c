#include <gthread.h>
#include <ulib.h>

static struct process_thread_info tinfo __attribute__((section(".user_data"))) = {};
/*XXX 
      Do not modifiy anything above this line. The global variable tinfo maintains user
      level accounting of threads. Refer gthread.h for definition of related structs.
 */
 
void exit_th_on_return(){
	u64 ret;
	asm("mov %%rax, %0" : "=r" (ret):);
	//printf("entered exit on ret\n");
	gthread_exit((void*)ret);
	//getpid();
}


int gthread_exit(void *retval);
/* Returns 0 on success and -1 on failure */
/* Here you can use helper system call "make_thread_ready" for your implementation */
int gthread_create(int *tid, void *(*fc)(void *), void *arg) {
        
	if(tinfo.num_threads > MAX_THREADS) return -1;
	
	struct thread_info *thread;
	
	for(int i=0; i<MAX_THREADS; i++){
		    thread = &tinfo.threads[i];
		    if(thread->status == TH_STATUS_UNUSED){
		    	tinfo.num_threads += 1;
		    	break;
		    }
	}
	
	void *stackp;
	stackp = mmap(NULL, TH_STACK_SIZE, PROT_READ|PROT_WRITE, 0);

	if(!stackp || stackp == MAP_ERR){
		printf("Can not allocate stack\n");
		return -1;
	}
	
	void *ret_addr;
	
	u64 temprsp = (u64)stackp + TH_STACK_SIZE - 8;
	*(u64*)temprsp = (u64)&exit_th_on_return;
	
	long pid = clone(*fc, (long)stackp + TH_STACK_SIZE -8, arg);

	*tid = tinfo.num_threads - 1;
	thread->tid = *tid;
	thread->pid = pid;
	thread->status = TH_STATUS_USED;
	thread->stack_addr = stackp + TH_STACK_SIZE -8;
	
	make_thread_ready(pid);
	thread->status = TH_STATUS_ALIVEUSED;

	return 0;
}

int gthread_exit(void *retval) {

	long pid = getpid();
	struct thread_info *thread;
	
	for(int i=0; i<MAX_THREADS; i++){
		    thread = &tinfo.threads[i];
		    if(thread->pid == (u8)pid){
		    	break;
		    }
	}

	thread->ret_addr = retval;
	thread->status = TH_STATUS_DEADUSED;

	exit(0);
}

void* gthread_join(int tid) {
        
     /* Here you can use helper system call "wait_for_thread" for your implementation */
     //wait_for_thread(pid) (lib.c). Return of a negative value implies that the thread has died or an invalid thread ID is passed in the system call argument.
	struct thread_info *thread;
	int i;
	for(i=0; i<MAX_THREADS; i++){
		thread = &tinfo.threads[i];
		if(thread->tid == (u8)tid){
			break;
		}
	}
	if(i==MAX_THREADS) return NULL;
	if(thread->status == TH_STATUS_UNUSED) return NULL;
	
	if(thread->status == TH_STATUS_ALIVEUSED){
		wait_for_thread(thread->pid);
	}
	thread->status = TH_STATUS_UNUSED;
	for(int i=0; i<MAX_GALLOC_AREAS; i++){
		if(thread->priv_areas[i].owner == thread){
			munmap((void*)thread->priv_areas[i].start, (int)thread->priv_areas[i].length);
			thread->priv_areas[i].owner=NULL;
		}
	}
	munmap((void*)((long)thread->stack_addr - TH_STACK_SIZE + 8), TH_STACK_SIZE);
	tinfo.num_threads -= 1;
 
	return thread->ret_addr;
}


/*Only threads will invoke this. No need to check if its a process
 * The allocation size is always < GALLOC_MAX and flags can be one
 * of the alloc flags (GALLOC_*) defined in gthread.h. Need to 
 * invoke mmap using the proper protection flags (for prot param to mmap)
 * and MAP_TH_PRIVATE as the flag param of mmap. The mmap call will be 
 * handled by handle_thread_private_map in the OS.
 * */

void* gmalloc(u32 size, u8 alloc_flag)
{
   
	/* You need to fill in your implementation here*/
	if(size>GALLOC_MAX) return NULL;
	int prot_alloc;
	if(alloc_flag==GALLOC_OWNONLY) prot_alloc = TP_SIBLINGS_NOACCESS;
	else if(alloc_flag==GALLOC_OTRDONLY) prot_alloc = TP_SIBLINGS_RDONLY;
	else if(alloc_flag==GALLOC_OTRDWR) prot_alloc = TP_SIBLINGS_RDWR;
	else return NULL;
	printf("enter(1)\n");
	//thread info of priv areas?
	long pid = getpid();
	printf("pidinmalloc=%d\n",pid);
	struct thread_info *thread;
	int k;
	for(k=0; k<MAX_THREADS; k++){
		    thread = &tinfo.threads[k];
		    if(thread->pid == (u8)pid){
		    	break;
		    }
	}
	if(k==MAX_THREADS) return NULL; //parent?
	int i=0;
	while(i<MAX_GALLOC_AREAS && thread->priv_areas[i].owner && thread->priv_areas[i].owner->tid == thread->tid){
		i++;
	}
	if(i==MAX_GALLOC_AREAS) return NULL;
	printf("enter(3)\n");
	void *private_map;
	private_map = mmap(NULL, (int)size, PROT_READ | PROT_WRITE | prot_alloc, MAP_TH_PRIVATE);
	printf("enter(2)\n");
	thread->priv_areas[i].owner = thread;
	thread->priv_areas[i].start = (u64)private_map;
	thread->priv_areas[i].length = size;
	thread->priv_areas[i].flags = (u32)alloc_flag; 
	
	
	return private_map;
}
/*
   Only threads will invoke this. No need to check if the caller is a process.
*/
int gfree(void *ptr)
{
	long pid = getpid();
   	struct thread_info *thread;
	int k;
	for(k=0; k<MAX_THREADS; k++){
		    thread = &tinfo.threads[k];
		    if(thread->pid == pid){
		    	break;
		    }
	}
	if(k==MAX_THREADS) return -1;
	int i;
	for(i=0; i<MAX_GALLOC_AREAS; i++){
		if(thread->priv_areas[i].owner == thread && thread->priv_areas[i].start == (u64)ptr) break;
	}
	if(i==MAX_GALLOC_AREAS) return -1;
	
	munmap((void*)thread->priv_areas[i].start, (int)thread->priv_areas[i].length);
	thread->priv_areas[i].owner=NULL;

     	return 0;
}
