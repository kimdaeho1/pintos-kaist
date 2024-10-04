#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address(void *addr);
void get_argument(void *rsp, int *arg, int count);
void halt(void);
void exit(int status);
int exec(char *cmd_line);
int fork(const char * thread_name, struct intr_frame *f);
int wait(int pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *filename);
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	uint64_t *sp = f->rsp;
	printf("rsp: %p\n", sp);
	check_address(sp);
	int syscall_number = *sp;
	printf("System call number: %d\n", syscall_number);


	printf ("system call!\n");
	switch(syscall_number){
		case SYS_HALT :	
			halt(); /* Halt the operating system. */
		case SYS_EXIT : 
			exit(f->R.rdi); /* Terminate this process. */
			break;
		case SYS_FORK : /* Clone current process. */
			f->R.rax = fork(f->R.rdi, f);
			break;
		case SYS_EXEC : 
			f->R.rax = exec(f->R.rdi); /* Switch current process. */
			break;
		case SYS_WAIT : 
			f->R.rax = wait(f->R.rdi); /* Wait for a child process to die. */
			break;
		case SYS_CREATE : {
			const char *filename = (const char *)f->R.rdi;
			check_address(filename);  // 파일 이름 유효성 검사
			printf("Filename: %s\n", filename);

			unsigned initial_size = (unsigned)f->R.rsi;
			f->R.rax = create(filename, initial_size);
			bool result = create(filename, initial_size);
			printf("Create result: %d\n", result);

		}; /* Create a file. */
		case SYS_REMOVE : {
			f->R.rax = remove(f->R.rdi); /* Delete a file. */
		// case SYS_OPEN : open();  /* Open a file. */
		// case SYS_FILESIZE : 
		// 	filesize(f->R.rdi); /* Obtain a file's size. */
		// case SYS_READ : 
		// 	read(f->R.rdi); /* Read from a file. */
		// // case SYS_WRITE : write();  /* Write to a file. */
		// case SYS_SEEK : 
		// 	seek(f->R.rdi); /* Change position in a file. */
		// case SYS_TELL : tell(); /* Report current position in a file. */
		// case SYS_CLOSE : close(); /* Close a file. */
		default : {
			printf("Invaild system call number. \n");
			exit(-1);
		}
	}
	thread_exit ();
}
}

void
check_address(void *addr){

if (addr == NULL || !is_kernel_vaddr(addr)) {
	 printf("Invalid address: %p\n", addr);
    exit(-1);
}
}

void 
halt(void){
	printf("Halt called, shutting down...\n");
	power_off();
}

void
exit(int status){
	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf("%s: exit%d\n", curr->name, status);
	thread_exit(); // 정상적으로 종료되었으면 0
}

int exec(char *cmd_line){
	// cmd_line이 유효한 사용자 주소인지 확인 -> 잘못된 주소인 경우 종료/예외 발생
	check_address(cmd_line);

	// process.c 파일의 process_create_initd 함수와 유사하다.
	// 단, 스레드를 새로 생성하는 건 fork에서 수행하므로
	// exec는 이미 존재하는 프로세스의 컨텍스트를 교체하는 작업을 하므로
	// 현재 프로세스의 주소 공간을 교체하여 새로운 프로그램을 실행
	// 이 함수에서는 새 스레드를 생성하지 않고 process_exec을 호출한다.

	
	// process_exec 함수 안에서 filename을 변경해야 하므로
	// 커널 메모리 공간에 cmd_line의 복사본을 만든다.
	// (현재는 const char* 형식이기 때문에 수정할 수 없다.)
	char *cmd_line_copy;
	cmd_line_copy = palloc_get_page(0);
	if (cmd_line_copy == NULL)
		exit(-1);							  // 메모리 할당 실패 시 status -1로 종료한다.
	strlcpy(cmd_line_copy, cmd_line, PGSIZE); // cmd_line을 복사한다.


	// 스레드의 이름을 변경하지 않고 바로 실행한다.
	if (process_exec(cmd_line_copy) == -1)
		exit(-1); // 실패 시 status -1로 종료한다.
}


bool
create(const char *filename, unsigned initial_size){
	return filesys_create(filename, initial_size);
}

bool
remove(const char *filename){
	return filesys_remove(filename);
}

int fork(const char * thread_name, struct intr_frame *f)
{
	return process_fork(thread_name, f);
}

int wait(int pid)
{
	return process_wait(pid);
}