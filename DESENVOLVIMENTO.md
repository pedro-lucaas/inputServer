# InputServer 0.1.0 — notas de desenvolvimento

Detalhes técnicos, decisões de projeto e estado da validação. Para instalar e
usar, veja o [README](README.md).

## Estado

- Servidor e interface Windows x64 compilados e testados em Windows 11 antes
  da troca de nome. O build com o nome InputServer ainda não foi validado.
- O aplicativo macOS ainda precisa ser compilado e validado em um Mac.
- Ainda não transmite áudio.

## Executáveis

| Papel | Windows | macOS (dentro de `InputServer.app/Contents/MacOS`) |
|---|---|---|
| Interface | `InputServer.exe` | `InputServer` |
| Motor servidor | `inputserver-server.exe` | `inputserver-server` |
| Motor cliente | `inputserver-client.exe` | `inputserver-client` |

A interface é dona do processo do motor (sem daemon/serviço). No Windows, um
Job Object encerra o motor se a interface for finalizada à força.

## Protocolo e compatibilidade

- A saudação do protocolo é `InputServer` (`kMsgHello`/`kMsgHelloBack` em
  `src/lib/inputleap/protocol_types.cpp`). **Clientes de outros programas
  baseados no mesmo protocolo não conectam mais**: servidor e clientes
  precisam ser InputServer.
- A conexão usa TLS; o certificado fica em `<perfil>/SSL/InputServer.pem`.
- Descoberta automática própria via UDP (`src/gui/src/UdpDiscovery.cpp`),
  sem Bonjour/mDNS: o cliente envia uma consulta JSON por broadcast para a
  porta UDP 24801 (porta TCP + 1) a cada 2 s; o servidor responde direto ao
  remetente com nome e porta TCP. Só o servidor precisa liberar UDP 24801 no
  firewall. A descoberta não é autenticada; quem garante a identidade do
  servidor é a verificação da impressão digital TLS ao conectar.

## Aprovação e cadastro de clientes

O servidor da interface exige certificado de cliente em conexões TLS. Um certificado desconhecido gera a pergunta de confiança; rejeitar mantém o cliente bloqueado e suprime novas perguntas para o mesmo certificado durante a sessão. Depois da aprovação, o cliente tenta conectar novamente.

Quando um cliente autenticado ainda não tem tela na configuração, o motor emite um registro com nome em hexadecimal e SHA256 do certificado efetivamente autenticado. A interface confere novamente a impressão digital no banco de clientes confiáveis, cadastra a tela numa célula adjacente livre, salva e reinicia o motor. Não há cadastro baseado em anúncios UDP ou em conexões sem autenticação. Configurações externas são mantidas manualmente.

A aprovação é persistida pelo certificado; mudar o certificado exige nova aprovação. Clientes existentes não são duplicados nem movidos. Os testes locais cobrem persistência, duplicação, nomes inválidos, grade cheia e o formato do registro. A conexão completa com um servidor Windows atualizado ainda precisa ser validada em Windows.

## Dados e configurações

| | Windows | macOS |
|---|---|---|
| Configurações da interface | `HKCU\Software\InputServer\InputServer` | `~/Library/Preferences/com.github.InputServer.plist` |
| Perfil (certificados, impressões digitais) | `%LOCALAPPDATA%\InputServer` | `~/Library/Application Support/InputServer` |
| Configuração do servidor | `inputserver.sgc` | `inputserver.conf` |

Não há importação de configurações de outros programas: a primeira execução
começa do zero e gera um certificado novo.

## Mudanças em relação à base

- Interface única por usuário; segunda abertura traz a existente à frente.
- Iniciar não cria outro servidor enquanto o atual está ativo.
- Parar funciona também durante a conexão e cancela a reconexão pendente.
- Sair encerra o motor; fechar a janela pode mantê-la na bandeja.
- Comandos `InputServer.exe --start`, `--stop` e `--quit` controlam a
  instância existente via canal local restrito ao usuário.
- Latência: no Windows o motor desativa o throttling de energia (EcoQoS)
  via `SetProcessInformation(ProcessPowerThrottling)`; no macOS mantém uma
  atividade `NSActivityLatencyCritical` enquanto a tela está habilitada,
  evitando App Nap e coalescência de timers. O trecho Windows foi compilado e
  executado isoladamente em Windows 11; o trecho macOS precisa ser validado.
- Nome InputServer em tudo que o usuário vê: janelas, mensagens, logs,
  executáveis, propriedades dos arquivos, pacote do Mac e instalador.
  Identificadores internos do código (namespace `inputleap`, pastas
  `src/lib/inputleap`, classes como `QInputLeapApplication`, macros
  `INPUTLEAP_*`) foram mantidos de propósito para reduzir o risco da mudança.

O modo de usuário não oferece acesso à tela de login/UAC do Windows; esse
cenário está fora desta versão.

## Compilar

Os passos completos estão no [README](README.md#instalação-no-windows). Resumo
para Windows, em um "x64 Native Tools Command Prompt for VS 2022":

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.6.3\msvc2019_64 -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64" -DCMAKE_INSTALL_PREFIX=dist-inputserver
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build
```

Instalador Windows (Inno Setup 6): depois do `cmake --install`,

```bat
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" build\installer-inno\inputserver.iss
```

gera `build\installer-inno\bin\InputServerSetup-<versão>.exe`. O `AppId` do
instalador nunca deve mudar: é por ele que uma nova versão encontra e
atualiza a instalação existente.

## Testes de ciclo de vida

```text
cmake -S tests/lifecycle -B lifecycle-build -DCMAKE_PREFIX_PATH=CAMINHO_DO_QT
cmake --build lifecycle-build --config Release
ctest --test-dir lifecycle-build -C Release --output-on-failure
```

Testam duplicação, liberação do lock, entrega do comando Parar e, no Windows,
encerramento exclusivo do filho vinculado ao Job Object. O workflow
`.github/workflows/lifecycle.yml` roda esses testes em Windows e macOS.

## Licença

GPLv2 conforme [LICENSE](LICENSE). Os avisos de copyright dos autores do
código original permanecem nos arquivos-fonte, no LICENSE e na janela Sobre,
como a licença exige.

Não versione certificados privados, configurações pessoais, credenciais ou
logs reais. A pasta `ext/gtest` usa a revisão
`15460959cbbfa20e66ef0b5ab497367e47fc0a04`.
