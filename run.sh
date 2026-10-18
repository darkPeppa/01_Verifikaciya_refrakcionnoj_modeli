#!/usr/bin/env bash
# Linux / macOS launcher. Needs only Docker. Usage:
#   ./run.sh            full calculation (validation + stage-2 data + poster), ~30-40 min
#   ./run.sh quick      short check of the whole chain, ~3-5 min
#   ./run.sh validate | stage2 | poster | unit
#   ./run.sh calc --config config/runs/objective1_full_paraxial.json --set source.energy_keV=30
#   ./run.sh shell      interactive shell inside the container
# Results are copied to ./output
set -euo pipefail
cd "$(dirname "$0")"
IMAGE=refraction-task01:1.0
if ! docker info >/dev/null 2>&1; then
  echo "ОШИБКА: Docker не запущен или не установлен. Запустите Docker Desktop / службу docker и повторите." >&2
  exit 1
fi
echo ">>> Сборка образа $IMAGE (первый раз 10-40 минут: скачивается ~2 ГБ — Geant4 и данные; дальше из кэша)"
docker build -t "$IMAGE" .
if [ "${1:-}" = "shell" ]; then
  exec docker run --rm -it "$IMAGE" shell
fi
NAME="refraction_task01_$$"
set +e
docker run --name "$NAME" "$IMAGE" "$@"
RC=$?
set -e
mkdir -p output
docker cp "$NAME:/work/out/." output/ && echo ">>> Результаты скопированы в $(pwd)/output"
docker rm "$NAME" >/dev/null
exit $RC
