# Para o Claude que está lendo isto (instalação do NOVA OS neste PC)

O usuário quer que este PC fique **idêntico** a outro que já roda o **NOVA OS**: um Debian 13 personalizado (Openbox + tint2 + Plank + picom, apps próprios em C/GTK3, login próprio no LightDM). Esta pasta é uma cópia completa do projeto. Tudo o que é preciso está aqui; a internet só é usada para baixar pacotes do Debian.

## Quem usa e como falar
- Os usuários finais (pais do dono) **não sabem usar terminal**. O dono do PC entende um pouco, mas prefere **explicações curtas, em português simples**.
- Teste ao fim de cada etapa e corrija até passar. Não dê instruções vagas: diga exatamente o que digitar.

## Passo a passo

1. **Confira o ponto de partida**: `cat /etc/os-release` deve mostrar Debian 13 (trixie). Rode como **usuário comum** (não root). Precisa de internet: `ping -c1 deb.debian.org`.
2. **Copie a pasta para o disco** (não rode direto do pendrive, ele pode ser somente leitura ou lento):
   `cp -r /caminho/do/pendrive/NOVA-OS ~/NOVA-OS && cd ~/NOVA-OS`
3. **Instale**: `./install.sh` (pede a senha do sudo; leva de 20 a 40 min; baixa mais de mil pacotes e compila os apps).
   - Para instalar também a tela de boot (Plymouth/GRUB): `./install.sh --boot`. **Antes**, abra `boot/instalar.sh` e ajuste `GRUB_GFXMODE=1366x768` para a resolução real do monitor deste PC (`xrandr` ou `cat /sys/class/drm/*/modes`). Se o PC for UEFI/BIOS diferente, confira que `update-grub` terminou sem erro.
4. **Reinicie** e, na tela de login, a sessão já deve ser a **NOVA OS** (vem como padrão pelo `lightdm.conf.d/50-nova.conf`).
5. **Confira, item por item** (e corrija o que falhar):
   - Login aparece com o "N" e os botões Desligar/Reiniciar funcionam (regra polkit `/etc/polkit-1/rules.d/49-nova-login-energia.rules`).
   - Dock (plank) e barra (tint2) aparecem; `nova-tema` aplica o visual (`~/.local/bin/nova-tema`).
   - Alt+Tab com miniaturas (`nova-janelas`), grade de Apps, Loja, Ajustes, Otimizador abrem.
   - Firefox abre maximizado, downloads vão para `~/Downloads` (policies.json em `/usr/lib/firefox-esr/distribution/`).
   - Firewall ativo: `sudo ufw status`. SSH **desligado** de propósito.
   - `sudo -n true` deve **pedir senha** (sudo sem senha foi removido de propósito; só `sudo -n /usr/local/sbin/nova-admin ...` roda sem senha).
6. **Ajustes que dependem do hardware do PC novo** (o PC original é um Celeron B830, 4 GB, tela 1366x768, GPU Intel Sandy Bridge):
   - Resolução/GRUB: ver passo 3.
   - Vídeo: o Firefox está forçado a H.264 + VA-API com `i965-va-driver` (só serve para GPU Intel antiga). Em Intel mais nova instale `intel-media-va-driver`; em AMD/NVIDIA troque o driver VA-API e confira `vainfo`. Se o vídeo falhar, edite as prefs em `policies.json`.
   - Se o PC tiver bem mais RAM/CPU, o **Modo desempenho** (`nova-desempenho`) pode ficar desligado.
   - Opção `dpi` do mouse e `tela_min` em `~/.config/nova/opcoes` podem precisar de ajuste.
   - Wi-Fi/firmware: se faltar, instale `firmware-iwlwifi`, `firmware-realtek` etc. (precisa do repositório `non-free-firmware` ativo).
7. **Primeiro uso**: crie/cheque o usuário, o idioma pt-BR e o fuso. Depois peça ao dono para testar com cliques, sem terminal.

## Se algo der errado
- `install.sh` aborta com `set -e`: leia a última linha de erro, corrija e **rode de novo** (é seguro repetir; guarda backup das configs antigas em `~/.config/nova-backup-*`).
- `./install.sh --sem-dependencias` repete só a parte de cópia/compilação.
- Compilar um app só: `~/nova-comum/compilar nome` (ex.: `greeter`, depois `sudo install -m755 ~/nova-greeter/nova-greeter /usr/local/bin/nova-greeter-bin`).
- Desinstalar: `./uninstall.sh` (e `boot/desinstalar.sh` para o boot).
- **Nunca** use `pkill -f` com padrão que apareça no próprio comando, e **nunca** `pkill -x openbox` na sessão real (derruba tudo).
- O plank recria arquivos `.dockitem` enquanto roda: pare-o antes de mexer nos lançadores.
- `desfazer.txt` do PC original (histórico do que foi alterado e como reverter) está em `docs-extra/desfazer.txt`, se existir.

## Regras do dono para trabalhar
- Explicar em linguagem leiga e resumida; testar ao fim de cada etapa; corrigir até passar.
- Não apague nada do usuário sem avisar. Não publique nada (GitHub etc.) sem pedir.
- Segurança: não reative sudo sem senha; não ligue o SSH sem o dono pedir.

## Mapa do projeto
- `install.sh` / `uninstall.sh`: instalador e desinstalador.
- `fontes/nova-*/`: código C dos apps; `fontes/nova-comum/compilar` compila.
- `bin/`: comandos `nova-*` (vão para `~/.local/bin`); `config/`: configs (placeholders `@HOME@`/`@USER@`, trocados na instalação).
- `sistema/`: arquivos que vão para o sistema (nova-admin, greeter, sessão, sysctl, polkit, política do Firefox).
- `share/`: ícones, papéis de parede, tema do plank, .desktop. `boot/`: tema Plymouth/GRUB (opcional).
