# TAS-ELRS v2 — Enlace C2 endurecido baseado no ExpressLRS 4.1.0

> **Licença:** o fork como um todo é **GPL-3.0** (herdado do ExpressLRS) · os nossos ficheiros totalmente originais (`src/lib/TAS/*`, docs, testes) estão adicionalmente sob **MIT** — detalhes em `NOTICE` e na secção *Licença* abaixo.

**O que é o TAS-ELRS?** Um enlace de rádio de código aberto e endurecido para controlo de drones, desenhado para ambientes de guerra eletrónica: comandos RC cifrados (ChaCha20), anti-replay, deteção de interferência reportada ao controlador de voo, rotação determinística de canais FHSS e espera por meses com despertar autenticado. Nasceu como fork do ExpressLRS 4.1.0.

> **Idiomas:** [Polski](../TAS_README.md) · [English](TAS_README_EN.md) · [Українська](TAS_README_UK.md) · [Español](TAS_README_ES.md) · **Português** · [العربية](TAS_README_AR.md)

> Documento escrito da perspectiva de uma equipa de engenharia que trabalha
> com drones e guerra eletrónica na Ucrânia desde 2022: RF/firmware,
> criptografia, formação de operadores, reparações de campo. Cada decisão de
> design deste repositório deriva de uma lição de campo concreta (secção 1),
> com referência ao código que a implementa.

**Âmbito do projeto: apenas a camada de comunicações (enlace C2 + telemetria).**
Sem cargas úteis, sem armamento, sem guiamento nem localização de alvos.
A conformidade regulamentar de rádio (ETSI EN 300 220/328, FCC Part 15) é
herdada do ExpressLRS e mantém-se obrigatória; a seleção de região é permanente.

---

## 1. Lições de campo 2022–2026 → requisitos → código

### L1. A deteção mata mais rápido que a interferência
O perigo não é o jammer — é o detetor de emissões combinado com radiogoniometria
e artilharia/drones kamikaze. Um operador que permaneça demasiado tempo numa
posição com o transmissor ativo torna-se ele próprio um alvo.
**Cada segundo de emissão tem um custo.**

Implementação:
- Perfil STEALTH (secção 5): telemetria OFF, potência mínima que mantenha o
  enlace, preâmbulos curtos;
- Sem campos constantes em texto claro pelo ar;
- Verificado upstream: o TX não emite sem handset ligado
  (`tx_main.cpp`: `UARTdisconnected() → hwTimer::stop()`).

### L2. Popularidade = assinatura registada
O ExpressLRS é o protocolo hobby mais popular do mundo; o seu sync-word,
preâmbulo LoRa e estrutura de tramas estão há anos nas tabelas de qualquer
scanner. O ELRS stock apresenta-se sozinho antes de fazer nada. Ter FHSS não
chega: o adversário interfere seletivamente os protocolos que reconhece.

Implementação: MAC com chave em vez da semente CRC estática derivada do UID;
cifragem de todo o payload RC; sem bytes constantes identificáveis; domínio
`"wake"` separado do domínio de dados (`TasWake.cpp`, `TasOta.cpp`).

### L3. A interferência é seletiva ou de banda larga — planeie para ambas
Classes descritas publicamente: cortinas de banda larga sobre bandas inteiras,
jammers varredores (sweep) e jammers orientados a protocolos conhecidos.
Sub-GHz costuma estar mais limpo mas oferece menos canais; 2,4 GHz está
saturado por drones próprios e Wi-Fi.

Implementação: classificação CLEAN/SUSPECT/WIDEBAND/SWEEP reportada ao FC
(`TasAfh.cpp`, trama CRSF 0x34); poda determinística por época dos canais FHSS
(anti-follow: quem aprendeu a sua sequência perde-a a cada época); dual-band
failover LR1121 — Fase B.

### L4. O hardware é consumível. A logística vence o desempenho
Os drones não voltam. Os RX são comprados a granel por voluntários. Se o seu
BOM exige peças com seis semanas de envio, o projeto está morto.

Implementação: metas de BOM (RX < $12, TX < $20 @ 100 un.), mínimo de duas
fontes independentes por componente, compilação sobre placas ELRS existentes
(sem hardware novo para a v1).

### L5. O inverno dura mais que o calendário
−20 °C ao vento, LiPo perdendo 30–40 % de capacidade, condensação ao entrar
no calor. O brownout a 3,0 V/célula deve ser seguro, não aleatório.

Implementação: critérios de aceitação −25…+55 °C (secção 7), reinício seguro
do rádio ante brownout (watchdog), testes de câmara como portão da Fase B.

### L6. O operador teve dois dias de formação e não lerá um manual numa trincheira
A configuração deve ser por presets (QR/ficheiro), offline, determinista, em
polaco e ucraniano. Nada de "definições avançadas" em campo.

Implementação: presets operacionais (secção 5) como especificação para
portal/Lua; diagnóstico por códigos LED (autocolante na caixa) — GUI Fase B.

### L7. Há vinte drones no ar e todos usam ELRS
Planeamento de espetro e isolamento de sessões são requisitos: o cross-talk
entre pares perde ambos os aparelhos.

Implementação: chave de sessão única por par derivada do material de binding
(HKDF sobre UID); MAC com chave que liga cada trama ao seu canal — tramas de
outro TX falham a validação (teste `tas: cross-slot injection rejected`).
Plano de espetro por unidade — Fase B.

### L8. O momento mais perigoso de uma missão é perder o enlace com FPV ativo
Um acelerador congelado é um presente para o adversário. O RX não deve
"recordar" os últimos valores por mais de 150 ms; o failsafe decide-o o FC.

Implementação: semântica failsafe stock preservada + anti-replay (duplicados
rejeitados nunca refrescam o estado de comandos) + watchdog do rádio
(`TasFailsafe.cpp`): silêncio > 500 ms com enlace vivo → reinit do chip,
contador visível na telemetria do FC.

### L9. Não há internet. As atualizações viajam em pendrive e BLE do telemóvel
Implementação: pipeline PlatformIO stock por target + exigência de artefactos
assinados — Fase B (manifestos ed25519). v1: distribuição offline (secção 10).

### L10. Um drone pode esperar semanas até ser necessário
A bateria de um drone à espera morre mais depressa que a sua eletrónica. A opção
"dormir até um mês, acordar apenas com o sinal explícito do TX emparelhado" é
uma necessidade real de armazém.

Implementação: deep sleep do RX + CAD com ciclo de trabalho + token de
despertar autenticado no domínio `"wake"` (`TasWake.cpp`). Orçamentos:
3000 mAh → ~44 dias, 5000 mAh → ~73 dias (poll 3 s). HAL por placa — Fase B
(`#error` deliberado sem hooks para ninguém enviar uma função de papel).

---

## 2. Estado de implementação

| Funcionalidade | Estado | Código |
|---|---|---|
| Cifragem ChaCha20 (RFC 8439) do payload RC | ✅ v1 | `TasCrypto.cpp`, `TasOta.cpp` |
| MAC por trama com chave (canal+nonce+época) | ✅ v1 | `TasSession.cpp` |
| HKDF-SHA256 do material de binding | ✅ v1 | `TasSession.cpp` |
| Anti-replay (janela monótona de nonce) | ✅ v1 | `TasSession.cpp`, `rx_main.cpp` |
| Classificação de interferência → FC | ✅ v1 | `TasAfh.cpp`, `TasTelemetry.cpp` |
| Poda FHSS determinística (anti-follow) | ✅ núcleo | `TasAfh.cpp` |
| Watchdog do rádio (reinit após >500 ms de silêncio) | ✅ v1 | `TasFailsafe.cpp` |
| Telemetria TAS_STATUS 0x34 | ✅ v1 | `TasTelemetry.cpp` |
| Deep sleep + token de despertar | ✅ lógica / ⚠ HAL | `TasWake.cpp` |
| Rotação de chaves em voo | 🔜 B (contador de época nos 4 bytes livres SYNC OTA8) | — |
| AFH por medição TX↔RX, dual-band failover | 🔜 B | — |
| OTA assinado (ed25519), secure boot | 🔜 B | — |
| Relay camada PHY | 🔜 C | — |

## 3. Modelo de ameaças

| # | Ameaça | Impacto sem mitigação | Mitigação v1 | Risco residual |
|---|---|---|---|---|
| T1 | Scanner de assinaturas → DF → fogo sobre a posição do operador | baixas | perfis L1, sem campos constantes, tudo com chave | emissão RF é sempre fisicamente detetável |
| T2 | Jammer consciente do protocolo ELRS | perda de enlace | poda por época, watchdog, failsafe FC | cortina total vence qualquer protocolo |
| T3 | Sweep/banda larga | perda de enlace | classificação + relatório (muda banda/posição) | idem |
| T4 | Replay/injeção de tramas | tomada de controlo | MAC ligado ao slot + janela nonce | — |
| T5 | Spoofing de SYNC | dessincronização | SYNC sem cifrar mas pouco sensível; dados validados | possível perda breve de lock |
| T6 | Captura física do RX | extração de chaves | rotação Fase B; rebind após perda | v1: chave estática no dispositivo |

## 4. Propriedades criptográficas v1 — honestamente

1. Payload RCDATA cifrado com ChaCha20; byte de tipo e CRC ficam em claro
   (framing); SYNC sem cifrar (necessário para ressincronizar após perda total).
2. MAC = SHA-256 de material da chave de época ligado ao canal FHSS e nonce,
   truncado no campo CRC. **Não é Poly1305**: o orçamento de bytes OTA4/8 não
   comporta um tag completo. Falsificação offline praticamente excluída;
   demonstrabilidade formal menor que AEAD — compromisso consciente, melhorado
   na Fase B.
3. Anti-replay: todo passo atrás ou duplicado é rejeitado; a recuperação vai
   por SYNC, nunca aceite tramas antigas.
4. Chave estática durante a vida da versão (sem campo de época no ar).
   Pares (canal, nonce) repetem-se no máximo cada ~768 tramas (domínios de 3
   canais): fug = XOR de dois payloads separados por múltiplo do período.
   **Onde o adversário arquiva emissões em massa, é necessária antes a Fase B.**
5. O token de despertar autentica o transmissor (comparação const-time), não a
   frescura; despertar não omite qualquer autorização de voo.

## 5. Presets operacionais (especificação portal/Lua, v0)

| Preset | Packet rate | TLM | Potência | Poda | Uso |
|---|---|---|---|---|---|
| STANDARD | 250–500 Hz | 1:16 | auto | on | trabalho por defeito |
| EW_HEAVY | 125–250 Hz | 1:32 | máxima legal | on, agressiva | interferência intensa |
| STEALTH | 100–150 Hz | **off** | mínima que sustente LQ | on | perto da frente, L1 |
| MAX_RANGE | 25–50 Hz | 1:64 | máxima legal | off | reconhecimento distante |
| LONG_WATCH | 50 Hz | 1:128 | baixa | off | vigilância + deep sleep |
| TRAINING/BENCH | qualquer | 1:8 | ≤25 mW | off | trabalho legal de bancada |

## 6. Despertar por sinal — detalhes

Ciclo: MCU deep sleep → cada `TAS_WAKE_POLL_MS` um scan CAD → atividade →
receção do token → comparação const-time → arranque completo do enlace.
O beacon do TX emite o token ≥1 período de poll no canal de sync.

Orçamentos (`TasWakeEstimateDays`, 80 mA / 100 ms por scan):

| Bateria | Poll | Corrente média | Espera |
|---|---|---|---|
| 3000 mAh | 3 s | ~2,9 mA | ~44 dias |
| 5000 mAh | 3 s | ~2,9 mA | ~73 dias |
| 3000 mAh | 1 s | ~8,2 mA | ~15 dias |

Procedimento de armazém: flash LONG_WATCH → dormir → despertar só com beacon
próprio. Perda de um RX = procedimento de rebind (chave por par; a rotação da
Fase B fecha definitivamente o tema). HAL (ESP32 ext0/timer + DIO1-CAD; STM32
standby/WKUP) — Fase B por placa; `-DTAS_WAKE_ON_SIGNAL` sem hooks termina o
build com `#error`.

## 7. Critérios de aceitação

Feito (nativo, 58 asserções PASS — `src/test/run_tas_tests.sh`): vetores RFC
(ChaCha20/HKDF/HMAC/SHA-256), determinismo e limites da poda (incl. domínios
EU868 de 13 canais e proteção de slots SYNC), roundtrip TAS sobre o `OTA.cpp`
real (ciphertext ≠ plaintext, tamper/cross-slot/replay rejeitados, SYNC passa,
OTA8 funciona), regressão stock sem flag, orçamentos de sono >30 dias @3000 mAh,
veredictos do watchdog.

Pendente em hardware (bloqueadores de implantação):
1. HIL com jammer SDR (sweep+wideband): curvas LQ vs potência, re-lock <2 s
   (STANDARD) / <5 s (low-signal) após 30 s de cortina.
2. Teste de alcance segundo protocolo stock com margem −3 dB por crypto/FEC.
3. Soak 24 h: zero perdas de sincronização, free-heap plano.
4. Câmara −25/+55 °C: boot e brownout a 3,0 V/célula.
5. Consumo real medido em deep sleep (os nossos números são modelo, não medidor).
6. Matriz de builds PIO por target — os nossos testes são nativos e não
   substituem a compilação do firmware!

## 8. Processo

- Qualquer alteração nos caminhos criptográficos: PR + atualização de vetores aqui.
- Revisão por duas pessoas para `TasCrypto/TasSession/TasOta`.
- Sem metadados de localização em commits; nomenclatura neutra.
- Fase B (ordem): rotação de chaves via SYNC OTA8 → AFH por medição →
  dual-band failover → OTA assinado → HAL wake por placa.
- Fase C: relay PHY (sem descifração no relay).

## 9. Limites e conformidade

Este projeto não contém funções de armamento, guiamento nem localização de
alvos — esses PR serão rejeitados. Os equipamentos devem operar dentro da
regulamentação nacional de rádio; exportação e transferência podem estar
sujeitas a controlos de dupla utilização — revisão legal antes de distribuir
fora do projeto.

## 10. Guia rápido para o construtor voluntário

1. Firmware: pipeline PlatformIO stock; em `user_defines.txt` descomente
   `-DTAS_HARDENING` (TX **e** RX sempre juntos! Stock e TAS nunca falam entre si).
2. Compile e corra a suite: `bash src/test/run_tas_tests.sh` — deve dar PASS.
3. Bind com a frase clássica; verifique no portal que `TAS_STATUS` chega ao FC.
4. Teste de failsafe em bancada: desligue o TX com gás armado no suporte —
   gás 0 em ≤150 ms, ação conforme configuração do FC.
5. Sleep/wake: só quando chegarem os hooks da sua placa (Fase B).

---

## Licença e atribuição

- **O fork como um todo permanece GPL-3.0** (ficheiro `LICENSE`) — requisito
  legal herdado do ExpressLRS. Alterações sobre ficheiros upstream e obras
  derivadas: apenas GPL-3.0.
- **Os nossos ficheiros totalmente originais** — `src/lib/TAS/*`,
  `docs/TAS_README_*`, `src/test/test_tas_*`, `src/test/run_tas_tests.sh` —
  estão adicionalmente **sob licença MIT** (Copyright © 2026 TAS-ELRS Team).
- **Atribuição:** equipa e comunidade
  [ExpressLRS](https://github.com/ExpressLRS/ExpressLRS) (GPL-3.0) pela base de
  código, arquitetura FHSS/OTA e anos de trabalho aberto.

## Apoie o projeto ☕

Se este projeto o ajudou: [**ofereça-nos um café → buycoffee.to/maha**](https://buycoffee.to/maha)
