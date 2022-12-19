#include <debug.h>
#include <context.h>
#include <entry.h>
#include <lib.h>
#include <memory.h>


/*****************************HELPERS******************************************/

/*
 * allocate the struct which contains information about debugger
 *
 */
struct debug_info *alloc_debug_info()
{
	struct debug_info *info = (struct debug_info *) os_alloc(sizeof(struct debug_info));
	if(info)
		bzero((char *)info, sizeof(struct debug_info));
	return info;
}
/*
 * frees a debug_info struct
 */
void free_debug_info(struct debug_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct debug_info));
}



/*
 * allocates a page to store registers structure
 */
struct registers *alloc_regs()
{
	struct registers *info = (struct registers*) os_alloc(sizeof(struct registers));
	if(info)
		bzero((char *)info, sizeof(struct registers));
	return info;
}

/*
 * frees an allocated registers struct
 */
void free_regs(struct registers *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct registers));
}

/*
 * allocate a node for breakpoint list
 * which contains information about breakpoint
 */
struct breakpoint_info *alloc_breakpoint_info()
{
	struct breakpoint_info *info = (struct breakpoint_info *)os_alloc(
		sizeof(struct breakpoint_info));
	if(info)
		bzero((char *)info, sizeof(struct breakpoint_info));
	return info;
}

/*
 * frees a node of breakpoint list
 */
void free_breakpoint_info(struct breakpoint_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct breakpoint_info));
}

/*
 * Fork handler.
 * The child context doesnt need the debug info
 * Set it to NULL
 * The child must go to sleep( ie move to WAIT state)
 * It will be made ready when the debugger calls wait
 */
void debugger_on_fork(struct exec_context *child_ctx)
{
	// printk("DEBUGGER FORK HANDLER CALLED\n");
	child_ctx->dbg = NULL;
	child_ctx->state = WAITING;
}


/******************************************************************************/
 
void save_registers(struct exec_context *cctx, struct debug_info *dbg){

	dbg->regs->entry_rip = cctx->regs.entry_rip - 1;
	dbg->regs->entry_rsp = cctx->regs.entry_rsp;
	dbg->regs->rbp = cctx->regs.rbp;
	dbg->regs->rax = cctx->regs.rax;
	dbg->regs->rdi = cctx->regs.rdi;
	dbg->regs->rsi = cctx->regs.rsi;
	dbg->regs->rdx = cctx->regs.rdx;
	dbg->regs->rcx = cctx->regs.rcx;
	dbg->regs->r8 = cctx->regs.r8;
	dbg->regs->r9 = cctx->regs.r9;
}

/* This is the int 0x3 handler
 * Hit from the childs context
 */
 
long int3_handler(struct exec_context *ctx)
{
	struct exec_context *pctx = NULL;
	
	if(ctx && !ctx->dbg)	pctx = get_ctx_by_pid((u32)ctx->ppid);
	if(!pctx || !pctx->dbg) return -1;
	
	ctx->state = WAITING;
	pctx->state = READY;
	
	save_registers(ctx, pctx->dbg);
	
	struct breakpoint_info *curr = pctx->dbg->head; 
	u64 enter_addr = (u64)ctx->regs.entry_rip - 1;
   	
   	if(enter_addr == (u64)pctx->dbg->end_handler){
		   		ctx->regs.entry_rsp-=0x8;
		   		save_registers(ctx, pctx->dbg);
				*((u64*)ctx->regs.entry_rsp) = (u64)pctx->dbg->ret_stack[pctx->dbg->stack_top]; 
				pctx->dbg->stack_top -= 1;
	}
	else{
		while(curr){ 
		   if(curr->end_breakpoint_enable == 1 && curr->addr == enter_addr) {

					pctx->dbg->stack_top += 1;
					pctx->dbg->ret_stack[pctx->dbg->stack_top] = *((u64*)ctx->regs.entry_rsp);
					pctx->dbg->fn_stack[pctx->dbg->stack_top] = curr->addr;
					*((u64*)ctx->regs.entry_rsp) = (u64)pctx->dbg->end_handler;
					break;
		   		}
		  
		   curr = curr->next;
	   	}
	}
	
	
	//push rbp 
	
	
	ctx->regs.entry_rsp-=0x8;
	*((u64*)ctx->regs.entry_rsp) = ctx->regs.rbp;
	
	pctx->regs.rax = enter_addr;
	
	
	//backtracing 
	
	int i = 0;
	u64 bt_addr = *(u64*)(ctx->regs.entry_rsp + 8);
	u64 rbp = ctx->regs.rbp;
	if(enter_addr != (u64)pctx->dbg->end_handler){
		pctx->dbg->bt_array[0] = enter_addr;
		i = 1;
	}
	while(bt_addr != END_ADDR){
			pctx->dbg->bt_array[i] = bt_addr;
			bt_addr = *((u64*)(rbp + 8));
			rbp = *(u64*)rbp;
			i++;
	}
	pctx->dbg->bt_count = i;
	
	schedule(pctx);
	
	return 0;
}

/*
 * Exit handler.
 * Deallocate the debug_info struct if its a debugger.
 * Wake up the debugger if its a child
 */
void debugger_on_exit(struct exec_context *ctx)
{
	if(ctx->dbg){
		struct breakpoint_info* curr = ctx->dbg->head;
		struct breakpoint_info* prev = NULL;
		while(curr){
			prev = curr;
			curr = curr->next;
			free_breakpoint_info(prev);
		}
		free_debug_info(ctx->dbg);	
	}
	else{
		struct exec_context* pctx = get_ctx_by_pid(ctx->ppid);
		pctx->state = READY;
		ctx->state = WAITING;
		pctx->regs.rax = CHILD_EXIT;
		//printk("child exited\n");
	}
	
}


/*
 * called from debuggers context
 * initializes debugger state
 */
int do_become_debugger(struct exec_context *ctx, void *addr)
{
	ctx->dbg = alloc_debug_info();
	
	if(!ctx || !ctx->dbg) return -1;
	
	ctx->dbg->breakpoint_count = 0;
	ctx->dbg->bp_top = 0;
	ctx->dbg->head = NULL; 
	ctx->dbg->end_handler = addr;
	ctx->dbg->stack_top = -1;
	ctx->dbg->bt_count = 0;
	ctx->dbg->regs = alloc_regs();
	
	*((u8*)addr) = INT3_OPCODE;

	return 0;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context *ctx, void *addr, int flag)
{
	
	if(!ctx || !ctx->dbg) return -1;
	struct breakpoint_info *curr = ctx->dbg->head; 
	struct breakpoint_info *prev = NULL;
	struct breakpoint_info *temp = alloc_breakpoint_info();
	if(!temp) return -1;

	temp->addr = (u64)addr;
	temp->next = NULL;
	temp->end_breakpoint_enable = flag;
	
	*((u8*)addr) = (u8)INT3_OPCODE;	
	
    if(ctx->dbg->head){  
		for(int i=0; i<ctx->dbg->breakpoint_count; i++){ 
			if(curr->addr == (u64)addr) { 

				if(curr->end_breakpoint_enable){
					for(int i=0;i<=ctx->dbg->stack_top;i++){
						if((u64)addr==ctx->dbg->fn_stack[i] && !flag){
							return -1;
						}
					}
					curr->end_breakpoint_enable = flag;
				}
				else{
					curr->end_breakpoint_enable = flag;
				}
				return 0;
			}
		   prev = curr;
		   curr = curr->next;
		}
		ctx->dbg->breakpoint_count += 1;
		ctx->dbg->bp_top +=1;
		temp->num = ctx->dbg->bp_top;
		prev->next = temp;
		if(ctx->dbg->breakpoint_count > MAX_BREAKPOINTS) return -1;
	}
	else{
		ctx->dbg->breakpoint_count=1;
		ctx->dbg->bp_top +=1;
		temp->num = ctx->dbg->bp_top;
		ctx->dbg->head = temp;
	}
	
	return 0;
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context *ctx, void *addr)
{
	struct breakpoint_info *curr = ctx->dbg->head;
	struct breakpoint_info *prev = NULL;
	for(int i=0;i<=ctx->dbg->stack_top;i++){
		if(curr->end_breakpoint_enable == 1 && (u64)addr==ctx->dbg->fn_stack[i]){
			return -1;
		}
	}
	int flag = 0;
	
	if(ctx->dbg->head){
		while(curr){ 
		   	if(curr->addr == (u64)addr) {
				if(!prev) ctx->dbg->head = curr->next;	
				else{
					prev->next = curr->next;
				}
				ctx->dbg->breakpoint_count -= 1;
				
				*((u8*)addr) = (u8)PUSHRBP_OPCODE;
			   return 0;
		   }
		   prev = curr;
		   curr = curr->next;
	   	}
	}

	return -1;
}


/*
 * called from debuggers context
 */

int do_info_breakpoints(struct exec_context *ctx, struct breakpoint *ubp)
{
	if(!ctx || !ctx->dbg || !ubp) return -1;

	struct breakpoint_info* curr = ctx->dbg->head;
	
	int i= 0;
	while(curr){
		ubp[i].num = curr->num;
		ubp[i].addr = curr->addr;
		ubp[i].end_breakpoint_enable = curr->end_breakpoint_enable;
		curr = curr->next;
		i++;
	}
	return ctx->dbg->breakpoint_count;
}


/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context *ctx, struct registers *regs)
{
	if(!ctx || !ctx->dbg) return -1; 
		
	regs->entry_rip = ctx->dbg->regs->entry_rip;
	regs->entry_rsp = ctx->dbg->regs->entry_rsp;
	regs->rbp = ctx->dbg->regs->rbp;
	regs->rax = ctx->dbg->regs->rax;
	regs->rdi = ctx->dbg->regs->rdi;
	regs->rsi = ctx->dbg->regs->rsi;
	regs->rdx = ctx->dbg->regs->rdx;
	regs->rcx = ctx->dbg->regs->rcx;
	regs->r8 = ctx->dbg->regs->r8;
	regs->r9 = ctx->dbg->regs->r9;

	return 0;
}

/*
 * Called from debuggers context
 */

int do_backtrace(struct exec_context *ctx, u64 bt_buf)
{

	if(!ctx || !ctx->dbg) return -1;
	
	int bt_count = ctx->dbg->bt_count;
	u64* bt_array = (u64*) bt_buf;
	int c = ctx->dbg->stack_top;
	
	for(int i=0; i<bt_count; i++){
		bt_array[i] = ctx->dbg->bt_array[i];
		if(bt_array[i]==(u64)ctx->dbg->end_handler){
			bt_array[i]=ctx->dbg->ret_stack[c];
			c--;
		}
	}
	
	return bt_count;
}

/*
 * When the debugger calls wait
 * it must move to WAITING state
 * and its child must move to READY state
 */

s64 do_wait_and_continue(struct exec_context *ctx)
{
	if(!ctx || !ctx->dbg) return -1; 
	
	struct exec_context *cctx = NULL;
	
	for(int i=1; i<MAX_PROCESSES; i++){		

		if(get_ctx_by_pid(i) && get_ctx_by_pid(i)->ppid == ctx->pid){
			cctx = get_ctx_by_pid(i);
			break;
		}
	}
	if(!cctx) return -1;
	ctx->state = WAITING;
	cctx->state = READY;
	
	schedule(cctx);
		
	return 0;
}



