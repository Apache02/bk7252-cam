#!/usr/bin/env sh

PROJECT_DIR=$(pwd)
BUILD_DIR=$(mktemp -d)
BOARD=A9_B_V1_3
TESTS="test_probe--iram test_random--iram test_security--iram test_gdma--iram test_i2c1_scan--iram test_uart_tx--iram"
TARGETS="blink hello shell freertos_example freertos_shell freertos_wifi camera ram_loader bootloader bootloader_installer--iram $TESTS"


cd $BUILD_DIR || exit 1
if [ $(pwd) = "$PROJECT_DIR" ]; then
  echo "Not in build directory"
  exit 1
fi

cmake -D BOARD=$BOARD $PROJECT_DIR 1>/dev/null

exit_status=0

for target in $TARGETS ; do
  error_output=$(cmake --build . --target $target 2>&1 >/dev/null)
  build_status=$?

  if [ $build_status -ne 0 ]; then
    echo "Build $target \033[31mFailed\033[0m"
    exit_status=1
    echo "----------------"
    echo "$error_output"
    echo "----------------"
  else
    echo "Build $target \033[32mOK\033[0m"
  fi
done

echo "Cleaning up..."
cd $PROJECT_DIR
rm -rf $BUILD_DIR

exit $exit_status
