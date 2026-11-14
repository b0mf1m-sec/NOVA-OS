#!/bin/bash
# Instala a tela de boot NOVA OS (Plymouth + GRUB discreto). Chamado por: ./install.sh --boot
# Direto:  sudo bash boot/instalar.sh     |  Desfazer:  sudo bash boot/desinstalar.sh
set -e
[ "$(id -u)" = 0 ] || { echo "Rode com sudo."; exit 1; }
D="$(cd "$(dirname "$0")" && pwd)"
command -v plymouthd >/dev/null || apt-get install -y plymouth plymouth-label
mkdir -p /var/lib/nova-boot
# guarda o estado anterior (só na primeira vez) para poder desfazer
[ -f /var/lib/nova-boot/grub.antes ] || cp /etc/default/grub /var/lib/nova-boot/grub.antes
[ -f /var/lib/nova-boot/plymouthd.antes ] || { cp /etc/plymouth/plymouthd.conf /var/lib/nova-boot/plymouthd.antes 2>/dev/null || : > /var/lib/nova-boot/plymouthd.antes; }
rm -rf /usr/share/plymouth/themes/nova-os
cp -r "$D/tema/nova-os" /usr/share/plymouth/themes/nova-os
chmod -R a+rX /usr/share/plymouth/themes/nova-os
mkdir -p /etc/plymouth
printf '[Daemon]\nTheme=nova-os\nShowDelay=0\nDeviceScale=1\n' > /etc/plymouth/plymouthd.conf
# GRUB: sem texto, menu escondido (aperte Esc ao ligar para ver o menu)
sed -i -E 's/^GRUB_CMDLINE_LINUX_DEFAULT=.*/GRUB_CMDLINE_LINUX_DEFAULT="quiet splash loglevel=0 rd.udev.log_level=0 udev.log_priority=0 vt.global_cursor_default=0"/' /etc/default/grub
sed -i -E 's/^GRUB_TIMEOUT=.*/GRUB_TIMEOUT=1/' /etc/default/grub
for kv in 'GRUB_TIMEOUT_STYLE=hidden' 'GRUB_GFXMODE=1366x768' 'GRUB_GFXPAYLOAD_LINUX=keep'; do
  k="${kv%%=*}"
  if grep -qE "^#?$k=" /etc/default/grub; then sed -i -E "s|^#?$k=.*|$kv|" /etc/default/grub; else echo "$kv" >> /etc/default/grub; fi
done
update-initramfs -u -k all
update-grub
echo; echo "Pronto! Reinicie para ver a nova tela. Para desfazer: sudo bash $D/desinstalar.sh"
