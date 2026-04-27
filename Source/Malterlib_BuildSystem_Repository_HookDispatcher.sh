#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Managed by Malterlib - do not edit
set -e

# Determine the worktree identity.
# Git runs hooks from $GIT_COMMON_DIR/hooks which is shared across
# worktrees. We resolve the actual git dir to figure out which
# worktree invoked us and dispatch to its hook scripts:
#   Main worktree:   malterlib/hooks/<type>/
#   Linked worktree: malterlib/worktrees/<name>/<type>/
#
# During `git worktree add`, Git runs post-checkout before mib has had a
# chance to install the linked worktree's own hook payload. In that one case,
# the linked worktree root under malterlib/worktrees/ is absent, so fall back
# to the main worktree hook payload and tell the hook payload about that
# fallback. Once mib has managed a linked worktree it creates that root
# directory even if no hook payload exists, so a missing hook type below an
# existing root is authoritative and must not fall back.
#
# We use "git rev-parse --git-dir" rather than $GIT_DIR because the
# environment variable is not consistently exported to all hooks
# across git versions (notably since Git 2.18).
#
# Note: submodules are not a concern here. Each repository (including
# git submodules) has its own hooks directory and gets its own copy of
# this dispatcher. Git only invokes hooks from the repository being
# operated on, so the resolved git dir always refers to the current
# repo — never a parent or child submodule. For submodules inside
# linked worktrees, the git dir points to the worktree-local module
# dir which has no commondir indirection, so they take the main
# branch correctly.
HOOK_TYPE="$(basename "$0")"
HOOKS_DIR="$(cd "$(dirname "$0")" && pwd)"
GIT_COMMON="$(dirname "$HOOKS_DIR")"
GIT_DIR_RESOLVED="$(cd "$(git rev-parse --git-dir)" && pwd)"
MAIN_HOOK_DIR="$HOOKS_DIR/malterlib/hooks/$HOOK_TYPE"

if [ "$GIT_DIR_RESOLVED" = "$GIT_COMMON" ]; then
	WORKTREE_HOOK_DIR="$MAIN_HOOK_DIR"
else
	WORKTREE_ID="$(basename "$GIT_DIR_RESOLVED")"
	WORKTREE_ROOT="$HOOKS_DIR/malterlib/worktrees/$WORKTREE_ID"
	WORKTREE_HOOK_DIR="$WORKTREE_ROOT/$HOOK_TYPE"
	if [ ! -d "$WORKTREE_ROOT" ]; then
		WORKTREE_HOOK_DIR="$MAIN_HOOK_DIR"
		export MalterlibHookMainWorktreeFallback=true
	fi
fi

if [ -d "$WORKTREE_HOOK_DIR" ]; then
	# Some hook types (pre-push, pre-receive, post-receive, proc-receive,
	# reference-transaction) receive protocol data on stdin. If we piped
	# git's stdin straight to each child, the first script would drain the
	# stream and later scripts would see EOF. Cache stdin once and replay
	# it for each hook so multi-script arrays work for stdin-driven hooks.
	STDIN_CACHE=""
	if [ ! -t 0 ]; then
		STDIN_CACHE="$(mktemp)"
		trap 'rm -f "$STDIN_CACHE"' EXIT
		cat > "$STDIN_CACHE"
	fi

	# Only files prefixed with a three-digit index are hooks to invoke.
	# Other files (helpers declared in Repository.HookHelperFiles) live
	# alongside in the same directory so scripts can reference them via
	# "$(dirname "$0")/<name>" but must not be executed here.
	for hook in "$WORKTREE_HOOK_DIR"/[0-9][0-9][0-9]_*; do
		if [ -x "$hook" ]; then
			if [ -n "$STDIN_CACHE" ]; then
				"$hook" "$@" < "$STDIN_CACHE"
			else
				"$hook" "$@"
			fi
		fi
	done
fi
