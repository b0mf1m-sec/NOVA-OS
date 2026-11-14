#!/bin/bash
# Volta o GRUB e o Plymouth ao estado de antes da tela de boot NOVA OS.
set -e
[ "$(id -u)" = 0 ] || { echo "Rode com sudo."; exit 1; }
[ -f /var/lib/nova-boot/grub.antes ] || { echo "Nada para desfazer (a tela de boot NOVA OS não foi instalada por aqui)."; exit 1; }
cp /var/lib/nova-boot/grub.antes /etc/default/grub
if [ -s /var/lib/nova-boot/plymouthd.antes ]; then cp /var/lib/nova-boot/plymouthd.antes /etc/plymouth/plymouthd.conf; else rm -f /etc/plymouth/plymouthd.conf; fi
rm -rf /usr/share/plymouth/themes/nova-os /var/lib/nova-boot
update-initramfs -u -k all
update-grub
echo "Pronto. Reinicie para voltar à tela de boot original."
