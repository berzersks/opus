# Build Notes - Extensão Opus

## Dependências

### Bibliotecas Obrigatórias
- **libopus** (>= 1.3): Codec de áudio Opus
- **libsoxr** (>= 0.1.3): Resampler de alta qualidade
- **libgomp**: Biblioteca GNU OpenMP (para paralelização)

### Compilação

```bash
# Instalar dependências (Ubuntu/Debian)
sudo apt-get install libopus-dev libsoxr-dev

# Compilar extensão
phpize
./configure --enable-opus
make
sudo make install
```

## Problemas Comuns

### 1. Erro: undefined reference to `omp_get_num_threads`
**Causa**: libsoxr compilada com OpenMP mas -lgomp não está sendo linkada
**Solução**: Certifique-se que config.m4 adiciona `-lgomp` (já corrigido nesta versão)

### 2. Erro: libsoxr not found or missing soxr_create()
**Causa**: Biblioteca não instalada ou em caminho não padrão
**Solução**: Instale libsoxr-dev ou especifique PKG_CONFIG_PATH

### 3. Erro: smart_string_* não encontrado
**Causa**: Header zend_smart_string.h não incluído
**Solução**: Adicione `#include "zend_smart_string.h"` no topo do arquivo .c
