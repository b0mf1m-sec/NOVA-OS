#!/bin/bash
# Atualiza o pendrive com a versão ATUAL (já commitada) do NOVA OS.
#   tools/atualizar-pendrive.sh [pasta-do-pendrive]      (padrão: /media/$USER/Ventoy)
# Grava NOVA-OS.tar.gz (com o número da versão), instalar-nova.sh e LEIA-ME-PRIMEIRO.txt.
# Não mexe no resto do pendrive (ISOs do Ventoy etc.).
set -e
AQUI=$(cd "$(dirname "$0")/.." && pwd); D="${1:-/media/$USER/Ventoy}"
mountpoint -q "$D" || { echo "Pendrive não encontrado em $D (plugue-o ou passe a pasta como argumento)."; exit 1; }
cd "$AQUI"
[ -z "$(git status --porcelain)" ] || { echo "Há alterações sem commit. Faça o commit antes (o pendrive leva só o que está commitado)."; exit 1; }
git fetch -q origin 2>/dev/null || true
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main 2>/dev/null)" ] || echo "Aviso: o GitHub não está no mesmo commit deste computador. Dê 'git push' para o pendrive e a internet ficarem iguais."
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
mkdir "$T/NOVA-OS"; git archive HEAD | tar -x -C "$T/NOVA-OS"
printf '%s\n%s\n' "$(git rev-parse HEAD)" "$(git log -1 --format=%cs)" > "$T/NOVA-OS/VERSAO"
tar czf "$D/NOVA-OS.tar.gz.novo" -C "$T" NOVA-OS && mv -f "$D/NOVA-OS.tar.gz.novo" "$D/NOVA-OS.tar.gz"
cp pendrive/instalar-nova.sh "$D/instalar-nova.sh"; cp pendrive/LEIA-ME-PRIMEIRO.txt "$D/LEIA-ME-PRIMEIRO.txt"
rm -rf "$D/NOVA-OS-pasta"       # formato antigo (pasta solta), se ainda existir
sync
echo "Pendrive atualizado com a versão $(git rev-parse --short HEAD) ($(git log -1 --format=%s | cut -c1-70))."
