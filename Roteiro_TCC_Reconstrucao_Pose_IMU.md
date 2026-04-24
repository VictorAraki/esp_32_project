# ROTEIRO DE ORIENTAÇÃO -- TCC

## Reconstrução de Pose Humana 3D via Sensores Inerciais

### Modelagem Biomecânica e Validação Quantitativa para Aplicação em Monitoramento de Epilepsia

------------------------------------------------------------------------

# 1. CONTEXTUALIZAÇÃO

Este projeto representa a segunda geração de um sistema vestível de
reconstrução de pose humana baseado em sensores inerciais (IMUs), com
aplicação potencial no monitoramento de pacientes com epilepsia.

O objetivo do TCC não é apenas implementar o sistema, mas:

> Desenvolver, otimizar e validar quantitativamente a reconstrução 3D da
> pose humana, reduzindo drift e aumentando estabilidade temporal, com
> análise biomecânica e potencial aplicação clínica.

------------------------------------------------------------------------

# 2. PROBLEMA DE PESQUISA

Como otimizar e validar quantitativamente a reconstrução de pose humana
3D baseada em IMUs, reduzindo erro acumulado e instabilidade temporal,
visando futura aplicação em monitoramento clínico?

------------------------------------------------------------------------

# 3. OBJETIVOS

## 3.1 Objetivo Geral

Desenvolver e validar um sistema de reconstrução 3D da pose humana via
sensores inerciais com modelagem biomecânica e avaliação quantitativa.

## 3.2 Objetivos Específicos

1.  Implementar e comparar algoritmos de fusão sensorial.
2.  Modelar cadeia cinemática com restrições anatômicas.
3.  Reduzir jitter e drift temporal.
4.  Construir pipeline de validação com ground truth visual.
5.  Extrair biomarcadores cinemáticos relevantes.

------------------------------------------------------------------------

# 4. ARQUITETURA DO SISTEMA

## 4.1 Camada 1 -- Engenharia do Sistema

-   Aquisição de dados IMU
-   Fusão sensorial
-   Reconstrução via cinemática direta
-   Aplicação de restrições articulares
-   Suavização temporal
-   Visualização 3D

## 4.2 Camada 2 -- Validação Quantitativa

-   Sincronização IMU--Vídeo
-   Comparação com estimativa visual (MediaPipe/OpenPose)
-   Cálculo de métricas objetivas

## 4.3 Camada 3 -- Biomarcadores

Extração de variáveis cinemáticas por janela temporal:

-   Velocidade angular média
-   Energia segmentar
-   Aceleração RMS
-   Simetria bilateral
-   Frequência dominante de movimento

------------------------------------------------------------------------

# 5. REQUISITOS DO PROJETO

## 5.1 Requisitos Funcionais

### Aquisição de Dados
O sistema deve ler os dados de aceleração e rotação (6 eixos) utilizando o sensor **ICM45686**, capturando informações em tempo real dos múltiplos módulos vestíveis.

### Processamento em Tempo Real
A ESP32 deve processar os dados brutos e calcular a orientação espacial, utilizando **Quaternions para evitar Gimbal Lock** e garantir representação contínua e sem singularidades da rotação.

### Comunicação
O microcontrolador deve enviar os dados para o computador com baixa latência, preferencialmente via **Wi-Fi (protocolo UDP)** para maior mobilidade do paciente durante o Video-EEG.

### Simulação 3D
O simulador deve ler os dados recebidos pelo computador e mapeá-los para os "ossos" (rigging) de um avatar 3D, correspondendo aos membros do paciente em tempo real.

## 5.2 Requisitos Não Funcionais

### Conforto e Ergonomia
Como será usado em pacientes, os módulos devem ser **compactos e leves**, permitindo uso prolongado sem desconforto ou restrição de movimento.

### Segurança Elétrica
**Isolamento adequado dos componentes eletrônicos**, proteção contra curto-circuito e conformidade com normas de equipamentos médicos vestíveis.

### Custo-Benefício
Utilização de **hardware acessível** (ESP32, sensores de baixo custo), viabilizando reprodução e distribuição do sistema a múltiplos pacientes.

### Confiabilidade
Operação estável em ambiente clínico, com baixa taxa de erros e comunicação robusta mesmo em ambientes com interferência eletromagnética.

------------------------------------------------------------------------

# 6. COMPONENTES NECESSÁRIOS

## 6.1 Hardware

### Microcontrolador
- **ESP32**: Excelente poder de processamento, Wi-Fi/Bluetooth nativos, ideal para recepção de dados sensorial e processamento de fusão em tempo real.

### Sensores de Movimento
- **Módulos com sensor ICM45686**: Múltiplas unidades (quantidade dependente de quantos membros serão rastreados simultaneamente)
  - Exemplo: Um módulo para braço, um para antebraço, módulos para pernas.
  - Comunicação via **I2C ou SPI**.

### Conexão Física
- Fios/Jumpers para prototipagem
- Cabo USB de dados (para programação e conexão direta da ESP32 ao computador durante testes)

### Alimentação
- **Versão de laboratório**: Alimentação via cabo USB
- **Versão clínica final**: Baterias LiPo (ex: 3.7V de pequena capacidade) com módulo de carregamento (TP4056)

### Fixação Mecânica
- Faixas de velcro, elásticos ou suportes impressos em 3D para prender os sensores nos segmentos corporais (braços, antebraços, pernas)

## 6.2 Software

### Ambiente de Desenvolvimento
- **Arduino IDE** ou **PlatformIO** (VS Code plugin recomendado para melhor integração de projeto)
- Drivers necessários para ESP32 e ICM45686

### Filtros de Fusão Sensorial
- **Madgwick Filter** ou **Mahony Filter** (essenciais para transformar dados puros de aceleração/giroscópio em ângulos utilizáveis)
- Bibliotecas: `MPU9250` ou `ICM45686_driver` com implementação de quatérnions

### Simulador 3D
- **Unreal Engine 5** (recomendado para visualização humana realista e performance)
- **Alternativas**: RViz (mais voltado para robótica) ou Godot (open-source, mas menos recursos)

### Middleware de Comunicação
- **Script Python** no computador para receber dados via porta Serial ou Wi-Fi da ESP32
- Serialização em formato leve: **JSON** ou **CSV** separado por vírgulas
- **Protocolo OSC (Open Sound Control)** ou UDP puro para envio à Unreal Engine
- Estrutura de dados: `[ID_Sensor, Roll, Pitch, Yaw, Timestamp]`

------------------------------------------------------------------------

# 7. PLANO DE AÇÃO E ETAPAS DO PROJETO

Este cronograma é dividido em **fases de validação incremental**, permitindo testar cada subsistema antes de integração total.

## 7.1 Fase 1: Setup Base e Validação do Hardware

**Duração**: Semanas 1-2

**Objetivos**: Validar ambiente de desenvolvimento e comunicação ESP32-sensor

- [ ] Instalar Arduino IDE/PlatformIO com suporte a ESP32
- [ ] Conectar ESP32 ao computador via USB e executar código de teste ("Blink" + print no monitor serial)
- [ ] Soldar/conectar o sensor ICM45686 à ESP32 (via I2C ou SPI conforme especificação)
- [ ] Ler dados brutos (aceleração em m/s² e giroscópio em °/s) no monitor serial
- [ ] Documentar frequência de amostragem e latência de leitura

**Critério de Sucesso**: Leitura estável de dados do sensor ICM45686 a 100+ Hz

## 7.2 Fase 2: Fusão Sensorial e Orientação

**Duração**: Semanas 3-5

**Objetivos**: Implementar algoritmo de fusão e validar orientação espacial

- [ ] Implementar **Filtro Madgwick** ou **Mahony** para fusão acelerômetro + giroscópio
- [ ] Converter dados em **Quaternions (q0, q1, q2, q3)** ou **Ângulos de Euler (Roll, Pitch, Yaw)**
- [ ] Implementar função de calibração: bias de acelerômetro, offset de giroscópio
- [ ] Testar manualmente: girar o sensor na mão e observar resposta dos ângulos em tempo real
- [ ] Medir e registrar drift acumulado após 5 minutos de leitura estática

**Critério de Sucesso**: Ângulos respondem corretamente à rotação manual, com drift < 5° após 5 min

## 7.3 Fase 3: Comunicação (ESP32 → Computador)

**Duração**: Semanas 6-7

**Objetivos**: Estabelecer pipeline de dados com baixa latência

- [ ] Configurar ESP32 para transmitir dados via **Wi-Fi UDP** (ou Serial para prototipagem)
- [ ] Definir estrutura de pacote: `ID_Sensor,Roll,Pitch,Yaw,Timestamp` (exemplo: `1,15.3,-8.2,102.1,1234567890`)
- [ ] Implementar verificação de integridade (checksum simples)
- [ ] Criar script Python no computador para receber, parsear e logar os dados
- [ ] Medir latência fim-a-fim (tempo entre leitura do sensor e recepção no PC)

**Critério de Sucesso**: Latência < 50 ms, zero perda de pacotes em 10 segundos de transmissão

## 7.4 Fase 4: Ambiente de Simulação 3D (Unreal Engine)

**Duração**: Semanas 8-10

**Objetivos**: Visualizar dados do sensor em avatar 3D em tempo real

- [ ] Criar projeto em branco na Unreal Engine 5
- [ ] Importar modelo 3D humano com esqueleto rigged (ex: modelo humanóide padrão)
- [ ] Instalar **OSC Plugin** (nativo ou via plugin community)
- [ ] Configurar receptor UDP/OSC em Blueprints ou C++
- [ ] Implementar mapeamento de dados → rotação dos ossos (bones):
  - Sensor 1 → Braço direito
  - Sensor 2 → Antebraço direito
  - Sensor 3 → Perna direita
  - etc.
- [ ] Visualizar modelo humano atualizando em tempo real com movimentos dos sensores

**Critério de Sucesso**: Avatar move-se visualmente em sincronismo com movimentos dos sensores

## 7.5 Fase 5: Integração, Testes e Calibração

**Duração**: Semanas 11-15

**Objetivos**: Validar sistema completo em condições controladas e realistas

### Testes de Bancada
- [ ] Mover sensor físico e verificar reflexo imediato no avatar (sem lag perceptível)
- [ ] Testar movimentos bruscos e mudanças de orientação rápidas
- [ ] Registrar vídeo do avatar vs. movimento físico para análise posterior

### Calibração
- [ ] Implementar função de "Tara" ou zero-offset: quando paciente está em posição neutra, sistema registra posição de repouso
- [ ] Testar botão/comando para re-calibração durante sessão de Video-EEG

### Testes com Voluntário
- [ ] Fixar módulos no corpo de um voluntário usando velcro/fitas
- [ ] Executar protocolo de movimentos padronizados (ex: levantamento de braço, flexão de joelho)
- [ ] Capturar dados simultâneos do sistema IMU e câmera RGB (para validação posterior)
- [ ] Verificar estabilidade, artefatos e queda de conectividade

### Análise de Métricas
- [ ] Calcular RMSE entre ângulos IMU e estimativa visual (MediaPipe/OpenPose)
- [ ] Medir jitter frame-a-frame
- [ ] Documentar casos de falha ou perda de sincronismo

**Critério de Sucesso**: RMSE < 10°, jitter < 2° RMS, latência média < 30 ms, uptime > 95%

## 7.6 Fase 6: Documentação e Escrita (TCC)

**Duração**: Semanas 16-20

**Objetivos**: Consolidar todas as descobertas em monografia e apresentação

- [ ] **Introdução**: Contextualizar Video-EEG clínico e necessidade de rastreamento de crises motoras
- [ ] **Revisão Teórica**: Fundamentos de IMU, quaternions, filtros de fusão sensorial
- [ ] **Metodologia**: 
  - Desenho do hardware (diagrama de blocos, lista de componentes)
  - Explicação matemática do filtro Madgwick/Mahony
  - Arquitetura de rede (ESP32 → Python → Unreal)
  - Protocolo de testes com voluntários
- [ ] **Resultados**: 
  - Gráficos de latência e precisão
  - Tabelas comparativas (ex: drift ao longo do tempo)
  - Fotos/vídeos do protótipo em uso
  - Gráficos de RMSE, jitter, taxa de atualização
- [ ] **Discussão**: Limitações, fontes de erro, comparação com trabalhos relacionados
- [ ] **Conclusão e Trabalhos Futuros**

------------------------------------------------------------------------

# 8. METODOLOGIA DETALHADA

## 8.1 Calibração

-   Bias de acelerômetro
-   Calibração de magnetômetro
-   Normalização de quatérnions

## 8.2 Fusão Sensorial

Comparação entre:

-   Filtro Complementar
-   Madgwick
-   EKF simplificado

## 8.3 Modelagem Biomecânica

-   Pelve como root
-   Comprimento fixo de segmentos
-   Limites articulares fisiológicos

## 8.4 Validação Experimental

Protocolo:

1.  Execução de movimentos padronizados.
2.  Captura simultânea IMU + vídeo.
3.  Sincronização por timestamp.
4.  Cálculo de métricas.

------------------------------------------------------------------------

# 9. MÉTRICAS DE AVALIAÇÃO

## 9.1 Erro Angular por Junta

RMSE entre estimativa IMU e referência visual.

## 9.2 Drift

Erro acumulado ao longo do tempo.

## 9.3 Jitter

Variabilidade frame a frame.

## 9.4 Latência

Tempo entre aquisição e reconstrução.

## 9.5 FPS

Taxa de atualização do sistema.

------------------------------------------------------------------------

# 10. CRONOGRAMA (20 SEMANAS)

  Semanas   Atividade
  --------- ------------------------------------------
  1--2      Revisão teórica e definição do esqueleto
  3--6      Implementação de calibração e filtros
  7--10     Modelagem cinemática e restrições
  11--14    Coleta experimental e validação
  15--17    Extração de biomarcadores
  18--20    Escrita, gráficos, slides e demo

------------------------------------------------------------------------

# 11. ESTRUTURA DO REPOSITÓRIO

pose-imu/ │ ├── firmware/ ├── data/ │ ├── raw/ │ ├── processed/ │ └──
logs/ ├── filters/ ├── kinematics/ ├── validation/ ├── biomarkers/ ├──
notebooks/ └── docs/

Requisitos:

-   README detalhado
-   Scripts reproduzíveis
-   Seeds fixadas
-   Logs com timestamp
-   Documentação das decisões técnicas

------------------------------------------------------------------------

# 9. ESTRUTURA DA MONOGRAFIA

1.  Introdução
2.  Fundamentação Teórica
3.  Arquitetura do Sistema
4.  Fusão Sensorial
5.  Modelagem Biomecânica
6.  Metodologia de Validação
7.  Resultados
8.  Discussão
9.  Aplicabilidade Clínica
10. Conclusão

------------------------------------------------------------------------

# 10. ROTEIRO DA APRESENTAÇÃO FINAL

1.  Problema clínico
2.  Motivação tecnológica
3.  Arquitetura do sistema
4.  Fusão sensorial
5.  Modelo biomecânico
6.  Pipeline de validação
7.  Métricas
8.  Resultados comparativos
9.  Robustez
10. Biomarcadores
11. Aplicabilidade clínica
12. Limitações
13. Próximos passos
14. Demo
15. Conclusão

------------------------------------------------------------------------

# 11. CRITÉRIOS DE SUCESSO DO TCC

O trabalho será considerado completo se:

-   Reconstrução 3D estiver estável
-   Comparação entre filtros for quantitativa
-   Métricas forem apresentadas com análise estatística
-   Pipeline for reproduzível
-   Biomarcadores forem extraídos automaticamente
-   Demonstração funcional estiver disponível

------------------------------------------------------------------------

# 12. IMPACTO ESPERADO

-   Consolidação de plataforma vestível
-   Base para futura validação clínica
-   Potencial publicação técnica
-   Integração com linha de pesquisa maior em tecnologias aplicadas à
    epilepsia
