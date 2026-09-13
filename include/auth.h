// SPDX-FileCopyrightText: 2026 Minetomba <minetomba@proton.me>
// SPDX-License-Identifier: GPL-3.0-only
#ifndef LOGIN_CLI_AUTH_H
#define LOGIN_CLI_AUTH_H

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * PAM conversation.
 *
 * pam_misc_conv() handles password prompts on the controlling
 * terminal for us.
 */
static int pam_conversation(
	int num_msg,
	const struct pam_message **msg,
	struct pam_response **resp,
	void *appdata_ptr
)
{
	(void)appdata_ptr;

	return misc_conv(num_msg, msg, resp, NULL);
}

static const struct pam_conv login_pam_conv = {
	pam_conversation,
	NULL
};

/*
 * Copy variables established by PAM into the real process
 * environment.
 *
 * This is particularly important for pam_systemd, which can
 * establish things such as XDG_RUNTIME_DIR through the PAM
 * environment.
 */
static int pam_import_environment(pam_handle_t *pamh)
{
	char **env = pam_getenvlist(pamh);

	if (env == NULL)
		return 0;

	for (char **p = env; *p != NULL; ++p) {
		if (putenv(*p) != 0) {
			perror("putenv");
			free(env);
			return -1;
		}

		/*
		 * Do not free *p individually.
		 *
		 * putenv() uses the supplied string directly.
		 */
	}

	free(env);
	return 0;
}

/*
 * Start and authenticate a PAM login session.
 *
 * Returns PAM_SUCCESS on success.
 */
static int authenticate_user(
	pam_handle_t **pamh,
	const char *username,
	const char *tty
)
{
	int ret;

	*pamh = NULL;

	ret = pam_start(
		"login",
		username,
		&login_pam_conv,
		pamh
	);

	if (ret != PAM_SUCCESS) {
		fprintf(
			stderr,
			"pam_start: %s\n",
			pam_strerror(*pamh, ret)
		);
		return ret;
	}

	/*
	 * Tell PAM which terminal is being used.
	 *
	 * This is important for a real login session.
	 */
	ret = pam_set_item(*pamh, PAM_TTY, tty);

	if (ret != PAM_SUCCESS) {
		fprintf(
			stderr,
			"pam_set_item(PAM_TTY): %s\n",
			pam_strerror(*pamh, ret)
		);

		pam_end(*pamh, ret);
		*pamh = NULL;
		return ret;
	}

	/*
	 * Authenticate.
	 */
	ret = pam_authenticate(*pamh, 0);

	if (ret != PAM_SUCCESS) {
		fprintf(
			stderr,
			"Authentication failed: %s\n",
			pam_strerror(*pamh, ret)
		);

		pam_end(*pamh, ret);
		*pamh = NULL;
		return ret;
	}

	/*
	 * Check whether the account is allowed to log in.
	 */
	ret = pam_acct_mgmt(*pamh, 0);

	if (ret != PAM_SUCCESS) {
		fprintf(
			stderr,
			"Account check failed: %s\n",
			pam_strerror(*pamh, ret)
		);

		pam_end(*pamh, ret);
		*pamh = NULL;
		return ret;
	}

	/*
	 * Establish PAM credentials.
	 */
	ret = pam_setcred(*pamh, PAM_ESTABLISH_CRED);

	if (ret != PAM_SUCCESS) {
		fprintf(
			stderr,
			"pam_setcred: %s\n",
			pam_strerror(*pamh, ret)
		);

		pam_end(*pamh, ret);
		*pamh = NULL;
		return ret;
	}

	/*
	 * Open the PAM session.
	 *
	 * pam_systemd is invoked here through the configured PAM
	 * stack and can establish the logind/user session.
	 */
	ret = pam_open_session(*pamh, 0);

	if (ret != PAM_SUCCESS) {
		fprintf(
			stderr,
			"pam_open_session: %s\n",
			pam_strerror(*pamh, ret)
		);

		pam_setcred(*pamh, PAM_DELETE_CRED);
		pam_end(*pamh, ret);
		*pamh = NULL;
		return ret;
	}

	/*
	 * Import PAM environment AFTER pam_open_session().
	 *
	 * This is important for pam_systemd.
	 */
	if (pam_import_environment(*pamh) != 0) {
		pam_close_session(*pamh, 0);
		pam_setcred(*pamh, PAM_DELETE_CRED);
		pam_end(*pamh, PAM_SYSTEM_ERR);
		*pamh = NULL;
		return PAM_SYSTEM_ERR;
	}

	return PAM_SUCCESS;
}

/*
 * Properly close the PAM session.
 */
static void close_pam_session(pam_handle_t *pamh)
{
	if (pamh == NULL)
		return;

	pam_close_session(pamh, 0);
	pam_setcred(pamh, PAM_DELETE_CRED);
	pam_end(pamh, PAM_SUCCESS);
}

#endif