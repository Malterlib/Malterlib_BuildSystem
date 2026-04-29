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
# chance to install the linked worktree's own hook payload. The destination
# worktree id can also collide with a previous, removed worktree and leave
# stale payload under malterlib/worktrees/<id>/. For that initial checkout,
# ignore the destination payload and fall back to the source/parent worktree
# payload, then the main worktree payload. Once mib has managed a linked
# worktree it creates that root directory even if no hook payload exists, so
# outside the initial checkout a missing hook type below an existing root is
# authoritative and must not fall back.
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
ZERO_REF="0000000000000000000000000000000000000000"
NEW_WORKTREE_CHECKOUT=false

if [ "$HOOK_TYPE" = "post-checkout" ] && [ "${1:-}" = "$ZERO_REF" ] && [ "${3:-}" = "1" ]; then
	NEW_WORKTREE_CHECKOUT=true
fi

GetProcessParentPid()
{
	local Pid="$1"

	ps -o ppid= -p "$Pid" 2>/dev/null | tr -d '[:space:]'
}

GetProcessCwd()
{
	local Pid="$1"
	local Line

	if [ -L "/proc/$Pid/cwd" ]; then
		readlink "/proc/$Pid/cwd" 2>/dev/null && return 0
	fi

	while IFS= read -r Line; do
		case "$Line" in
			n*)
				echo "${Line#n}"
				return 0
				;;
		esac
	done < <(lsof -a -p "$Pid" -d cwd -Fn 2>/dev/null)
}

GetGitPath()
{
	local Worktree="$1"
	local Command="$2"
	local GitPath

	GitPath="$(
		(
			unset GIT_DIR GIT_WORK_TREE GIT_PREFIX GIT_COMMON_DIR GIT_INDEX_FILE
			git -C "$Worktree" rev-parse "$Command" 2>/dev/null
		) || true
	)"
	[ -n "$GitPath" ] || return 0

	case "$GitPath" in
		/*)
			;;
		*)
			GitPath="$Worktree/$GitPath"
			;;
	esac

	(cd "$GitPath" 2>/dev/null && pwd) || true
}

GetWorktreeRootForPath()
{
	local Path="$1"

	(unset GIT_DIR GIT_WORK_TREE GIT_PREFIX GIT_COMMON_DIR GIT_INDEX_FILE; git -C "$Path" rev-parse --show-toplevel 2>/dev/null) || true
}

GetHookDirForWorktree()
{
	local Worktree="$1"
	local WorktreeCommon
	local WorktreeGitDir
	local WorktreeId

	[ -n "$Worktree" ] || return 0

	WorktreeCommon="$(GetGitPath "$Worktree" --git-common-dir)"
	[ "$WorktreeCommon" = "$GIT_COMMON" ] || return 0

	WorktreeGitDir="$(GetGitPath "$Worktree" --git-dir)"
	[ -n "$WorktreeGitDir" ] || return 0

	if [ "$WorktreeGitDir" = "$GIT_COMMON" ]; then
		echo "$MAIN_HOOK_DIR"
	else
		WorktreeId="$(basename "$WorktreeGitDir")"
		echo "$HOOKS_DIR/malterlib/worktrees/$WorktreeId/$HOOK_TYPE"
	fi
}

FindParentHookDir()
{
	local DestinationWorktree="$1"
	local Pid="$PPID"
	local ParentPid
	local Cwd
	local Worktree
	local HookDir

	while [ -n "$Pid" ] && [ "$Pid" != "0" ] && [ "$Pid" != "1" ]; do
		Cwd="$(GetProcessCwd "$Pid" || true)"
		if [ -n "$Cwd" ]; then
			Worktree="$(GetWorktreeRootForPath "$Cwd" || true)"
			if [ -n "$Worktree" ] && [ "$Worktree" != "$DestinationWorktree" ]; then
				HookDir="$(GetHookDirForWorktree "$Worktree" || true)"
				if [ -n "$HookDir" ]; then
					echo "$HookDir"
					return 0
				fi
			fi
		fi

		ParentPid="$(GetProcessParentPid "$Pid" || true)"
		if [ -z "$ParentPid" ] || [ "$ParentPid" = "$Pid" ]; then
			break
		fi

		Pid="$ParentPid"
	done
}

if [ "$GIT_DIR_RESOLVED" = "$GIT_COMMON" ]; then
	WORKTREE_HOOK_DIR="$MAIN_HOOK_DIR"
else
	WORKTREE_ID="$(basename "$GIT_DIR_RESOLVED")"
	WORKTREE_ROOT="$HOOKS_DIR/malterlib/worktrees/$WORKTREE_ID"
	WORKTREE_HOOK_DIR="$WORKTREE_ROOT/$HOOK_TYPE"
	if [ "$NEW_WORKTREE_CHECKOUT" = true ]; then
		DESTINATION_WORKTREE="$(git rev-parse --show-toplevel 2>/dev/null || true)"
		PARENT_HOOK_DIR="$(FindParentHookDir "$DESTINATION_WORKTREE" || true)"
		if [ -n "$PARENT_HOOK_DIR" ] && [ -d "$PARENT_HOOK_DIR" ]; then
			WORKTREE_HOOK_DIR="$PARENT_HOOK_DIR"
			if [ "$WORKTREE_HOOK_DIR" = "$MAIN_HOOK_DIR" ]; then
				export MalterlibHookMainWorktreeFallback=true
			else
				export MalterlibHookParentWorktreeFallback=true
			fi
		else
			WORKTREE_HOOK_DIR="$MAIN_HOOK_DIR"
			export MalterlibHookMainWorktreeFallback=true
		fi
	elif [ ! -d "$WORKTREE_ROOT" ]; then
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
