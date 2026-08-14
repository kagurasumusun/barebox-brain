/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SHELL_H
#define SHELL_H

#include "ui.h"

/*
 * Minimal command line using the same verbs as barebox
 * (help, md, mw, reset, poweroff, detect, go, boot).
 * Returns an ACT_* if the user asked to boot / power off, else 0.
 */
int shell_run(struct boot_ctx *ctx);

#endif
