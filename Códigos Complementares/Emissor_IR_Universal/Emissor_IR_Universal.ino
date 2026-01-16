#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRac.h>     // <--- A CLASSE UNIVERSAL
#include <IRutils.h>  // Útil para debug

// Otimização de memória: pinos cabem em 8 bits
const uint8_t pinoEmissor = 4;

// Instanciamos o objeto Universal (não mais IRMirageAc)
IRac ac(pinoEmissor); 

// --- CONFIGURAÇÃO UNIVERSAL ---
//Para trocar o protocolo, mude apenas isto:
//exemplos: decode_type_t::SAMSUNG, decode_type_t::LG, decode_type_t::GREE
decode_type_t protocolo = decode_type_t::MIRAGE;

// Variáveis Universais para guardar suas preferências
std_ac_fan_t velocidadePadrao = std_ac_fan_t::kAuto; // Universal
std_ac_opmode_t modoPadrao    = std_ac_opmode_t::kCool; // Universal
int temperatura_1 = 24;


void ligarAr() {
  // Atualizamos o estado "próximo" (next) para ligado
  ac.next.power = true; 
  
  //é interessante enviar o sinal mais de uma vez, para garantir que vai ser captado
  ac.sendAc(); //primeiro envio
  delay(50); //pausa minúscula (padrão em muitos protocolos)
  ac.sendAc(); //envia a segunda vez
  delay(50); //pausa minúscula (padrão em muitos protocolos)
  ac.sendAc(); //envia a terceira vez para garantir
  Serial.println("Comando LIGAR enviado.");
}

void desligarAr() {
  // Atualizamos o estado "próximo" (next) para desligado
  ac.next.power = false;

  //é interessante enviar o sinal mais de uma vez, para garantir que vai ser captado
  ac.sendAc(); //primeiro envio
  delay(50); //pausa minúscula (padrão em muitos protocolos)
  ac.sendAc(); //envia a segunda vez
  delay(50); //pausa minúscula (padrão em muitos protocolos)
  ac.sendAc(); //envia a terceira vez para garantir
  Serial.println("Comando DESLIGAR enviado.");
}

void mudarTemperatura(int novaTemp) {
  ac.next.degrees = novaTemp; // Atualiza a memória
  ac.next.power = true;       // Garante que o ar esteja ligado ao mudar temp
  enviarSinal();              // Envia o comando atualizado
  
  Serial.print("Temperatura alterada para: ");
  Serial.println(novaTemp);
}


void setup() {
  Serial.begin(115200);
  delay(200);

  ac.begin();

  //1- definimos qual protocolo o controle vai usar
  ac.next.protocol = protocolo; 

  // 2- Aplicamos as configurações iniciais usando os tipos universais
  ac.next.fanspeed = velocidadePadrao;
  ac.next.mode     = modoPadrao;
  ac.next.celsius  = true;
  ac.next.degrees  = temperatura_1;
  ac.next.power    = false; //começa desligado na memória do ESP, não necessariamente condiz com a realidade do aparelho
  
  Serial.print("Sistema iniciado para o protocolo: ");
  Serial.println(typeToString(ac.next.protocol));
}


void loop() {
  delay(10000); 
  ligarAr();
  
  delay(10000);
  desligarAr();
}
