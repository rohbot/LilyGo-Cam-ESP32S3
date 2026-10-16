#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
destination='roh@rohbot.cc:/home/roh/hab/'

cd -- "$script_dir"
printf 'Installing locked build dependencies...\n'
npm ci --include=dev --no-audit --no-fund
printf 'Building web monitor...\n'
npm run build

shopt -s dotglob nullglob
deploy_files=("$script_dir/build/"*)
if (( ${#deploy_files[@]} == 0 )); then
    printf 'No build files found in %s/build\n' "$script_dir" >&2
    exit 1
fi

printf 'Deploying built web monitor to %s\n' "$destination"
scp -r "${deploy_files[@]}" "$destination"
printf 'Deployment complete.\n'
