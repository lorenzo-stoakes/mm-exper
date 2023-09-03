#!/bin/bash
set -e; set -o pipefail

gcc -Wall -Werror posix_shmem_client.c -o posix_shmem_client
gcc -Wall -Werror posix_shmem_server.c -o posix_shmem_server

./posix_shmem_server &
./posix_shmem_client
