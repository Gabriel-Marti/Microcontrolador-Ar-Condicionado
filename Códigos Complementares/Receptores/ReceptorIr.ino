/*
 * IRremoteESP8266: IRrecvDemo - demonstrates receiving IR codes with IRrecv
 * This is very simple teaching code to show you how to use the library.
 * If you are trying to decode your Infra-Red remote(s) for later replay,
 * use the IRrecvDumpV2.ino (or later) example code instead of this.
 * An IR detector/demodulator must be connected to the input kRecvPin.
 * Copyright 2009 Ken Shirriff, http://arcfn.com
 * Example circuit diagram:
 *  https://github.com/crankyoldgit/IRremoteESP8266/wiki#ir-receiving
 * Changes:
 *   Version 0.2 June, 2017
 *     Changed GPIO pin to the same as other examples.
 *     Used our own method for printing a uint64_t.
 *     Changed the baud rate to 115200.
 *   Version 0.1 Sept, 2015
 *     Based on Ken Shirriff's IrsendDemo Version 0.1 July, 2009
 */

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

// --- CONFIGURAÇÕES ---
const uint16_t kRecvPin = 4; // Seu pino receptor
const uint16_t kCaptureBufferSize = 1024; // AUMENTADO! Essencial para Ar-Condicionado
const uint8_t kTimeout = 50; // Tempo de espera para finalizar o comando
const uint16_t kMinUnknownSize = 12; // Ignora ruídos muito curtos

IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true); //instancia o objeto irrecv a paritr da classe IRrecv ele é responsável pelos comandos de coleta de dados
decode_results results; //instancia o objeto results a partir da classe decode_results, ele armazena os dados coletados pelo receptor

void setup() {
  Serial.begin(115200);
  // Garante que o pino está como entrada (algumas placas precisam disso explicito)
  pinMode(kRecvPin, INPUT); 
  
  irrecv.enableIRIn();  // Liga o receptor
  
  Serial.println("------------------------------------------------");
  Serial.println("SNIFFER PRONTO! Aponte o controle do AC e aperte.");
  Serial.println("Lembre-se: Apague as luzes fortes do ambiente.");
  Serial.println("------------------------------------------------");
}

//COUNT É O TAMANHO DO ARRAY(RAWLEN), COMEÇA DO 1 (PULANDO O 0) POIS O INDICE 0 É O LIXO QUE DIZ RESPEITO A ANTES DO RECEPTOR SER ATIVADO
//CRIA UM NOVO ARRAY NO PRINT COM UM ESPAÇO A MENOS (COUNT-1), DESCONTANDO O ESPAÇO QUE ESTAVA OCUPADO PELO LIXO NO INDICE 0
/*O LED do Controle Pisca: Ele envia rajadas de luz infravermelha.

O Sensor (Pino 4) Reage: O pino vai de ALTO (3.3V) para BAIXO (0V) quando vê luz.

A Interrupção (O Cronômetro):

O processador percebe a mudança de voltagem.

Ele para o cronômetro anterior e salva o valor no vetor rawbuf.

Ele zera o cronômetro e começa a contar de novo para o próximo estado (silêncio).

O Loop dumpRaw:

Depois que o sinal acaba, seu código varre essa lista de "contagens de cronômetro".

Ele pega cada contagem, multiplica por 2 (para virar microssegundos reais) e imprime na tela para você copiar.
 */
void dumpRaw(decode_results *results) {
  uint16_t count = results->rawlen;
  
  Serial.println("");
  Serial.println("// --- INICIO DO CODIGO PARA COPIAR ---");
  Serial.print("uint16_t rawData[");
  Serial.print(count - 1);
  Serial.print("] = {");

  for (uint16_t i = 1; i < count; i++) {
    //pega o valor armazenado no array, ele é medido em ticks, então esses ticks devem ser convertidos para microssegundos (kRawTick = 2 microssegundos, o valor de um tick)
    uint32_t usecs = results->rawbuf[i] * kRawTick;
    
    Serial.print(usecs, DEC);
    if (i < count - 1) {
      Serial.print(", ");
    }
    // Quebra linha a cada 20 numeros para ficar legível
    if ((i % 20) == 0) Serial.print("\n  "); 
  }
  
  Serial.println("};");
  Serial.println("// --- FIM DO CODIGO ---");
  Serial.println("");
  
  // Mostra também qual protocolo ele achou que era (só por curiosidade)
  Serial.print("Protocolo detectado (apenas informativo): ");
  serialPrintUint64(results->value, HEX);
  Serial.println("");
}

void loop() {
  //se algum sinal for detectado, inicia a execução do código
  if (irrecv.decode(&results)) {
    // Só mostra se for um sinal longo o suficiente (filtra ruído básico)
    if (results.rawlen > kMinUnknownSize) {
      dumpRaw(&results);
    }
    irrecv.resume(); // Prepara para receber o próximo, o receptor normalmente se desligaria sem isso
  }
}
