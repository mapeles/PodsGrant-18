#include <dispatch/dispatch.h>
#include <errno.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xpc/xpc.h>

#define PGS_HELPER_SERVICE "com.lns.pogr.respring"
#define PGS_PREFERENCES_PATH "/Applications/Preferences.app/Preferences"
#define PGS_JBCTL_PATH "/var/jb/basebin/jbctl"
#define PGS_KILLALL_PATH "/var/jb/usr/bin/killall"

extern char **environ;
extern int proc_pidpath(int pid, void *buffer, uint32_t buffersize);
extern xpc_connection_t PGSXPCConnectionCreateMachService(const char *name, dispatch_queue_t queue, uint64_t flags)
	__asm("_xpc_connection_create_mach_service");
extern pid_t PGSXPCConnectionGetPid(xpc_connection_t connection)
	__asm("_xpc_connection_get_pid");

static bool PGSClientIsPreferences(xpc_connection_t peer) {
	if(xpc_connection_get_euid(peer)!=501)
		return false;
	pid_t pid=PGSXPCConnectionGetPid(peer);
	if(pid<=0)
		return false;
	char path[4096]={0};
	if(proc_pidpath(pid, path, sizeof(path))<=0)
		return false;
	return strcmp(path, PGS_PREFERENCES_PATH)==0;
}

static int PGSSpawn(const char *path, char *const argv[], bool wait_for_exit) {
	pid_t pid=0;
	int result=posix_spawn(&pid, path, NULL, NULL, argv, environ);
	if(result!=0 || !wait_for_exit)
		return result;
	int status=0;
	if(waitpid(pid, &status, 0)<0)
		return errno;
	return WIFEXITED(status) && WEXITSTATUS(status)==0 ? 0 : EIO;
}

static void PGSSendReply(xpc_object_t event, bool success, int error_code) {
	xpc_object_t reply=xpc_dictionary_create_reply(event);
	if(!reply)
		return;
	xpc_dictionary_set_bool(reply, "success", success);
	xpc_dictionary_set_int64(reply, "error", error_code);
	xpc_connection_t remote=xpc_dictionary_get_remote_connection(event);
	if(remote)
		xpc_connection_send_message(remote, reply);
}

static void PGSHandleMessage(xpc_connection_t peer, xpc_object_t event) {
	if(xpc_get_type(event)!=XPC_TYPE_DICTIONARY)
		return;
	if(!PGSClientIsPreferences(peer)) {
		PGSSendReply(event, false, EPERM);
		return;
	}
	const char *command=xpc_dictionary_get_string(event, "command");
	if(!command) {
		PGSSendReply(event, false, EINVAL);
		return;
	}
	int result=EINVAL;
	if(strcmp(command, "respring")==0) {
		char *const argv[]={(char *)PGS_JBCTL_PATH, "respring", NULL};
		result=PGSSpawn(PGS_JBCTL_PATH, argv, false);
	}else if(strcmp(command, "reload-daemons")==0) {
		char *const argv[]={(char *)PGS_KILLALL_PATH, "-TERM", "bluetoothd", "sharingd", "SharingViewService", NULL};
		result=PGSSpawn(PGS_KILLALL_PATH, argv, true);
	}
	PGSSendReply(event, result==0, result);
}

static void PGSHandlePeer(xpc_connection_t peer) {
	xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
		PGSHandleMessage(peer, event);
	});
	xpc_connection_resume(peer);
}

int main(void) {
	xpc_connection_t listener=PGSXPCConnectionCreateMachService(PGS_HELPER_SERVICE,
		dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_LISTENER);
	if(!listener)
		return 1;
	xpc_connection_set_event_handler(listener, ^(xpc_object_t peer) {
		if(xpc_get_type(peer)==XPC_TYPE_CONNECTION)
			PGSHandlePeer(peer);
	});
	xpc_connection_resume(listener);
	dispatch_main();
}
