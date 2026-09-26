#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/jonas-safety-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude test/host/safety_test.cpp -o "$work/safety"
"$work/safety"
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude -Itest/host/stubs test/host/integration_test.cpp -o "$work/integration"
"$work/integration"
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude -Itest/host/stubs test/host/round2_test.cpp -o "$work/round2"
"$work/round2"
"$work/round2" boot-loss
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude -Itest/host/stubs test/host/priority_test.cpp -o "$work/priority"
"$work/priority"
"$work/priority" init-fail
