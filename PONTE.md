# Ponte 0.1.0 — desenvolvimento

Compartilhamento de teclado, mouse e área de transferência, derivado do
[Input Leap v3.0.2](https://github.com/input-leap/input-leap/tree/v3.0.2),
commit `7e5889dc6fc907a0dd218d94623378cc53417cb2`.

## Estado

Servidor e interface Windows x64 compilados e testados em Windows 11.
Uma conexão de um cliente Mac foi confirmada com TLS 1.3.
O aplicativo macOS próprio ainda precisa ser compilado e validado em um Mac.
O protocolo foi preservado para compatibilidade com os clientes originais.
Esta versão não transmite áudio e não promete corrigir problemas da rede.

## Mudanças

- Nome da interface Windows: Ponte; executáveis `Ponte.exe`,
  `ponte-server.exe` e `ponte-client.exe`.
- Interface única por usuário; segunda abertura traz a existente à frente.
- Iniciar não cria outro servidor enquanto o atual está ativo.
- Parar funciona também durante a conexão e cancela a reconexão pendente.
- A interface Windows é proprietária do processo servidor, sem daemon.
- Job Object do Windows encerra o processo filho se a interface for morta.
- Sair encerra o servidor; fechar a janela pode mantê-la na bandeja.
- Comandos locais `Ponte.exe --start`, `--stop` e `--quit` controlam a
  instância existente via canal restrito ao usuário.
- Configurações da interface separadas em `HKCU\Software\Ponte\Ponte`,
  com importação inicial do Input Leap. O perfil SSL original é preservado
  para manter a confiança dos clientes existentes.
- Ícones, traduções e alguns textos internos ainda são herdados da base.

O modo de usuário não oferece o mesmo acesso à tela de login/UAC que um
serviço privilegiado. Esse cenário permanece fora da validação desta versão.
O canal local confirma recebimento do comando; o encerramento do processo
deve ser confirmado pelo estado da interface ou externamente.

## Compilar no Windows

Dependências usadas: Visual Studio Build Tools 2022 / MSVC 19.44, SDK Windows
10.0.26100, CMake 3.31.10, Ninja, Qt 6.6.3 MSVC2019 x64, OpenSSL 3.5.9 com
bibliotecas estáticas MD e Bonjour SDK compatível x64.

Em um Developer Command Prompt x64, configure os caminhos reais:

```bat
set BONJOUR_SDK_HOME=C:\deps\BonjourSDKLike
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.6.3\msvc2019_64 -DOPENSSL_ROOT_DIR=C:\deps\OpenSSL -DOPENSSL_SSL_LIBRARY=C:\deps\OpenSSL\lib\VC\x64\MD\libssl_static.lib -DOPENSSL_CRYPTO_LIBRARY=C:\deps\OpenSSL\lib\VC\x64\MD\libcrypto_static.lib -DDNSSD_LIB=C:\deps\BonjourSDKLike\Lib\x64\dnssd.lib -DCMAKE_INSTALL_PREFIX=dist-ponte
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
cmake --install build
```

O motor e o cliente são compilados junto à interface. O daemon herdado
também pode ser produzido pelo build original, mas o Ponte não o utiliza.

## Testes novos

```text
cmake -S tests/lifecycle -B lifecycle-build -DCMAKE_PREFIX_PATH=CAMINHO_DO_QT
cmake --build lifecycle-build --config Release
ctest --test-dir lifecycle-build -C Release --output-on-failure
```

Testam duplicação, liberação do lock, entrega do comando Parar e, no Windows,
encerramento exclusivo do filho vinculado ao Job Object.
O workflow `lifecycle.yml` está preparado para Windows e macOS. O resultado
local em Windows não comprova o resultado em macOS ou no GitHub Actions.
O workflow completo herdado está arquivado em `.github/legacy-builds.yml`;
seus runners/dependências precisam ser atualizados antes de reativá-lo.

## Validação realizada em Windows

- 131 testes unitários, 58 de interface e 26 de integração passaram.
- Um teste de layout japonês foi ignorado porque o layout não está instalado.
- Quatro cenários adicionais de ciclo de vida passaram.
- Teste real: uma interface e um servidor, Parar/Iniciar e encerramento
  forçado da interface sem deixar servidor órfão.
- Handshake TLS 1.3 com validação do certificado original e saudação de
  protocolo `Barrier`; hash do certificado original permaneceu inalterado.
- Cliente macOS conectado ao servidor Windows na porta TCP 24800.

Foram corrigidas premissas dos testes herdados: arquivos temporários de
QSettings usam IniFormat, `--daemon` é esperado como não suportado em Windows,
e o teste japonês exige um layout instalado de verdade.

## Trabalho no Mac

Use esta mesma revisão. A arquitetura e o protocolo permanecem os originais.
Consulte `README.upstream.md` para dependências da plataforma e receitas em
`.github/legacy-builds.yml` como referência. Renomear o bundle, ajustar o
Info.plist, testar permissões, assinatura e notarização são tarefas pendentes
do cliente Mac. Não altere os nomes de protocolo ou desative TLS.

## Licença e segurança dos arquivos

GPL conforme `LICENSE`, com a exceção OpenSSL da base. Créditos e avisos
originais foram preservados; isto é uma derivação, não uma criação do zero.
Bibliotecas de terceiros mantêm suas respectivas licenças.

Não versione certificados privados, configurações pessoais, credenciais,
logs reais do usuário ou ferramentas baixadas. Os ZIPs de distribuição não
devem conter o perfil SSL. A pasta `ext/gtest` usa a revisão original
`15460959cbbfa20e66ef0b5ab497367e47fc0a04`.
