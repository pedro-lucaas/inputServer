# Nota histórica: primeira revisão

O estado atual está documentado em PONTE.md. O texto abaixo descreve a etapa anterior à compilação Windows.

# Versão experimental baseada no Input Leap

## Estado real

Primeira revisão de código, ainda NÃO compilada nem validada em Windows ou macOS.
Não é um aplicativo novo concluído e não contém instaladores.
Não substitui nem modifica o Input Leap instalado nesta máquina.

Base completa: https://github.com/input-leap/input-leap, tag v3.0.2,
commit 7e5889dc6fc907a0dd218d94623378cc53417cb2.
Mantém o motor, protocolo, TLS, configurações e recursos originais.
É uma derivação do código aberto, não uma implementação independente do zero.
Licença original e créditos preservados em LICENSE e nos arquivos.

## Mudanças desta revisão

- Uma interface por usuário: segunda abertura tenta trazer a existente à frente.
- Proteção contra iniciar outro processo enquanto o atual ainda executa.
- Parar disponível também durante a conexão; Iniciar desabilitado nesse estado.
- Reconexão com temporizador cancelável ao clicar em Parar.
- Encerramento do processo filho com prazo, seguido de kill somente no processo
  pertencente à interface, se necessário; erro exposto se continuar ativo.
- Sair solicita parada também no modo de serviço do Windows.
- Estado exibido no tooltip do ícone da bandeja.

## Limitações e pendências

- Não encerra instâncias antigas já abertas pelo aplicativo original.
- O lock protege apenas esta revisão; executáveis antigos não conhecem o lock.
- O protocolo IPC do serviço ainda não confirma que o processo realmente parou.
  A entrega de Stop ao serviço ao sair precisa de teste e confirmação adicional.
- O ciclo de vida do daemon Windows, inclusive múltiplos servidores observados
  neste PC, ainda exige investigação. Não há causa comprovada para esses processos.
- Inicialização automática e recuperação do ícone após reinício do Explorer
  ainda precisam de validação prática nas duas plataformas.
- Nenhuma promessa de resolver latência ou engasgos de rede.
- Nome, ícones e instaladores ainda são os originais da base; não há identidade
  ou pacote de instalação próprio nesta etapa.

## Compilação

Requer CMake >= 3.21, compilador C++17, Qt6 (Core, Widgets, Network,
LinguistTools), OpenSSL e dependências descritas no README original.
No Windows, use Visual Studio com C++ Desktop e Bonjour SDK compatível.
No macOS, use Xcode Command Line Tools e SDK/dependências nativos.
O macOS deve ser compilado e testado em um Mac; esta máquina é Windows.

Para inicializar os testes originais, dentro do diretório fonte:

```text
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Os parâmetros de Qt/OpenSSL/Bonjour variam com sua instalação. Use também
as receitas da base em .github/workflows/builds.yml; seus runners e versões
antigos precisam ser atualizados antes de usar essa receita em CI.
O ZIP distribuído não contém metadados Git, mas inclui o conteúdo do submódulo
gtest na revisão esperada pela base. No ZIP, omita o comando git submodule.

Os testes novos de instância única são independentes de OpenSSL/Bonjour/gtest:

```text
cmake -S tests/lifecycle -B lifecycle-build
cmake --build lifecycle-build --config Release
ctest --test-dir lifecycle-build -C Release --output-on-failure
```

Foi incluído .github/workflows/lifecycle.yml para executar esses testes no
Windows e no macOS. Não foi publicado nem executado nesta sessão.

## Validação necessária antes de instalar

1. Compilar e executar os testes nas duas plataformas.
2. Abrir duas vezes: somente uma interface, a primeira é trazida à frente.
3. Conectar ao original 3.0.2, validar certificado, teclado, mouse e clipboard.
4. Clicar em Parar durante conexão, funcionamento e espera de reconexão:
   processo filho encerra e não reaparece após alguns segundos.
5. Simular processo que ignora término: encerrar dentro do prazo e registrar log.
6. Em Windows/serviço, confirmar externamente o fechamento da porta ao parar/sair.
7. Reiniciar o sistema e o Explorer e verificar ícone, estado e início automático.
8. No Mac, verificar permissões de Acessibilidade e Monitoramento de Entrada;
   assinatura/notarização e pacote final permanecem pendentes.

