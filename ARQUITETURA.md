# Pedaleira NAM

Processador de guitarra digital para uso ao vivo — engine de áudio realtime em C++/JUCE, Neural Amp Modeling e integração TONE3000. Validado primeiro em PC x86, portado depois para hardware dedicado.

> Documento de referência. Versão web com diagramas: `Pedaleira NAM` (artifact publicado em 2026-09-06).
> Rev. 0.1 — 2026-09-06

**Estratégia de desenvolvimento:** todas as Fases 0–5 rodam em PC x86 (Linux, ALSA/PipeWire, JUCE nativo). O hardware final (Radxa Cubie A7S + touchscreen) só entra na Fase 6, depois que o Audio Engine já estiver validado sem depender dele.

---

## 0. Resumo e classificação

| Componente | Existe? | ARM / Linux | Realtime | Licença | Uso comercial | Classificação |
|---|---|---|---|---|---|---|
| **NeuralAmpModelerCore** (`sdatkinson/NeuralAmpModelerCore`) | Sim, ativo | Sim — comprovado em RPi4/5 | Sim, engine própria (Eigen), sem depender de NPU | MIT | Livre | 🟢 JÁ EXISTE |
| **NeuralAudio** (`mikeoliphant/NeuralAudio`) | Sim, ativo | Sim — SIMD dedicado p/ RPi4 (128-bit) e RPi5 (256-bit) | Sim | MIT | Livre | 🟢 JÁ EXISTE |
| **RTNeural** | Sim, ativo | Sim, indireto — produção via AIDA-X no MOD Dwarf | Sim | BSD-3 (+ herda licença do backend Eigen/xsimd) | Livre | 🟢 JÁ EXISTE |
| **AIDA-X** | Sim, sem commit há ~1,8 ano | Sim — produção real (MOD Dwarf) | Sim | GPL-3.0 | Copyleft forte — não linkar em produto fechado | 🟡 REFERÊNCIA, NÃO DEPENDÊNCIA |
| **GuitarML / NeuralPi** | Sim, abandonado (1,5–4 anos sem commit) | Sim — RPi4 + Elk Audio OS | Sim | GPL-3.0 | Copyleft + sem manutenção | 🟠 ESTUDO DE CASO |
| **TONE3000 API** | Sim, oficial e documentada (OAuth2+PKCE) | N/A — serviço em nuvem | N/A — fora do caminho de áudio | Termos próprios por tone (T3K / CC / CC0) | **Não confirmado** p/ hardware embarcado comercial — exige contato prévio | 🟡 VIÁVEL, PENDÊNCIA CONTRATUAL |
| **NPU (RKNN / eIQ / VIP9000) para NAM** | NPU existe; aceleração de NAM, não | — | Sem evidência — falta Conv1D causal/dilatada nos runtimes | — | — | 🔴 NÃO USAR |
| **Elk Audio OS** | Sim, ativo | Sim — oficial só em RPi4 | Sim, sub-ms (Sushi/RASPA/TWINE) | Mista: AGPL-3.0 / GPL-3.0 / GPL-2.0 / MIT | Provavelmente exige licença comercial paga p/ produto fechado | 🟡 VIÁVEL, DECISÃO DE LICENCIAMENTO |
| **Radxa Cubie A7S** (hardware-alvo) | Sim, placa real (lançada fev/2026) | ARM64 sim; kernel é BSP vendor, não mainline | PREEMPT_RT não confirmado nesse SoC | Hardware | — | 🟠 APOSTA DE BRING-UP |
| **PREEMPT_RT mainline** (kernel ≥6.12) | Sim, desde set/2024 | Sim — x86 / ARM64 / RISC-V | Sim, mas exige validação por SoC | GPL-2.0 | Livre | 🟢 MADURO NO PC · NÃO CONFIRMADO NA A7S |

**Leitura direta:** o motor de inferência (NAM Core / NeuralAudio / RTNeural) está resolvido — MIT/BSD, ativo, já roda em ARM Cortex-A72/A76 sem NPU. Pendências reais: (1) acordo comercial da TONE3000 para hardware, (2) Elk Audio OS vs. stack própria ALSA+JACK2, (3) quanto o bring-up da Cubie A7S atrasa a Fase 6. Nenhuma bloqueia a Fase 0–5.

---

## A. Arquitetura completa

Duas linhas de execução, uma regra única: **a thread de áudio nunca espera a thread de controle.**

```
THREAD DE CONTROLE / REDE              │  THREAD DE ÁUDIO (REALTIME)
                                        │
TOUCHSCREEN UI                         │  GUITARRA
      │                                │      │
      ▼                                │      ▼
PRESET MANAGER ─────grafo pré-carregado┼──►  INPUT (ADC)
      │                                │      │
      ▼                                │      ▼
TONE3000 MANAGER                       │  AUDIO ENGINE (loop de blocos, sample-accurate)
      │                                │      │
      ▼                                │      ▼
MODEL REPOSITORY ───swap atômico───────┼──►  SIGNAL GRAPH
 (download, validação, cache)          │  (Gate→Comp→Drive→NAM→Cab→EQ→Delay→Reverb,
                                        │   ordem 100% reconfigurável)
      ▲                                │      │
      └──── CPU/clipping/meters ───────┼──── fila lock-free (SPSC)
            (fila lock-free)           │      ▼
                                        │  OUTPUT (DAC)
                                        │      │
                                        │      ▼
                                        │  AMPLIFICADOR / PA
```

A ponte entre os dois lados é sempre uma de três primitivas: **troca atômica de ponteiro** (modelos/grafos), **fila SPSC lock-free** (parâmetros e telemetria) ou **double buffering** (presets inteiros). Nenhuma outra forma de comunicação atravessa essa linha.

### Split / merge

O Signal Graph é uma caixa única porque a ordem interna é livre, mas a mesma engine suporta ramificação:

```
Serial:                          Split / Merge:
INPUT                             INPUT
  │                                 │
  ▼                              split
GATE + COMPRESSOR                 ├──────────────┐
  │                                ▼              ▼
  ▼                             AMP A          AMP B
DRIVE                              │              │
  │                                ▼              ▼
  ▼                             CAB A          CAB B
NAM AMP                            │              │
  │                                └──────┬───────┘
  ▼                                       ▼
CAB / IR                                merge
  │                                       │
  ▼                                       ▼
EQ + DELAY + REVERB                   OUTPUT
  │
  ▼
OUTPUT
```

---

## B. Hardware

### Fase 0–5 · Bancada de dev
- Plataforma: PC x86-64, Linux (qualquer distro com ALSA/PipeWire)
- Áudio: interface USB classe-2 (ex. Focusrite Scarlett Solo) p/ guitarra real
- Toolchain: JUCE nativo + CMake — mesmo código-fonte do alvo ARM
- Custo marginal: ≈ US$0 (máquina já existente) + interface de áudio se necessário

### Fase 6+ · Hardware alvo
- SBC: **Radxa Cubie A7S** — Allwinner A733 (2×A76 + 6×A55), NPU 3 TOPS **não utilizável para NAM**
- Áudio: **sem codec onboard confirmado** — exige HAT/placa I2S externa nível-instrumento
- Tela: USB-C c/ DisplayPort Alt-Mode (4Kp60) + header GPIO com função LCD; touch via controlador USB-HID
- Kernel: BSP vendor (5.15 / 6.6) — mainline e PREEMPT_RT ainda em bring-up comunitário

### Fallback documentado
Se o bring-up de áudio/RT na Cubie A7S não amadurecer a tempo: **Raspberry Pi 4/5** (kernel maduro, precedente real — NeuralPi, blog do autor do NAM, Zynthian) ou **Orange Pi 5 Plus / RK3588** (mais CPU, kernel mainline ainda incompleto em 2026).

| Critério | Radxa Cubie A7S | Raspberry Pi 5 | Orange Pi 5 Plus (RK3588) |
|---|---|---|---|
| CPU | 2×A76@2.0GHz + 6×A55@1.8GHz | 4×A76@2.4GHz | 4×A76@2.4GHz + 4×A55 |
| RAM | LPDDR5, 4–16GB | LPDDR4X, até 8GB | LPDDR4/4X, 4–32GB |
| Codec de áudio onboard | 🔴 não confirmado | 🟡 não, mas HiFiBerry testado | 🟡 não, I2S documentado |
| Kernel | 🟠 BSP, não-mainline | 🟢 maduro, mainline | 🟡 mainline em progresso |
| PREEMPT_RT | 🔴 não confirmado | 🟡 funciona, spikes sob estresse | 🟡 mainline desde 6.12, não validado |
| Precedente de áudio RT | 🔴 nenhum encontrado | 🟢 NeuralPi, blog NAM, Zynthian | 🔴 nenhum encontrado |
| Preço aprox. (2026) | US$25–44 | US$110–175 (alta de DRAM) | US$90–129 |

---

## C. Software

```
PedaleiraNAM/
├── Source/
│   ├── Engine/
│   │   ├── AudioEngine.{h,cpp}          # loop de blocos, device I/O, orçamento de CPU
│   │   ├── SignalGraph.{h,cpp}          # grafo serial/split/merge, reordenação em runtime
│   │   └── ParameterManager.{h,cpp}     # automação, smoothing, lock-free param queue
│   ├── Effects/
│   │   ├── EffectProcessor.h            # classe-base abstrata de todo processor
│   │   ├── GateProcessor.{h,cpp}
│   │   ├── CompressorProcessor.{h,cpp}
│   │   ├── OverdriveProcessor.{h,cpp}
│   │   ├── DistortionProcessor.{h,cpp}
│   │   ├── FuzzProcessor.{h,cpp}
│   │   ├── EQProcessor.{h,cpp}
│   │   ├── NAMProcessor.{h,cpp}         # wrapper realtime-safe sobre NeuralAudio/NAM Core
│   │   └── CabIRProcessor.{h,cpp}       # convolução particionada
│   ├── Models/
│   │   ├── ModelRepository.{h,cpp}      # biblioteca local, metadata, checksum
│   │   ├── ModelValidator.{h,cpp}
│   │   └── ModelLoader.{h,cpp}          # carrega fora da audio thread, entrega por swap atômico
│   ├── Tone3000/
│   │   └── Tone3000Manager.{h,cpp}      # OAuth2+PKCE, busca, download — módulo opcional/plugável
│   ├── Presets/
│   │   └── PresetManager.{h,cpp}        # serialização JSON, double-buffer de troca
│   ├── UI/
│   │   └── (Fase 7 — thread separada, lê estado via FIFO)
│   └── EffectRegistry.{h,cpp}           # factory: nome → std::unique_ptr<EffectProcessor>
└── Tests/                               # benchmarks de CPU/latência por processor isolado
```

### Contrato da classe-base

```cpp
// EffectProcessor.h — todo bloco da cadeia implementa este contrato
class EffectProcessor {
public:
    virtual ~EffectProcessor() = default;
    virtual void prepare(double sampleRate, int maxBlockSize, int numChannels) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer) = 0;   // nunca aloca, nunca bloqueia
    virtual void reset() = 0;

    void setBypassed(bool shouldBypass) noexcept { bypassed.store(shouldBypass); }
    bool isBypassed() const noexcept { return bypassed.load(); }

    virtual juce::RangedAudioParameterGroup* getParameters() = 0;
    virtual std::unique_ptr<juce::XmlElement> getState() const = 0;
    virtual void setState(const juce::XmlElement&) = 0;

private:
    std::atomic<bool> bypassed { false };
};
```

---

## D. Audio Engine (realtime)

**A thread de áudio nunca:**
- chama `malloc`/`new`/`free` depois do `prepare()`
- toma um mutex bloqueante (usa apenas estruturas lock-free ou `std::atomic`)
- faz I/O de rede, disco, ou log em arquivo
- chama funções com latência não-determinística (alocação de string, exceções, RTTI dinâmico)
- espera por qualquer resposta da thread de controle

| Block size | @ 48 kHz | @ 96 kHz | Uso recomendado |
|---|---|---|---|
| 32 samples | 0,67 ms | 0,33 ms | alvo final ao vivo, após profiling |
| 64 samples | 1,33 ms | 0,67 ms | alvo de produção padrão |
| **128 samples** | **2,67 ms** | 1,33 ms | **ponto de partida da Fase 1, em PC** |
| 256 samples | 5,33 ms | 2,67 ms | debug / profiling, nunca em produção |

Meta de latência round-trip (entrada → saída, incluindo driver): **< 10 ms**. Sample rate padrão: **48 kHz**; 96 kHz reservado para quando o CPU budget permitir sem sacrificar instâncias simultâneas de NAM.

---

## E. Fase 1 em detalhe

**Pedais:** Noise Gate, Compressor, Boost, Overdrive, Distortion, Fuzz, EQ — cada um um `EffectProcessor` independente, sem estado compartilhado entre instâncias.

**NAM — motor de inferência:** escolha primária **NeuralAudio** (MIT, SIMD dedicado p/ RPi4/RPi5, compatível com modelos `.nam` Standard/Lite/Feather/Nano), com **NeuralAmpModelerCore** (MIT, oficial) como referência/fallback. RTNeural para modelos LSTM leves. Todo processamento é CPU (NEON quando disponível) — nenhum depende de NPU.

**Cab / IR:** convolução **particionada** — IRs longas (>100ms) custam caro demais em convolução direta, e FFT "pura" introduz latência de bloco incompatível com uso ao vivo. `juce::dsp::Convolution` já expõe particionamento uniforme pronto.

**TONE3000 — pipeline:**

```
Tone3000Manager → ModelRepository → ModelValidator → ModelLoader → EffectRegistry
 (OAuth2+PKCE,      (biblioteca        (checksum,       (parse fora       (swap atômico
  busca, download)   local + cache)     formato)          da audio thread)  no Signal Graph)
```

Autenticação OAuth 2.0 + PKCE (`client_id` público + chave secreta server-side). Endpoints: `/tones`, `/models`, `/makes`, `/tags`, rate limit 100 req/min. **Pendência:** termos públicos não distinguem "app de software" de "hardware embarcado" — contato com `support@tone3000.com` é pré-requisito antes de comprometer marketing/arquitetura. `Tone3000Manager` é módulo plugável e opcional: o produto funciona sem ele (modelos carregados manualmente via `.nam`).

**Model Repository:**

```
/models
  /nam        # modelos NeuralAmpModeler (.nam / JSON)
  /aida-x     # referência, não usado em runtime por padrão (GPL)
  /ir         # respostas ao impulso de cabinet
/metadata     # id, origem, autor, checksum, tamanho, data
/presets      # JSON: cadeia + ordem + parâmetros + referências de modelo
/cache        # downloads temporários da TONE3000, nunca lidos pela audio thread
```

**Presets:** serializam cadeia (tipo + ordem), parâmetros, referência de modelo/IR (por checksum), e ID do tone na TONE3000 quando aplicável. Troca usa **double buffering**: próximo grafo montado e "aquecido" numa cópia inteira, trocado por ponteiro atômico — zero clique, zero silêncio.

---

## F. Roadmap

| Fase | Entrega | Hardware |
|---|---|---|
| 0 | Arquitetura + esqueleto JUCE/CMake | PC x86 |
| 1 | Audio Engine + Signal Graph + Pedais básicos + NAM + Cab/IR + TONE3000 + Presets | PC x86 |
| 2 | Delay + Reverb (algorítmico e convolution) | PC x86 |
| 3 | Modulações (chorus, flanger, phaser, tremolo, vibrato, rotary, ring mod) | PC x86 |
| 4 | Pitch (shifter, octaver, harmonizer, detune) | PC x86 |
| 5 | Looper, afinador, noise reduction, EQ/compressão avançados, routing avançado, MIDI, expression | PC x86 |
| 6 | Port para Radxa Cubie A7S: bring-up de kernel/RT, codec via HAT I2S, benchmarks reais | Cubie A7S |
| 7 | UI touch completa (sobre a fila lock-free já existente desde a Fase 1) | Cubie A7S + touchscreen |
| 8 | Footswitches, MIDI físico, expression pedal | Cubie A7S |
| 9 | Hardware customizado (PCB, chassis, fonte, EMI/EMC) | PCB próprio |
| 10 | Otimização e validação para uso ao vivo (soak test, cyclictest, shows reais) | Produto final |

A UI já nasce desenhada para touch desde a Fase 1 — o Audio Engine expõe estado via fila lock-free desde o primeiro commit, evitando reescrever a fronteira realtime na Fase 7.

---

## G. Benchmarks

Nenhuma fonte oficial publica número confiável de CPU/latência para NAM em ARM — os relatos encontrados são anedóticos de fórum. Os valores abaixo são **metas de projeto a validar empiricamente**, não citações da literatura.

| Cenário | CPU alvo | Xruns em 30 min |
|---|---|---|
| 1 · NAM sozinho | < 15% | 0 |
| 2 · NAM + IR (cab) | < 25% | 0 |
| 3 · NAM + 1 pedal | < 30% | 0 |
| 4 · Vários pedais (gate+comp+drive+eq) | < 40% | 0 |
| 5 · Dois amps em paralelo (split/merge) | < 55% | 0 |
| 6 · Cadeia completa | < 70% | 0 |

Métricas por cenário: CPU%, RAM, latência round-trip medida, xruns, temperatura do SoC, tempo de troca de preset (meta < 5 ms de silêncio/click). `cyclictest` roda em paralelo a todo soak test no hardware-alvo.

---

## H. BOM (estimativa inicial)

Preços em USD, set/2026 — sujeitos à volatilidade de DRAM que já afetou o Raspberry Pi 5 este ano.

| Item | Fase | Custo aprox. |
|---|---|---|
| PC de desenvolvimento | 0–5 | US$0 (existente) |
| Interface de áudio USB classe-2 | 0–5 | US$100–200 |
| Radxa Cubie A7S (8GB) | 6 | US$30–40 |
| HAT/placa codec de áudio I2S nível-instrumento | 6 | US$15–35 *(não confirmado — a validar eletricamente)* |
| Painel touchscreen + controlador USB-HID | 7 | US$25–60 |
| Footswitches, encoders, LEDs, enclosure, fonte | 8–9 | US$50–150 *(placeholder, detalhar na Fase 9)* |

---

## I. Riscos

| Severidade | Risco | Impacto | Mitigação |
|---|---|---|---|
| 🔴 Alto | Cubie A7S sem codec de áudio onboard confirmado, kernel BSP não-mainline, RT não validado | Pode atrasar a Fase 6 inteira | Estratégia PC-primeiro isola das Fases 0–5; RPi5/RK3588 como fallback documentado |
| 🔴 Alto | Termos da TONE3000 para hardware embarcado comercial não confirmados | Pode bloquear distribuição comercial | Contato direto com `support@tone3000.com`; `Tone3000Manager` opcional |
| 🟡 Médio | NPU do SoC-alvo inútil para NAM (sem Conv1D causal/dilatada) | Nenhum — restrição de design conhecida | Arquitetura já assume CPU-only |
| 🟡 Médio | PREEMPT_RT sem número de latência garantido em SBC ARM de baixo custo | Buffer size pode precisar subir em produção | `cyclictest` + soak test antes de fixar buffer final |
| 🟡 Médio | Código de referência (AIDA-X, GuitarML) é GPL-3.0 | Contaminação de licença se copiado literalmente | Usar só como referência; reimplementação própria sobre NAM Core/RTNeural (MIT/BSD) |
| 🟢 Baixo | Volatilidade de preço de DRAM | BOM pode variar da estimativa | Tratar BOM como faixa, revisar antes da Fase 9 |

---

## Fontes principais

- github.com/sdatkinson/NeuralAmpModelerCore · github.com/mikeoliphant/NeuralAudio · github.com/jatinchowdhury18/RTNeural
- github.com/AidaDSP/AIDA-X · mod.audio/aida-x
- tone3000.com/api · github.com/tone-3000/api
- elk-audio.github.io/elk-docs (licenças e hardware suportado)
- docs.radxa.com/en/cubie/a7s
- raspberrypi.com (product briefs BCM2711/BCM2712) · wiki.friendlyelec.com (datasheet RK3588)
- docs.kernel.org/core-api/real-time · wiki.mod.audio/wiki/Developers (stack ALSA+JACK2 do MOD Dwarf)
- docs.pipewire.org · docs.yoctoproject.org

Documento compilado a partir de pesquisa dirigida em fontes primárias. Onde uma fonte confiável não foi encontrada, isso está declarado explicitamente no texto — nenhum número, licença ou capacidade foi inventado.
