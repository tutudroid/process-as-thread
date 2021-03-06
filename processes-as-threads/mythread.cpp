#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <assert.h>
#include <signal.h>
#include "mythread.h"
#include "atomic.h"
#include "debug.h"
#include "myrun.h"
#include "myglobals.h"

unsigned int mythread::_nestingLevel = 0;
int mythread::_tpid;

void * mythread::spawn(threadFunction *fn, void * arg, int parent_index){
	/*
	 *  Parent process didn't know his grandchildren. He only know his child. So when He create child 
	 * 	process, He can use shared memory to trace his child.
	*/
	void * buf = allocateSharedObject(4096);
	//char * buf = new char(100);
	//HL::sassert<(4096 > sizeof(ThreadStatus))> checkSize;
	ThreadStatus * t = new(buf) ThreadStatus;

	return forkSpawn(fn, t, arg, parent_index);
}
	
void mythread::join(void *threadId, void ** result){
	ThreadStatus *t = (ThreadStatus *)threadId;
	if(result != NULL)
		*result = t->retval;
	freeSharedObject(t, 4096);
	return;
}
	
int mythread::cancel(void *threadId){
	ThreadStatus * t = (ThreadStatus *) threadId;

  	int threadindex = t->threadIndex;

  	kill(t->tid, SIGKILL);

  	// Free the shared object held by this thread.
  	freeSharedObject(t, 4096);
  	return threadindex;
}
	
int mythread::thread_kill(void *threadId, int signal){
	int threadindex;
 	ThreadStatus * t = (ThreadStatus *) threadId;

  	threadindex = t->threadIndex;

  	kill(t->tid, signal);
  
  	freeSharedObject(t, 4096);
  	return threadindex;
}
	
int mythread::getThreadIndex(void *threadStatus){
	assert( threadStatus != NULL);
	ThreadStatus * t = (ThreadStatus *)threadStatus;
	return t->threadIndex;
}

	
int mythread::getThreadPid(void *threadStatus){
	assert( threadStatus != NULL);
	ThreadStatus * t = (ThreadStatus *)threadStatus;
	return t->tid;
}

void * mythread::forkSpawn(threadFunction *fn, ThreadStatus * t, void * arg, int parent_index){
	int child = syscall(SYS_clone, CLONE_FS | CLONE_FILES | SIGCHLD, (void *)0);
	
	if(child == 0){
		// child process running
		//Record current process id to threadStatus
		pid_t mypid = syscall(SYS_getpid);
    	setId(mypid);
		
		
		//Store thread statue information
		int threadindex = myrun::childRegister(mypid, parent_index);
    	t->threadIndex = threadindex;
    	t->tid = mypid;
		global_data->thread_index = threadindex;
		
		myrun::waitParentNotify();

		_nestingLevel ++;
		run_thread(fn, t, arg);		
		_nestingLevel --;
		
		exit(0);	
	}else{
		//dthread:parent need to wait child
		myrun::waitChildRegistered();
		//Mythread: I didn't need to wait child. I continue to run. If I want to wait child. I need to wait at join() function.
	}
	//void * stackTop = allocateSharedObject(1024*1024*1024);	
	//clone(fn, stackTop, CLONE_FS | CLONE_FILES | SIGCHLD, arg);
	return (void *)t;
}

void mythread::run_thread(threadFunction * fn, ThreadStatus * t, void * arg) {
  //xrun::atomicBegin(true);
  void * result = fn(arg);
  myrun::deleteChildRegister();

  t->retval = result;
}
