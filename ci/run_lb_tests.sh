#!/usr/bin/env bash

# Standalone helper to run only load-balancing policy tests (e.g., maglev)
# using the same clang/libc++ toolchain configuration as the `dev` target,
# without invoking ci/do_ci.sh. Can be used directly or via ci/run_envoy_docker.sh.

set -euo pipefail

# Ensure SRCDIR/ENVOY_SRCDIR as in do_ci.sh
export SRCDIR="${SRCDIR:-$PWD}"
export ENVOY_SRCDIR="${ENVOY_SRCDIR:-$PWD}"

CURRENT_SCRIPT_DIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

# shellcheck source=ci/build_setup.sh
. "${CURRENT_SCRIPT_DIR}"/build_setup.sh

# Determine build arch directory (mirrors do_ci.sh logic)
if [[ "${ENVOY_BUILD_ARCH}" == "x86_64" ]]; then
  BUILD_ARCH_DIR="/linux/amd64"
elif [[ "${ENVOY_BUILD_ARCH}" == "aarch64" ]]; then
  BUILD_ARCH_DIR="/linux/arm64"
else
  BUILD_ARCH_DIR="/linux/${ENVOY_BUILD_ARCH}"
fi

# Replicate the clang toolchain setup used by do_ci.sh
setup_clang_toolchain() {
  if [[ -n "${CLANG_TOOLCHAIN_SETUP:-}" ]]; then
    return
  fi
  CONFIG_PARTS=()
  if [[ -n "${ENVOY_RBE:-}" ]]; then
    CONFIG_PARTS+=("remote")
  fi
  if [[ "${ENVOY_BUILD_ARCH}" == "aarch64" ]]; then
    CONFIG_PARTS+=("arm64")
  fi
  CONFIG_PARTS+=("clang")
  CONFIG="$(IFS=- ; echo "${CONFIG_PARTS[*]}")"
  BAZEL_BUILD_OPTIONS+=("--config=${CONFIG}")
  BAZEL_BUILD_OPTION_LIST="${BAZEL_BUILD_OPTIONS[*]}"
  export BAZEL_BUILD_OPTION_LIST
  export CLANG_TOOLCHAIN_SETUP=1
  echo "clang toolchain configured: ${CONFIG}"
}

usage() {
  cat <<'EOF'
Usage:
  ci/run_lb_tests.sh <policy|bazel-target>

Examples:
  ci/run_lb_tests.sh maglev
  ci/run_lb_tests.sh ring_hash
  ci/run_lb_tests.sh //test/extensions/load_balancing_policies/maglev:maglev_lb_test

Notes:
  - Runs with -c fastbuild and the same clang/libc++ toolchain as `dev`.
  - Intended to be run inside the build container, or via:
      ./ci/run_envoy_docker.sh './ci/run_lb_tests.sh maglev'
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

arg="$1"

# Map short policy names to their test directories
declare -A POLICY_MAP=(
  [maglev]="//test/extensions/load_balancing_policies/maglev/..."
  [ring_hash]="//test/extensions/load_balancing_policies/ring_hash/..."
)

if [[ "$arg" == //* ]]; then
  TEST_TARGET="$arg"
else
  if [[ -n "${POLICY_MAP[$arg]:-}" ]]; then
    TEST_TARGET="${POLICY_MAP[$arg]}"
  else
    echo "Unknown policy '$arg'"
    usage
    exit 1
  fi
fi

echo "Running LB tests for target: ${TEST_TARGET}"

cd "${SRCDIR}"

setup_clang_toolchain

# Use fastbuild as in `dev` target
bazel test "${BAZEL_BUILD_OPTIONS[@]}" -c fastbuild -- "${TEST_TARGET}"


