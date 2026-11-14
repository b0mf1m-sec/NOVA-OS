# NOVA OS

Um sistema **leve, bonito e fácil de usar** montado sobre o **Debian 13**, pensado para computadores simples e para quem **não usa terminal**: tudo se faz com cliques e textos claros em português.

![Loja do NOVA OS](docs/loja.png)

## Vantagens

- **Roda em máquina fraca.** Desenvolvido e usado num notebook com Celeron B830, 4 GB de RAM e tela 1366x768. Os apps são escritos em C (GTK3), sem Electron nem navegador embutido (exceto o WhatsApp), e há um **Modo desempenho** de um clique.
- **Visual coeso, estilo dock + barra.** Dock translúcido com Alt+Tab com miniaturas, tema claro/escuro, cor de destaque e papéis de parede próprios.
- **Loja de apps de um clique.** Instala e desinstala programas (apt e Flatpak) com barra de progresso e mensagens de erro claras, sem terminal.
- **Dock inteligente.** Passe o mouse num ícone para ver as janelas abertas daquele programa e escolher qual abrir; clique direito abre nova janela, nova aba ou janela privada.
- **Segurança de fábrica.**
  - Firewall ligado (bloqueia tudo que entra) e acesso remoto desligado.
  - Atualizações de segurança automáticas.
  - Antivírus ClamAV com proteção em tempo real nos downloads e varredura agendada.
  - Endurecimento do kernel e bloqueio de tela ao voltar do descanso.
  - Sem `sudo` sem senha: só um ajudante com ações fixas e validadas (`nova-admin`).
- **Feito para a família.** Ajustes simples, Otimizador (limpeza e desempenho), Central de controle, gravador de voz, tela de login própria.

## O que vem dentro

| Peça | Função |
|---|---|
| `nova-loja` | Loja de apps |
| `nova-apps` | Grade de todos os programas (clique direito: executar como administrador / desinstalar) |
| `nova-ajustes` | Tela, som, rede, segurança, aplicativos, atalhos |
| `nova-otimizador` | Limpeza e Modo desempenho |
| `nova-central` | Central rápida (Wi-Fi, volume, energia) |
| `nova-janelas` | Alt+Tab com miniaturas e pré-visualização no dock |
| `nova-greeter` | Tela de login (LightDM) |
| `nova-papel` | Papel de parede (inclui animado) |
| `nova-whatsapp` | WhatsApp numa janela própria |
| `nova-gravador` | Gravador de voz |

Base: Openbox + tint2 + Plank + picom.

## Instalação

Requisitos: **Debian 13 (Trixie)** já instalado (de preferência instalação mínima, sem ambiente gráfico), com internet.

```bash
sudo apt install git
git clone https://github.com/SEU-USUARIO/NOVA-OS.git
cd NOVA-OS
./install.sh
```

Rode como usuário comum; o instalador pede a senha quando precisar. Depois **reinicie** e, na tela de login, escolha a sessão **NOVA OS**.

Opções:

- `./install.sh --so-usuario` instala só a parte da sua conta (sem tela de login, firewall e ajustes de sistema).
- `./install.sh --sem-dependencias` pula a instalação de pacotes.

Suas configurações anteriores (openbox, tint2 etc.) são guardadas em `~/.config/nova-backup-DATA`.

## Estrutura do repositório

```
fontes/    código dos apps (C/GTK3) e script de compilação
bin/       comandos nova-* (shell)
config/    configurações (openbox, tint2, picom, plank, dunst...)
share/     ícones, papéis de parede, temas, lançadores
sistema/   o que vai para fora da sua pasta (login, sudo restrito, sysctl, políticas do Firefox)
install.sh instalador
```

## Segurança: como o `sudo` funciona

O NOVA OS **não** libera `sudo` sem senha. Só `/usr/local/sbin/nova-admin` roda sem senha, com ações fixas (instalar/remover pacotes da lista da Loja, ligar/desligar firewall, etc.), validando cada argumento. Pacotes essenciais do sistema não podem ser removidos por ele. Qualquer outra coisa pede a senha.

## Aviso

Projeto pessoal, testado em Debian 13 num único notebook. O instalador altera o login e regras de sistema: leia `install.sh` antes e use em máquina que possa reconfigurar. Sem garantias.

## Licença

MIT, veja `LICENSE`.
