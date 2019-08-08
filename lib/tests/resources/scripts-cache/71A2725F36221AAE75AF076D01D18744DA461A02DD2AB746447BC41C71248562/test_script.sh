#!/usr/bin/env bash

if [ -z $1 ]; then
  echo 'TESTING'
elif [ "$1" = "FAIL" ]; then
  echo 'FAIL TESTING' >&2
  exit 5
else
  str=""
  for VAR in "$@"
  do
    str="$str | $VAR"
  done
  echo $str
fi
