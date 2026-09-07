# AGENT.md — guia para agentes trabalhando neste repositório

Este arquivo existe para dar contexto a um agente de IA (Claude Code ou outro) que abrir este repositório sem ter participado das decisões anteriores. Leia isto antes de tocar em qualquer código.

A fonte de verdade da arquitetura é [`ARQUITETURA.md`](./ARQUITETURA.md) — este arquivo aqui é um resumo operacional, não uma duplicata. Se os dois divergirem, `ARQUITETURA.md` vence, e este arquivo deve ser corrigido.

---

## O que é este projeto

Uma pedaleira/processador de guitarra digital para uso ao vivo: hardware + software, C++/JUCE, com Neural Amp Modeling (NAM), pedais, cab/IR, e integração com a TONE3000. O objetivo final é um produto físico real, não um protótipo de demonstração.

**Estratégia de desenvolvimento em duas pernas:**
1. Tudo (Fases 0–5) é escrito e validado num **PC x86 Linux comum**, com JUCE nativo sobre ALSA/PipeWire.
2. Só na **Fase 6** o mesmo código é portado para o hardware-alvo: **Radxa Cubie A7S** (Allwinner A733), com fallback documentado para Raspberry Pi 5 / Orange Pi 5 Plus caso o bring-up do A733 não amadureça a tempo.

Isso significa: **nenhum código de Fase 0–5 deve assumir hardware ARM específico.** Se você é um agente escrevendo código nessa fase e sente vontade de otimizar para um SoC específico, pare — isso pertence à Fase 6.

## A regra que não se negocia: a fronteira realtime

Existem duas threads conceituais no sistema, e a comunicação entre elas é estritamente unidirecional em primitivas permitidas:

- **Thread de áudio (realtime):** guitarra → input → `AudioEngine` → `SignalGraph` → output. Processa em blocos de tamanho fixo, com orçamento de tempo em microssegundos.
- **Thread de controle/rede:** UI, `PresetManager`, `Tone3000Manager`, `ModelRepository`. Pode demorar o quanto precisar.

**A thread de áudio nunca:**
- aloca memória (`new`/`malloc`) fora do `prepare()`
- toma um mutex bloqueante
- faz I/O de rede, disco, ou log em arquivo
- chama qualquer coisa com latência não-determinística (exceções, RTTI dinâmico, strings)
- espera resposta da thread de controle

A ponte entre as duas threads é sempre uma destas três primitivas — nunca outra coisa:
1. **Troca atômica de ponteiro** — para modelos NAM e grafos de sinal completos
2. **Fila SPSC lock-free** — para parâmetros e telemetria (CPU%, clipping)
3. **Double buffering** — para troca de presets inteiros

Se uma mudança de código introduzir qualquer chamada bloqueante dentro de `AudioEngine::process()` ou de qualquer `EffectProcessor::process()`, é um bug de arquitetura, não um detalhe de implementação — rejeite ou corrija antes de prosseguir.

## Estrutura de diretórios (ver `ARQUITETURA.md` seção C para o contrato completo)

```
Source/
├── Engine/       # AudioEngine, SignalGraph, ParameterManager — a fronteira realtime vive aqui
├── Effects/      # EffectProcessor (classe-base) + cada pedal/NAM/cab como subclasse independente
├── Models/       # ModelRepository, ModelValidator, ModelLoader — nada aqui roda na audio thread
├── Tone3000/     # Tone3000Manager — módulo OPCIONAL e plugável, o produto funciona sem ele
├── Presets/      # PresetManager — serialização + double buffering de troca
└── UI/           # só a partir da Fase 7 — lê estado via FIFO lock-free, nunca chama o Engine direto
```

Todo novo efeito (pedal, modulação, delay, reverb, pitch, o que for) é uma nova subclasse de `EffectProcessor` em `Effects/`, registrada em `EffectRegistry`. Não crie caminhos especiais no `SignalGraph` para tipos específicos de efeito — o grafo não sabe (nem deve saber) a diferença entre um `GateProcessor` e um `NAMProcessor`.

## Status do roadmap (ver `ARQUITETURA.md` seção F)

| Fase | Status | Hardware |
|---|---|---|
| 0 — Arquitetura + esqueleto JUCE/CMake | **em andamento** | PC x86 |
| 1 — Audio Engine + Pedais + NAM + Cab/IR + TONE3000 + Presets | não iniciada | PC x86 |
| 2 — Delay + Reverb | não iniciada | PC x86 |
| 3 — Modulações | não iniciada | PC x86 |
| 4 — Pitch | não iniciada | PC x86 |
| 5 — Looper, afinador, MIDI, routing avançado | não iniciada | PC x86 |
| 6 — Port para Radxa Cubie A7S | não iniciada | Cubie A7S |
| 7 — UI touch completa | não iniciada | Cubie A7S + touchscreen |
| 8–10 — Footswitches, PCB, validação final | não iniciada | hardware final |

Atualize esta tabela quando uma fase for concluída — não deixe ela ficar desatualizada silenciosamente.

## Decisões já tomadas (não reabrir sem motivo novo)

- **Motor de NAM:** `NeuralAudio` (mikeoliphant, MIT) como backend primário — já tem SIMD dedicado para RPi4/RPi5. `NeuralAmpModelerCore` (MIT, oficial) como referência/fallback. **Não usar AIDA-X ou código GuitarML diretamente** — ambos GPL-3.0, contaminação de licença em produto fechado. Usar apenas como referência de arquitetura, se necessário.
- **NPU:** não conte com ela para NAM. RKNN/eIQ/VIP9000 não têm suporte confirmado a Conv1D causal/dilatada — todo o processamento de NAM é CPU (NEON quando disponível).
- **Convolução de IR:** particionada, via `juce::dsp::Convolution` como ponto de partida.
- **TONE3000:** integração via API oficial (OAuth2+PKCE), mas como módulo **opcional** em `Tone3000/` — o uso comercial em hardware embarcado ainda não tem confirmação contratual (ver riscos em `ARQUITETURA.md` seção I). Não acoplar nenhuma feature central do produto a essa integração.
- **Sample rate/buffer padrão da Fase 1:** 48 kHz, bloco de 128 samples (~2,7 ms) — vai apertando conforme o profiling permitir, meta final de round-trip < 10 ms.

## Convenções de código

- C++/JUCE, CMake como build system.
- Sem comentários explicando o óbvio. Comente apenas a razão não-óbvia (ex.: por que um lock-free queue tem esse tamanho específico, por que um cast unsafe é seguro aqui).
- Toda classe de efeito implementa o contrato completo de `EffectProcessor` (ver `ARQUITETURA.md` seção C) — sem exceções parciais.
- Testes de benchmark isolado por processor ficam em `Tests/`, não misturados com testes funcionais.

## Onde ficam as pendências e riscos

Não repita a análise de risco aqui — ela vive em `ARQUITETURA.md` seção I e é mantida lá. Se você, como agente, encontrar um risco novo durante a implementação, adicione-o na tabela de riscos do `ARQUITETURA.md`, não neste arquivo.
