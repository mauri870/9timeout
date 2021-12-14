#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>

Channel *cwait; // channel for threadchanwait
Channel *cexecpid; // channel for the exec pid
Channel *ctimeoutpid; // channel for the timeout sleep pid
int timeout; // timeout in seconds
char cmd[4096]; // command to execute
int debug; // debug mode

#define DPRINT if(debug)fprint

static void 
rcexecl(Channel *cpid, char *cmd)
{
	procexecl(cpid, "/bin/rc", "rc", "-c", cmd, nil);
}

static void 
timeoutproc(void *)
{
	threadsetname("timeoutproc");	
	rfork(RFFDG);
	char buf[32];
	sprint(buf, "sleep %d\n", timeout);
	rcexecl(ctimeoutpid, buf);
}

static void 
execproc(void *) 
{
	threadsetname("execproc");
	rfork(RFFDG);
	rcexecl(cexecpid, cmd);
}

void
usage(void)
{
	fprint(2, "usage: timeout [duration [command ...]]\n");
	threadexitsall("usage");
}

void
kill(int pid)
{
	int fd;
	char buf[80];

	sprint(buf, "/proc/%d/notepg", pid);
	fd = open(buf, OWRITE);
	rfork(RFNOTEG);
	write(fd, "kill", 4);
	close(fd);
}

void 
threadmain(int argc, char **argv) 
{
	Waitmsg *wmsg;
	unsigned long execpid;
	unsigned long timeoutpid;
	char *exitmsg = nil;

	if (argc < 3) {
		usage();
	}

	timeout = atoi(argv[1]);
	DPRINT(2, "Timeout is %d\n", timeout);

	// build the command string
	strcpy(cmd, argv[2]);
	for (int i = 3; i < argc; i++) {
		strcat(cmd, " ");
		strcat(cmd, argv[i]);
	}
	DPRINT(2, "Cmd is \"%s\"", cmd);

	// threadwaitchan is used to detect when one of the commands 
	// finishes execution
	cwait = threadwaitchan();
	if (cwait == nil) {
		sysfatal("timeout: can't get thread waiting channel");
		threadexitsall("threadwaitchan");
	}

	DPRINT(2, "Setting up channels for exec and timeout\n");
	ctimeoutpid = chancreate(sizeof(ulong), 0);
	cexecpid = chancreate(sizeof(ulong), 0);

	// create a new proc to run the sleep command
	proccreate(timeoutproc, nil, mainstacksize);
	timeoutpid  = recvul(ctimeoutpid);
	chanfree(ctimeoutpid);
	DPRINT(2, "Timeout pid is %uld\n", timeoutpid);


	// create a new proc to run the specified command
	proccreate(execproc, nil, mainstacksize);
	execpid  = recvul(cexecpid);
	chanfree(cexecpid);
	DPRINT(2, "Exec pid %uld\n", execpid);

	// waiting for one of the commands to exit
	if ((wmsg = recvp(cwait)) != nil) {
		DPRINT(2, "Got pid: %d\n", wmsg->pid);
		if (wmsg->pid == timeoutpid) {
			DPRINT(2, "Command timed out");
			kill(execpid);
			exitmsg = "timeout";
		} else if (wmsg->pid == execpid) {
			kill(timeoutpid);
		}

		free(wmsg);
	}

	chanclose(cwait);
	threadexitsall(exitmsg);
}
