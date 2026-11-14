#!/bin/bash
# Remove o NOVA OS. Rode como usuário comum:  ./uninstall.sh   (ou --so-usuario para manter o login e o firewall)
# Não apaga seus arquivos pessoais. Suas configurações antigas, se houver, estão em ~/.config/nova-backup-*
set -e
SO_USUARIO=0; [ "$1" = "--so-usuario" ] && SO_USUARIO=1
[ "$(id -u)" -ne 0 ] || { echo "Rode como usuário comum."; exit 1; }
read -r -p "Remover o NOVA OS deste computador? (s/N) " r; [ "$r" = s ] || [ "$r" = S ] || exit 0
pkill -x nova-janelas 2>/dev/null || true
rm -f "$HOME"/.local/bin/nova-*
rm -rf "$HOME"/nova-* "$HOME/.local/share/nova" "$HOME"/.local/share/applications/nova-*.desktop "$HOME/.local/share/plank/themes/Nova"
rm -rf "$HOME"/.config/nova "$HOME"/.config/plank/dock1/launchers/nova-*.dockitem
if [ $SO_USUARIO -eq 0 ]; then
  echo "Removendo a parte do sistema (pede a senha)..."
  sudo rm -f /usr/local/sbin/nova-admin /etc/sudoers.d/90-nova-admin /usr/local/bin/nova-greeter /usr/local/bin/nova-greeter-bin /usr/local/bin/nova-session \
    /usr/share/xsessions/nova.desktop /usr/share/xgreeters/nova-greeter.desktop /etc/lightdm/lightdm.conf.d/50-nova.conf /etc/sysctl.d/60-nova-seguranca.conf
  sudo rm -rf /usr/share/nova
fi
echo "Pronto. O NOVA OS foi removido. Reinicie e escolha outra sessão (por exemplo, openbox) na tela de login."
echo "Firewall (ufw), antivírus e demais pacotes instalados continuam; remova com 'sudo apt remove' se quiser."
