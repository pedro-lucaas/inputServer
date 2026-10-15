# Ponte

Compartilhe teclado, mouse e área de transferência entre Windows e Mac.

Derivado do [Input Leap 3.0.2](https://github.com/input-leap/input-leap/tree/v3.0.2), com melhorias no controle de processos, botão Parar e instância única.

**Estado:** servidor Windows compilado e testado. O cliente Mac próprio ainda está em desenvolvimento; uma conexão de um cliente Mac com o servidor Windows já foi validada com TLS 1.3.

Veja [PONTE.md](PONTE.md) para compilação, recursos, limitações e resultados dos testes. A documentação original está em [README.upstream.md](README.upstream.md).

## Windows

Execute `Ponte.exe` com as DLLs e subpastas da distribuição. A interface controla o servidor diretamente, sem daemon. Fechar a janela pode manter o aplicativo na bandeja; use Sair para encerrar interface e servidor.

Os comandos `Ponte.exe --start`, `--stop` e `--quit` controlam a instância existente.

## Licença

GPL conforme [LICENSE](LICENSE), com os créditos e a exceção OpenSSL da base preservados. O Ponte é uma derivação do código aberto do Input Leap.
