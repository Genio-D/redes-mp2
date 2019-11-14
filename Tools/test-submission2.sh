#!/bin/bash

set -euo pipefail

SUBMISSION_ZIP="${1}"

echo "Checking submission file: $SUBMISSION_ZIP"

if [ ! -f "$SUBMISSION_ZIP" ]; then
  echo FAIL
  exit
fi

echo -ne "Checking file name...\t\t"
if [ "$(basename $SUBMISSION_ZIP)" == "project2.zip" ]; then
  echo OK
else
  echo FAIL
  exit
fi

echo -ne "Checking file type...\t\t"
if [ "$(file -bi $SUBMISSION_ZIP)" == "application/zip; charset=binary" ]; then
  echo OK
else
  echo FAIL
  exit
fi

echo "Checking submission contents."

WORK_DIR="$(mktemp -d)"

function cleanup {
  echo "Cleaning up."
  rm -rf "$WORK_DIR"
  killall file-sender 2>/dev/null || true
  killall file-receiver 2>/dev/null || true
}
trap cleanup EXIT

cp "$SUBMISSION_ZIP" "$WORK_DIR"
cd "$WORK_DIR"
unzip project2.zip

echo -ne "Checking for Makefile...\t"
if [ -f "Makefile" ]; then
  echo "OK"
else
  echo "FAIL"
  exit
fi

echo "Building project."

make

echo -ne "Checking for file-sender...\t"
if [ -x "file-sender" ]; then
  echo "OK"
else
  echo "FAIL"
  exit
fi

echo -ne "Checking for file-receiver...\t"
if [ -x "file-receiver" ]; then
  echo "OK"
else
  echo "FAIL"
  exit
fi

echo "Running project."

./file-receiver received-file.txt 1234 1 &
RECEIVER_PID=$!
sleep .1

echo "Test" > sent-file.txt
./file-sender sent-file.txt localhost 1234 1

wait $RECEIVER_PID || true

echo -ne "Checking received file...\t"
if diff sent-file.txt received-file.txt
then
  echo OK
else
  echo FAIL
  exit
fi

echo "All basic checks passed."
