# 📦 PHP Opus Extension

Uma extensão nativa para PHP que fornece codificação e decodificação de áudio usando o codec [Opus](https://opus-codec.org/), com uma interface orientada a objetos de alto desempenho.

---

## 🎧 Sobre o Codec Opus

**Opus** é um codec de áudio aberto, versátil e gratuito (sem royalties), padronizado pela [IETF (RFC 6716)](https://datatracker.ietf.org/doc/html/rfc6716). Ele é altamente eficiente para:

- Áudio em tempo real (VoIP, videoconferência)
- Streaming de música e voz
- Armazenamento de áudio de alta qualidade
- Aplicações de baixa latência

---

## ✨ Funcionalidades da Extensão

- ✅ Codifica áudio PCM 16-bit linear para Opus  
- ✅ Decodifica frames Opus de volta para PCM  
- ✅ Suporte a taxas de amostragem variáveis (8kHz, 16kHz, 48kHz etc)  
- ✅ Suporte a mono e estéreo  
- ✅ Interface PHP moderna orientada a objetos  
- ✅ Ideal para aplicações VoIP e áudio em tempo real  

---

## 📂 Estrutura

```
├── config.m4                # Configuração de build via phpize
├── php_opus.h              # Header principal
├── opus.c                  # Entry point da extensão
├── opus_channel.c          # Implementação da classe opusChannel
└── README.md               # Este arquivo
```

---

## 🛠️ Requisitos

- PHP **7.4+** ou **PHP 8.x**
- `libopus-dev` instalado no sistema
- Ferramentas de desenvolvimento PHP (phpize, php-config)
- Um compilador C (gcc, clang)

---

## 🧪 Instalação (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install php-dev libopus-dev build-essential
```

---

## ⚙️ Compilando a Extensão

```bash
phpize
./configure --enable-opus
make -j$(nproc)
sudo make install
```

E adicione ao seu `php.ini`:

```ini
extension=opus.so
```

---

## ✅ Exemplo de uso

```php
<?php

$opus = new opusChannel(16000, 1); // 16kHz mono

$pcm = file_get_contents('input.pcm');
$encoded = $opus->encode($pcm);

$decoded = $opus->decode($encoded);
file_put_contents('output.pcm', $decoded);
```

---

## 📄 Licença

MIT License. Desenvolvido por [@berzersks](https://github.com/berzersks).
