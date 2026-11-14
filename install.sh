#!/bin/bash
# Instalador do NOVA OS (Debian 13 "Trixie"). Rode como usuário comum, NÃO como root:  ./install.sh
#   --so-usuario       instala só a parte do usuário (sem login, firewall e ajustes de sistema)
#   --sem-dependencias não instala pacotes (use se já tiver tudo)
#   --boot             também instala a tela de boot NOVA OS (opcional; mexe no GRUB)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd); U=$(id -un)
SO_USUARIO=0; SEM_DEP=0; BOOT=0
for a in "$@"; do case $a in --so-usuario) SO_USUARIO=1;; --sem-dependencias) SEM_DEP=1;; --boot) BOOT=1;; *) echo "opção desconhecida: $a"; exit 2;; esac; done
[ "$(id -u)" -ne 0 ] || { echo "Rode como usuário comum (o instalador pede a senha só quando precisar)."; exit 1; }
grep -q '^VERSION_ID="13"' /etc/os-release 2>/dev/null || echo "Aviso: o NOVA OS foi feito para Debian 13; em outra versão pode não funcionar."

DEPS="openbox tint2 plank picom lightdm lightdm-gtk-greeter dunst feh pcmanfm xterm mpv zathura zathura-pdf-poppler clamav clamav-freshclam
 flameshot network-manager network-manager-gnome ufw unattended-upgrades flatpak fonts-inter fonts-jetbrains-mono papirus-icon-theme
 imagemagick inotify-tools xdotool xss-lock xcape udiskie xclip brightnessctl pipewire pipewire-pulse wireplumber libnotify-bin zenity
 dconf-cli xinput x11-utils x11-xserver-utils xinit xorg mousepad blueman sudo diodon librsvg2-bin python3
 firefox-esr firefox-esr-l10n-pt-br xsettingsd libfuse2t64 alsa-utils pipewire-alsa gvfs dbus-x11 xdg-utils curl git i965-va-driver vainfo
 build-essential pkg-config libgtk-3-dev libwebkit2gtk-4.1-dev liblightdm-gobject-dev libpulse-dev libxcomposite-dev libx11-dev"
if [ $SEM_DEP -eq 0 ]; then
  echo "==> Instalando pacotes (pede a senha)"; sudo apt-get update; sudo apt-get install -y $DEPS
fi

echo "==> Copiando arquivos para a sua conta"
BK="$HOME/.config/nova-backup-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$HOME/.local/bin" "$HOME/.config" "$HOME/.local/share/nova" "$HOME/.local/share/applications" "$HOME/.local/share/plank/themes" "$HOME/.config/plank/dock1/launchers"
subst() { [ $# -gt 0 ] || return 0; sed -i "s#@HOME@#$HOME#g; s#@USER@#$U#g" "$@"; }
for d in openbox tint2 picom dunst nova pcmanfm zathura mpv gtk-3.0; do
  [ -d "$AQUI/config/$d" ] || continue
  if [ -d "$HOME/.config/$d" ]; then mkdir -p "$BK"; cp -r "$HOME/.config/$d" "$BK/"; fi
  mkdir -p "$HOME/.config/$d"; cp -r "$AQUI/config/$d/." "$HOME/.config/$d/"; subst $(find "$HOME/.config/$d" -type f)
done
[ -f "$HOME/.xinitrc" ] && { mkdir -p "$BK"; cp "$HOME/.xinitrc" "$BK/xinitrc"; }
cp "$AQUI/config/xinitrc" "$HOME/.xinitrc"; chmod +x "$HOME/.xinitrc"
cp "$AQUI"/bin/nova-* "$HOME/.local/bin/"; chmod +x "$HOME"/.local/bin/nova-*; subst "$HOME"/.local/bin/nova-*
cp -r "$AQUI/share/nova/." "$HOME/.local/share/nova/"
cp "$AQUI"/share/applications/*.desktop "$HOME/.local/share/applications/"; subst "$HOME"/.local/share/applications/nova-*.desktop
cp -r "$AQUI/share/plank-themes/Nova" "$HOME/.local/share/plank/themes/"
cp "$AQUI"/config/plank/launchers/*.dockitem "$HOME/.config/plank/dock1/launchers/"; subst "$HOME"/.config/plank/dock1/launchers/*.dockitem
if command -v dconf >/dev/null; then dconf load /net/launchpad/plank/ < "$AQUI/config/plank/plank.dconf" 2>/dev/null || true; fi
if command -v flatpak >/dev/null; then flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo 2>/dev/null || true; fi

echo "==> Compilando os apps do NOVA OS"
for d in "$AQUI"/fontes/nova-*/; do n=$(basename "$d"); mkdir -p "$HOME/$n"; cp "$d"/* "$HOME/$n/"; done
chmod +x "$HOME/nova-comum/compilar"
"$HOME/nova-comum/compilar"
if [ -x "$HOME/.local/bin/nova-tema" ]; then "$HOME/.local/bin/nova-tema" >/dev/null 2>&1 || true; fi

if [ $SO_USUARIO -eq 0 ]; then
  echo "==> Parte do sistema (login, firewall, segurança) — pede a senha"
  sudo install -m755 -o root -g root "$AQUI/sistema/nova-admin" /usr/local/sbin/nova-admin
  echo "$U ALL=(root) NOPASSWD: /usr/local/sbin/nova-admin" | sudo tee /etc/sudoers.d/90-nova-admin >/dev/null
  sudo chmod 440 /etc/sudoers.d/90-nova-admin
  sudo visudo -cf /etc/sudoers.d/90-nova-admin >/dev/null || { sudo rm -f /etc/sudoers.d/90-nova-admin; echo "ERRO no sudoers"; exit 1; }
  sudo install -m755 "$AQUI/sistema/nova-greeter" /usr/local/bin/nova-greeter
  sudo install -m755 "$AQUI/sistema/nova-session" /usr/local/bin/nova-session
  sudo install -m755 "$HOME/nova-greeter/nova-greeter" /usr/local/bin/nova-greeter-bin
  sudo install -m644 "$AQUI/sistema/xsession-nova.desktop" /usr/share/xsessions/nova.desktop
  sudo install -m644 "$AQUI/sistema/xgreeter-nova.desktop" /usr/share/xgreeters/nova-greeter.desktop
  sudo mkdir -p /usr/share/nova /etc/lightdm/lightdm.conf.d
  sudo cp "$HOME"/.local/share/nova/icons/marca.* /usr/share/nova/ 2>/dev/null || true
  printf '[Seat:*]\ngreeter-session=nova-greeter\nuser-session=nova\ngreeter-hide-users=false\nallow-guest=false\n' | sudo tee /etc/lightdm/lightdm.conf.d/50-nova.conf >/dev/null
  sudo install -m644 "$AQUI/sistema/60-nova-seguranca.conf" /etc/sysctl.d/60-nova-seguranca.conf; sudo sysctl --system >/dev/null
  if [ -d /usr/lib/firefox-esr/distribution ]; then sed "s#@HOME@#$HOME#g; s#@USER@#$U#g" "$AQUI/sistema/firefox-policies.json" | sudo tee /usr/lib/firefox-esr/distribution/policies.json >/dev/null; fi
  sudo install -d /etc/polkit-1/rules.d; sudo install -m644 "$AQUI/sistema/49-nova-login-energia.rules" /etc/polkit-1/rules.d/49-nova-login-energia.rules
  printf 'APT::Periodic::Update-Package-Lists "1";\nAPT::Periodic::Unattended-Upgrade "1";\n' | sudo tee /etc/apt/apt.conf.d/20auto-upgrades >/dev/null
  sudo ufw default deny incoming >/dev/null; sudo ufw default allow outgoing >/dev/null; sudo ufw --force enable >/dev/null
  sudo systemctl enable --now clamav-freshclam unattended-upgrades 2>/dev/null || true
  sudo systemctl enable lightdm 2>/dev/null || true
  sudo systemctl set-default graphical.target 2>/dev/null || true
  if [ $BOOT -eq 1 ]; then echo "==> Tela de boot NOVA OS"; sudo bash "$AQUI/boot/instalar.sh"; fi
fi
[ $BOOT -eq 1 ] && [ $SO_USUARIO -eq 1 ] && echo "Aviso: --boot é ignorado com --so-usuario (mexe no sistema)." || true
echo
echo "Pronto! Reinicie o computador e escolha a sessão \"NOVA OS\" na tela de login."
[ -d "$BK" ] && echo "Suas configurações antigas ficaram guardadas em: $BK" || true
