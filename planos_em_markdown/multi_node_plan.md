# Plano: Sistema Multi-nó com Múltiplos ESP32 + MPU6050

## Visão geral

```
[ESP32 N01] ── WiFi ──┐
[ESP32 N02] ── WiFi ──┤── TCP :12345 ──> server.py ──> CSV / Unreal Engine 5
[ESP32 N03] ── WiFi ──┘
```

Cada ESP32 pode carregar 1 ou 2 sensores MPU6050. Cada sensor corresponde
a um segmento do corpo do paciente. O mapeamento segmento ↔ nó é feito no
servidor — o firmware só sabe seu NODE_ID técnico.

Exemplo com 5 nós (cobertura básica de membro superior bilateral):

| NODE_ID | Sensores | Segmento sugerido |
|---------|----------|-------------------|
| N01     | IMU01    | Braço direito     |
| N02     | IMU01    | Antebraço direito |
| N03     | IMU01    | Braço esquerdo    |
| N04     | IMU01    | Antebraço esquerdo|
| N05     | IMU01    | Tronco            |

---

## Problema central: NODE_ID único por dispositivo

Atualmente `NODE_ID = "N01"` está hardcoded no firmware. Com múltiplos
dispositivos isso exige um build separado por ESP32, o que é impraticável.

### Solução: NODE_ID derivado do MAC address

O ESP32 tem um MAC address único de fábrica. Podemos usar os últimos 3 bytes
como NODE_ID automático, sem precisar de builds distintos.

**Mudança no firmware (`main_print_mpu6050.cpp`):**

```cpp
// Remover:
static const char *NODE_ID = "N01";

// Adicionar (em setup(), antes de qualquer envio):
char _node_id_buf[8];

void initNodeId() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(_node_id_buf, sizeof(_node_id_buf),
           "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

// NODE_ID passa a ser _node_id_buf
```

Resultado: cada ESP32 gera automaticamente um ID como `"A3F2C1"`.

Para manter IDs legíveis (N01, N02…) sem builds separados, uma alternativa
é ler o NODE_ID do `.env` e passá-lo como define de build — mas isso ainda
exige um build por dispositivo. A abordagem do MAC é a mais escalável.

### Alternativa: NODE_ID configurável via EEPROM/NVS

Gravar o NODE_ID na NVS (Non-Volatile Storage) do ESP32 via comando Serial
na primeira inicialização. Permite renomear sem reflash.

```cpp
#include <Preferences.h>
Preferences prefs;

void initNodeId() {
  prefs.begin("node", false);
  if (!prefs.isKey("id")) {
    // Fallback: MAC-based ID na primeira vez
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(_node_id_buf, sizeof(_node_id_buf),
             "%02X%02X%02X", mac[3], mac[4], mac[5]);
    prefs.putString("id", _node_id_buf);
  } else {
    prefs.getString("id", _node_id_buf, sizeof(_node_id_buf));
  }
  prefs.end();
}
```

---

## Problema: sincronização de tempo entre nós

Cada ESP32 inicia `millis()` do zero no boot. Nós ligados em momentos
diferentes terão `t_ms` incomparáveis entre si.

### Solução A (simples): timestamp do servidor

O servidor adiciona um campo `server_ts` com `time.time()` no momento em
que recebe cada mensagem. O `t_ms` do ESP32 serve apenas para calcular
intervalos dentro do mesmo nó.

**Mudança em `server.py`:**

```python
import time

def process_line(line, writer, file_handle, client_addr):
    ...
    if msg_type == 'data':
        writer.writerow({
            ...todos os campos atuais...,
            'server_ts': time.time(),   # novo campo
            'client':    str(client_addr),
        })
```

Adicionar `'server_ts'` e `'client'` em `CSV_FIELDS`.

### Solução B (melhor): NTP no ESP32

O ESP32 sincroniza com um servidor NTP na rede local ou internet e usa
`gettimeofday()` como timestamp. Todos os nós ficam no mesmo referencial.

```cpp
#include <time.h>

void syncNTP() {
  configTime(0, 0, "pool.ntp.org");
  struct tm t;
  while (!getLocalTime(&t)) delay(500);
}

uint64_t nowMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
```

Substituir `millis()` por `nowMs()` nos campos `t_ms`.

---

## Mudanças em `server.py`

### 1. Race condition no CSV (crítico)

O código atual compartilha um único `writer`/`file_handle` entre threads sem
lock. Com múltiplos clientes simultâneos, escritas podem se entrelaçar e
corromper o CSV.

```python
import threading

csv_lock = threading.Lock()

def handle_client(client_socket, writer, file_handle, addr):
    ...
    # Em process_line, envolver a escrita:
    if msg_type == 'data':
        with csv_lock:
            writer.writerow({...})
            file_handle.flush()
```

### 2. CSV por nó vs. CSV combinado

**Opção A — um CSV por nó** (recomendada para análise por segmento):

```python
csv_files = {}   # node_id -> (file, writer)
csv_lock  = threading.Lock()

def get_writer(node_id, output_dir):
    with csv_lock:
        if node_id not in csv_files:
            path = os.path.join(output_dir,
                f"session_{datetime.now().strftime('%Y%m%d_%H%M%S')}_{node_id}.csv")
            f = open(path, 'w', newline='', encoding='utf-8')
            w = csv.DictWriter(f, fieldnames=CSV_FIELDS, extrasaction='ignore')
            w.writeheader()
            csv_files[node_id] = (f, w)
            print(f"[csv] novo arquivo para {node_id}: {path}")
        return csv_files[node_id]
```

**Opção B — CSV único com coluna `node`** (já funciona hoje, só precisa do lock).

### 3. Rastreamento de conexões ativas

```python
active_nodes = {}   # node_id -> addr
nodes_lock   = threading.Lock()

# Em handle_client, quando receber msg_type == 'boot':
with nodes_lock:
    active_nodes[msg.get('node')] = addr
    print(f"[nodes] conectados: {list(active_nodes.keys())}")

# Ao desconectar:
with nodes_lock:
    active_nodes.pop(node_id, None)
```

### 4. Mapeamento NODE_ID → segmento corporal

Arquivo `segment_map.json` na raiz do projeto (não commitado com dados de paciente):

```json
{
  "A3F2C1": "right_upper_arm",
  "B1D4E2": "right_forearm",
  "C5A3F1": "left_upper_arm"
}
```

O servidor carrega esse arquivo na inicialização e acrescenta o campo
`segment` no CSV:

```python
import json

segment_map = {}
if os.path.exists('segment_map.json'):
    with open('segment_map.json') as f:
        segment_map = json.load(f)

# Em process_line:
segment = segment_map.get(msg.get('node'), 'unknown')
writer.writerow({..., 'segment': segment})
```

---

## Resumo das mudanças por arquivo

### `main_print_mpu6050.cpp`

| # | Mudança | Prioridade |
|---|---------|-----------|
| 1 | NODE_ID derivado do MAC address | Alta |
| 2 | NTP sync para timestamp absoluto | Média |
| 3 | NODE_ID configurável via NVS | Baixa |

### `server.py`

| # | Mudança | Prioridade |
|---|---------|-----------|
| 1 | Lock no CSV para múltiplos clientes | **Crítica** |
| 2 | Timestamp do servidor (`server_ts`) | Alta |
| 3 | CSV separado por nó | Média |
| 4 | Rastreamento de nós conectados | Média |
| 5 | Mapeamento NODE_ID → segmento | Baixa |

---

## Ordem de implementação sugerida

1. **Corrigir o lock do CSV** — único bug que pode corromper dados hoje
2. **Adicionar `server_ts`** — necessário para alinhar dados de múltiplos nós
3. **NODE_ID por MAC address** — permite usar o mesmo firmware em todos os ESP32
4. **Testar com 2 nós** antes de escalar
5. **CSV por nó** — simplifica o pipeline de processamento downstream
6. **NTP** — necessário quando a latência de sincronização importar (< 10 ms)
7. **Mapeamento de segmentos** — necessário para integrar com Unreal Engine 5
