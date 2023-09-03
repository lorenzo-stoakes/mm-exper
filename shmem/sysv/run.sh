#!/bin/bash
set -e; set -o pipefail

gcc -Wall -Werror shmem_sysv_client.c -o shmem_sysv_client
gcc -Wall -Werror shmem_sysv_server.c -o shmem_sysv_server

./shmem_sysv_server &
./shmem_sysv_client
