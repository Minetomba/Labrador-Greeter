// SPDX-FileCopyrightText: 2026 Minetomba <minetomba@proton.me>
// SPDX-License-Identifier: GPL-3.0-only
#define _GNU_SOURCE

#include "../include/auth.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static void print_banner(void)
{
	sleep(1);
	printf(
		"\n\x1b[36mArch Linux\x1b[0m "
		"\x1b[32m(tty1)\x1b[0m\n\n"
	);
}

/*
 * Set the basic login environment.
 *
 * PAM environment is imported separately by auth.h.
 */
static int setup_environment(const struct passwd *pw)
{
	if (setenv("USER", pw->pw_name, 1) != 0)
		return -1;

	if (setenv("LOGNAME", pw->pw_name, 1) != 0)
		return -1;

	if (setenv("HOME", pw->pw_dir, 1) != 0)
		return -1;

	if (setenv("SHELL", pw->pw_shell, 1) != 0)
		return -1;

	if (setenv(
			"PATH",
			"/usr/local/sbin:/usr/local/bin:/usr/bin",
			1
		) != 0)
		return -1;

	return 0;
}

/*
 * Become the authenticated user.
 *
 * This function is called ONLY in the child process.
 */
static int drop_privileges(const struct passwd *pw)
{
	/*
	 * We must initialize supplementary groups while still root.
	 */
	if (initgroups(pw->pw_name, pw->pw_gid) != 0) {
		perror("initgroups");
		return -1;
	}

	/*
	 * Drop supplementary/root group identity.
	 */
	if (setresgid(
			pw->pw_gid,
			pw->pw_gid,
			pw->pw_gid
		) != 0) {
		perror("setresgid");
		return -1;
	}

	/*
	 * Drop all root user IDs.
	 */
	if (setresuid(
			pw->pw_uid,
			pw->pw_uid,
			pw->pw_uid
		) != 0) {
		perror("setresuid");
		return -1;
	}

	/*
	 * Sanity check.
	 */
	if (geteuid() != pw->pw_uid ||
		getuid()  != pw->pw_uid) {
		fprintf(stderr, "Failed to drop privileges\n");
		return -1;
	}

	return 0;
}

/*
 * Start the user's shell as a login shell.
 */
static void launch_shell(const struct passwd *pw)
{
	const char *shell = pw->pw_shell;

	if (shell == NULL || shell[0] == '\0')
		shell = "/bin/sh";

	/*
	 * The leading '-' in argv[0] tells the shell that this is
	 * a login shell.
	 *
	 * For /bin/bash this becomes:
	 *
	 *	 -bash
	 */
	const char *slash = strrchr(shell, '/');
	const char *name = slash ? slash + 1 : shell;

	char argv0[PATH_MAX];

	snprintf(
		argv0,
		sizeof(argv0),
		"-%s",
		name
	);

	char *const argv[] = {
		argv0,
		NULL
	};

	execv(shell, argv);

	perror("execv");
	_exit(127);
}

/*
 * Wait for the user's login shell.
 *
 * Returns the shell's exit status.
 */
static int wait_for_shell(pid_t pid)
{
	int status;

	while (1) {
		pid_t result = waitpid(pid, &status, 0);

		if (result >= 0)
			break;

		if (errno == EINTR)
			continue;

		perror("waitpid");
		return 1;
	}

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);

	return 1;
}

/*
 * Read a username from the terminal.
 */
static int read_username(
	char *username,
	size_t size
)
{
	printf("Username: ");
	fflush(stdout);

	if (fgets(username, size, stdin) == NULL) {
		if (feof(stdin))
			return -1;

		clearerr(stdin);
		return 1;
	}

	username[strcspn(username, "\n")] = '\0';

	if (username[0] == '\0')
		return 1;

	return 0;
}

int main(void)
{
	char username[256];

	/*
	 * labrador-greeter should be running as root.
	 *
	 * The systemd service, rather than a setuid bit, provides
	 * the privileged execution context.
	 */
	if (geteuid() != 0) {
		fprintf(
			stderr,
			"labrador-greeter must run as root\n"
		);
		return 1;
	}

	/*
	 * We expect to be attached to a terminal.
	 */
	if (!isatty(STDIN_FILENO)) {
		fprintf(
			stderr,
			"labrador-greeter: stdin is not a terminal\n"
		);
		return 1;
	}

	print_banner();

	while (1) {
		pam_handle_t *pamh = NULL;

		int ret = read_username(
			username,
			sizeof(username)
		);

		if (ret < 0)
			break;

		if (ret > 0)
			continue;

		/*
		 * Determine the current TTY.
		 *
		 * ttyname() returns something such as:
		 *
		 *	 /dev/tty1
		 */
		char *tty = ttyname(STDIN_FILENO);

		if (tty == NULL) {
			perror("ttyname");
			continue;
		}

		/*
		 * Authenticate and open the PAM session while still root.
		 */
		ret = authenticate_user(
			&pamh,
			username,
			tty
		);

		if (ret != PAM_SUCCESS) {
			printf("Login incorrect\n\n");
			continue;
		}

		/*
		 * Get account information.
		 */
		struct passwd *pw = getpwnam(username);

		if (pw == NULL) {
			fprintf(
				stderr,
				"User not found\n"
			);

			close_pam_session(pamh);
			continue;
		}

		/*
		 * Copy passwd information because getpwnam() uses static
		 * storage.
		 */
		struct passwd user_pw;

		char pw_name[256];
		char pw_dir[PATH_MAX];
		char pw_shell[PATH_MAX];

		snprintf(
			pw_name,
			sizeof(pw_name),
			"%s",
			pw->pw_name
		);

		snprintf(
			pw_dir,
			sizeof(pw_dir),
			"%s",
			pw->pw_dir
		);

		snprintf(
			pw_shell,
			sizeof(pw_shell),
			"%s",
			pw->pw_shell
		);

		user_pw.pw_name = pw_name;
		user_pw.pw_uid  = pw->pw_uid;
		user_pw.pw_gid  = pw->pw_gid;
		user_pw.pw_dir  = pw_dir;
		user_pw.pw_shell = pw_shell;

		/*
		 * Establish the basic login environment before fork().
		 *
		 * The PAM environment has already been imported by
		 * authenticate_user().
		 */
		if (setup_environment(&user_pw) != 0) {
			perror("setup_environment");
			close_pam_session(pamh);
			continue;
		}

		/*
		 * Change to the user's home directory while we are
		 * still privileged.
		 */
		if (chdir(user_pw.pw_dir) != 0) {
			perror("chdir");
			close_pam_session(pamh);
			continue;
		}

		/*
		 * Restrict newly-created files.
		 */
		umask(0077);

		/*
		 * Fork the login shell.
		 *
		 * Parent:
		 *	 stays root
		 *	 owns the PAM handle
		 *	 waits for the shell
		 *
		 * Child:
		 *	 drops privileges
		 *	 becomes the authenticated user
		 *	 execs the login shell
		 */
		pid_t pid = fork();

		if (pid < 0) {
			perror("fork");
			close_pam_session(pamh);
			continue;
		}

		if (pid == 0) {
			/*
			 * Child.
			 *
			 * The parent remains responsible for PAM session
			 * cleanup.
			 */
			if (drop_privileges(&user_pw) != 0)
				_exit(1);

			launch_shell(&user_pw);

			_exit(127);
		}

		/*
		 * Parent.
		 *
		 * Wait until the user's login shell exits.
		 */
		wait_for_shell(pid);

		/*
		 * Now that the user's entire login session has ended,
		 * properly close PAM.
		 */
		close_pam_session(pamh);

		printf("\n");

		/*
		 * Display the login prompt again.
		 */
	}

	return 0;
}