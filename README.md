# PHP Opus Extension

- Suporte a taxas de amostragem variáveis (8kHz, 16kHz, 48kHz etc)  
- Suporte a mono e estéreo  
- Interface PHP moderna orientada a objetos  
- Ideal para aplicações VoIP e áudio em tempo real
- 
---

## Requisitos

- PHP **7.4+** ou **PHP 8.x**
- `libopus-dev` instalado no sistema
- Ferramentas de desenvolvimento PHP (phpize, php-config)
- Um compilador C (gcc, clang)

---

## Instalação (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install php-dev libopus-dev build-essential
phpize
./configure --enable-opus
make -j$(nproc)
sudo make install
```

Adicione em `php.ini` a linha:

```ini
extension=opus.so
```

---

## STUBS e exemplo:

```php
class opusChannel {
    public function __construct(int $sample_rate, int $channels) {
    }

    public function encode(string $pcm_data, int $pcm_rate): string {
        return "";
    }

    public function decode(string $encoded_data, int $pcm_rate_out): string {
        return "";
    }

    public function setBitrate(int $value) {
    }

    public function setVBR(bool $enable) {
    }

    public function setComplexity(int $value) {
    }

    public function setDTX(bool $enable) {
    }

    public function setSignalVoice(bool $enable) {
    }

    public function reset() {
    }

    public function destroy() {
    }

}
$opus = new opusChannel(48000, 1);
$opus->setBitrate(8000);
$opus->setSignalVoice(true);
$opus->setComplexity(8);
$opus->setDTX(true);
$opus->setVBR(true);

$pcm = file_get_contents('input.pcm');

// a entrada pcm é 8000Hz
$encoded = $opus->encode($pcm, 8000);

// receberemos um pcm de 8000Hz
$decoded = $opus->decode($encoded, 8000);
file_put_contents('output.pcm', $decoded);
```

---


Desenvolvido por [@berzersks](https://github.com/berzersks).
