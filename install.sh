#!/bin/bash
# Instalador do NOVA OS (Debian 13 "Trixie"). Rode como usuário comum, NÃO como root:  ./install.sh
#   --so-usuario       instala só a parte do usuário (sem login, firewall e ajustes de sistema)
#   --sem-dependencias não instala pacotes (use se já tiver tudo)
#   --boot / --sem-boot  instala (ou não) a tela de boot NOVA OS (mexe no GRUB). Sem nenhum dos dois, pergunta.
#   --sim              não faz perguntas (usa as respostas padrão)
#   --atualizar        modo atualização: mantém as suas escolhas (tema, dock, opções, login automático), instala só
#                      pacotes que faltam e recompila só o que mudou. É o que a tela Ajustes → Atualizações usa.
# Variáveis: NOVA_SIMULA=1 só mostra os comandos de administrador, sem executá-los (para testes).
set -e
AQUI=$(cd "$(dirname "$0")" && pwd); U=$(id -un)
SO_USUARIO=0; SEM_DEP=0; BOOT=ask; ATUALIZAR=0; SIM=0
for a in "$@"; do case $a in
  --so-usuario) SO_USUARIO=1;; --sem-dependencias) SEM_DEP=1;; --boot) BOOT=1;; --sem-boot) BOOT=0;;
  --sim) SIM=1;; --atualizar) ATUALIZAR=1;; *) echo "opção desconhecida: $a"; exit 2;; esac; done
[ "$(id -u)" -ne 0 ] || { echo "Rode como usuário comum (o instalador pede a senha só quando precisar)."; exit 1; }
grep -q '^VERSION_ID="13"' /etc/os-release 2>/dev/null || echo "Aviso: o NOVA OS foi feito para Debian 13; em outra versão pode não funcionar."

# registro do que foi feito (ajuda a achar o problema se algo falhar)
mkdir -p "$HOME/.local/share/nova"; LOG="$HOME/.local/share/nova/instalacao.log"
exec > >(tee -a "$LOG") 2>&1
echo "=== $(date '+%F %T') install.sh $* ==="
trap 'echo; echo "ERRO: algo deu errado (veja acima ou o arquivo $LOG). Pode rodar o instalador de novo: ele continua de onde parou."' ERR

# comando de administrador: com senha por programa auxiliar (usado pela tela de atualização) ou pelo sudo normal
S() {
  if [ -n "$NOVA_SIMULA" ]; then echo "[simula] sudo $*"; return 0; fi
  if [ -n "$SUDO_ASKPASS" ]; then sudo -A "$@"; else sudo "$@"; fi
}
# o sudo guarda a senha por 15 minutos e a instalação demora mais: renova enquanto o instalador roda
if [ -z "$NOVA_SIMULA" ] && [ -z "$SUDO_ASKPASS" ] && [ $SO_USUARIO -eq 0 ]; then
  sudo -v
  ( while kill -0 $$ 2>/dev/null; do sudo -n -v 2>/dev/null; sleep 45; done ) &
fi

DEPS="openbox tint2 plank picom lightdm lightdm-gtk-greeter dunst feh pcmanfm xterm mpv zathura zathura-pdf-poppler clamav clamav-freshclam
 flameshot network-manager network-manager-gnome ufw unattended-upgrades flatpak fonts-inter fonts-jetbrains-mono papirus-icon-theme
 imagemagick inotify-tools xdotool xss-lock xcape udiskie xclip brightnessctl pipewire pipewire-pulse wireplumber libnotify-bin zenity
 dconf-cli xinput x11-utils x11-xserver-utils xinit xorg mousepad blueman sudo diodon librsvg2-bin python3 pciutils
 firefox-esr firefox-esr-l10n-pt-br xsettingsd libfuse2t64 alsa-utils pipewire-alsa gvfs dbus-x11 xdg-utils curl git vainfo
 build-essential pkg-config libgtk-3-dev libwebkit2gtk-4.1-dev liblightdm-gobject-dev libpulse-dev libxcomposite-dev libx11-dev"
# decodificação de vídeo por hardware conforme a placa de vídeo deste computador
GPU=$(lspci 2>/dev/null | grep -Ei 'vga|3d controller|display controller' || true)
echo "$GPU" | grep -qi intel && DEPS="$DEPS i965-va-driver intel-media-va-driver"
echo "$GPU" | grep -Eqi '\b(amd|ati)\b|radeon|advanced micro' && DEPS="$DEPS mesa-va-drivers"

if [ $SEM_DEP -eq 0 ] && [ $ATUALIZAR -eq 0 ]; then
  echo "==> Instalando pacotes (pode demorar; depende da internet)"
  echo "lightdm shared/default-x-display-manager select lightdm" | S debconf-set-selections
  S apt-get update
  S env DEBIAN_FRONTEND=noninteractive apt-get install -y $DEPS
elif [ $ATUALIZAR -eq 1 ] && [ $SO_USUARIO -eq 0 ]; then
  FALTAM=""; for p in $DEPS; do dpkg -s "$p" >/dev/null 2>&1 || FALTAM="$FALTAM $p"; done
  if [ -n "$FALTAM" ]; then echo "==> Instalando pacotes novos:$FALTAM"; S apt-get update; S env DEBIAN_FRONTEND=noninteractive apt-get install -y $FALTAM; fi
fi

echo "==> Copiando arquivos para a sua conta"
BK="$HOME/.config/nova-backup-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$HOME/.local/bin" "$HOME/.config" "$HOME/.local/share/nova" "$HOME/.local/share/applications" "$HOME/.local/share/plank/themes" "$HOME/.config/plank/dock1/launchers"
subst() { [ $# -gt 0 ] || return 0; sed -i "s#@HOME@#$HOME#g; s#@USER@#$U#g" "$@"; }
for d in openbox tint2 picom dunst nova pcmanfm zathura mpv gtk-3.0; do
  [ -d "$AQUI/config/$d" ] || continue
  if [ -d "$HOME/.config/$d" ]; then mkdir -p "$BK"; cp -r "$HOME/.config/$d" "$BK/"; fi
  mkdir -p "$HOME/.config/$d"
  if [ $ATUALIZAR -eq 1 ] && { [ "$d" = nova ] || [ "$d" = pcmanfm ]; }; then
    cp -rn "$AQUI/config/$d/." "$HOME/.config/$d/" 2>/dev/null || true       # atualização: só o que ainda não existe (mantém as suas escolhas)
  else
    cp -r "$AQUI/config/$d/." "$HOME/.config/$d/"
  fi
  subst $(find "$HOME/.config/$d" -type f)
done
[ -f "$HOME/.xinitrc" ] && { mkdir -p "$BK"; cp "$HOME/.xinitrc" "$BK/xinitrc"; }
cp "$AQUI/config/xinitrc" "$HOME/.xinitrc"; chmod +x "$HOME/.xinitrc"
cp "$AQUI"/bin/nova-* "$HOME/.local/bin/"; chmod +x "$HOME"/.local/bin/nova-*; subst "$HOME"/.local/bin/nova-*
cp -r "$AQUI/share/nova/." "$HOME/.local/share/nova/"
cp "$AQUI"/share/applications/*.desktop "$HOME/.local/share/applications/"; subst "$HOME"/.local/share/applications/nova-*.desktop
cp -r "$AQUI/share/plank-themes/Nova" "$HOME/.local/share/plank/themes/"
if [ $ATUALIZAR -eq 1 ]; then
  cp -n "$AQUI"/config/plank/launchers/*.dockitem "$HOME/.config/plank/dock1/launchers/" 2>/dev/null || true   # só lançadores novos; o dock do usuário fica como está
else
  cp "$AQUI"/config/plank/launchers/*.dockitem "$HOME/.config/plank/dock1/launchers/"
  if command -v dconf >/dev/null; then dconf load /net/launchpad/plank/ < "$AQUI/config/plank/plank.dconf" 2>/dev/null || true; fi
fi
subst "$HOME"/.config/plank/dock1/launchers/*.dockitem
if command -v flatpak >/dev/null; then flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo 2>/dev/null || true; fi

# o que mudou nos fontes? (na atualização, recompila só isso; se a base comum mudou, recompila tudo)
APPS=""
if [ $ATUALIZAR -eq 1 ]; then
  TUDO=0
  for d in "$AQUI"/fontes/nova-*/; do
    n=$(basename "$d"); mud=0
    for f in "$d"*; do cmp -s "$f" "$HOME/$n/$(basename "$f")" 2>/dev/null || mud=1; done
    if [ $mud -eq 1 ]; then if [ "$n" = nova-comum ]; then TUDO=1; else APPS="$APPS ${n#nova-}"; fi; fi
  done
  [ $TUDO -eq 1 ] && APPS="TUDO"
fi
for d in "$AQUI"/fontes/nova-*/; do n=$(basename "$d"); mkdir -p "$HOME/$n"; cp "$d"/* "$HOME/$n/"; done
chmod +x "$HOME/nova-comum/compilar"
if [ $ATUALIZAR -eq 0 ] || [ "$APPS" = TUDO ]; then
  echo "==> Compilando os apps do NOVA OS (demora uns minutos)"; "$HOME/nova-comum/compilar"
elif [ -n "$APPS" ]; then
  echo "==> Compilando o que mudou:$APPS"; "$HOME/nova-comum/compilar" $APPS
else
  echo "==> Nenhum app precisou ser recompilado"
fi
echo "==> Aplicando o visual"
if [ -x "$HOME/.local/bin/nova-tema" ]; then "$HOME/.local/bin/nova-tema" >/dev/null 2>&1 || true; fi
if [ $ATUALIZAR -eq 1 ] && [ -n "$DISPLAY" ] && pgrep -x openbox >/dev/null; then openbox --reconfigure 2>/dev/null || true; fi

if [ $SO_USUARIO -eq 0 ]; then
  echo "==> Parte do sistema (login, segurança)"
  S install -m755 -o root -g root "$AQUI/sistema/nova-admin" /usr/local/sbin/nova-admin
  echo "$U ALL=(root) NOPASSWD: /usr/local/sbin/nova-admin" | S tee /etc/sudoers.d/90-nova-admin >/dev/null
  S chmod 440 /etc/sudoers.d/90-nova-admin
  if [ -z "$NOVA_SIMULA" ]; then S visudo -cf /etc/sudoers.d/90-nova-admin >/dev/null || { S rm -f /etc/sudoers.d/90-nova-admin; echo "ERRO no sudoers"; exit 1; }; fi
  # quem instalou precisa poder usar o sudo (com senha) para atualizações e para a Loja
  getent group sudo | grep -qw "$U" || S usermod -aG sudo "$U"
  S install -m755 "$AQUI/sistema/nova-greeter" /usr/local/bin/nova-greeter
  S install -m755 "$AQUI/sistema/nova-session" /usr/local/bin/nova-session
  S install -m755 "$HOME/nova-greeter/nova-greeter" /usr/local/bin/nova-greeter-bin
  S install -m644 "$AQUI/sistema/xsession-nova.desktop" /usr/share/xsessions/nova.desktop
  S install -m644 "$AQUI/sistema/xgreeter-nova.desktop" /usr/share/xgreeters/nova-greeter.desktop
  S mkdir -p /usr/share/nova /etc/lightdm/lightdm.conf.d
  S sh -c "cp '$HOME'/.local/share/nova/icons/marca.* /usr/share/nova/ 2>/dev/null || true"
  # o arquivo do login pode ter sido ajustado pelo usuário (login automático): na atualização não é reescrito
  if [ $ATUALIZAR -eq 0 ] || [ ! -f /etc/lightdm/lightdm.conf.d/50-nova.conf ]; then
    printf '[Seat:*]\ngreeter-session=nova-greeter\nuser-session=nova\ngreeter-hide-users=false\nallow-guest=false\n' | S tee /etc/lightdm/lightdm.conf.d/50-nova.conf >/dev/null
  fi
  S install -m644 "$AQUI/sistema/60-nova-seguranca.conf" /etc/sysctl.d/60-nova-seguranca.conf
  [ -n "$NOVA_SIMULA" ] || S sysctl --system >/dev/null
  if [ -d /usr/lib/firefox-esr/distribution ] || [ -n "$NOVA_SIMULA" ]; then sed "s#@HOME@#$HOME#g; s#@USER@#$U#g" "$AQUI/sistema/firefox-policies.json" | S tee /usr/lib/firefox-esr/distribution/policies.json >/dev/null; fi
  S install -d /etc/polkit-1/rules.d; S install -m644 "$AQUI/sistema/49-nova-login-energia.rules" /etc/polkit-1/rules.d/49-nova-login-energia.rules
  printf 'APT::Periodic::Update-Package-Lists "1";\nAPT::Periodic::Unattended-Upgrade "1";\n' | S tee /etc/apt/apt.conf.d/20auto-upgrades >/dev/null
  if [ $ATUALIZAR -eq 0 ]; then       # na atualização não se religa o que o usuário possa ter desligado de propósito
    S ufw default deny incoming >/dev/null; S ufw default allow outgoing >/dev/null; S ufw --force enable >/dev/null
    S systemctl enable --now clamav-freshclam unattended-upgrades 2>/dev/null || true
    S systemctl enable lightdm 2>/dev/null || true
    S sh -c 'echo /usr/sbin/lightdm > /etc/X11/default-display-manager'
    S systemctl set-default graphical.target 2>/dev/null || true
    if [ "$BOOT" = ask ]; then
      if [ $SIM -eq 0 ] && [ -t 0 ]; then
        read -r -p "Instalar também a tela de boot do NOVA OS (logo ao ligar, sem textos)? [S/n] " r; case "$r" in n|N) BOOT=0;; *) BOOT=1;; esac
      else BOOT=1; fi
    fi
    if [ "$BOOT" = 1 ]; then echo "==> Tela de boot NOVA OS"; S bash "$AQUI/boot/instalar.sh"; fi
  fi
fi

# registra a versão instalada (a tela Ajustes → Atualizações compara com a do GitHub)
SHA=""; DT=""
[ -f "$AQUI/VERSAO" ] && { SHA=$(sed -n 1p "$AQUI/VERSAO"); DT=$(sed -n 2p "$AQUI/VERSAO"); }
if [ -z "$SHA" ] && command -v git >/dev/null; then SHA=$(git -C "$AQUI" rev-parse HEAD 2>/dev/null || true); DT=$(git -C "$AQUI" log -1 --format=%cs 2>/dev/null || true); fi
if printf '%s' "$SHA" | grep -Eq '^[0-9a-f]{40}$'; then printf '%s\n%s\n' "$SHA" "${DT:-$(date +%F)}" > "$HOME/.local/share/nova/versao"; fi

trap - ERR
echo
if [ $ATUALIZAR -eq 1 ]; then
  echo "Atualização concluída! Para aplicar tudo, reinicie o computador (ou saia e entre de novo)."
else
  echo "Pronto! Reinicie o computador e escolha a sessão \"NOVA OS\" na tela de login."
fi
[ -d "$BK" ] && echo "Suas configurações antigas ficaram guardadas em: $BK" || true
