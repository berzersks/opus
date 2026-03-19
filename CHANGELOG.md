# Changelog - Extensão Opus PHP

## [1.1.1] - 2026-03-19

### Fixed
- Adicionado suporte a OpenMP (libgomp) no config.m4
- Corrigido erro de linkagem com libsoxr compilada com OpenMP
- Removida duplicação de `#ifndef SOXR_LOW_LATENCY` em opus_channel.c
- Melhorada documentação e estrutura dos arquivos

### Added
- Script automático de correção (fix_opus_extension.sh)
- Changelog para rastreamento de mudanças
- Melhor tratamento de erros de compilação

### Technical Details
- O problema ocorria porque libsoxr foi compilada com suporte OpenMP
- As funções `omp_get_num_threads`, `GOMP_parallel`, etc. precisam de `-lgomp`
- A correção adiciona a biblioteca gomp tanto no teste quanto no link final
