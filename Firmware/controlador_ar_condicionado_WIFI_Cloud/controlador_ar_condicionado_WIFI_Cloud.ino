#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRac.h> //A CLASSE UNIVERSAL
#include <IRutils.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
//NOTA: para conectar em rede comercial (com login e senha de usuário invés de somente senha do wifi) usar outra biblioteca como esp_wpa2.h
//Caso seja o modo simples (PEAP) é possível fazer conexão sem criptografia, usando login e senha de uma conta específica, que não teria problemas de ser comprometida, para simplificar
//Caso seja o modo seguro (EAT-TLS) é necessário fazer conexão com criptografia com certificados expiráveois e chaves privadas, gerando uma complexidade a mais
//Uma vez que os dados desse sistema são smples e não sensíveis é recomendável fazer uma conexão sem não segura para evitar complexidades desnecessárias

//PINOS
//uint8_t está sendo usado para otimização, pois consome menos bits que o tipo int, ele consome 8 bits, enquanto o int são 32 bits
const uint8_t pinoLED_IR = 4;
const uint8_t pinoDHT = 14;
const uint8_t pinoPIR = 12;//NOTA: configurar o pir para tempo de 5 minutos de verificação e o alcance(sensibilidade) tem que ser analisado em relação ao contexto para evitar imprecisão
//sensibilidade alta pode fazer o pir dispsarar com ruido eletrico do próprio microcontrolador, com feixes de sol ou fluxos de vento
//o posicionamento tem que ser bem pensado, no canto da parede pode ser uma boa opção

#define DHTTYPE DHT11

IRac ac(pinoLED_IR);
DHT dht(pinoDHT, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// --- CONFIGURAÇÃO UNIVERSAL ---
//Para trocar o protocolo, mude apenas isto:
//exemplos: decode_type_t::SAMSUNG, decode_type_t::LG, decode_type_t::GREE
decode_type_t protocolo = decode_type_t::MIRAGE;


// Variáveis Universais para guardar preferências
//const uint8_t velocidadePadrao = 0; // MODO AUTO. IMPORTANTE: CONSULTAR NO PROTOCOLO SE A VELOCIDADE 0 DE FATO É O MODO AUTO, ATUALIZAR SE NECESSÁRIO (abordagem antiga)
//const uint8_t modoPadrao = 1;// COOL (FRIO). IMPORTANTE: CONSULTAR NO PROTOCOLO EM QUESTÃO SE DE FATO 1 É COOL, ATUALIZAR SE NECESSÁRIO (abordagem antiga)

const stdAc::fanspeed_t velocidadePadrao = stdAc::fanspeed_t::kAuto;
const stdAc::opmode_t modoPadrao = stdAc::opmode_t::kCool;

//Lista de temperaturaas para alterar
uint8_t temperatura_1 = 24;
uint8_t temperatura_2 = 20;
uint8_t temperatura_3 = 18;
//outtras temperaturas poderão ser adicionadas posteriormente


bool pareceLigado = false; //estado do ar condicionado baseado no sensor de temperatura/umidade
bool arCondicionadoLigado = false; // Estado atual do Ar (falso = desligado)
unsigned long ultimoTempoMovimento = 0; // Guarda o tempo do último movimento

//TEMPO DE TOLERANCIA PARA DELISGAR O AR CASO A PRESENÇA ACABE
// Para teste : 10000 (10 segundos)
// Para vida real: 600000 (10 minutos)

//NOTA: PARA ARCONDICIONADOS INVERTER
//o modelo inverter é tão econômico que só vale a pena desligar ele caso a sala se mantenha vazia por 90 minutos.
const unsigned long TEMPO_ESPERA = 10000;

//Tempo para o caso do dia frio e seco, evitando spam de ar bipes de desligar
unsigned long ultimaTentativaReforco = 0; 
const unsigned long INTERVALO_REFORCO = 300000; // 5 minutos


//Captados pelo DHT
float TemperaturaMedida;
float UmidadeMedida;

//variáveis para conectividade

const char* Client_ID = "ESP32_Clima_Control"; //NÃO PODE TER ESPACOS OU ACENTOS, MUDAR ESSA PARTE PARA CADA APARELHO NOVO

String topico_temperatura = String(Client_ID) + "/temperatura";
String topico_umidade     = String(Client_ID) + "/umidade";
String topico_estado      = String(Client_ID) + "/estado";
String topico_comando     = String(Client_ID) + "/comando";

//tópico global de descoberta (fixo para todos)
const char* topico_discovery = "sistema/global/descobrir";

const char* ssid = "NOME_DO_WIFI"; //nome do wifi do roteador que pretende se conectar
const char* senha = "SENHA_DO_WIFI"; //senha do wifi do roteador a ser conectado
const char* server = "broker.hivemq.com"; //endereço do servidor de mqtt a se conectar
const int porta = 1883; //porta na nuvem SEM CRIPTOGRAFIA, uma vez que os dados trafegados não são sensíveis não é preciso se preocupar com segurança
//Para conexão por rede local é posível deixar conexão sem senha e comentar essa parte de baixo, pois os dados não são sensíveis, mas por boa´prática pod deixar com também
const char* mqtt_login = "usuario_da_nuvem"; //login do usuário no servicço de mqtt
const char* mqtt_senha = "senha_da_nuvem"; //senhad o usuário no serviço de mqtt

String comandoRecebido;
bool chegouNovoComando;
//NOTA: os comandos remotos "LIGAR" liga o ar "DESLIGAR" desliga o ar


void ligarAr() {
  //atualizamos o estado do objeto para ligado
  ac.next.power = true;
  
  //é interessante enviar o sinal mais de uma vez, para garantir que vai ser captado
  ac.sendAc(); //primeiro envio
  esperar(50); //pausa minúscula (padrão em muitos protocolos)
  ac.sendAc(); //envia a segunda vez
  esperar(50); //pausa minúscula (padrão em muitos protocolos)
  ac.sendAc(); //envia a terceira vez para garantir
}

void desligarAr() {
  //atualizamo o estado do objeto para desligado
  ac.next.power = false;

  //é interessante enviar o sinal mais de uma vez, para garantir que vai ser captado
  ac.sendAc(); //primeiro envio
  esperar(50); //pausa minúscula (padrão em muitos protocolos)
  ac.sendAc(); //envia a segunda vez
  esperar(50); //pausa minúscula (padrão em muitos protocolos)
  ac.sendAc(); //envia a terceira vez para garantir
}


void mudarTemperatura(uint8_t novaTemp) {

  if (ac.next.degrees != novaTemp) {
    ac.next.degrees = novaTemp; //atualiza a memória do objeto
    ac.next.power = true;       //garante que o ar esteja ligado ao mudar temperatura
    // Envia o comando atualizado
     ac.sendAc(); //primeiro envio
    esperar(50); //pausa minúscula (padrão em muitos protocolos)
    ac.sendAc(); //envia a segunda vez
    esperar(50); //pausa minúscula (padrão em muitos protocolos)
    ac.sendAc(); //envia a terceira vez para garantir
  }
}


//delay alternativo, observa a conexão enquanto espera para não derrubar a conxeão
void esperar(int tempo) {

  unsigned long inicio = millis();

  while (millis() - inicio < tempo) {

  client.loop(); //mantém o MQTT ativo durante a espera

  yield(); //Não faz nada, "respira"

  }

}

//gerenciamento de controle para ligar/desligar basedo no estado e presença
void gerenciarArCondicionado() {
  bool movimentoAtual = digitalRead(pinoPIR);
  //registro de tempo baseada numa função interna que conta o tempo em milissegundos, fica sempre ativo (mesmo sem ser convocado) e reseta acada 49 dias
  unsigned long tempoAtual = millis(); //unsigned long funciona como um ponteiro de um relógio, se uma operação estourar para um negativo ele faz a conta ao contrário em relação ao máximo

  // CENÁRIO 1: TEM GENTE (Movimento Detectado)
  if (movimentoAtual == HIGH) {
    // 1. Reseta o cronômetro (renova os 10 minutos)
    ultimoTempoMovimento = tempoAtual; //anota quanto o cronômetro marcava da última vez que alguém se mecheu

    //Se o ar estava desligado, liga AGORA.
    //NOTA: TALVEZ VALA A PENA REMOVER A CONDIÇÃO pareceLigado == false PARA FORÇAR QUE LIGUE DE TODO MODO, CASO JÁ ESTEJA LIGADO, NÃO OCORRERIA NADA
    //talvez não vala apena, pois se parecer ligado está frio e se estiver frio não precisa ser ligado, ao menos não de forma automática
    if (arCondicionadoLigado == false && pareceLigado == false ) {
      arCondicionadoLigado = true; // Atualiza estado
      ac.next.degrees = temperatura_1;
      ligarAr();              //LIGA O AR
    }
  }
  
  //CENÁRIO 2: NÃO TEM GENTE (Timeout)
  else {
    //NOTA: se alguém "quebrar o sistema" desligando manualmente vai ter que consertar manualmente ou esperar o tempo até que a solução automática ligue de novo
    if (tempoAtual - ultimoTempoMovimento > TEMPO_ESPERA) {
      //SUBTRAINDO o tempo que o cronometro registra do tempo que o cronometro marcou da ultima vez que alguém se mecheu
      //É possível conseguir o intervalo de tempo a partir do último estado de presença para verificar se passou o tempo de tolerância
      //caso o cronometro do millis reincie e a conta retorne um valor negativo, a aritmédica modular aplicada aos números unsigned resolverá isso, impedindo valores gradnes demais
      if (arCondicionadoLigado == true) {
        
        // DESLIGAR: Só se a sala estiver FRIA (Confirmando que está ligado)
        if (pareceLigado == true) { 

           arCondicionadoLigado = false; 
           desligarAr();             
        }
        else {
           // Se deveria desligar, mas a sala JÁ ESTÁ QUENTE, significa que 
           // o ar desligou de outro modo ou quebrou. 
           // Atualizamos a variável para 'false' para alinhar o sistema.
           arCondicionadoLigado = false; 
           //atualiza o estado corretamente
        }
      }
        // Tem o cenário onde o sitema acha que o ar está desligado, mas ele está ligado, o sensor de presença deve dedeuzir que está ligado então
        // Caso o sistema achar que desligou, mas a sala continuar fria, então manda o comando desligar, para que a sala se desligue
        //se o dia estiver frio e seco acabará gerando um pequeno spam de sinais desligar no mecanismo de segurança, mas o comum é estar frio e úmido
        //então como dias frios normalmente são umidos, é mais provavel que o ar esteja ligado caso esteja frio e seco, assim vale a pena arriscar o "spam"
      else if (pareceLigado == true) {
        
        //Para evitar spam no caso de dias frios e secos ó manda o sinal se já passou 5 minutos desde a última tentativa
        if (tempoAtual - ultimaTentativaReforco > INTERVALO_REFORCO) {
           
           //manda o sinal de desligar novamente (mesmo a variável já sendo false)
           desligarAr(); 
           
           // Atualiza o timer para esperar mais 5 minutos antes de tentar de novo
           ultimaTentativaReforco = tempoAtual; 
        }
      }
    }
  }
  //NOTA: se o dia estiver frio a ponto do sensor detectar a temperatura fria, então a temperatura desejada já está ativa e não precisa ligar o ar
}

//vai deduzir o estado do ar condicionado baseado na temperatura e umidade
bool DeduzirEstadoFisico(float temp, float umid) {
  float limiteTemp = 26.0; 
  float limiteUmid = 60.0; 

  if (temp < limiteTemp && umid < limiteUmid) { 
    return true; // "Parece ligado"
  } else {
    return false; // "Parece desligado"
  }
}


//FUNÇÃO BÁSICA DE EXEMPLO PARA AJUSTAR A TEMPERATURA com histerese (zona morta)
void AjustarTemperatura(float temp) {
  //Se ficar quente, esfria
  if ( arCondicionadoLigado ) {
    if (temp > 26.0) {
      mudarTemperatura(temperatura_3);
      //Se ficar frio, esquenta
    } else if (temp < 16.0 ) {
      mudarTemperatura(temperatura_2);
      }
  }
}

//função necessária para a biblioteca operar com mqtt
//serve para que a bilbioteca analise autoamticamente os dados recebidos pela conexão remota
//caso sejam enviados varios comandos em sequência só executará o útlimo, considerando que a automatização é para um ar condicioando isso adiciona segurança
void callback(char* topic, byte* payload, unsigned int length) {
  //converte os bits recebidos pelo callback para uma string, traduzindo a instrução do comando
  comandoRecebido = String((char*)payload, length);
  
  //
  chegouNovoComando = true;
}

void setup() {
  Serial.begin(115200);

  //inicia o conexão wifi
  //é necessário colocar a senha do wifi a ser conectado
  WiFi.begin(ssid, senha);
  client.setServer(server, porta);
  client.setCallback(callback);
  //trava o código aqui até conectar para evitar bugs de conexão
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  
  // Dizer ao ESP32 o que cada pino faz
  dht.begin();
  pinMode(pinoPIR, INPUT);  //O PIR envia sinal
  pinMode(pinoLED_IR, OUTPUT); 
  //O pino do DHT a própria biblioteca configura sozinha
  
  //ac.begin(); //Desnecessário nas bibliotecas modernas

  //definimos qual protocolo o controle vai usar
  ac.next.protocol = protocolo; 

  //Aplicamos as configurações iniciais usando os tipos universais
  //definindo o modo e velocidade no setup, para evitar erros de tratar eles como variáveis globais 
  ac.next.fanspeed = velocidadePadrao; 
  ac.next.mode     = modoPadrao;
  ac.next.celsius  = true; //definindo celsius como o sistema padrão de temperatura
  ac.next.degrees  = temperatura_1; //definindo a temperatura padrão, com a qual o ar deverá ligar
  ac.next.power    = false; //começa desligado na memória do ESP, não necessariamente condiz com a realidade do aparelho

  Serial.print("Sistema iniciado para o protocolo: ");
  Serial.println(typeToString(ac.next.protocol));

}

void loop() {

  //verificar conexão 

  if (!client.connected()) {
    //tenta conectar, o if só entra se a conexão der certo, condicional importante, pois tentar mandar os pacotes com a conexão desativada causará erros indesejados
    if (client.connect(Client_ID, mqtt_login, mqtt_senha, topico_estado.c_str(), 0, true, "0")) {
      //Se increve no tópico de comando correto
      client.subscribe(topico_comando.c_str()); 
      //Avisa que está ligado visualmente
      client.publish(topico_estado.c_str(), "1");
      
      //envia o pacote de descoberta (obrigatório para o site criar o Card)
      //monta o JSON: {"id":"...", "nome":"...", "prefixo":"..."}
      String jsonDiscovery = "{";
      jsonDiscovery += "\"id\": \"" + String(Client_ID) + "\",";
      // O nome pode ter espaços, o ID não. Aqui usei o ID como nome pra simplificar
      jsonDiscovery += "\"nome\": \"Ar Condicionado " + String(Client_ID) + "\","; 
      jsonDiscovery += "\"prefixo\": \"" + String(Client_ID) + "\"";
      jsonDiscovery += "}";
      
      //envia para o canal global
      client.publish(topico_discovery, jsonDiscovery.c_str(), true);
      Serial.println("Pacote de descoberta enviado: " + jsonDiscovery);
      } else {
      Serial.println(client.state()); //retorna um numero que explica o erro no terminal
      delay(3000); //espera 2 segundos antes de tentar de novo para não travar o chip nem ser bloqueado no servidor
    }
  }

 //Verifica buffer
  if (chegouNovoComando == true) {
    //muda o estado de chegou novo comando para false para evitar repetições de comands antigos
    chegouNovoComando = false;
    //se outros comandos chegarem remotamente durante a verificação no esperar(), chamado pelos próprios comandos, eles serão executados

    // Agora estamos no Loop Principal, é seguro demorar o quanto quiser!
    if (comandoRecebido == "LIGAR") {
      arCondicionadoLigado = true;
      ligarAr(); // Essa função usa esperar(), e AQUI é seguro usar.
    }
    else if (comandoRecebido == "DESLIGAR") {
      arCondicionadoLigado = false;
      desligarAr();
      }
  }
  
  //chama a função luz
  gerenciarArCondicionado();
  
 esperar(100);

  //DHT é lento, então é importante esperar mais que um único loop para ler ele
  static uint8_t contadorTempo = 0;
  contadorTempo++;
  
  if (contadorTempo > 20) {
    TemperaturaMedida = dht.readTemperature();
    UmidadeMedida = dht.readHumidity();
    
    // Leitura atual do PIR
    bool leituraPIR = digitalRead(pinoPIR);
    if (leituraPIR) {
      Serial.println("Tem gente");
    } else {
      Serial.println("Não tem gente");
    }

 
    //se a temperatura e umidade foram medidaas, ai atualiza o estado físico, condicional importante para evitar erros
    if (!isnan(TemperaturaMedida) && !isnan(UmidadeMedida)) {


       //ENVIO MQTT
      //verificamos se o cliente está conectado antes de tentar enviar
      if (client.connected()) {
        
        //O mqtt não suporta o tipo double, então as variáveis são convertidas para String antes de enviar com o publish
        // O tópico TAMBÉM precisa do .c_str() agora
        client.publish(topico_temperatura.c_str(), String(TemperaturaMedida).c_str());
        client.publish(topico_umidade.c_str(),     String(UmidadeMedida).c_str());

        // Para o estado (ligado/desligado)
        client.publish(topico_estado.c_str(),      arCondicionadoLigado ? "1" : "0");
      }

      
   
      // Atualiza a dedução física
      pareceLigado = DeduzirEstadoFisico(TemperaturaMedida, UmidadeMedida);
      AjustarTemperatura(TemperaturaMedida);

      Serial.println("----Dados de exibição-----");
      Serial.print("Temp: ");
      Serial.print(TemperaturaMedida);
      Serial.print("C ");
      Serial.print("Umidade: ");
      Serial.println(UmidadeMedida);
      Serial.print("Estado registrado: ");
      Serial.print(arCondicionadoLigado);
      Serial.print(" Estado aparente: ");
      Serial.println(pareceLigado);

      
 
      
      Serial.println("----------------------------------------");
    }
    contadorTempo = 0; //reseta o contador para rodar o dht posteriormente
  }
  
} 
