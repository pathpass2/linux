// SPDX-License-Identifier: GPL-2.0
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <linux/compiler.h>
#include <linux/string.h>
#include <errno.h>
#include <sys/wait.h>
#include "subcmd-util.h"
#include "run-command.h"
#include "exec-cmd.h"

#define STRERR_BUFSIZE 128

static inline void close_pair(int fd[2])
{
	close(fd[0]);
	close(fd[1]);
}

static inline void dup_devnull(int to)
{
	int fd = open("/dev/null", O_RDWR);
	dup2(fd, to);
	close(fd);
}

int start_command(struct child_process *cmd)
{
	int need_in, need_out, need_err;
	int fdin[2], fdout[2], fderr[2];
	char sbuf[STRERR_BUFSIZE];

	/*
	 * In case of errors we must keep the promise to close FDs
	 * that have been passed in via ->in and ->out.
	 */

	need_in = !cmd->no_stdin && cmd->in < 0;
	if (need_in) {
		if (pipe(fdin) < 0) {
			if (cmd->out > 0)
				close(cmd->out);
			return -ERR_RUN_COMMAND_PIPE;
		}
		cmd->in = fdin[1];
	}

	need_out = !cmd->no_stdout
		&& !cmd->stdout_to_stderr
		&& cmd->out < 0;
	if (need_out) {
		if (pipe(fdout) < 0) {
			if (need_in)
				close_pair(fdin);
			else if (cmd->in)
				close(cmd->in);
			return -ERR_RUN_COMMAND_PIPE;
		}
		cmd->out = fdout[0];
	}

	need_err = !cmd->no_stderr && cmd->err < 0;
	if (need_err) {
		if (pipe(fderr) < 0) {
			if (need_in)
				close_pair(fdin);
			else if (cmd->in)
				close(cmd->in);
			if (need_out)
				close_pair(fdout);
			else if (cmd->out)
				close(cmd->out);
			return -ERR_RUN_COMMAND_PIPE;
		}
		cmd->err = fderr[0];
	}

	fflush(NULL);
	cmd->pid = fork();
	if (!cmd->pid) {
		if (cmd->no_stdin)
			dup_devnull(0);
		else if (need_in) {
			dup2(fdin[0], 0);
			close_pair(fdin);
		} else if (cmd->in) {
			dup2(cmd->in, 0);
			close(cmd->in);
		}

		if (cmd->no_stderr)
			dup_devnull(2);
		else if (need_err) {
			dup2(fderr[1], 2);
			close_pair(fderr);
		}

		if (cmd->no_stdout)
			dup_devnull(1);
		else if (cmd->stdout_to_stderr)
			dup2(2, 1);
		else if (need_out) {
			dup2(fdout[1], 1);
			close_pair(fdout);
		} else if (cmd->out > 1) {
			dup2(cmd->out, 1);
			close(cmd->out);
		}

		if (cmd->dir && chdir(cmd->dir))
			die("exec %s: cd to %s failed (%s)", cmd->argv[0],
			    cmd->dir, str_error_r(errno, sbuf, sizeof(sbuf)));
		if (cmd->env) {
			for (; *cmd->env; cmd->env++) {
				if (strchr(*cmd->env, '='))
					putenv((char*)*cmd->env);
				else
					unsetenv(*cmd->env);
			}
		}
		if (cmd->preexec_cb)
			cmd->preexec_cb();
		if (cmd->no_exec_cmd)
			exit(cmd->no_exec_cmd(cmd));
		if (cmd->exec_cmd) {
			execv_cmd(cmd->argv);
		} else {
			execvp(cmd->argv[0], (char *const*) cmd->argv);
		}
		exit(127);
	}

	if (cmd->pid < 0) {
		int err = errno;
		if (need_in)
			close_pair(fdin);
		else if (cmd->in)
			close(cmd->in);
		if (need_out)
			close_pair(fdout);
		else if (cmd->out)
			close(cmd->out);
		if (need_err)
			close_pair(fderr);
		return err == ENOENT ?
			-ERR_RUN_COMMAND_EXEC :
			-ERR_RUN_COMMAND_FORK;
	}

	if (need_in)
		close(fdin[0]);
	else if (cmd->in)
		close(cmd->in);

	if (need_out)
		close(fdout[1]);
	else if (cmd->out)
		close(cmd->out);

	if (need_err)
		close(fderr[1]);

	return 0;
}

static int wait_or_whine(struct child_process *cmd, bool block)
{
	bool finished = cmd->finished;
	int result = cmd->finish_result;

	while (!finished) {
		int status, code;
		pid_t waiting = waitpid(cmd->pid, &status, block ? 0 : WNOHANG);

		if (!block && waiting == 0)
			break;

		if (waiting < 0 && errno == EINTR)
			continue;

		finished = true;
		if (waiting < 0) {
			char sbuf[STRERR_BUFSIZE];

			fprintf(stderr, " Error: waitpid failed (%s)",
				str_error_r(errno, sbuf, sizeof(sbuf)));
			result = -ERR_RUN_COMMAND_WAITPID;
		} else if (waiting != cmd->pid) {
			result = -ERR_RUN_COMMAND_WAITPID_WRONG_PID;
		} else if (WIFSIGNALED(status)) {
			result = -ERR_RUN_COMMAND_WAITPID_SIGNAL;
		} else if (!WIFEXITED(status)) {
			result = -ERR_RUN_COMMAND_WAITPID_NOEXIT;
		} else {
			code = WEXITSTATUS(status);
			switch (code) {
			case 127:
				result = -ERR_RUN_COMMAND_EXEC;
				break;
			case 0:
				result = 0;
				break;
			default:
				result = -code;
				break;
			}
		}
	}
	if (finished) {
		cmd->finished = 1;
		cmd->finish_result = result;
	}
	return result;
}

/*
 * Conservative estimate of number of characaters needed to hold an a decoded
 * integer, assume each 3 bits needs a character byte and plus a possible sign
 * character.
 */
#ifndef is_signed_type
#define is_signed_type(type) (((type)(-1)) < (type)1)
#endif
#define MAX_STRLEN_TYPE(type) (sizeof(type) * 8 / 3 + (is_signed_type(type) ? 1 : 0))

int check_if_command_finished(struct child_process *cmd)
{
#ifdef __linux__
	char filename[6 + MAX_STRLEN_TYPE(typeof(cmd->pid)) + 7 + 1];
	char status_line[256];
	FILE *status_file;

	/*
	 * Check by reading /proc/<pid>/status as calling waitpid causes
	 * stdout/stderr to be closed and data lost.
	 */
	sprintf(filename, "/proc/%u/status", cmd->pid);
	status_file = fopen(filename, "r");
	if (status_file == NULL) {
		/* Open failed assume finish_command was called. */
		return true;
	}
	while (fgets(status_line, sizeof(status_line), status_file) != NULL) {
		char *p;

		if (strncmp(status_line, "State:", 6))
			continue;

		fclose(status_file);
		p = status_line + 6;
		while (isspace(*p))
			p++;
		return *p == 'Z' ? 1 : 0;
	}
	/* Read failed assume finish_command was called. */
	fclose(status_file);
	return 1;
#else
	wait_or_whine(cmd, /*block=*/false);
	return cmd->finished;
#endif
}

int finish_command(struct child_process *cmd)
{
	return wait_or_whine(cmd, /*block=*/true);
}

int run_command(struct child_process *cmd)
{
	int code = start_command(cmd);
	if (code)
		return code;
	return finish_command(cmd);
}

static void prepare_run_command_v_opt(struct child_process *cmd,
				      const char **argv,
				      int opt)
{
	memset(cmd, 0, sizeof(*cmd));
	cmd->argv = argv;
	cmd->no_stdin = opt & RUN_COMMAND_NO_STDIN ? 1 : 0;
	cmd->exec_cmd = opt & RUN_EXEC_CMD ? 1 : 0;
	cmd->stdout_to_stderr = opt & RUN_COMMAND_STDOUT_TO_STDERR ? 1 : 0;
}

int run_command_v_opt(const char **argv, int opt)
{
	struct child_process cmd;
	prepare_run_command_v_opt(&cmd, argv, opt);
	return run_command(&cmd);
}
