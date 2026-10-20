# InputServer

Use **um único teclado e mouse** para controlar vários computadores — Windows e Mac — pela rede, como se fossem um só. Leve o cursor até a borda da tela e ele "atravessa" para o outro computador. A área de transferência (copiar/colar) também é compartilhada.

O InputServer tem instância única, botão Parar confiável e ajustes para reduzir o atraso no Windows e no Mac.

> **Áudio (fone e microfone):** em desenvolvimento. A versão atual transmite apenas teclado, mouse e área de transferência. Veja [Roadmap](#roadmap).

---

## Sumário

1. [Como funciona](#como-funciona)
2. [Requisitos](#requisitos)
3. [Instalação no Windows](#instalação-no-windows)
4. [Instalação no Mac](#instalação-no-mac)
5. [Primeira configuração](#primeira-configuração)
6. [Uso no dia a dia](#uso-no-dia-a-dia)
7. [Reduzindo o atraso (latência)](#reduzindo-o-atraso-latência)
8. [Solução de problemas](#solução-de-problemas)
9. [Roadmap](#roadmap)
10. [Licença](#licença)

---

## Como funciona

```
 ┌────────────────────────┐        rede local (TCP 24800, TLS)        ┌────────────────────────┐
 │  SERVIDOR              │  ───────────────────────────────────────▶  │  CLIENTE               │
 │  teclado + mouse       │     movimentos, cliques, teclas,           │  recebe e executa      │
 │  físicos ficam aqui    │     área de transferência                  │  os comandos           │
 └────────────────────────┘                                            └────────────────────────┘
```

- **Servidor:** o computador onde o teclado e o mouse estão conectados.
- **Cliente:** o(s) computador(es) que você quer controlar. Pode haver vários.
- A conexão é criptografada (TLS). Na primeira conexão o cliente pede para você confirmar a "impressão digital" (fingerprint) do servidor.
- Servidor e clientes precisam ser **InputServer** (o protocolo é próprio; outros programas de compartilhamento de teclado e mouse não conectam).

Qualquer combinação funciona: Windows → Mac, Mac → Windows, Windows → Windows, Mac → Mac.

## Requisitos

| | Windows | Mac |
|---|---|---|
| Sistema | Windows 10 ou 11, 64 bits | macOS 11 (Big Sur) ou mais recente — Intel ou Apple Silicon |
| Rede | Os dois computadores na **mesma rede local**. Cabo é recomendado (veja [latência](#reduzindo-o-atraso-latência)). | |
| Porta | TCP **24800** liberada no firewall do **servidor** | |

Ainda não há instaladores publicados: o InputServer é compilado a partir do código-fonte. Os passos abaixo cobrem tudo, do zero.

---

## Instalação no Windows

### 1. Instale as ferramentas de compilação

Abra o **PowerShell** e instale com o `winget` (já vem no Windows 10/11):

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
winget install --id Kitware.CMake
winget install --id Ninja-build.Ninja
winget install --id ShiningLight.OpenSSL.Dev
winget install --id Git.Git
```

Instale o **Qt 6** (Qt 6.6 ou mais recente, componente *MSVC 2019 64-bit*). O jeito mais simples é com o `aqt`, que não exige conta Qt:

```powershell
winget install --id Python.Python.3.12
pip install aqtinstall
aqt install-qt windows desktop 6.6.3 win64_msvc2019_64 -O C:\Qt
```

> Feche e abra o PowerShell depois das instalações para atualizar o `PATH`.

### 2. Baixe o código

```powershell
git clone --recurse-submodules https://github.com/pedro-lucaas/inputServer.git
cd inputServer
```

### 3. Compile

**Jeito mais fácil:** no PowerShell, dentro da pasta do projeto, rode o script que faz tudo (configura, compila, testa e gera o instalador):

```powershell
winget install --id JRSoftware.InnoSetup
.\scripts\build-windows.ps1 -Installer
```

O instalador fica em `build\installer-inno\bin\InputServerSetup-<versão>.exe` — pule para o [passo 4](#4-instale). Se o PowerShell bloquear o script, rode antes `Set-ExecutionPolicy -Scope Process Bypass`.

**Passo a passo manual:** abra o **"x64 Native Tools Command Prompt for VS 2022"** (procure no menu Iniciar), entre na pasta do projeto e rode:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.6.3\msvc2019_64 ^
  -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64" ^
  -DCMAKE_INSTALL_PREFIX=dist-inputserver
cmake --build build --parallel
cmake --install build
```

Se o OpenSSL foi instalado em outro lugar, ajuste `OPENSSL_ROOT_DIR`.

Opcional — rodar os testes:

```bat
ctest --test-dir build --output-on-failure
```

### 4. Instale

O `cmake --install` monta a pasta `dist-inputserver` com `InputServer.exe`, `inputserver-server.exe`, `inputserver-client.exe` e todas as DLLs necessárias. A partir daí há dois caminhos:

**Opção A — instalador (recomendado).** Instale o [Inno Setup 6](https://jrsoftware.org/isdl.php) (`winget install --id JRSoftware.InnoSetup`) e gere o instalador:

```bat
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" build\installer-inno\inputserver.iss
```

Execute `build\installer-inno\bin\InputServerSetup-0.1.0.exe`. Ele instala em *Arquivos de Programas*, cria o atalho no menu Iniciar e já libera o firewall. Para atualizar, basta rodar o instalador de uma versão nova: ele fecha o InputServer, substitui os arquivos e mantém suas configurações.

**Opção B — pasta portátil.** Mova a pasta `dist-inputserver` para onde quiser (por exemplo `C:\InputServer`) e crie um atalho para `InputServer.exe`. Nesse caso, libere o firewall manualmente (passo 5).

### 5. Libere o firewall (somente no servidor)

Na primeira execução o Windows pergunta se o InputServer pode usar a rede — marque **Redes privadas** e clique em **Permitir**. Se a pergunta não aparecer, rode como administrador:

```powershell
New-NetFirewallRule -DisplayName "InputServer" -Direction Inbound -Protocol TCP -LocalPort 24800 -Action Allow -Profile Private
New-NetFirewallRule -DisplayName "InputServer (descoberta)" -Direction Inbound -Protocol UDP -LocalPort 24801 -Action Allow -Profile Private
```

> Confira se a sua rede está marcada como **Privada** em *Configurações → Rede e Internet → Propriedades*. Em redes "Públicas" o Windows bloqueia conexões de entrada.

---

## Instalação no Mac

### 1. Instale as ferramentas de compilação

Abra o **Terminal** e instale as ferramentas da Apple:

```bash
xcode-select --install
```

Instale o [Homebrew](https://brew.sh) (se ainda não tiver) e as dependências:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
brew install cmake ninja qt openssl@3 git
```

### 2. Baixe o código

```bash
git clone --recurse-submodules https://github.com/pedro-lucaas/inputServer.git
cd inputServer
```

### 3. Compile

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11
cmake --build build --parallel
```

O aplicativo é gerado em `build/bundle/InputServer.app`. Arraste-o para a pasta **Aplicativos**.

### 4. Abra pela primeira vez

Como o app não é assinado pela Apple, o macOS bloqueia a primeira abertura. Para liberar:

1. Na pasta Aplicativos, clique com o **botão direito** no app → **Abrir** → **Abrir**.
2. Se ainda assim for bloqueado, vá em *Ajustes do Sistema → Privacidade e Segurança* e clique em **Abrir Mesmo Assim**.

### 5. Conceda as permissões (obrigatório)

Sem estas permissões o Mac não deixa o InputServer mover o mouse nem digitar. Em *Ajustes do Sistema → Privacidade e Segurança*:

| Permissão | Para quê | Quando |
|---|---|---|
| **Acessibilidade** | Mover o mouse e digitar no Mac | Mac como cliente **e** como servidor |
| **Monitoramento de Entrada** | Ler o teclado e o mouse físicos | Mac como servidor |

Ative o InputServer nas duas listas (use o **+** se ele não aparecer) e **reabra o app** depois de mudar as permissões.

> Depois de recompilar, o macOS pode tratar o app como "novo". Se o mouse parar de funcionar após uma atualização, remova o app das listas com **−** e adicione de novo.

### 6. Firewall (somente se o Mac for o servidor)

Se o firewall do macOS estiver ligado (*Ajustes → Rede → Firewall*), permita conexões de entrada para o app quando o sistema perguntar.

---

## Primeira configuração

### No servidor (computador com o teclado e o mouse)

1. Abra o InputServer, escolha **Servidor** e clique em **Iniciar**.
2. Anote o **endereço IP** mostrado na janela (por exemplo `192.168.0.10`).
3. Quando um cliente novo tentar conectar, aparece a pergunta para confiar no certificado dele. Confira a impressão digital na tela do cliente e aceite.
4. O cliente é cadastrado automaticamente pelo nome que enviou. Sua tela é colocada à direita quando há espaço, ou em outra posição adjacente livre. Use **Configurar servidor** se quiser mudar a posição depois.

Não é necessário copiar nomes nem criar o monitor antes de conectar. O cadastro automático exige TLS com certificado do cliente e a configuração interna do servidor; arquivos de configuração externos continuam sendo administrados manualmente.

### No cliente (computador a ser controlado)

1. Abra o InputServer e escolha **Cliente**.
2. Ative **Encontrar servidor automaticamente** para abrir a janela de servidores disponíveis. Selecione o servidor pelo nome e IP e confirme; o botão **Escolher servidor** permite trocar a seleção. A lista é atualizada por UDP na rede local. Se nenhum servidor aparecer, cancele, desligue a descoberta e digite o **IP do servidor**.
3. Clique em **Iniciar**.
4. Na primeira conexão aparece a **impressão digital do servidor**. Confira se ela é igual à mostrada no servidor (*Editar → Configurações → impressão digital*) e aceite.

Pronto: leve o mouse até a borda configurada e ele passa para o outro computador.

## Uso no dia a dia

- **Iniciar/Parar:** botão principal da janela. Parar funciona também durante a conexão e cancela reconexões pendentes.
- **Ícone de acesso rápido:** o InputServer fica num ícone na bandeja do Windows (perto do relógio) ou na barra de menus do Mac. Um clique abre o mesmo menu nos dois sistemas: estado atual, **Iniciar/Parar**, **Função** (controlar outros computadores ou ser controlado), **Organizar telas…**, **Abrir InputServer**, **Mostrar log**, **Configurações…** e **Sair**. Fechar a janela mantém o app rodando no ícone; use **Sair** para encerrar tudo.
  > No Windows 11 o ícone pode ficar escondido na setinha **^** ao lado do relógio. Arraste-o para a barra para deixá-lo sempre visível.
- **Linha de comando (Windows):** controle a instância já aberta com
  `InputServer.exe --start`, `InputServer.exe --stop` e `InputServer.exe --quit`.
- **Iniciar com o sistema:** ative em *Editar → Configurações*.
- **Atalho para travar o cursor numa tela:** tecla **Scroll Lock** no servidor.

---

## Reduzindo o atraso (latência)

O atraso entre mover o mouse e o cursor andar no outro computador deve ser imperceptível (poucos milissegundos). Se estiver lento, verifique nesta ordem:

1. **Use cabo, principalmente no cliente.** Wi‑Fi é a maior fonte de atraso e "travadas". Se não der, use Wi‑Fi de 5 GHz e deixe os computadores perto do roteador.
2. **Mac no Wi‑Fi: desligue o AWDL.** O AirDrop/Handoff do Mac faz o Wi‑Fi trocar de canal periodicamente, o que causa engasgos. Para testar (volta ao normal ao reiniciar):
   ```bash
   sudo ifconfig awdl0 down
   ```
   AirDrop, Sidecar e Espelhamento ficam indisponíveis enquanto estiver desligado.
3. **Nível de log em INFO ou menor.** Em *Editar → Configurações*, os níveis DEBUG geram uma linha de log para cada movimento do mouse e deixam tudo lento.
4. **Modo de economia de energia.** Desde esta versão o InputServer pede ao sistema para não ser desacelerado em segundo plano:
   - **Windows 11:** o servidor desativa o "modo de eficiência" (EcoQoS) automaticamente.
   - **Mac:** o InputServer desativa o *App Nap* enquanto está conectado.
5. **VPN ou rede corporativa** no meio do caminho aumenta muito o atraso. Desligue a VPN ou conecte pela rede local.
6. **Teste a rede:** do cliente, rode `ping IP_DO_SERVIDOR`. Valores acima de ~5 ms numa rede local (ou variações grandes) indicam problema na rede, não no InputServer.

## Solução de problemas

| Sintoma | Causa provável | Solução |
|---|---|---|
| Cliente não conecta | Firewall ou rede "Pública" no servidor | Veja o passo do firewall do [Windows](#5-libere-o-firewall-somente-no-servidor) |
| Conecta, mas o cursor não passa para o outro lado | Posição das telas ou aprovação pendente | Confirme o certificado do cliente no servidor e confira a posição em **Configurar servidor** |
| No Mac o cursor não se move / teclas não funcionam | Falta permissão | Ative **Acessibilidade** (e **Monitoramento de Entrada** no servidor) e reabra o app |
| Pediu de novo para aceitar a impressão digital | Certificado do servidor mudou | Confira se é o mesmo servidor antes de aceitar |
| Não funciona na tela de login/UAC do Windows | O InputServer roda como usuário comum | Limitação conhecida desta versão |
| Atraso ou travadas | Rede/energia | Veja [Reduzindo o atraso](#reduzindo-o-atraso-latência) |

Os logs ficam na janela *Log* do InputServer e podem ser gravados em arquivo em *Editar → Configurações*.

---

## Roadmap

- [x] Servidor e interface Windows
- [x] Correções de latência (EcoQoS no Windows, App Nap no Mac)
- [x] Nome e protocolo próprios (InputServer)
- [x] Instalador Windows que atualiza sem reinstalar do zero
- [ ] Interface nova, mais simples
- [ ] **Áudio do cliente no fone do servidor**
- [ ] **Microfone do servidor como microfone do cliente**
- [ ] Atualização automática dentro do app (Windows e Mac)
- [ ] Assinatura e notarização do app Mac
- [ ] Instaladores publicados nas Releases do GitHub

Detalhes técnicos, decisões de projeto e resultados de testes: [DESENVOLVIMENTO.md](DESENVOLVIMENTO.md).

## Licença

Software livre sob a licença GPLv2 — veja [LICENSE](LICENSE). Os avisos de copyright dos autores do código original são mantidos, como a licença exige.
