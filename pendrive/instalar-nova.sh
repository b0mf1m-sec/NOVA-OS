#!/bin/bash
# Instalador do NOVA OS a partir do pendrive.  Uso (num Debian 13 recém-instalado, com internet):
#   su -c 'mkdir -p /mnt/pen && mount -o ro /dev/disk/by-label/Ventoy /mnt/pen' && bash /mnt/pen/instalar-nova.sh
# Faz tudo sozinho: libera o sudo, instala o NOVA OS (pacotes, apps, login, boot) e, se houver internet, avisa
# quando existir uma versão mais nova no GitHub e pergunta se você quer usá-la. Rode como usuário comum.
AQUI=$(cd "$(dirname "$0")" && pwd); U=$(id -un)
REPO="b0mf1m-sec/NOVA-OS"; API="https://api.github.com/repos/$REPO"
PACOTE="$AQUI/NOVA-OS.tar.gz"
cor() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }
erro() { printf '\n\033[1;31mERRO: %s\033[0m\n' "$*"; exit 1; }
pergunta() {   # pergunta "texto" padrao(S|N)  -> 0 = sim
  local r p="[S/n]"; [ "$2" = N ] && p="[s/N]"
  [ -t 0 ] || { [ "$2" = S ]; return; }
  read -r -p "$1 $p " r; case "$r" in s|S|y|Y) return 0;; n|N) return 1;; *) [ "$2" = S ];; esac
}

echo "============================================"
echo "   NOVA OS: instalação a partir do pendrive"
echo "============================================"
[ "$(id -u)" -ne 0 ] || erro "Rode como usuário comum (não como root). Entre com o seu usuário e rode de novo."
grep -q '^VERSION_ID="13"' /etc/os-release 2>/dev/null || echo "Aviso: o NOVA OS foi feito para o Debian 13; em outra versão pode não funcionar."
[ -f "$PACOTE" ] || erro "Não achei o arquivo NOVA-OS.tar.gz ao lado deste script ($AQUI)."

# 1) sudo: no Debian instalado com senha de root, o sudo não existe; libera uma vez usando a senha de root
if [ -z "$NOVA_SIMULA" ] && { ! command -v sudo >/dev/null 2>&1 || ! getent group sudo | grep -qw "$U"; }; then
  cor "Liberando o uso do administrador (sudo). Digite a senha de ROOT quando pedir:"
  su -c "apt-get update -qq && apt-get install -y sudo && usermod -aG sudo '$U'" || erro "Não foi possível liberar o sudo. Confira a senha de root."
fi
cor "Digite a sua senha de usuário (para instalar):"
[ -n "$NOVA_SIMULA" ] || sudo -v || erro "Senha incorreta."

# 2) internet e curl
TEM_NET=0; if command -v curl >/dev/null 2>&1 || { [ -z "$NOVA_SIMULA" ] && sudo apt-get install -y curl >/dev/null 2>&1; }; then curl -fsS --max-time 8 -o /dev/null "$API" 2>/dev/null && TEM_NET=1; fi
[ $TEM_NET -eq 1 ] || echo "Sem internet agora: vou instalar a versão que está no pendrive (os pacotes do Debian precisam de internet depois)."

# 3) versão do pendrive e, se houver internet, da internet
DEST="$HOME/NOVA-OS"
[ -e "$DEST" ] && { mv "$DEST" "$DEST.antigo-$(date +%Y%m%d-%H%M%S)"; echo "A pasta antiga ~/NOVA-OS foi guardada com o sufixo .antigo-DATA"; }
mkdir -p "$DEST" && tar -xzf "$PACOTE" -C "$DEST" --strip-components=1 --no-same-owner || erro "Não consegui abrir o pacote do pendrive."
NOSSA=$(sed -n 1p "$DEST/VERSAO" 2>/dev/null)
if [ $TEM_NET -eq 1 ]; then
  ULT=$(curl -fsS --max-time 15 "$API/commits/main" 2>/dev/null | sed -n 's/^  "sha": "\([0-9a-f]\{40\}\)".*/\1/p' | head -1)
  if [ -n "$ULT" ] && [ "$ULT" != "$NOSSA" ]; then
    ST=""; [ -n "$NOSSA" ] && ST=$(curl -fsS --max-time 15 "$API/compare/$NOSSA...$ULT" 2>/dev/null | sed -n 's/^  "status": "\([a-z]*\)".*/\1/p' | head -1)
    if [ -z "$NOSSA" ] || [ "$ST" = ahead ] || [ "$ST" = diverged ]; then
      cor "Existe uma versão mais nova do NOVA OS na internet."
      if [ -n "$NOSSA" ]; then
        curl -fsS --max-time 15 "$API/compare/$NOSSA...$ULT" 2>/dev/null | sed -n 's/^        "message": "\([^"\\]*\).*/  • \1/p' | head -8
      fi
      if pergunta "Usar a versão mais nova da internet?" S; then
        T=$(mktemp -d); if curl -fsSL --max-time 600 -o "$T/n.tar.gz" "https://codeload.github.com/$REPO/tar.gz/$ULT" && mkdir -p "$T/x" && tar -xzf "$T/n.tar.gz" -C "$T/x" --strip-components=1 --no-same-owner && [ -f "$T/x/install.sh" ]; then
          rm -rf "$DEST"; mv "$T/x" "$DEST"; printf '%s\n%s\n' "$ULT" "$(date +%F)" > "$DEST/VERSAO"; echo "Usando a versão da internet."
        else echo "Não consegui baixar; sigo com a versão do pendrive."; fi
        rm -rf "$T"
      fi
    fi
  fi
fi

# 4) pergunta única e instalação
BOOTARG=--boot; pergunta "Instalar também a tela de boot do NOVA OS (logo ao ligar, sem textos)?" S || BOOTARG=--sem-boot
JOGOSARG=""; pergunta "Instalar também o RetroArch (emuladores de jogos clássicos; baixa uns 1 GB)?" N && JOGOSARG=--com-jogos
cor "Instalando o NOVA OS. Leva de 20 a 40 minutos; pode deixar o computador sozinho."
bash "$DEST/install.sh" --sim $BOOTARG $JOGOSARG || erro "A instalação parou. Rode de novo: bash $DEST/install.sh --sim   (o log fica em ~/.local/share/nova/instalacao.log)"

echo
echo "================================================"
echo "  Pronto! O NOVA OS foi instalado."
echo "  Dentro dele: Ajustes → Atualizações verifica novidades."
echo "================================================"
if pergunta "Reiniciar o computador agora?" S; then echo "Reiniciando… pode tirar o pendrive."; [ -n "$NOVA_SIMULA" ] && echo "[simula] reboot" || sudo systemctl reboot; fi
